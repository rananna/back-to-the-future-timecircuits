// main_ui.js

// Global state variables for UI management
let animationPreviewInterval = null;
let isLoading = true;
let isDataLinkLoaded = false;
let settingsChanged = false;
let weatherInterval;
let activeWizardTarget = null;
let anyInputInvalid = false; // Flag to track if there are any invalid inputs in the forms

/**
 * Initializes the UI when the DOM is fully loaded.
 */
document.addEventListener('DOMContentLoaded', async () => {
    const isReady = await checkServerReady();
    if (isReady) {
        initializeUI();
    } else {
        document.body.innerHTML = '<div class="container"><h1>Connection Failed</h1><p>Could not connect to the Time Circuits device. Please check the connection and refresh the page.</p></div>';
        showMessage('Could not connect to device.', 'error', 10000);
    }
});

/**
 * Initializes the main UI components and fetches initial data.
 */
async function initializeUI() {
    try {
        const initialEndpoints = [
            '/api/settings/timecircuits', '/api/settings/temporal',
            '/api/settings/datalink', '/api/timezones',
            '/api/getPresets', '/api/getTheme', '/api/api_examples'
        ];
        const promises = initialEndpoints.map(url => fetch(url).then(res => {
            if (!res.ok) return Promise.reject(new Error(`Request to ${url} failed`));
            return url.endsWith('Theme') ? res.text() : res.json();
        }));

        const [timecircuits, temporal, datalink, timezones, presets, theme, examples] = await Promise.all(promises);

        window.apiExamples = examples;
        document.body.className = theme.trim();
        populateTimezoneSelects(timezones);
        populatePresetsSelect(presets);
        await applySettings(timecircuits, temporal, datalink);
        document.querySelector('.header-circuits').classList.add('visible');

        initWebSocket();

        fetchTime();
        setInterval(fetchTime, 1000);
        fetchWeatherData();
        weatherInterval = setInterval(fetchWeatherData, 300000);
        fetchSystemStatus();
        setInterval(fetchSystemStatus, 5000);
        attachEventListeners();
        showMessage('System Online', 'success');

    } catch (error) {
        console.error("CLIENT_DEBUG: Failed during essential initialization:", error);
        showMessage(`Critical error loading settings: ${error.message}. Please refresh.`, 'error');
    } finally {
        isLoading = false;
    }
}

/**
 * Populates the timezone select dropdowns.
 */
function populateTimezoneSelects(data) {
    timezoneOptions = [];
    const selects = [document.getElementById('presentTimezoneSelect'), document.getElementById('destinationTimezoneSelect')];
    selects.forEach(s => s.innerHTML = '');
    for (const country in data) {
        const optgroup = document.createElement('optgroup');
        optgroup.label = country;
        data[country].forEach(tz => {
            const option = document.createElement('option');
            option.value = tz.value;
            option.textContent = tz.text;
            optgroup.appendChild(option);
            timezoneOptions[tz.value] = tz;
        });
        selects.forEach(s => s.appendChild(optgroup.cloneNode(true)));
    }
}

/**
 * Populates the preset select dropdown.
 */
function populatePresetsSelect(data) {
    const select = document.getElementById('presetDateSelect');
    let customGroup = select.querySelector('optgroup[label="Custom Time Jumps"]');
    if (customGroup) customGroup.remove();
    if (data && data.length > 0) {
        customGroup = document.createElement('optgroup');
        customGroup.label = 'Custom Time Jumps';
        data.forEach(preset => {
            const option = document.createElement('option');
            option.value = preset.value;
            option.textContent = preset.name;
            customGroup.appendChild(option);
        });
        select.appendChild(customGroup);
    }
}

/**
 * Updates the "Last Time Departed" display.
 */
function updateLastDepartedDisplay(year, month, day, hour, minute) {
    const is24h = document.getElementById('displayFormat24h').checked;
    let displayHour = parseInt(hour, 10);
    let ampm = '';
    if (!is24h) {
        ampm = displayHour >= 12 ? 'PM' : 'AM';
        if (displayHour > 12) displayHour -= 12;
        if (displayHour === 0) displayHour = 12;
    }
    const monthStr = String(month).padStart(2, '0');
    const dayStr = String(day).padStart(2, '0');
    const hourStr = String(displayHour).padStart(2, '0');
    const minuteStr = String(minute).padStart(2, '0');
    document.getElementById('lastTimeDepartedDisplay').textContent = `${monthStr}/${dayStr}/${year} ${hourStr}:${minuteStr} ${ampm}`.trim();
    document.getElementById('lastTimeDepartedYear').textContent = year;
    document.getElementById('lastTimeDepartedMonth').textContent = month;
    document.getElementById('lastTimeDepartedDay').textContent = day;
    document.getElementById('lastTimeDepartedHour').textContent = hour;
    document.getElementById('lastTimeDepartedMinute').textContent = minute;
}

/**
 * Applies the fetched settings to the UI.
 */
async function applySettings(timecircuits, temporal, datalink) {
    if (timecircuits) {
        document.getElementById('destinationYear').value = timecircuits.destinationYear;
        document.getElementById('destinationTimezoneSelect').value = timecircuits.destinationTimezoneIndex;
        document.getElementById('presentTimezoneSelect').value = timecircuits.presentTimezoneIndex;
    }
    if (temporal) {
        document.getElementById('departureTime').value = `${String(temporal.departureHour).padStart(2, '0')}:${String(temporal.departureMinute).padStart(2, '0')}`;
        document.getElementById('arrivalTime').value = `${String(temporal.arrivalHour).padStart(2, '0')}:${String(temporal.arrivalMinute).padStart(2, '0')}`;
        ['brightness', 'notificationVolume', 'timeTravelAnimationDuration', 'timeTravelAnimationInterval', 'presetCycleInterval', 'glitchEffectFrequency', 'malfunctionFrequency'].forEach(id => {
            const slider = document.getElementById(id);
            if (slider) {
                slider.value = temporal[id];
                const valueSpan = document.getElementById(`${id}Value`);
                if (valueSpan) valueSpan.textContent = temporal[id];
            }
        });
        ['timeTravelSoundToggle', 'timeTravelVolumeFade', 'displayFormat24h'].forEach(id => {
            document.getElementById(id).checked = temporal[id];
        });
        document.getElementById('animationStyleSelect').value = temporal.animationStyle;
        document.getElementById('loggingEnabled').checked = temporal.loggingEnabled;
    }
    if (timecircuits) {
        updateLastDepartedDisplay(timecircuits.lastTimeDepartedYear, timecircuits.lastTimeDepartedMonth, timecircuits.lastTimeDepartedDay, timecircuits.lastTimeDepartedHour, timecircuits.lastTimeDepartedMinute);
    }
    if (datalink) {
        await applyDataLinkSettings(datalink);
        isDataLinkLoaded = true;
    }
    updateSleepVisual();
}

/**
 * Applies the fetched Data Link settings to the UI.
 */
