// Forcing a recompile to resolve build cache issues.
let settingsChanged = false;
let timezoneOptions = [];
let isDataLinkLoaded = false;
let anyInputInvalid = false;
let analyzedDataCache = {};
let apiExamples = {}; // This will be populated on load

let dataPointStateCache = {};
let lastFocusedApiExample = {};
let activeWizardTarget = null;
let dataPointStatus = {};

let ws;
let weatherInterval;

let isLoading = true;

function initWebSocket() {
    ws = new WebSocket('ws://' + window.location.host + '/ws');

    ws.onopen = function() {
        console.log("CLIENT_DEBUG: WebSocket connection established.");
        showMessage('Data Link channel open', 'success', 2000);
    };

    ws.onmessage = function(event) {
        console.log("CLIENT_DEBUG: WebSocket message received:", event.data);
        const msg = JSON.parse(event.data);

        if (msg.action === 'apiResult') {
            const button = document.querySelector('.analyze-api-btn.analyzing, .dp-test-btn.analyzing');
            if (button) {
                 button.disabled = false;
                 button.classList.remove('analyzing');
                 button.textContent = button.classList.contains('dp-test-btn') ? 'Test' : 'Analyze API';
                 const index = button.dataset.index;
                 updateDataPointStatus(index, msg.status === 'success');

                 if (msg.status === 'success') {
                    analyzedDataCache[index] = msg.payload;
                    if (button.classList.contains('analyze-api-btn')) {
                        console.log(`CLIENT_DEBUG: API analysis for index ${index} successful. Payload:`, msg.payload);
                        displayApiWizardResults(index, msg.payload);
                        document.getElementById(`dp_display_mode_container_${index}`).style.display = 'block';
                        document.getElementById(`dp_formatting_container_${index}`).style.display = 'block';
                    } else {
                        showMessage(`Data Point ${parseInt(index) + 1} test successful.`, 'success');
                    }
                    updateMarqueePreview(index); // This line was missing!
                 } else {
                    const errorMsg = `API Error: ${msg.payload}`;
                    console.error(`CLIENT_DEBUG: API analysis for index ${index} failed. Error:`, msg.payload);
                    showMessage(errorMsg, 'error');
                    if (button.classList.contains('analyze-api-btn')) {
                        document.getElementById(`wizard_results_${index}`).innerHTML = `<span class="error-text">${errorMsg}</span>`;
                    }
                 }
            }
        }
    };

    ws.onclose = function() {
        console.log("CLIENT_DEBUG: WebSocket connection closed. Attempting to reconnect...");
        showMessage('Data Link channel closed. Retrying...', 'error', 3000);
        setTimeout(initWebSocket, 3000);
    };

    ws.onerror = function(err) {
        console.error('CLIENT_DEBUG: WebSocket error:', err);
    };
}


async function checkServerReady(retries = 5, delay = 1000) {
    for (let i = 0; i < retries; i++) {
        try {
            const response = await fetch('/api/isReady');
            if (response.ok) {
                console.log("CLIENT_DEBUG: Server is ready.");
                return true;
            }
        } catch (error) {
            console.log(`CLIENT_DEBUG: Server readiness check, attempt ${i + 1} failed. Retrying...`);
            await new Promise(resolve => setTimeout(resolve, delay));
        }
    }
    return false;
}

document.addEventListener('DOMContentLoaded', async () => {
    const isReady = await checkServerReady();
    if (isReady) {
        initializeUI();
    } else {
        document.body.innerHTML = '<div class="container"><h1>Connection Failed</h1><p>Could not connect to the Time Circuits device. Please check the connection and refresh the page.</p></div>';
        showMessage('Could not connect to device.', 'error', 10000);
    }
});

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
        attachEventListeners();
        showMessage('System Online', 'success');

    } catch (error) {
        console.error("CLIENT_DEBUG: Failed during essential initialization:", error);
        showMessage(`Critical error loading settings: ${error.message}. Please refresh.`, 'error');
    } finally {
        isLoading = false;
        setSettingsChanged(false);
    }
}


function loadDataLinkSettings() {
    if (isDataLinkLoaded) return;
    showMessage('Loading Data Link settings...', 'info');
    fetch('/api/settings/datalink').then(res => res.ok ? res.json() : Promise.reject('Failed to load'))
        .then(datalink => {
            applyDataLinkSettings(datalink);
            isDataLinkLoaded = true;
            showMessage('Data Link settings loaded.', 'success');
        }).catch(error => {
            console.error("CLIENT_DEBUG: Failed to load Data Link settings:", error);
            showMessage(`Error loading Data Link: ${error.message}`, 'error');
        });
}

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