async function applyDataLinkSettings(datalink) {
    if (!datalink) return;

    // Top-level Data Link settings
    document.getElementById('dataLinkEnabled').checked = datalink.dataLinkEnabled;
    document.getElementById('dataLinkRefreshInterval').value = datalink.dataLinkRefreshInterval;
    document.getElementById('dataLinkSettingsContainer').style.display = datalink.dataLinkEnabled ? 'block' : 'none';

    // MQTT settings
    document.getElementById('mqttBroker').value = datalink.mqttBroker || '';
    document.getElementById('mqttPort').value = datalink.mqttPort || 1883;
    document.getElementById('mqttUser').value = datalink.mqttUser || '';
    document.getElementById('mqttPassword').value = datalink.mqttPassword || '';

    // Weather settings
    document.getElementById('weatherModeEnabled').checked = datalink.weatherModeEnabled;
    document.getElementById('cityName').value = datalink.cityName || '';
    document.getElementById('useMetricUnits').checked = datalink.useMetricUnits;
    document.getElementById('weatherSettingsContainer').style.display = datalink.weatherModeEnabled ? 'block' : 'none';

    // Stock Ticker settings
    document.getElementById('stockTickerModeEnabled').checked = datalink.stockTickerModeEnabled;
    document.getElementById('alphaVantageApiKey').value = datalink.alphaVantageApiKey || '';
    document.getElementById('stockRow1_symbol').value = datalink.stockRow1_symbol || '';
    document.getElementById('stockRow2_symbol').value = datalink.stockRow2_symbol || '';
    document.getElementById('stockRow3_symbol').value = datalink.stockRow3_symbol || '';
    document.getElementById('stockTickerSettingsContainer').style.display = datalink.stockTickerModeEnabled ? 'block' : 'none';

    // Handle exclusive enabling of modes
    if (datalink.weatherModeEnabled) {
        document.getElementById('dataLinkGroup').classList.add('disabled');
        document.getElementById('stockTickerGroup').classList.add('disabled');
    } else if (datalink.dataLinkEnabled) {
        document.getElementById('weatherModeGroup').classList.add('disabled');
        document.getElementById('stockTickerGroup').classList.add('disabled');
    } else if (datalink.stockTickerModeEnabled) {
        document.getElementById('dataLinkGroup').classList.add('disabled');
        document.getElementById('weatherModeGroup').classList.add('disabled');
    }


    // Data Points settings
    const numDataPoints = datalink.numDataPoints || 0;
    document.getElementById('numDataPoints').value = numDataPoints;
    document.getElementById('numDataPointsValue').textContent = numDataPoints;

    await updateDataPointsUI(numDataPoints); // This is an async function now

    if (datalink.dataPoints && datalink.dataPoints.length > 0) {
        for (let i = 0; i < numDataPoints; i++) {
            const point = datalink.dataPoints[i];
            if (point) {
                // Set data source type
                const dataSourceSelect = document.getElementById(`dp_dataSourceType_${i}`);
                if (point.dataSourceType === 1) {
                    dataSourceSelect.value = 'mqtt';
                } else if (point.dataSourceType === 2) {
                    dataSourceSelect.value = 'ha';
                } else {
                    dataSourceSelect.value = 'api';
                }
                dataSourceSelect.dispatchEvent(new Event('change')); // Trigger visibility change

                // Set display mode
                 const displayModeSelect = document.getElementById(`dp_displayMode_${i}`);
                 displayModeSelect.value = point.displayMode || 0;
                 displayModeSelect.dispatchEvent(new Event('change'));


                // Populate fields
                document.getElementById(`dp_url_${i}`).value = point.url || '';
                document.getElementById(`dp_monthPath_${i}`).value = point.monthPath || '';
                document.getElementById(`dp_dayPath_${i}`).value = point.dayPath || '';
                document.getElementById(`dp_yearPath_${i}`).value = point.yearPath || '';
                document.getElementById(`dp_timePath_${i}`).value = point.timePath || '';
                document.getElementById(`dp_prefix_${i}`).value = point.prefix || '';
                document.getElementById(`dp_suffix_${i}`).value = point.suffix || '';
                document.getElementById(`dp_icon_${i}`).value = point.icon || '';
                document.getElementById(`dp_scrollSpeed_${i}`).value = point.scrollSpeed || 150;
                document.getElementById(`dp_scrollSpeed_${i}Value`).textContent = point.scrollSpeed || 150;
                document.getElementById(`dp_mqttTopic_${i}`).value = point.mqttTopic || '';
                document.getElementById(`dp_yearPrefix_${i}`).value = point.yearPrefix || '';
                document.getElementById(`dp_yearSuffix_${i}`).value = point.yearSuffix || '';
                document.getElementById(`dp_scrollingText_${i}`).value = point.scrollingText || '';
                document.getElementById(`dp_authHeaderKey_${i}`).value = point.authHeaderKey || '';
                document.getElementById(`dp_authHeaderValue_${i}`).value = point.authHeaderValue || '';
                document.getElementById(`api_example_${i}`).value = point.apiExampleKey || '';

                updateMarqueePreview(i);
            }
        }
    }
}

/**
 * Attaches all the necessary event listeners to the UI elements.
 */