async function applyDataLinkSettings(datalink) {
    document.getElementById('weatherModeEnabled').checked = datalink.weatherModeEnabled;
    document.getElementById('dataLinkEnabled').checked = datalink.dataLinkEnabled;
    
    document.getElementById('weatherSettingsContainer').style.display = datalink.weatherModeEnabled ? 'block' : 'none';
    document.getElementById('dataLinkSettingsContainer').style.display = datalink.dataLinkEnabled ? 'block' : 'none';

    document.getElementById('weatherModeGroup').classList.toggle('disabled', datalink.dataLinkEnabled);
    document.getElementById('dataLinkGroup').classList.toggle('disabled', datalink.weatherModeEnabled);

    document.getElementById('cityName').value = datalink.cityName || '';
    document.getElementById('useMetricUnits').checked = datalink.useMetricUnits;
    
    document.getElementById('dataLinkRefreshInterval').value = datalink.dataLinkRefreshInterval;
    document.getElementById('dataLinkRefreshIntervalValue').textContent = datalink.dataLinkRefreshInterval;
    document.getElementById('mqttBroker').value = datalink.mqttBroker || '';
    document.getElementById('mqttPort').value = datalink.mqttPort || 1883;
    document.getElementById('mqttUser').value = datalink.mqttUser || '';
    document.getElementById('mqttPassword').value = datalink.mqttPassword || '';
    document.getElementById('numDataPoints').value = datalink.numDataPoints;
    document.getElementById('numDataPointsValue').textContent = datalink.numDataPoints;
    await updateDataPointsUI(datalink.numDataPoints);
    if (datalink.dataPoints) {
        datalink.dataPoints.forEach((point, i) => {
            document.getElementById(`dp_dataSourceType_${i}`).value = point.dataSourceType === 1 ? 'mqtt' : 'api';
            document.getElementById(`dp_displayMode_${i}`).value = point.displayMode || 0;
            document.getElementById(`dp_url_${i}`).value = point.url || '';
            document.getElementById(`dp_monthPath_${i}`).value = point.monthPath || '';
            document.getElementById(`dp_dayPath_${i}`).value = point.dayPath || '';
            document.getElementById(`dp_yearPath_${i}`).value = point.yearPath || '';
            document.getElementById(`dp_timePath_${i}`).value = point.timePath || '';
            document.getElementById(`dp_prefix_${i}`).value = point.prefix || '';
            document.getElementById(`dp_suffix_${i}`).value = point.suffix || '';
            document.getElementById(`dp_icon_${i}`).value = point.icon || '';
            document.getElementById(`dp_scrollSpeed_${i}`).value = point.scrollSpeed || 150;
            document.getElementById(`dp_mqttTopic_${i}`).value = point.mqttTopic || '';
            document.getElementById(`dp_scrollingText_${i}`).value = point.scrollingText || '';
            document.getElementById(`dp_authHeaderKey_${i}`).value = point.authHeaderKey || '';
            document.getElementById(`dp_authHeaderValue_${i}`).value = point.authHeaderValue || '';
            document.getElementById(`api_example_${i}`).value = point.apiExampleKey || '';

            document.getElementById(`dp_dataSourceType_${i}`).dispatchEvent(new Event('change'));
            document.getElementById(`dp_displayMode_${i}`).dispatchEvent(new Event('change'));
            updateMarqueePreview(i);
        });
    }
}

function handleDataLinkToggle(changedToggleId) {
    const weatherToggle = document.getElementById('weatherModeEnabled');
    const dataLinkToggle = document.getElementById('dataLinkEnabled');

    // If the changed toggle is now checked, uncheck the other one.
    if (changedToggleId === 'weatherModeEnabled' && weatherToggle.checked) {
        dataLinkToggle.checked = false;
    } else if (changedToggleId === 'dataLinkEnabled' && dataLinkToggle.checked) {
        weatherToggle.checked = false;
    }

    // If the user's action resulted in both being unchecked, re-check the one they didn't touch.
    // This enforces that at least one is always active.
    if (!weatherToggle.checked && !dataLinkToggle.checked) {
        if (changedToggleId === 'weatherModeEnabled') {
             dataLinkToggle.checked = true; // User tried to uncheck weather, so re-activate datalink
        } else {
             weatherToggle.checked = true; // User tried to uncheck datalink, so re-activate weather
        }
    }
    
    // Now, update the UI based on the final state of the toggles
    const isWeatherChecked = weatherToggle.checked;
    const isDataLinkChecked = dataLinkToggle.checked;

    document.getElementById('weatherSettingsContainer').style.display = isWeatherChecked ? 'block' : 'none';
    document.getElementById('dataLinkSettingsContainer').style.display = isDataLinkChecked ? 'block' : 'none';

    document.getElementById('weatherModeGroup').classList.toggle('disabled', isDataLinkChecked);
    document.getElementById('dataLinkGroup').classList.toggle('disabled', isWeatherChecked);

    if (isWeatherChecked) {
        fetchWeatherData();
    }
    setSettingsChanged(true);
}