function attachEventListeners() {
    document.getElementById('loggingEnabled').onchange = (e) => {
        const isEnabled = e.target.checked;
        const logContainer = document.getElementById('logContainer');
        if (ws && ws.readyState === WebSocket.OPEN) {
            ws.send(JSON.stringify({ action: 'setLogging', payload: isEnabled }));
        }
        if (!isEnabled) {
            logContainer.innerHTML = ''; // Clear the log when disabled
            showMessage('Log stream disabled.', 'info');
        } else {
            showMessage('Log stream enabled.', 'info');
        }
    };
    document.getElementById('header-dest').onclick = () => scrollToSettings('TimeCircuits', 'destinationTimeSettings');
    document.getElementById('header-pres').onclick = () => scrollToSettings('System', 'presentTimeSettings');
    document.getElementById('header-last').onclick = () => scrollToSettings('TimeCircuits', 'lastDepartedSettings');
    document.getElementById('greatScottBtn').onclick = () => fetch('/api/greatScott', { method: 'POST' });
    document.getElementById('saveSettingsBtn').onclick = saveSettings;
    document.querySelectorAll('.tab-link').forEach(btn => btn.onclick = (e) => {
        const tabName = e.target.getAttribute('data-tab');
        openTab(e, tabName);
        if (tabName === 'DataLink' && !isDataLinkLoaded) loadDataLinkSettings();
    });
    ['destinationTimezoneSelect', 'presentTimezoneSelect'].forEach(id => {
        document.getElementById(id).onchange = () => {
            if (!isLoading) setSettingsChanged(true);
            updateHeaderClocks(new Date());
        };
    });
    document.getElementById('destinationYear').oninput = () => updateHeaderClocks(new Date());
    document.getElementById('presetDateSelect').onchange = handlePresetSelectionChange;
    document.getElementById('savePresetBtn').onclick = handleSavePreset;
    document.getElementById('deletePresetBtn').onclick = deletePreset;
    document.getElementById('newPresetBtn').onclick = resetPresetForm;
    document.getElementById('refreshWeatherBtn').onclick = refreshWeatherData;
    document.getElementById('testAllDataPointsBtn').onclick = testAllDataPoints;
    document.getElementById('displayFormat24h').addEventListener('change', () => {
        const year = document.getElementById('lastTimeDepartedYear').textContent;
        const month = document.getElementById('lastTimeDepartedMonth').textContent;
        const day = document.getElementById('lastTimeDepartedDay').textContent;
        const hour = document.getElementById('lastTimeDepartedHour').textContent;
        const minute = document.getElementById('lastTimeDepartedMinute').textContent;
        if (year && month && day && hour && minute) {
            updateLastDepartedDisplay(year, month, day, hour, minute);
        }
        fetchTime();
    });
    document.getElementById('weatherModeEnabled').onchange = (e) => {
        const isChecked = e.target.checked;
        document.getElementById('weatherSettingsContainer').style.display = isChecked ? 'block' : 'none';
        document.getElementById('dataLinkGroup').classList.toggle('disabled', isChecked);
        document.getElementById('stockTickerGroup').classList.toggle('disabled', isChecked);
        if (isChecked) {
            document.getElementById('dataLinkEnabled').checked = false;
            document.getElementById('dataLinkSettingsContainer').style.display = 'none';
            document.getElementById('stockTickerModeEnabled').checked = false;
            document.getElementById('stockTickerSettingsContainer').style.display = 'none';
            document.getElementById('weatherModeGroup').classList.remove('disabled');
            fetchWeatherData();
        }
        if (!isLoading) setSettingsChanged(true);
    };
    document.getElementById('dataLinkEnabled').onchange = (e) => {
        const isChecked = e.target.checked;
        document.getElementById('dataLinkSettingsContainer').style.display = isChecked ? 'block' : 'none';
        document.getElementById('weatherModeGroup').classList.toggle('disabled', isChecked);
        document.getElementById('stockTickerGroup').classList.toggle('disabled', isChecked);
        if (isChecked) {
            document.getElementById('weatherModeEnabled').checked = false;
            document.getElementById('weatherSettingsContainer').style.display = 'none';
            document.getElementById('stockTickerModeEnabled').checked = false;
            document.getElementById('stockTickerSettingsContainer').style.display = 'none';
            document.getElementById('dataLinkGroup').classList.remove('disabled');
        }
        if (!isLoading) setSettingsChanged(true);
    };
    document.getElementById('stockTickerModeEnabled').onchange = (e) => {
        const isChecked = e.target.checked;
        document.getElementById('stockTickerSettingsContainer').style.display = isChecked ? 'block' : 'none';
        document.getElementById('dataLinkGroup').classList.toggle('disabled', isChecked);
        document.getElementById('weatherModeGroup').classList.toggle('disabled', isChecked);
        if (isChecked) {
            document.getElementById('dataLinkEnabled').checked = false;
            document.getElementById('dataLinkSettingsContainer').style.display = 'none';
            document.getElementById('weatherModeEnabled').checked = false;
            document.getElementById('weatherSettingsContainer').style.display = 'none';
            document.getElementById('stockTickerGroup').classList.remove('disabled');
        }
        if (!isLoading) setSettingsChanged(true);
    };
    document.getElementById('stockTickerSettingsContainer').addEventListener('click', function(event) {
        if (event.target && event.target.classList.contains('fetch-stock-btn')) {
            fetchStockQuote(event);
        }
    });
    document.getElementById('numDataPoints').oninput = (e) => {
        document.getElementById('numDataPointsValue').textContent = e.target.value;
        updateDataPointsUI(parseInt(e.target.value, 10));
        if (!isLoading) setSettingsChanged(true);
    };
    document.querySelectorAll('input, select').forEach(el => {
        el.addEventListener('change', () => {
            if (!isLoading) setSettingsChanged(true);
        });
        el.addEventListener('input', (e) => {
            if (!isLoading) {
                const valueSpan = document.getElementById(`${e.target.id}Value`);
                if (valueSpan) valueSpan.textContent = e.target.value;
                setSettingsChanged(true);
            }
        });
    });
    document.getElementById('resetDefaultsBtn').onclick = () => {
        if (confirm("Are you sure? This will reset all settings to their defaults.")) {
            fetch('/api/resetSettings', { method: 'POST' })
                .then(res => res.text()).then(text => {
                    showMessage(text, 'success');
                    setTimeout(() => window.location.reload(), 1500);
                });
        }
    };
    document.getElementById('syncNtpBtn').onclick = () => {
        fetch('/api/syncTime', { method: 'POST' }).then(res => res.text()).then(text => showMessage(text, 'info'));
    };
    document.querySelectorAll('.theme-option').forEach(el => {
        el.onclick = () => {
            const theme = el.getAttribute('data-theme');
            document.body.className = theme;
            fetch('/api/setTheme', { method: 'POST', body: new URLSearchParams({ theme }) });
        };
    });
    document.getElementById('departureTime').onchange = updateSleepVisual;
    document.getElementById('arrivalTime').onchange = updateSleepVisual;
    document.addEventListener('keydown', (e) => {
        if (e.key === "Escape" && activeWizardTarget) {
            activeWizardTarget.classList.remove('is-wizard-target');
            activeWizardTarget = null;
            showMessage('Wizard target deselected.', 'info', 2000);
        }
    });
    document.getElementById('previewAnimationBtn').onclick = previewAnimationStyle;
}

/**
 * Handles preset selection change.
 */
function handlePresetSelectionChange(event) {
    applySelectedPreset(event);
    const select = event.target;
    const selectedOption = select.options[select.selectedIndex];
    const isCustomPreset = selectedOption.parentElement.label === 'Custom Time Jumps';

    if (isCustomPreset) {
        document.getElementById('presetFormTitle').textContent = 'Edit Selected Preset';
        document.getElementById('savePresetBtn').textContent = 'Update Preset';
        document.getElementById('deletePresetBtn').classList.remove('hidden');
        document.getElementById('newPresetBtn').classList.remove('hidden');
        document.getElementById('presetName').value = selectedOption.textContent;
        const [year, month, day, hour, minute] = selectedOption.value.split('-');
        document.getElementById('presetDate').value = `${year}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
        document.getElementById('presetTime').value = `${String(hour).padStart(2, '0')}-${String(minute).padStart(2, '0')}`;
    } else {
        resetPresetForm(false);
    }
}

/**
 * Handles save preset button click.
 */
function handleSavePreset() {
    const select = document.getElementById('presetDateSelect');
    const selectedOption = select.options[select.selectedIndex];
    const isCustomPreset = selectedOption.parentElement.label === 'Custom Time Jumps';
    if (isCustomPreset) {
        updatePreset();
    } else {
        addPreset();
    }
}

/**
 * Resets the preset form.
 */
function resetPresetForm(resetDropdown = true) {
    document.getElementById('presetFormTitle').textContent = 'Add a New Custom Time Jump';
    document.getElementById('savePresetBtn').textContent = 'Add to Presets';
    document.getElementById('deletePresetBtn').classList.add('hidden');
    document.getElementById('newPresetBtn').classList.add('hidden');
    ['presetName', 'presetDate', 'presetTime'].forEach(id => document.getElementById(id).value = '');
    if (resetDropdown) {
        document.getElementById('presetDateSelect').value = '';
    }
}

/**
 * Applies the selected preset.
 */
function applySelectedPreset(event) {
    const select = event.target;
    if (!select.value) return;
    const [year, month, day, hour, minute] = select.value.split('-');
    updateLastDepartedDisplay(year, month, day, hour, minute);
    showMessage(`Last Time Departed set to: ${select.options[select.selectedIndex].text}`, 'info');
    if (!isLoading) setSettingsChanged(true);
    updateHeaderClocks(new Date());
}

/**
 * Scrolls to a specific setting.
 */
function scrollToSettings(tabName, elementId) {
    const tabButton = document.querySelector(`.tab-link[data-tab='${tabName}']`);
    if (tabButton) {
        openTab({ currentTarget: tabButton }, tabName);
        setTimeout(() => {
            const element = document.getElementById(elementId);
            if (element) {
                element.scrollIntoView({ behavior: 'smooth', block: 'center' });
                element.classList.add('highlight-saved');
                setTimeout(() => element.classList.remove('highlight-saved'), 2000);
            }
        }, 100);
    }
}

/**
 * Opens a tab.
 */
function openTab(evt, tabName) {
    document.querySelectorAll('.tab-content').forEach(tc => tc.style.display = "none");
    document.querySelectorAll('.tab-link').forEach(tl => tl.classList.remove('active'));
    document.getElementById(tabName).style.display = "block";
    evt.currentTarget.classList.add('active');
}

/**
 * Sets the settings changed flag.
 */
function setSettingsChanged(isChanged) {
    settingsChanged = isChanged;
    document.getElementById('saveSettingsBtn').disabled = !isChanged;
    if (isChanged) {
        document.getElementById('saveSettingsBtn').classList.add('needs-save');
    } else {
        document.getElementById('saveSettingsBtn').classList.remove('needs-save');
    }
}

/**
 * Updates the header clocks.
 */
function updateHeaderClocks(presentTimeRaw) {
    const months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
    const is24h = document.getElementById('displayFormat24h').checked;

    const populateHeaderRow = (prefix, unixTimestamp, yearOverride = null) => {
        const timezoneSelectId = (prefix === 'dest') ? 'destinationTimezoneSelect' : 'presentTimezoneSelect';
        const timezoneSelect = document.getElementById(timezoneSelectId);
        if (!timezoneSelect || timezoneOptions.length === 0) return;
        const timezoneIndex = parseInt(timezoneSelect.value, 10) || 0;
        const formatted = formatDateTimeInTimezone(unixTimestamp, timezoneIndex, is24h);
        if (!formatted) return;
        const [hour, minute] = formatted.time.split(' ')[0].split(':');
        const ampm = (formatted.time.split(' ').length > 1 ? formatted.time.split(' ')[1] : '').trim();
        const [monthNum, day, year] = formatted.date.split('/');
        const setContent = (id, text) => { document.getElementById(id).textContent = text; };
        setContent(`header-${prefix}-month`, months[parseInt(monthNum, 10) - 1] || '---');
        setContent(`header-${prefix}-day`, day || '00');
        setContent(`header-${prefix}-year`, yearOverride || year || '0000');
        setContent(`header-${prefix}-hour`, hour || '00');
        setContent(`header-${prefix}-minute`, minute || '00');
        setContent(`header-${prefix}-ampm`, is24h ? '' : ampm);
    };

    const presentUnixTimestamp = presentTimeRaw.getTime() / 1000;
    populateHeaderRow('pres', presentUnixTimestamp);

    const destYearInput = document.getElementById('destinationYear');
    if (destYearInput && destYearInput.value) {
        const destinationTime = new Date(presentTimeRaw.getTime());
        destinationTime.setFullYear(parseInt(destYearInput.value, 10));
        populateHeaderRow('dest', destinationTime.getTime() / 1000, destYearInput.value);
    }

    const lastYear = document.getElementById('lastTimeDepartedYear').textContent;
    const lastMonth = parseInt(document.getElementById('lastTimeDepartedMonth').textContent, 10);
    const lastDay = document.getElementById('lastTimeDepartedDay').textContent;
    const lastHour = parseInt(document.getElementById('lastTimeDepartedHour').textContent, 10);
    const lastMinute = document.getElementById('lastTimeDepartedMinute').textContent;

    if (lastYear && !isNaN(lastMonth) && lastDay && !isNaN(lastHour) && lastMinute) {
        let displayHour = lastHour;
        let ampm = '';
        if (!is24h) {
            ampm = displayHour >= 12 ? 'PM' : 'AM';
            if (displayHour > 12) displayHour -= 12;
            if (displayHour === 0) displayHour = 12;
        }
        const setContent = (id, text) => { document.getElementById(id).textContent = text; };
        setContent('header-last-month', months[lastMonth - 1] || '---');
        setContent('header-last-day', String(lastDay).padStart(2, '0'));
        setContent('header-last-year', lastYear);
        setContent('header-last-hour', String(displayHour).padStart(2, '0'));
        setContent('header-last-minute', String(lastMinute).padStart(2, '0'));
        setContent('header-last-ampm', is24h ? '' : ampm);
    }

    const now = new Date();
    const totalMinutes = now.getHours() * 60 + now.getMinutes();
    document.getElementById('currentTimeMarker').style.left = `${(totalMinutes / 1440) * 100}%`;
}

/**
 * Formats date and time in a specific timezone.
 */
function formatDateTimeInTimezone(unixTimestamp, timezoneIndex, is24HourFormat) {
    if (!timezoneOptions || timezoneIndex < 0 || !timezoneOptions[timezoneIndex]) return null;
    const tzIANA = timezoneOptions[timezoneIndex].ianaTzName;
    const dateObj = new Date(unixTimestamp * 1000);
    try {
        const timeOptions = { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: !is24HourFormat, timeZone: tzIANA };
        const dateOptions = { year: 'numeric', month: '2-digit', day: '2-digit', timeZone: tzIANA };
        return { time: dateObj.toLocaleTimeString('en-US', timeOptions), date: dateObj.toLocaleDateString('en-US', dateOptions) };
    } catch (e) { return { time: "Error", date: "Error" }; }
}

/**
 * Updates the data points UI.
 */
function updateDataPointsUI(numPoints) {
    return new Promise((resolve) => {
        const container = document.getElementById('dataPointsConfigContainer');
        container.innerHTML = '';
        if (numPoints > 0) {
            for (let i = 0; i < numPoints; i++) {
                if (!dataPointStateCache[i]) {
                    dataPointStateCache[i] = { modifiedUrls: {} };
                }
                const block = document.createElement('div');
                block.className = 'setting-group data-point-block collapsed';
                block.innerHTML = `
                    <div class="dp-header">
                        <div class="dp-title-group">
                            <span class="dp-status-indicator" id="dp_status_${i}"></span>
                            <h4>Data Point ${i + 1}</h4>
                        </div>
                        <div class="dp-action-bar">
                            <button class="action-button dp-clear-btn" data-index="${i}">Clear</button>
                            <button class="action-button dp-dup-btn" data-index="${i}">Duplicate</button>
                            <button class="action-button dp-test-btn" data-index="${i}">Test</button>
                        </div>
                    </div>
                    <label for="dp_dataSourceType_${i}">Data Source:</label>
                    <select id="dp_dataSourceType_${i}" class="data-source-select" data-index="${i}">
                        <option value="api">Web API (HTTP)</option>
                        <option value="mqtt">MQTT Broker</option>
                        <option value="ha">Home Assistant Push</option>
                    </select>

                    <div id="dp_api_container_${i}">
                        <label for="api_example_${i}">API Examples (optional):</label>
                        <select id="api_example_${i}" class="api-example-select" data-index="${i}"></select>
                        <label for="dp_url_${i}">API URL:</label>
                        <input type="text" id="dp_url_${i}" placeholder="http://api.example.com/data.json">
                        <label for="dp_authHeaderKey_${i}">Auth Header Key (optional):</label>
                        <input type="text" id="dp_authHeaderKey_${i}" placeholder="e.g., X-API-Key">
                        <label for="dp_authHeaderValue_${i}">Auth Header Value (optional):</label>
                        <input type="text" id="dp_authHeaderValue_${i}" placeholder="e.g., your-api-key">
                        <button class="analyze-api-btn" data-index="${i}">Analyze API</button>
                        <div class="api-wizard-results" id="wizard_results_${i}"></div>
                    </div>

                    <div id="dp_mqtt_container_${i}" style="display: none;">
                        <label for="dp_mqttTopic_${i}">MQTT Topic:</label>
                        <input type="text" id="dp_mqttTopic_${i}" placeholder="e.g., /home/livingroom/temperature">
                    </div>

                    <label for="dp_displayMode_${i}" style="margin-top: 20px;">Display Mode:</label>
                    <select id="dp_displayMode_${i}" class="display-mode-select" data-index="${i}">
                        <option value="0">Four Column Data</option>
                        <option value="1">Scrolling Text</option>
                    </select>

                    <div class="display-mode-container" id="four_column_container_${i}">
                        <div class="time-circuit-row">
                            <label for="dp_monthPath_${i}" class="time-circuit-label">MONTH</label>
                            <input type="text" id="dp_monthPath_${i}" class="time-circuit-input wizard-target-input" maxlength="3">
                        </div>
                        <div class="time-circuit-row">
                            <label for="dp_dayPath_${i}" class="time-circuit-label">DAY</label>
                            <input type="text" id="dp_dayPath_${i}" class="time-circuit-input wizard-target-input" maxlength="2">
                            <select id="dp_icon_${i}" class="icon-select" data-index="${i}" style="width: 100px; margin-left: 10px;">
                                <option value="">No Icon</option>
                                <option value="SU">Sun</option>
                                <option value="CL">Cloud</option>
                                <option value="RN">Rain</option>
                                <option value="SN">Snow</option>
                                <option value="ST">Storm</option>
                                <option value="WD">Wind</option>
                                <option value="^">Up Arrow</option>
                                <option value="v">Down Arrow</option>
                                <option value="==">Equal</option>
                                <option value="WF">WiFi</option>
                                <option value="HM">Home</option>
                                <option value="WK">Work</option>
                                <option value="CR">Car</option>
                                <option value="BK">Bike</option>
                                <option value="RN">Run</option>
                                <option value="<3">Heart</option>
                                <option value="$$">Money</option>
                                <option value="TM">Clock</option>
                                <option value="DT">Calendar</option>
                            </select>
                        </div>
                        <div class="time-format-group">
                            <div class="time-circuit-row">
                                <label for="dp_yearPath_${i}" class="time-circuit-label">YEAR</label>
                                <input type="text" id="dp_yearPath_${i}" class="time-circuit-input wizard-target-input">
                            </div>
                            <div class="time-circuit-row">
                                <label for="dp_yearPrefix_${i}" class="time-circuit-label">[PREFIX]</label>
                                <input type="text" id="dp_yearPrefix_${i}" class="time-circuit-input" maxlength="15">
                            </div>
                            <div class="time-circuit-row">
                                <label for="dp_yearSuffix_${i}" class="time-circuit-label">[SUFFIX]</label>
                                <input type="text" id="dp_yearSuffix_${i}" class="time-circuit-input" maxlength="15">
                            </div>
                        </div>
                        <div class="time-format-group">
                            <div class="time-circuit-row">
                                <label for="dp_timePath_${i}" class="time-circuit-label">TIME</label>
                                <input type="text" id="dp_timePath_${i}" class="time-circuit-input wizard-target-input">
                            </div>
                            <div class="time-circuit-row">
                                <label for="dp_prefix_${i}" class="time-circuit-label">[PREFIX]</label>
                                <input type="text" id="dp_prefix_${i}" class="time-circuit-input" maxlength="15">
                            </div>
                            <div class="time-circuit-row">
                                <label for="dp_suffix_${i}" class="time-circuit-label">[SUFFIX]</label>
                                <input type="text" id="dp_suffix_${i}" class="time-circuit-input" maxlength="15">
                            </div>
                        </div>
                        <div class="marquee-preview-container">
                            <label>Live Preview:</label>
                            <div class="marquee-preview" id="marquee_preview_${i}">
                                <span class="preview-month">MON</span>
                                <span class="preview-day">DAY</span>
                                <div class="preview-year-container"><span class="preview-year">YEAR</span></div>
                                <div class="preview-time-container"><span class="preview-time">TIME</span></div>
                            </div>
                        </div>
                    </div>

                    <div class="display-mode-container" id="scrolling_text_container_${i}" style="display: none;">
                        <label for="dp_scrollingText_${i}" style="margin-top: 15px;">Scrolling Text:</label>
                        <input type="text" id="dp_scrollingText_${i}" class="wizard-target-input" placeholder="Enter text or map a value...">
                        <div class="marquee-preview-container">
                            <label>Live Preview (13 Chars):</label>
                            <div class="marquee-preview-13" id="marquee_preview_13_${i}">
                                <span class="preview-scrolling-text">PREVIEW</span>
                            </div>
                        </div>
                    </div>

                    <label for="dp_scrollSpeed_${i}" style="margin-top: 20px;">Scroll Speed (ms/char): <span id="dp_scrollSpeed_${i}Value">150</span></label>
                    <input type="range" id="dp_scrollSpeed_${i}" min="50" max="500" step="10" value="150">
                `;
                container.appendChild(block);
            }
            populateApiExampleDropdowns();
            attachDataPointEventListeners();
            for (let i = 0; i < numPoints; i++) {
                updateMarqueePreview(i);
            }
        }
        resolve();
    });
}

/**
 * Gets the display value for a path.
 */
function getDisplayValue(path, placeholder, index) {
    if (!path) return placeholder;
    if (analyzedDataCache[index] !== undefined) {
        const resolvedValue = getValueFromPath(analyzedDataCache[index], path);
        if (resolvedValue !== null && resolvedValue !== undefined) {
            return resolvedValue;
        }
    }
    if (path.includes('.') || path.includes('[')) {
        return placeholder;
    } else {
        return path;
    }
}

/**
 * Updates the marquee preview.
 */
function updateMarqueePreview(index) {
    const displayMode = document.getElementById(`dp_displayMode_${index}`).value;
    if (displayMode === '0') {
        const monthPath = document.getElementById(`dp_monthPath_${index}`).value;
        const dayPath = document.getElementById(`dp_dayPath_${index}`).value;
        const yearPath = document.getElementById(`dp_yearPath_${index}`).value;
        const timePath = document.getElementById(`dp_timePath_${index}`).value;
        const icon = document.getElementById(`dp_icon_${index}`).value;
        let monthValue = getDisplayValue(monthPath, 'MON', index);
        let dayValue = getDisplayValue(dayPath, 'DAY', index);
        let yearValue = getDisplayValue(yearPath, 'YEAR', index);
        let timeValue = getDisplayValue(timePath, 'TIME', index);
        const yearPrefix = document.getElementById(`dp_yearPrefix_${index}`).value;
        const yearSuffix = document.getElementById(`dp_yearSuffix_${index}`).value;
        const yearFinalValue = `${yearPrefix}${yearValue}${yearSuffix}`;
        const prefix = document.getElementById(`dp_prefix_${index}`).value;
        const suffix = document.getElementById(`dp_suffix_${index}`).value;
        const timeFinalValue = `${prefix}${timeValue}${suffix}`;
        document.querySelector(`#marquee_preview_${index} .preview-month`).textContent = String(monthValue).substring(0, 3).toUpperCase();
        const dayPreview = document.querySelector(`#marquee_preview_${index} .preview-day`);
        if (icon) {
            dayPreview.textContent = icon;
        } else {
            dayPreview.textContent = String(dayValue).substring(0, 2).toUpperCase();
        }
        const setupScrolling = (text, valueSpan) => {
            valueSpan.textContent = text;
            valueSpan.classList.remove('scrolling-text');
            if (text.length > 4) {
                const scrollSpeed = document.getElementById(`dp_scrollSpeed_${index}`).value;
                const duration = (text.length + 4) * (scrollSpeed / 1000);
                valueSpan.style.animationDuration = `${duration}s`;
                requestAnimationFrame(() => { valueSpan.classList.add('scrolling-text'); });
            }
        };
        setupScrolling(yearFinalValue, document.querySelector(`#marquee_preview_${index} .preview-year`));
        setupScrolling(timeFinalValue, document.querySelector(`#marquee_preview_${index} .preview-time`));
    } else {
        const scrollingPath = document.getElementById(`dp_scrollingText_${index}`).value;
        const text = getDisplayValue(scrollingPath, 'PREVIEW', index);
        const previewSpan = document.querySelector(`#marquee_preview_13_${index} .preview-scrolling-text`);
        previewSpan.textContent = text;
        previewSpan.classList.remove('scrolling-text');
        if (text.length > 13) {
            const scrollSpeed = document.getElementById(`dp_scrollSpeed_${index}`).value;
            const duration = (text.length) * (scrollSpeed / 100);
            previewSpan.style.animationDuration = `${duration}s`;
            requestAnimationFrame(() => {
                previewSpan.classList.add('scrolling-text');
            });
        }
    }
}