function attachEventListeners() {
    const container = document.body;

    const delegatedChangeHandler = (e) => {
        if (!isLoading && e.target.closest('.container')) {
            if (e.target.matches('input, select, textarea')) {
                 if (e.target.type === 'range') {
                    const valueSpan = document.getElementById(`${e.target.id}Value`);
                    if (valueSpan) valueSpan.textContent = e.target.value;
                }
                setSettingsChanged(true);
            }
        }
    };
    container.addEventListener('change', delegatedChangeHandler);
    container.addEventListener('input', delegatedChangeHandler);

    document.body.addEventListener('click', function(e) {
        const target = e.target;
        
        // Save button
        if (target.id === 'saveSettingsBtn') {
            saveSettings();
        }
        // Great Scott button
        if (target.id === 'greatScottBtn') {
            fetch('/api/greatScott', { method: 'POST' });
        }
        // Header navigation
        if (target.closest('#header-dest')) {
             scrollToSettings('TimeCircuits', 'destinationTimeSettings');
        }
        if (target.closest('#header-pres')) {
            scrollToSettings('System', 'presentTimeSettings');
        }
        if (target.closest('#header-last')) {
            scrollToSettings('TimeCircuits', 'lastDepartedSettings');
        }
        // Tabs
        const tabLink = target.closest('.tab-link');
        if (tabLink) {
            const tabName = tabLink.getAttribute('data-tab');
            openTab({ currentTarget: tabLink }, tabName);
            if (tabName === 'DataLink' && !isDataLinkLoaded) {
                loadDataLinkSettings();
            }
        }
        // Preset buttons
        if(target.id === 'savePresetBtn') {
            handleSavePreset();
        }
        if(target.id === 'deletePresetBtn') {
            deletePreset();
        }
        if(target.id === 'newPresetBtn') {
            resetPresetForm();
        }
        // Weather refresh
        if (target.id === 'refreshWeatherBtn') {
            refreshWeatherData();
        }
        // Reset and Sync
        if (target.id === 'resetDefaultsBtn') {
            if (confirm("Are you sure? This will reset all settings to their defaults.")) {
                fetch('/api/resetSettings', { method: 'POST' })
                    .then(res => res.text()).then(text => {
                        showMessage(text, 'success');
                        setTimeout(() => window.location.reload(), 1500);
                    });
            }
        }
        if (target.id === 'syncNtpBtn' || target.id === 'timeSyncStatus') {
            showMessage('Requesting time sync...', 'info');
            fetch('/api/syncTime', { method: 'POST' }).then(res => res.text()).then(text => showMessage(text, 'info'));
        }
        // Themes
        const themeOption = target.closest('.theme-option');
        if (themeOption) {
            const theme = themeOption.getAttribute('data-theme');
            document.body.className = theme;
            fetch('/api/setTheme', { method: 'POST', body: new URLSearchParams({ theme }) });
        }
    });
    
    // *** ROBUST EVENT DELEGATION FOR DYNAMIC ELEMENTS ***
    const dataPointsContainer = document.getElementById('dataPointsConfigContainer');

    dataPointsContainer.addEventListener('click', (e) => {
        const target = e.target;
        if (target.classList.contains('analyze-api-btn')) {
            startApiWizard(e);
        } else if (target.classList.contains('dp-clear-btn')) {
            clearDataPointFields(e);
        } else if (target.classList.contains('dp-dup-btn')) {
            duplicateDataPoint(e);
        } else if (target.classList.contains('dp-test-btn')) {
            testDataPoint(e);
        } else if (target.classList.contains('wizard-target-input')) {
            document.querySelectorAll('.wizard-target-input').forEach(el => el.classList.remove('is-wizard-target'));
            if (activeWizardTarget !== target) {
                activeWizardTarget = target;
                activeWizardTarget.classList.add('is-wizard-target');
            } else {
                activeWizardTarget = null;
            }
        }
    });

    dataPointsContainer.addEventListener('change', (e) => {
        const target = e.target;
        if (target.classList.contains('api-example-select')) {
            const index = target.dataset.index;
            const exampleKey = target.value;
            if (exampleKey && window.apiExamples[exampleKey]) {
                document.getElementById(`dp_url_${index}`).value = window.apiExamples[exampleKey].url;
                // Trigger input event for live preview update
                document.getElementById(`dp_url_${index}`).dispatchEvent(new Event('input', { bubbles: true }));
            }
        }
    });


    // Specific listeners that need more complex logic
    document.getElementById('weatherModeEnabled').addEventListener('change', (e) => handleDataLinkToggle(e.target.id));
    document.getElementById('dataLinkEnabled').addEventListener('change', (e) => handleDataLinkToggle(e.target.id));

    document.getElementById('numDataPoints').addEventListener('input', (e) => {
        document.getElementById('numDataPointsValue').textContent = e.target.value;
        updateDataPointsUI(parseInt(e.target.value, 10));
    });

    document.getElementById('destinationYear').addEventListener('input', () => {
        updateHeaderClocks(new Date());
        validateYearInput();
    });

    document.getElementById('presetDateSelect').onchange = handlePresetSelectionChange;
    document.getElementById('departureTime').onchange = updateSleepVisual;
    document.getElementById('arrivalTime').onchange = updateSleepVisual;

    document.addEventListener('keydown', (e) => {
        if (e.key === "Escape" && activeWizardTarget) {
            activeWizardTarget.classList.remove('is-wizard-target');
            activeWizardTarget = null;
            showMessage('Wizard target deselected.', 'info', 2000);
        }
    });
}


function validateYearInput() {
    const input = document.getElementById('destinationYear');
    const validationMsg = document.getElementById('destinationYearValidation');
    const value = parseInt(input.value, 10);
    if (isNaN(value) || value < 1000 || value > 9999) {
        input.classList.add('invalid');
        validationMsg.textContent = 'Year must be between 1000 and 9999.';
        return false;
    } else {
        input.classList.remove('invalid');
        validationMsg.textContent = '';
        return true;
    }
}

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
        document.getElementById('presetTime').value = `${String(hour).padStart(2, '0')}:${String(minute).padStart(2, '0')}`;
    } else {
        resetPresetForm(false);
    }
}

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

function addPreset() {
    const name = document.getElementById('presetName').value;
    const date = document.getElementById('presetDate').value;
    const time = document.getElementById('presetTime').value;
    if (!name || !date || !time) {
        showMessage('Preset name, date, and time are required.', 'error');
        return;
    }
    const [year, month, day] = date.split('-');
    const [hour, minute] = time.split(':');
    const value = `${year}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}-${String(hour).padStart(2, '0')}-${String(minute).padStart(2, '0')}`;
    
    fetch('/api/addPreset', { method: 'POST', body: new URLSearchParams({ name, value }) })
        .then(res => {
            if (!res.ok) {
                throw new Error('Failed to save preset.');
            }
            return res.text();
        })
        .then(text => {
            showMessage(text, 'success');
            
            const select = document.getElementById('presetDateSelect');
            let customGroup = select.querySelector('optgroup[label="Custom Time Jumps"]');
            
            if (!customGroup) {
                customGroup = document.createElement('optgroup');
                customGroup.label = 'Custom Time Jumps';
                select.appendChild(customGroup);
            }
            
            const option = document.createElement('option');
            option.value = value;
            option.textContent = name;
            customGroup.appendChild(option);
            
            resetPresetForm();
        })
        .catch(err => showMessage(`Error: ${err.message}`, 'error'));
}