/**
 * Populates the API example dropdowns.
 */
function populateApiExampleDropdowns() {
    document.querySelectorAll('.api-example-select').forEach(select => {
        select.innerHTML = '';
        const defaultOption = document.createElement('option');
        defaultOption.value = '';
        defaultOption.textContent = '-- Select an Example --';
        select.appendChild(defaultOption);
        for (const key in window.apiExamples) {
            const option = document.createElement('option');
            option.value = key;
            option.textContent = window.apiExamples[key].name;
            select.appendChild(option);
        }
    });
}

/**
 * Attaches event listeners to the data point UI elements.
 */
function attachDataPointEventListeners() {
    document.querySelectorAll('.data-source-select, .display-mode-select').forEach(select => {
        select.onchange = (e) => {
            const index = e.target.dataset.index;
            const dataSource = document.getElementById(`dp_dataSourceType_${index}`).value;
            const displayMode = document.getElementById(`dp_displayMode_${index}`).value;
            document.getElementById(`dp_api_container_${index}`).style.display = dataSource === 'api' ? 'block' : 'none';
            document.getElementById(`dp_mqtt_container_${index}`).style.display = dataSource === 'mqtt' ? 'block' : 'none';
            if (dataSource === 'ha') {
                document.getElementById(`dp_api_container_${index}`).style.display = 'none';
                document.getElementById(`dp_mqtt_container_${index}`).style.display = 'none';
            }
            document.getElementById(`four_column_container_${index}`).style.display = displayMode === '0' ? 'block' : 'none';
            document.getElementById(`scrolling_text_container_${index}`).style.display = displayMode === '1' ? 'block' : 'none';
            updateMarqueePreview(index);
        };
    });
    document.querySelectorAll('.icon-select').forEach(select => {
        select.onchange = (e) => {
            const index = e.target.dataset.index;
            const dayInput = document.getElementById(`dp_dayPath_${index}`);
            if (e.target.value) {
                dayInput.value = '';
                dayInput.disabled = true;
            } else {
                dayInput.disabled = false;
            }
        };
    });
    document.querySelectorAll('.analyze-api-btn').forEach(btn => btn.onclick = startApiWizard);
    document.querySelectorAll('.dp-clear-btn').forEach(btn => btn.onclick = clearDataPointFields);
    document.querySelectorAll('.dp-dup-btn').forEach(btn => btn.onclick = duplicateDataPoint);
    document.querySelectorAll('.dp-test-btn').forEach(btn => btn.onclick = testDataPoint);
    document.querySelectorAll('.api-example-select').forEach(select => {
        select.addEventListener('focus', (e) => {
            const index = e.target.dataset.index;
            lastFocusedApiExample[index] = e.target.value;
        });
        select.addEventListener('change', (e) => {
            const index = e.target.dataset.index;
            const urlInput = document.getElementById(`dp_url_${index}`);
            const previousKey = lastFocusedApiExample[index];
            if (previousKey) {
                dataPointStateCache[index].modifiedUrls[previousKey] = urlInput.value;
            }
            const newKey = e.target.value;
            const cachedUrl = dataPointStateCache[index].modifiedUrls[newKey];
            if (cachedUrl) {
                urlInput.value = cachedUrl;
            } else {
                const templateUrl = window.apiExamples[newKey]?.url;
                urlInput.value = templateUrl || '';
            }
        });
    });
    document.querySelectorAll('.data-point-block input, .data-point-block select, .data-point-block textarea').forEach(input => {
        input.addEventListener('input', (e) => {
            const indexMatch = e.target.id.match(/_(\d+)$/);
            if (!indexMatch) return;
            const index = indexMatch[1];
            if (e.target.id.startsWith('dp_dayPath_')) {
                document.getElementById(`dp_icon_${index}`).value = '';
            }
            if (e.target.id.includes('requestBody')) {
                validateJson(e.target);
            }
            updateMarqueePreview(index);
        });
    });
    document.querySelectorAll('.wizard-target-input').forEach(input => {
        input.addEventListener('click', (e) => {
            const clickedTarget = e.target;
            if (activeWizardTarget === clickedTarget) {
                activeWizardTarget.classList.remove('is-wizard-target');
                activeWizardTarget = null;
            } else {
                if (activeWizardTarget) {
                    activeWizardTarget.classList.remove('is-wizard-target');
                }
                activeWizardTarget = clickedTarget;
                activeWizardTarget.classList.add('is-wizard-target');
            }
        });
    });
    document.querySelectorAll('.dp-header').forEach(header => {
        header.onclick = (e) => {
            if (e.target.tagName === 'BUTTON') return;
            const block = header.closest('.data-point-block');
            block.classList.toggle('collapsed');
        };
    });
}

/**
 * Validates JSON input.
 */
function validateJson(textarea) {
    const validationMessage = document.getElementById(`${textarea.id}_validation`);
    try {
        if (textarea.value.trim() !== '') {
            JSON.parse(textarea.value);
        }
        textarea.classList.remove('invalid');
        validationMessage.textContent = '';
        return true;
    } catch (e) {
        textarea.classList.add('invalid');
        validationMessage.textContent = e.message;
        return false;
    }
}

/**
 * Displays the API wizard results.
 */
function displayApiWizardResults(index, jsonData) {
    const container = document.getElementById(`wizard_results_${index}`);
    const displayMode = document.getElementById(`dp_displayMode_${index}`).value;
    let instructions = '';
    if (displayMode === '0') {
        instructions = 'Click a form field (Month, Day, etc.), then click a value below to map it.';
    } else {
        instructions = 'Click the "Scrolling Text" field, then click a value below to map it.';
    }
    container.innerHTML = `<strong>${instructions}</strong>`;
    const mainList = document.createElement('ul');
    mainList.className = 'wizard-list';
    const buildListRecursive = (data, parentPath = '') => {
        const elements = [];
        if (Array.isArray(data)) {
            data.forEach((item, i) => {
                const currentPath = `${parentPath}[${i}]`;
                const li = document.createElement('li');
                if (typeof item === 'object' && item !== null) {
                    li.innerHTML = `<span class="wizard-key">[${i}]:</span>`;
                    const subList = document.createElement('ul');
                    const childElements = buildListRecursive(item, currentPath);
                    childElements.forEach(el => subList.appendChild(el));
                    li.appendChild(subList);
                } else {
                     li.innerHTML = `<span class="wizard-clickable-item" data-path="${currentPath}"><span class="wizard-key">${currentPath}:</span> <span class="wizard-value">"${String(item)}"</span></span>`;
                }
                elements.push(li);
            });
        } else if (typeof data === 'object' && data !== null) {
            for (const key in data) {
                const currentPath = parentPath ? `${parentPath}.${key}` : key;
                const value = data[key];
                const li = document.createElement('li');
                if (typeof value === 'object' && value !== null) {
                    li.innerHTML = `<span class="wizard-key">${key}:</span>`;
                    const subList = document.createElement('ul');
                    const childElements = buildListRecursive(value, currentPath);
                    childElements.forEach(el => subList.appendChild(el));
                    li.appendChild(subList);
                } else {
                    li.innerHTML = `<span class="wizard-clickable-item" data-path="${currentPath}"><span class="wizard-key">${currentPath}:</span> <span class="wizard-value">"${String(value)}"</span></span>`;
                }
                elements.push(li);
            }
        }
        return elements;
    };
    const allElements = buildListRecursive(jsonData);
    allElements.forEach(el => mainList.appendChild(el));
    container.appendChild(mainList);
    container.querySelectorAll('.wizard-clickable-item').forEach(item => {
        item.onclick = (e) => {
            if (activeWizardTarget) {
                const path = e.currentTarget.dataset.path;
                activeWizardTarget.value = path;
                activeWizardTarget.dispatchEvent(new Event('input'));
                showMessage(`Mapped "${path}" to the selected field.`, 'success', 2000);
                activeWizardTarget.classList.remove('is-wizard-target');
                activeWizardTarget = null;
            } else {
                showMessage('Click a form field first to select it as the target.', 'error');
            }
        };
    });
}