function updatePreset() {
    const originalName = document.getElementById('presetDateSelect').options[document.getElementById('presetDateSelect').selectedIndex].text;
    const newName = document.getElementById('presetName').value;
    const date = document.getElementById('presetDate').value;
    const time = document.getElementById('presetTime').value;

    if (!newName || !date || !time) {
        showMessage('Preset name, date, and time are required.', 'error');
        return;
    }
    const [year, month, day] = date.split('-');
    const [hour, minute] = time.split(':');
    const value = `${year}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}-${String(hour).padStart(2, '0')}-${String(minute).padStart(2, '0')}`;
    
    fetch('/api/updatePreset', { method: 'POST', body: new URLSearchParams({ name: originalName, newName: newName, value: value }) })
        .then(res => res.text()).then(text => {
            showMessage(text, 'success');
            fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
            resetPresetForm();
        });
}


function deletePreset() {
    const name = document.getElementById('presetDateSelect').options[document.getElementById('presetDateSelect').selectedIndex].text;
    if (confirm(`Are you sure you want to delete the preset "${name}"?`)) {
        fetch('/api/deletePreset', { method: 'POST', body: new URLSearchParams({ name }) })
            .then(res => res.text()).then(text => {
                showMessage(text, 'success');
                fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
                resetPresetForm();
            });
    }
}

function applySelectedPreset(event) {
    const select = event.target;
    if (!select.value) return;
    const [year, month, day, hour, minute] = select.value.split('-');
    updateLastDepartedDisplay(year, month, day, hour, minute);
    showMessage(`Last Time Departed set to: ${select.options[select.selectedIndex].text}`, 'info');
    if (!isLoading) setSettingsChanged(true);
    updateHeaderClocks(new Date());
}

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

function openTab(evt, tabName) {
    document.querySelectorAll('.tab-content').forEach(tc => tc.style.display = "none");
    document.querySelectorAll('.tab-link').forEach(tl => tl.classList.remove('active'));
    document.getElementById(tabName).style.display = "block";
    evt.currentTarget.classList.add('active');
}