/**
 * Updates the sleep visual.
 */
function updateSleepVisual() {
    const depTime = document.getElementById('departureTime').value;
    const arrTime = document.getElementById('arrivalTime').value;
    if (!depTime || !arrTime) return;
    const [depH, depM] = depTime.split(':').map(Number);
    const [arrH, arrM] = arrTime.split(':').map(Number);
    const depTotalMins = depH * 60 + depM;
    const arrTotalMins = arrH * 60 + arrM;
    const bar1 = document.getElementById('sleepScheduleBar');
    const bar2 = document.getElementById('sleepScheduleBar2');
    if (arrTotalMins < depTotalMins) {
        const awakeDuration = depTotalMins - arrTotalMins;
        bar1.style.left = `${(arrTotalMins / 1440) * 100}%`;
        bar1.style.width = `${(awakeDuration / 1440) * 100}%`;
        bar2.style.display = 'none';
    } else {
        const firstAwakeDuration = depTotalMins;
        bar1.style.left = '0%';
        bar1.style.width = `${(firstAwakeDuration / 1440) * 100}%`;
        const secondAwakeDuration = 1440 - arrTotalMins;
        bar2.style.left = `${(arrTotalMins / 1440) * 100}%`;
        bar2.style.width = `${(secondAwakeDuration / 1440) * 100}%`;
        bar2.style.display = 'block';
    }
}

/**
 * Shows a loading indicator.
 */
function showLoading(buttonId, isLoading) {
    const button = document.getElementById(buttonId);
    if (!button) return;
    if (isLoading) {
        button.dataset.originalText = button.textContent;
        button.innerHTML = '<span class="loading-spinner"></span> Saving...';
        button.disabled = true;
    } else {
        button.textContent = button.dataset.originalText || 'Save';
        button.disabled = false;
    }
}

/**
 * Shows a message banner.
 */
function showMessage(message, type = 'info', duration = 4000) {
    const banner = document.getElementById('messageBanner');
    if (!banner) return;
    banner.textContent = message;
    banner.className = `message-banner ${type}`;
    banner.style.visibility = 'visible';
    banner.style.opacity = '1';
    if (banner.timer) clearTimeout(banner.timer);
    banner.timer = setTimeout(() => {
        banner.style.opacity = '0';
        setTimeout(() => banner.style.visibility = 'hidden', 500);
    }, duration);
}

/**
 * Updates the data point status indicator.
 */
function updateDataPointStatus(index, isSuccess) {
    const indicator = document.getElementById(`dp_status_${index}`);
    if (indicator) {
        indicator.className = 'dp-status-indicator';
        indicator.classList.add(isSuccess ? 'success' : 'error');
    }
    dataPointStatus[index] = isSuccess;
}

/**
 * Clears the data point fields.
 */
function clearDataPointFields(event) {
    const index = event.target.dataset.index;
    const fields = ['url', 'monthPath', 'dayPath', 'yearPath', 'timePath', 'prefix', 'suffix', 'icon', 'mqttTopic', 'yearPrefix', 'yearSuffix', 'scrollingText', 'authHeaderKey', 'authHeaderValue', 'requestBody'];
    fields.forEach(field => {
        const el = document.getElementById(`dp_${field}_${index}`);
        if (el) el.value = '';
    });
    document.getElementById(`wizard_results_${index}`).innerHTML = '';
    delete analyzedDataCache[index];
    updateMarqueePreview(index);
    showMessage(`Data Point ${parseInt(index) + 1} fields cleared.`, 'info');
    if (!isLoading) setSettingsChanged(true);
}

/**
 * Duplicates a data point.
 */