function setSettingsChanged(isChanged) {
    settingsChanged = isChanged;
    const saveButtonContainer = document.querySelector('.save-button-container');
    const saveButton = document.getElementById('saveSettingsBtn');
    if (isChanged) {
        saveButtonContainer.classList.add('visible');
        saveButton.disabled = false;
    } else {
        saveButtonContainer.classList.remove('visible');
        saveButton.disabled = true;
    }
}

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
                block.className = 'setting-group data-point-block';
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
                    </select>

                    <div id="dp_api_container_${i}">
                        <label for="api_example_${i}">API Examples (optional):</label>
                        <select id="api_example_${i}" class="api-example-select" data-index="${i}"></select>
                        <label for="dp_url_${i}">API URL:</label>
                        <input type="text" id="dp_url_${i}" placeholder="http://api.example.com/data.json" data-index="${i}">
                        <label for="dp_authHeaderKey_${i}">Auth Header Key (optional):</label>
                        <input type="text" id="dp_authHeaderKey_${i}" placeholder="e.g., X-API-Key" data-index="${i}">
                        <label for="dp_authHeaderValue_${i}">Auth Header Value (optional):</label>
                        <input type="text" id="dp_authHeaderValue_${i}" placeholder="e.g., your-api-key" data-index="${i}">
                        <button class="analyze-api-btn" data-index="${i}">Analyze API</button>
                        <div class="api-wizard-results" id="wizard_results_${i}"></div>
                    </div>

                    <div id="dp_mqtt_container_${i}" style="display: none;">
                        <label for="dp_mqttTopic_${i}">MQTT Topic:</label>
                        <input type="text" id="dp_mqttTopic_${i}" placeholder="e.g., /home/livingroom/temperature" data-index="${i}">
                    </div>

                    <div id="dp_display_mode_container_${i}" style="display:none;">
                        <label for="dp_displayMode_${i}" style="margin-top: 20px;">Display Mode:</label>
                        <select id="dp_displayMode_${i}" class="display-mode-select" data-index="${i}">
                            <option value="0">Four Column Data</option>
                            <option value="1">Scrolling Text</option>
                        </select>
                    </div>

                    <div id="dp_formatting_container_${i}" style="display:none;">
                        <div class="display-mode-container" id="four_column_container_${i}">
                             <div class="time-circuit-row">
                                <label for="dp_monthPath_${i}" class="time-circuit-label">MONTH</label>
                                <input type="text" id="dp_monthPath_${i}" class="time-circuit-input wizard-target-input" maxlength="3" data-index="${i}">
                            </div>
                            <div class="time-circuit-row">
                                <label for="dp_dayPath_${i}" class="time-circuit-label">DAY</label>
                                <input type="text" id="dp_dayPath_${i}" class="time-circuit-input wizard-target-input" maxlength="2" data-index="${i}">
                                <select id="dp_icon_${i}" class="icon-select" data-index="${i}" style="width: 100px; margin-left: 10px;">
                                    <option value="">No Icon</option>
                                    <option value="SU">Sun</option> <option value="CL">Cloud</option> <option value="RN">Rain</option>
                                    <option value="SN">Snow</option> <option value="ST">Storm</option> <option value="WD">Wind</option>
                                    <option value="^">Up</option> <option value="v">Down</option> <option value="==">Equal</option>
                                </select>
                            </div>
                            <div class="time-circuit-row">
                                <label for="dp_yearPath_${i}" class="time-circuit-label">YEAR</label>
                                <input type="text" id="dp_yearPath_${i}" class="time-circuit-input wizard-target-input" data-index="${i}">
                            </div>
                            <div class="time-circuit-row">
                                <label for="dp_timePath_${i}" class="time-circuit-label">TIME</label>
                                <input type="text" id="dp_timePath_${i}" class="time-circuit-input wizard-target-input" data-index="${i}">
                            </div>
                        </div>

                        <div class="display-mode-container" id="scrolling_text_container_${i}" style="display: none;">
                            <label for="dp_scrollingText_${i}" style="margin-top: 15px;">Scrolling Text:</label>
                            <input type="text" id="dp_scrollingText_${i}" class="wizard-target-input" placeholder="Enter text or map a value..." data-index="${i}">
                        </div>
                        
                        <div class="time-format-group">
                             <div class="time-circuit-row">
                                <label for="dp_prefix_${i}" class="time-circuit-label">[PREFIX]</label>
                                <input type="text" id="dp_prefix_${i}" class="time-circuit-input" maxlength="15" data-index="${i}">
                            </div>
                            <div class="time-circuit-row">
                                <label for="dp_suffix_${i}" class="time-circuit-label">[SUFFIX]</label>
                                <input type="text" id="dp_suffix_${i}" class="time-circuit-input" maxlength="15" data-index="${i}">
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
                             <div class="marquee-preview-13" id="marquee_preview_13_${i}" style="display:none;">
                                <span class="preview-scrolling-text">PREVIEW</span>
                            </div>
                        </div>
                        
                        <label for="dp_scrollSpeed_${i}" style="margin-top: 20px;">Scroll Speed (ms/char): <span id="dp_scrollSpeed_${i}Value">150</span></label>
                        <input type="range" id="dp_scrollSpeed_${i}" min="50" max="500" step="10" value="150" data-index="${i}">
                    </div>
                `;
                container.appendChild(block);
            }
            populateApiExampleDropdowns();
            for (let i = 0; i < numPoints; i++) {
                updateMarqueePreview(i);
            }
        }
        resolve();
    });
}

function getDisplayValue(path, placeholder, index) {
    if (!path) return placeholder;
    // Check if there is analyzed data and if the path is a JSON path, not a literal string
    if (analyzedDataCache[index] && (path.includes('.') || path.includes('['))) {
        const resolvedValue = getValueFromPath(analyzedDataCache[index], path);
        if (resolvedValue !== null && resolvedValue !== undefined) {
            return resolvedValue;
        }
    }
    // If no data or not a path, return the path string itself as a literal
    return path;
}


function updateMarqueePreview(index) {
    const displayMode = document.getElementById(`dp_displayMode_${index}`).value;

    document.getElementById(`marquee_preview_${index}`).style.display = (displayMode === '0') ? 'flex' : 'none';
    document.getElementById(`marquee_preview_13_${index}`).style.display = (displayMode === '1') ? 'block' : 'none';

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

        const prefix = document.getElementById(`dp_prefix_${index}`).value;
        const suffix = document.getElementById(`dp_suffix_${index}`).value;

        const yearFinalValue = `${prefix}${yearValue}${suffix}`;

        document.querySelector(`#marquee_preview_${index} .preview-month`).textContent = String(monthValue).substring(0, 3).toUpperCase();
        const dayPreview = document.querySelector(`#marquee_preview_${index} .preview-day`);
        dayPreview.textContent = icon ? icon : String(dayValue).substring(0, 2).toUpperCase();
        
        const setupScrolling = (text, valueSpan) => {
            valueSpan.textContent = text;
            valueSpan.style.animation = 'none';
            setTimeout(() => {
                if (valueSpan.offsetWidth > valueSpan.parentElement.offsetWidth) {
                    const scrollSpeed = document.getElementById(`dp_scrollSpeed_${index}`).value;
                    const duration = (text.length + 4) * (scrollSpeed / 1000);
                    valueSpan.style.animation = `scroll-left ${duration}s linear infinite`;
                }
            }, 50);
        };
        
        setupScrolling(yearFinalValue, document.querySelector(`#marquee_preview_${index} .preview-year`));
        setupScrolling(String(timeValue), document.querySelector(`#marquee_preview_${index} .preview-time`));

    } else { // Scrolling Text
        const scrollingPath = document.getElementById(`dp_scrollingText_${index}`).value;
        let text = getDisplayValue(scrollingPath, 'PREVIEW', index);

        const prefix = document.getElementById(`dp_prefix_${index}`).value;
        const suffix = document.getElementById(`dp_suffix_${index}`).value;
        text = `${prefix}${text}${suffix}`;

        const previewSpan = document.querySelector(`#marquee_preview_13_${index} .preview-scrolling-text`);
        previewSpan.textContent = text;
        previewSpan.style.animation = 'none';
        
        setTimeout(() => {
             if (previewSpan.offsetWidth > previewSpan.parentElement.offsetWidth) {
                const scrollSpeed = document.getElementById(`dp_scrollSpeed_${index}`).value;
                const duration = (text.length) * (scrollSpeed / 100);
                previewSpan.style.animation = `scroll-left ${duration}s linear infinite`;
            }
        }, 50);
    }
}

function getProcessedUrl(index) {
    let apiUrl = document.getElementById(`dp_url_${index}`).value;
    const authValue = document.getElementById(`dp_authHeaderValue_${index}`).value;
    if (apiUrl.includes('YOUR_API_KEY') && authValue) {
        return apiUrl.replace('YOUR_API_KEY', authValue);
    }
    return apiUrl;
}


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

function startApiWizard(event) {
    console.log("CLIENT_DEBUG: 'Analyze API' button clicked.");
    const index = event.target.dataset.index;
    const apiUrl = getProcessedUrl(index);
    const authKey = document.getElementById(`dp_authHeaderKey_${index}`).value;
    const authValue = document.getElementById(`dp_authHeaderValue_${index}`).value;
    const button = event.target;

    console.log(`CLIENT_DEBUG: Starting API Wizard for index ${index}. URL: ${apiUrl}`);

    if (!apiUrl) {
        showMessage('Please enter an API URL first.', 'error');
        return;
    }

    if (!ws || ws.readyState !== WebSocket.OPEN) {
        showMessage('Data Link channel is not open. Please wait.', 'error');
        return;
    }

    const resultsContainer = document.getElementById(`wizard_results_${index}`);
    resultsContainer.innerHTML = '<span class="loading-spinner"></span> Analyzing...';
    button.disabled = true;
    button.classList.add('analyzing');

    const message = {
        action: "testApi",
        data: { url: apiUrl, authKey, authValue }
    };
    console.log("CLIENT_DEBUG: Sending 'testApi' message to WebSocket:", message);
    ws.send(JSON.stringify(message));
}

function displayApiWizardResults(index, jsonData) {
    const container = document.getElementById(`wizard_results_${index}`);
    container.innerHTML = '<strong>Click a form field (Month, Day, etc.), then click a value below to map it.</strong>';

    const mainList = document.createElement('ul');
    mainList.className = 'wizard-list';

    const buildListRecursive = (data, parentPath = '') => {
        const elements = [];
        const processData = (key, value, currentPath) => {
            const li = document.createElement('li');
            if (typeof value === 'object' && value !== null) {
                li.innerHTML = `<span class="wizard-key">${key}:</span>`;
                const subList = document.createElement('ul');
                const childElements = buildListRecursive(value, currentPath);
                childElements.forEach(el => subList.appendChild(el));
                li.appendChild(subList);
            } else {
                 li.innerHTML = `<span class="wizard-clickable-item" data-path="${currentPath}"><span class="wizard-key">${key}:</span> <span class="wizard-value">"${String(value)}"</span></span>`;
            }
            return li;
        };

        if (Array.isArray(data)) {
            data.forEach((item, i) => {
                const currentPath = `${parentPath}[${i}]`;
                elements.push(processData(`[${i}]`, item, currentPath));
            });
        } else if (typeof data === 'object' && data !== null) {
            for (const key in data) {
                const currentPath = parentPath ? `${parentPath}.${key}` : key;
                elements.push(processData(key, data[key], currentPath));
            }
        }
        return elements;
    };

    buildListRecursive(jsonData).forEach(el => mainList.appendChild(el));
    container.appendChild(mainList);

    container.querySelectorAll('.wizard-clickable-item').forEach(item => {
        item.onclick = (e) => {
            if (activeWizardTarget) {
                const path = e.currentTarget.dataset.path;
                activeWizardTarget.value = path;
                activeWizardTarget.dispatchEvent(new Event('input', { bubbles: true })); // Ensure event bubbles up for delegated listener
                showMessage(`Mapped "${path}" to the selected field.`, 'success', 2000);
                activeWizardTarget.classList.remove('is-wizard-target');
                activeWizardTarget = null;
            } else {
                showMessage('Click a form field first to select it as the target, then click a value.', 'error');
            }
        };
    });
}