function duplicateDataPoint(event) {
    const sourceIndex = parseInt(event.target.dataset.index, 10);
    const numDataPoints = parseInt(document.getElementById('numDataPoints').value, 10);
    if (numDataPoints >= 5) {
        showMessage('Cannot duplicate, maximum number of data points reached.', 'error');
        return;
    }
    const targetIndex = numDataPoints;
    document.getElementById('numDataPoints').value = targetIndex + 1;
    document.getElementById('numDataPointsValue').textContent = targetIndex + 1;
    updateDataPointsUI(targetIndex + 1).then(() => {
        const fields = ['dataSourceType', 'displayMode', 'url', 'monthPath', 'dayPath', 'yearPath', 'timePath', 'prefix', 'suffix', 'icon', 'scrollSpeed', 'mqttTopic', 'yearPrefix', 'yearSuffix', 'scrollingText', 'authHeaderKey', 'authHeaderValue', 'api_example'];
        fields.forEach(field => {
            const sourceEl = document.getElementById(`dp_${field}_${sourceIndex}`);
            const targetEl = document.getElementById(`dp_${field}_${targetIndex}`);
            if (sourceEl && targetEl) {
                targetEl.value = sourceEl.value;
                targetEl.dispatchEvent(new Event('change'));
            }
        });
        showMessage(`Data Point ${sourceIndex + 1} duplicated to Data Point ${targetIndex + 1}.`, 'success');
        if (!isLoading) setSettingsChanged(true);
    });
}

/**
 * Previews the animation style.
 */
function previewAnimationStyle() {
    const style = document.getElementById('animationStyleSelect').value;
    const headerRows = [
        document.querySelectorAll('#header-dest .circuit-content span'),
        document.querySelectorAll('#header-pres .circuit-content span'),
        document.querySelectorAll('#header-last .circuit-content span')
    ];
    const allElements = document.querySelectorAll('.header-circuits .circuit-content span');
    const months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];

    if (animationPreviewInterval) {
        clearInterval(animationPreviewInterval);
        animationPreviewInterval = null;
    }
    allElements.forEach(el => {
        el.className = el.className.split(' ')[0];
        el.style.opacity = 1;
        el.style.transform = 'none';
    });
    
    let startTime = Date.now();
    const duration = 8000;
    const randomChar = (chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789") => chars[Math.floor(Math.random() * chars.length)];
    const randomString = (length, chars) => Array.from({ length }, () => randomChar(chars)).join('');

    const runPreview = () => {
        const elapsed = Date.now() - startTime;
        if (elapsed > duration) {
            clearInterval(animationPreviewInterval);
            animationPreviewInterval = null;
            allElements.forEach(el => {
                el.className = el.className.split(' ')[0];
                el.style.opacity = 1;
                el.style.transform = 'none';
            });
            updateHeaderClocks(new Date());
            return;
        }

        const progress = elapsed / duration;

        switch (style) {
            case '0': 
            case '1': 
            case '2':
            case '5':
                 allElements.forEach(el => {
                    if (Math.random() > 0.1) {
                        el.textContent = randomString(el.textContent.length);
                    }
                });
                break;
            case '3':
            case '9': {
                const startYear = (style === '3') ? new Date().getFullYear() : 1885;
                const endYear = (style === '3') ? startYear + 100 : (parseInt(document.getElementById('destinationYear').value, 10) || 2015);
                
                const totalYears = endYear - startYear;
                const yearsToAdd = totalYears * (1 - Math.pow(1 - progress, 3));
                
                const startDate = new Date(startYear, 0, 1);
                const daysToAdd = yearsToAdd * 365.25;
                
                const currentDate = new Date(startDate.getTime() + daysToAdd * 24 * 60 * 60 * 1000 + elapsed * 10000);

                const year = currentDate.getFullYear();
                const month = months[currentDate.getMonth()];
                const day = String(currentDate.getDate()).padStart(2, '0');
                const hour = String(currentDate.getHours()).padStart(2, '0');
                const minute = String(currentDate.getMinutes()).padStart(2, '0');
                
                ['header-dest', 'header-pres', 'header-last'].forEach(prefix => {
                    document.getElementById(`${prefix}-year`).textContent = String(year).padStart(4, '0');
                    document.getElementById(`${prefix}-month`).textContent = month;
                    document.getElementById(`${prefix}-day`).textContent = day;
                    document.getElementById(`${prefix}-hour`).textContent = hour;
                    document.getElementById(`${prefix}-minute`).textContent = minute;
                });
                break;
            }
            case '4':
            case '8':
                const wavePatterns = ["-___-", "_--_-", "__-__"];
                const waveIndex = Math.floor((elapsed / 200) % 3);
                const scrollOffset = Math.floor((elapsed / 100) % 5);
                
                headerRows.forEach((row, rowIndex) => {
                    let basePattern = wavePatterns[waveIndex];
                    if (rowIndex === 1) { 
                        basePattern = basePattern.split('').map(c => (c === '-') ? '_' : '-').join('');
                    }
                    let scrolledPattern = basePattern.slice(scrollOffset) + basePattern.slice(0, scrollOffset);
                    
                    row.forEach(el => {
                        el.textContent = scrolledPattern.repeat(Math.ceil(el.textContent.length / 5)).substring(0, el.textContent.length);
                    });
                });
                break;
            case '6':
                headerRows.forEach((row, rowIndex) => {
                    const phaseProgress = progress * 3 - rowIndex;
                    if (phaseProgress >= 0) {
                        row.forEach(el => {
                            const charsToShow = Math.ceil(el.textContent.length * Math.min(phaseProgress, 1));
                            el.textContent = '#'.repeat(charsToShow).padEnd(el.textContent.length, ' ');
                        });
                    } else {
                        row.forEach(el => el.textContent = ' '.repeat(el.textContent.length));
                    }
                });
                break;
            case '7':
                 allElements.forEach(el => el.textContent = randomString(el.textContent.length));
                 break;
        }
    };

    animationPreviewInterval = setInterval(runPreview, 250);
}

/**
 * Tests all data points.
 */
function testAllDataPoints() {
    const numDataPoints = parseInt(document.getElementById('numDataPoints').value, 10);
    if (numDataPoints === 0) {
        showMessage('No data points to test.', 'info');
        return;
    }
    showMessage(`Starting test for ${numDataPoints} data point(s)...`, 'info');
    for (let i = 0; i < numDataPoints; i++) {
        const testButton = document.querySelector(`.dp-test-btn[data-index="${i}"]`);
        if (testButton) {
            const syntheticEvent = { target: testButton };
            testDataPoint(syntheticEvent);
        }
    }
}

/**
 * Fetches the system status.
 */
function fetchSystemStatus() {
    fetch('/api/system/status')
        .then(res => {
            if (res.ok) {
                return res.json();
            }
            return Promise.reject('Failed to fetch system status');
        })
        .then(data => {
            document.getElementById('freeMemory').textContent = `${(data.freeHeap / 1024).toFixed(1)} KB`;
            document.getElementById('wifiSignal').textContent = `${data.rssi} dBm`;
            const uptime = data.uptime;
            const days = Math.floor(uptime / 86400);
            const hours = Math.floor((uptime % 86400) / 3600);
            const minutes = Math.floor((uptime % 3600) / 60);
            document.getElementById('deviceUptime').textContent = `${days}d ${hours}h ${minutes}m`;
        })
        .catch(err => {
            console.warn("CLIENT_DEBUG: Could not fetch system status:", err);
            document.getElementById('freeMemory').textContent = 'Error';
            document.getElementById('wifiSignal').textContent = 'Error';
            document.getElementById('deviceUptime').textContent = 'Error';
        });
}