function saveSettings() {
    console.log("CLIENT_DEBUG: 'Engage Time Circuits' button clicked. Initiating save process.");
    if (!validateYearInput()) {
        showMessage('Please correct the invalid fields before saving.', 'error');
        scrollToSettings('TimeCircuits', 'destinationTimeSettings');
        return;
    }

    showLoading('saveSettingsBtn', true);

    const settings = {
        destinationYear: parseInt(document.getElementById('destinationYear').value, 10),
        destinationTimezoneIndex: parseInt(document.getElementById('destinationTimezoneSelect').value, 10),
        presentTimezoneIndex: parseInt(document.getElementById('presentTimezoneSelect').value, 10),
        lastTimeDepartedYear: parseInt(document.getElementById('lastTimeDepartedYear').textContent, 10),
        lastTimeDepartedMonth: parseInt(document.getElementById('lastTimeDepartedMonth').textContent, 10),
        lastTimeDepartedDay: parseInt(document.getElementById('lastTimeDepartedDay').textContent, 10),
        lastTimeDepartedHour: parseInt(document.getElementById('lastTimeDepartedHour').textContent, 10),
        lastTimeDepartedMinute: parseInt(document.getElementById('lastTimeDepartedMinute').textContent, 10),
        departureHour: parseInt(document.getElementById('departureTime').value.split(':')[0], 10),
        departureMinute: parseInt(document.getElementById('departureTime').value.split(':')[1], 10),
        arrivalHour: parseInt(document.getElementById('arrivalTime').value.split(':')[0], 10),
        arrivalMinute: parseInt(document.getElementById('arrivalTime').value.split(':')[1], 10),
        brightness: parseInt(document.getElementById('brightness').value, 10),
        notificationVolume: parseInt(document.getElementById('notificationVolume').value, 10),
        timeTravelAnimationDuration: parseInt(document.getElementById('timeTravelAnimationDuration').value, 10),
        timeTravelAnimationInterval: parseInt(document.getElementById('timeTravelAnimationInterval').value, 10),
        animationStyle: parseInt(document.getElementById('animationStyleSelect').value, 10),
        glitchEffectFrequency: parseInt(document.getElementById('glitchEffectFrequency').value, 10),
        malfunctionFrequency: parseInt(document.getElementById('malfunctionFrequency').value, 10),
        presetCycleInterval: parseInt(document.getElementById('presetCycleInterval').value, 10),
        timeTravelSoundToggle: document.getElementById('timeTravelSoundToggle').checked,
        timeTravelVolumeFade: document.getElementById('timeTravelVolumeFade').checked,
        displayFormat24h: document.getElementById('displayFormat24h').checked,
        dataLinkEnabled: document.getElementById('dataLinkEnabled').checked,
        dataLinkRefreshInterval: parseInt(document.getElementById('dataLinkRefreshInterval').value, 10),
        mqttBroker: document.getElementById('mqttBroker').value,
        mqttPort: parseInt(document.getElementById('mqttPort').value, 10),
        mqttUser: document.getElementById('mqttUser').value,
        mqttPassword: document.getElementById('mqttPassword').value,
        weatherModeEnabled: document.getElementById('weatherModeEnabled').checked,
        cityName: document.getElementById('cityName').value,
        useMetricUnits: document.getElementById('useMetricUnits').checked,
        numDataPoints: parseInt(document.getElementById('numDataPoints').value, 10),
        dataPoints: []
    };

    for (let i = 0; i < settings.numDataPoints; i++) {
        settings.dataPoints.push({
            dataSourceType: document.getElementById(`dp_dataSourceType_${i}`).value === 'mqtt' ? 1 : 0,
            displayMode: parseInt(document.getElementById(`dp_displayMode_${i}`).value, 10),
            url: document.getElementById(`dp_url_${i}`).value,
            monthPath: document.getElementById(`dp_monthPath_${i}`).value,
            dayPath: document.getElementById(`dp_dayPath_${i}`).value,
            yearPath: document.getElementById(`dp_yearPath_${i}`).value,
            timePath: document.getElementById(`dp_timePath_${i}`).value,
            prefix: document.getElementById(`dp_prefix_${i}`).value,
            suffix: document.getElementById(`dp_suffix_${i}`).value,
            icon: document.getElementById(`dp_icon_${i}`).value,
            scrollSpeed: parseInt(document.getElementById(`dp_scrollSpeed_${i}`).value, 10),
            mqttTopic: document.getElementById(`dp_mqttTopic_${i}`).value,
            scrollingText: document.getElementById(`dp_scrollingText_${i}`).value,
            authHeaderKey: document.getElementById(`dp_authHeaderKey_${i}`).value,
            authHeaderValue: document.getElementById(`dp_authHeaderValue_${i}`).value,
            apiExampleKey: document.getElementById(`api_example_${i}`).value
        });
    }
    console.log("CLIENT_DEBUG: Sending settings object to /api/saveSettings:", settings);

    fetch('/api/saveSettings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
    })
    .then(res => res.ok ? res.text() : Promise.reject(`Save failed: ${res.statusText}`))
    .then(text => {
        showMessage(text, 'success');
        setSettingsChanged(false);
        console.log("CLIENT_DEBUG: Settings saved successfully. Now triggering animation.");
        return fetch('/api/triggerAnimation', { method: 'POST' });
    })
    .then(res => {
        if (!res.ok) throw new Error('Failed to trigger animation');
        console.log("CLIENT_DEBUG: Animation trigger successful.");
        document.body.classList.add('time-travel-active');
        setTimeout(() => document.body.classList.remove('time-travel-active'), settings.timeTravelAnimationDuration);
    })
    .catch(err => showMessage(`Error: ${err.message}`, 'error'))
    .finally(() => showLoading('saveSettingsBtn', false));
}

function fetchTime() {
    fetch('/api/time').then(res => res.json()).then(data => {
        const statusEl = document.getElementById('timeSyncStatus');
        if (data.timeSynchronized) {
            statusEl.textContent = 'Synchronized';
            statusEl.className = 'status-text status-yes';
        } else {
            statusEl.textContent = 'Not Synchronized (Click to Calibrate)';
            statusEl.className = 'status-text status-no';
        }
        if (data.unixTime) updateHeaderClocks(new Date(data.unixTime * 1000));
    });
}

function refreshWeatherData() {
    const preview = document.getElementById('weatherPreview');
    preview.textContent = 'Fetching...';
    
    const city = document.getElementById('cityName').value;
    if (!city) {
        showMessage('Please enter a city name.', 'error');
        preview.textContent = 'Enter a city';
        return;
    }

    fetch('/api/weather/refresh', { 
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ cityName: city })
    })
    .then(res => {
        if (res.ok) {
            showMessage('Weather refresh triggered. Please wait a moment.', 'info');
            setTimeout(fetchWeatherData, 3000); // Give server time to fetch
        } else {
            throw new Error('Failed to trigger refresh.');
        }
    })
    .catch(err => {
        showMessage(`Error: ${err.message}`, 'error');
        preview.textContent = 'Error';
    });
}

function getWeatherIcon(code) {
    const icons = {
        0: 'SU', 1: 'SU', 2: 'CL', 3: 'CL', 45: 'CL', 48: 'CL',
        51: 'RN', 53: 'RN', 55: 'RN', 61: 'RN', 63: 'RN', 65: 'RN',
        66: 'RN', 67: 'RN', 71: 'SN', 73: 'SN', 75: 'SN', 77: 'SN',
        80: 'RN', 81: 'RN', 82: 'RN', 85: 'SN', 86: 'SN', 95: 'ST',
        96: 'ST', 99: 'ST'
    };
    return icons[code] || '--';
}

function fetchWeatherData() {
    if (!document.getElementById('weatherModeEnabled').checked) {
        document.getElementById('weatherDisplay').style.display = 'none';
        return;
    }

    const weatherDisplay = document.getElementById('weatherDisplay');
    const loadingSpinner = weatherDisplay.querySelector('.loading-spinner-container');
    const preview = document.getElementById('weatherPreview');
    loadingSpinner.style.display = 'block';

    fetch('/api/weather')
        .then(res => res.ok ? res.json() : Promise.reject('Weather data not ready'))
        .then(data => {
            const isMetric = document.getElementById('useMetricUnits').checked;
            const tempUnit = isMetric ? '°C' : '°F';
            const speedUnit = isMetric ? ' km/h' : ' mph';

            weatherDisplay.style.display = 'grid';
            document.getElementById('weatherTemp').textContent = `${data.temperature.toFixed(1)}${tempUnit}`;
            document.getElementById('weatherFeelsLike').textContent = `${data.apparentTemperature.toFixed(1)}${tempUnit}`;
            document.getElementById('weatherHumidity').textContent = `${data.humidity}%`;
            document.getElementById('weatherWind').textContent = `${data.windSpeed.toFixed(1)}${speedUnit}`;
            document.getElementById('weatherHighLow').textContent = `${data.dailyHigh.toFixed(0)}° / ${data.dailyLow.toFixed(0)}°`;
            
            preview.textContent = `Live data for ${document.getElementById('cityName').value}: ${data.temperature.toFixed(1)}${tempUnit}`;

            const hourlyContainer = document.getElementById('hourlyForecastContainer');
            hourlyContainer.innerHTML = '';
            let currentHour = new Date().getHours();
            data.hourly.forEach((hour, index) => {
                let forecastHour = (currentHour + index + 1) % 24;
                let ampm = forecastHour >= 12 ? 'PM' : 'AM';
                let displayHour = forecastHour % 12;
                if (displayHour === 0) displayHour = 12;

                const item = document.createElement('div');
                item.className = 'hourly-item';
                item.innerHTML = `
                    <div class="hourly-time">${displayHour} ${ampm}</div>
                    <div class="hourly-icon">${getWeatherIcon(hour.code)}</div>
                    <div class="hourly-temp">${hour.temp.toFixed(0)}°</div>
                `;
                hourlyContainer.appendChild(item);
            });
        })
        .catch(err => {
            console.warn("CLIENT_DEBUG: Could not fetch weather data:", err);
            weatherDisplay.style.display = 'grid';
            ['weatherTemp', 'weatherFeelsLike', 'weatherHumidity', 'weatherWind', 'weatherHighLow'].forEach(id => {
                document.getElementById(id).textContent = '--';
            });
            document.getElementById('hourlyForecastContainer').innerHTML = '';
            preview.textContent = 'Data not available. Check city name.';
        })
        .finally(() => loadingSpinner.style.display = 'none');
}

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
        bar1.style.left = `${(arrTotalMins / 1440) * 100}%`;
        bar1.style.width = `${((depTotalMins - arrTotalMins) / 1440) * 100}%`;
        bar2.style.display = 'none';
    } else {
        bar1.style.left = '0%';
        bar1.style.width = `${(depTotalMins / 1440) * 100}%`;
        
        bar2.style.left = `${(arrTotalMins / 1440) * 100}%`;
        bar2.style.width = `${((1440 - arrTotalMins) / 1440) * 100}%`;
        bar2.style.display = 'block';
    }
}

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

function getValueFromPath(obj, path) {
    if (!path || !obj) return null;
    try {
        const value = path.split(/[.\[\]]+/).filter(Boolean).reduce((o, k) => (o || {})[k], obj);
        return value !== undefined ? value : null;
    } catch (e) {
        return null;
    }
}

function updateDataPointStatus(index, isSuccess) {
    const indicator = document.getElementById(`dp_status_${index}`);
    if (indicator) {
        indicator.className = 'dp-status-indicator';
        indicator.classList.add(isSuccess ? 'success' : 'error');
    }
    dataPointStatus[index] = isSuccess;
}

function clearDataPointFields(event) {
    const index = event.target.dataset.index;
    const fields = ['url', 'monthPath', 'dayPath', 'yearPath', 'timePath', 'prefix', 'suffix', 'icon', 'mqttTopic', 'scrollingText', 'authHeaderKey', 'authHeaderValue'];
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
        const fields = ['dataSourceType', 'displayMode', 'url', 'monthPath', 'dayPath', 'yearPath', 'timePath', 'prefix', 'suffix', 'icon', 'scrollSpeed', 'mqttTopic', 'scrollingText', 'authHeaderKey', 'authHeaderValue', 'api_example'];
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


function testDataPoint(event) {
    const index = event.target.dataset.index;
    const button = event.target;
    
    if (document.getElementById(`dp_dataSourceType_${index}`).value !== 'api') {
        showMessage('Test is only available for API data points.', 'error');
        return;
    }

    const apiUrl = getProcessedUrl(index);
    if (!apiUrl) {
        showMessage('Please enter an API URL first.', 'error');
        return;
    }

    if (!ws || ws.readyState !== WebSocket.OPEN) {
        showMessage('Data Link channel is not open. Please wait.', 'error');
        return;
    }

    button.disabled = true;
    button.classList.add('analyzing');
    button.innerHTML = '<span class="loading-spinner"></span>';

    const message = {
        action: "testApi",
        data: {
            url: apiUrl,
            authKey: document.getElementById(`dp_authHeaderKey_${index}`).value,
            authValue: document.getElementById(`dp_authHeaderValue_${index}`).value
        }
    };
    ws.send(JSON.stringify(message));
}