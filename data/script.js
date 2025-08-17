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
                        const resultsContainer = document.getElementById(`wizard_results_${index}`);
                        displayApiWizardResults(index, msg.payload);
                    } else {
                        showMessage(`Data Point ${parseInt(index) + 1} test successful.`, 'success');
                    }
                    updateMarqueePreview(index);
                 } else {
                    const errorMsg = `API Error: ${msg.payload}`;
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
        applySettings(timecircuits, temporal, datalink);
        document.querySelector('.header-circuits').classList.add('visible');
        
        initWebSocket();

        fetchTime();
        setInterval(fetchTime, 1000);
        attachEventListeners();
        showMessage('System Online', 'success');

    } catch (error) {
        console.error("CLIENT_DEBUG: Failed during essential initialization:", error);
        showMessage(`Critical error loading settings: ${error.message}. Please refresh.`, 'error');
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

function applySettings(timecircuits, temporal, datalink) {
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
        applyDataLinkSettings(datalink);
        isDataLinkLoaded = true;
    }
    updateSleepVisual();
}

function applyDataLinkSettings(datalink) {
    document.getElementById('dataLinkEnabled').checked = datalink.dataLinkEnabled;
    document.getElementById('dataLinkSettingsContainer').style.display = datalink.dataLinkEnabled ? 'block' : 'none';
    document.getElementById('dataLinkTargetRow').value = datalink.dataLinkTargetRow;
    document.getElementById('dataLinkRefreshInterval').value = datalink.dataLinkRefreshInterval;
    document.getElementById('dataLinkRefreshIntervalValue').textContent = datalink.dataLinkRefreshInterval;
    document.getElementById('mqttBroker').value = datalink.mqttBroker || '';
    document.getElementById('mqttPort').value = datalink.mqttPort || 1883;
    document.getElementById('mqttUser').value = datalink.mqttUser || '';
    document.getElementById('mqttPassword').value = datalink.mqttPassword || '';
    document.getElementById('numDataPoints').value = datalink.numDataPoints;
    document.getElementById('numDataPointsValue').textContent = datalink.numDataPoints;
    updateDataPointsUI(datalink.numDataPoints).then(() => {
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
                document.getElementById(`dp_yearPrefix_${i}`).value = point.yearPrefix || '';
                document.getElementById(`dp_yearSuffix_${i}`).value = point.yearSuffix || '';
                document.getElementById(`dp_scrollingText_${i}`).value = point.scrollingText || '';
                document.getElementById(`dp_authHeaderKey_${i}`).value = point.authHeaderKey || '';
                document.getElementById(`dp_authHeaderValue_${i}`).value = point.authHeaderValue || '';
                document.getElementById(`dp_httpMethod_${i}`).value = point.httpMethod || 0;
                document.getElementById(`dp_requestBody_${i}`).value = point.requestBody || '';
                document.getElementById(`api_example_${i}`).value = point.apiExampleKey || '';

                document.getElementById(`dp_dataSourceType_${i}`).dispatchEvent(new Event('change'));
                document.getElementById(`dp_displayMode_${i}`).dispatchEvent(new Event('change'));
                document.getElementById(`dp_httpMethod_${i}`).dispatchEvent(new Event('change'));
                updateMarqueePreview(i);
            });
        }
    });
}

function attachEventListeners() {
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
        document.getElementById(id).onchange = () => { setSettingsChanged(true); updateHeaderClocks(new Date()); };
    });
    document.getElementById('destinationYear').oninput = () => updateHeaderClocks(new Date());

    document.getElementById('presetDateSelect').onchange = handlePresetSelectionChange;
    document.getElementById('savePresetBtn').onclick = handleSavePreset;
    document.getElementById('deletePresetBtn').onclick = deletePreset;
    document.getElementById('newPresetBtn').onclick = resetPresetForm;

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


    document.getElementById('dataLinkEnabled').onchange = (e) => {
        document.getElementById('dataLinkSettingsContainer').style.display = e.target.checked ? 'block' : 'none';
        setSettingsChanged(true);
    };
    document.getElementById('numDataPoints').oninput = (e) => {
        document.getElementById('numDataPointsValue').textContent = e.target.value;
        updateDataPointsUI(parseInt(e.target.value, 10));
        setSettingsChanged(true);
    };
    document.querySelectorAll('input, select').forEach(el => {
        el.addEventListener('change', () => setSettingsChanged(true));
        el.addEventListener('input', (e) => {
            const valueSpan = document.getElementById(`${e.target.id}Value`);
            if (valueSpan) valueSpan.textContent = e.target.value;
            setSettingsChanged(true);
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
        .then(res => res.text()).then(text => {
            showMessage(text, 'success');
            fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
            resetPresetForm();
        });
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
            fetch('/api/getPresets').then(res => res.json()).then(data => {
                populatePresetsSelect(data);
                resetPresetForm();
            });
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
    setSettingsChanged(true);
    updateHeaderClocks(new Date());
}

function scrollToSettings(tabName, elementId) {
    const tabButton = document.querySelector(`.tab-link[data-tab='${tabName}']`);
    if (tabButton) {
        openTab({ currentTarget: tabButton }, tabName);
        setTimeout(() => {
            const element = document.getElementById(elementId);
            if (element) element.scrollIntoView({ behavior: 'smooth', block: 'center' });
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
    document.getElementById('saveSettingsBtn').disabled = !isChanged;
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
    const year = document.getElementById('lastTimeDepartedYear').textContent;
    const month = parseInt(document.getElementById('lastTimeDepartedMonth').textContent, 10) - 1;
    const day = document.getElementById('lastTimeDepartedDay').textContent;
    const hour = document.getElementById('lastTimeDepartedHour').textContent;
    const minute = document.getElementById('lastTimeDepartedMinute').textContent;
    if(year && !isNaN(month) && day && hour && minute) {
        const lastDepartedTime = new Date(year, month, day, hour, minute);
        populateHeaderRow('last', lastDepartedTime.getTime() / 1000, year);
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
                        <label for="dp_httpMethod_${i}">HTTP Method:</label>
                        <select id="dp_httpMethod_${i}" class="http-method-select" data-index="${i}">
                            <option value="0">GET</option>
                            <option value="1">POST</option>
                        </select>
                        <label for="dp_url_${i}">API URL:</label>
                        <input type="text" id="dp_url_${i}" placeholder="http://api.example.com/data.json">
                        <div id="dp_post_body_container_${i}" style="display: none;">
                            <label for="dp_requestBody_${i}">Request Body (JSON):</label>
                            <textarea id="dp_requestBody_${i}" placeholder='{"key": "value"}' rows="4"></textarea>
                            <div class="validation-message" id="dp_requestBody_validation_${i}"></div>
                        </div>
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
                            <select id="dp_icon_${i}" style="width: 100px; margin-left: 10px;">
                                <option value="">Icon</option><option value="SUN">Sun</option><option value="CLOUD">Cloud</option><option value="RAIN">Rain</option><option value="SNOW">Snow</option><option value="STORM">Storm</option><option value="WIND">Wind</option><option value="UP">Up</option><option value="DOWN">Down</option><option value="EQUAL">Equal</option><option value="WIFI">WiFi</option><option value="HOME">Home</option><option value="WORK">Work</option><option value="CAR">Car</option><option value="BIKE">Bike</option><option value="RUN">Run</option><option value="HEART">Heart</option><option value="MONEY">Money</option><option value="CLOCK">Clock</option><option value="CAL">Calendar</option>
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

function updateMarqueePreview(index) {
    const displayMode = document.getElementById(`dp_displayMode_${index}`).value;

    if (displayMode === '0') { // Four Column Data
        const monthPath = document.getElementById(`dp_monthPath_${index}`).value;
        const dayPath = document.getElementById(`dp_dayPath_${index}`).value;
        const yearPath = document.getElementById(`dp_yearPath_${index}`).value;
        const timePath = document.getElementById(`dp_timePath_${index}`).value;

        const monthValue = getValueFromPath(analyzedDataCache[index], monthPath) || monthPath;
        const dayValue = getValueFromPath(analyzedDataCache[index], dayPath) || dayPath;
        
        const yearPrefix = document.getElementById(`dp_yearPrefix_${index}`).value;
        const yearSuffix = document.getElementById(`dp_yearSuffix_${index}`).value;
        const yearData = getValueFromPath(analyzedDataCache[index], yearPath) || yearPath;
        const yearFinalValue = `${yearPrefix}${yearData}${yearSuffix}`;

        const prefix = document.getElementById(`dp_prefix_${index}`).value;
        const suffix = document.getElementById(`dp_suffix_${index}`).value;
        const timeData = getValueFromPath(analyzedDataCache[index], timePath) || timePath;
        const timeFinalValue = `${prefix}${timeData}${suffix}`;

        document.querySelector(`#marquee_preview_${index} .preview-month`).textContent = monthValue.substring(0, 3).toUpperCase();
        document.querySelector(`#marquee_preview_${index} .preview-day`).textContent = dayValue.substring(0, 2).toUpperCase();
        
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
    } else { // Scrolling Text
        const scrollingPath = document.getElementById(`dp_scrollingText_${index}`).value;
        const text = getValueFromPath(analyzedDataCache[index], scrollingPath) || scrollingPath;
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


function attachDataPointEventListeners() {
    document.querySelectorAll('.data-source-select, .display-mode-select, .http-method-select').forEach(select => {
        select.onchange = (e) => {
            const index = e.target.dataset.index;
            const dataSource = document.getElementById(`dp_dataSourceType_${index}`).value;
            const displayMode = document.getElementById(`dp_displayMode_${index}`).value;
            const httpMethod = document.getElementById(`dp_httpMethod_${index}`).value;

            document.getElementById(`dp_api_container_${index}`).style.display = dataSource === 'api' ? 'block' : 'none';
            document.getElementById(`dp_mqtt_container_${index}`).style.display = dataSource === 'mqtt' ? 'block' : 'none';
            document.getElementById(`four_column_container_${index}`).style.display = displayMode === '0' ? 'block' : 'none';
            document.getElementById(`scrolling_text_container_${index}`).style.display = displayMode === '1' ? 'block' : 'none';
            document.getElementById(`dp_post_body_container_${index}`).style.display = httpMethod === '1' ? 'block' : 'none';
            
            updateMarqueePreview(index);
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

            if (e.target.id.includes('requestBody')) {
                validateJson(e.target);
            }
            updateMarqueePreview(index);
        });
    });

    document.querySelectorAll('.wizard-target-input').forEach(input => {
        input.addEventListener('click', (e) => {
            if (activeWizardTarget) {
                activeWizardTarget.classList.remove('is-wizard-target');
            }
            activeWizardTarget = e.target;
            activeWizardTarget.classList.add('is-wizard-target');
        });
    });
}

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

function startApiWizard(event) {
    const index = event.target.getAttribute('data-index');
    const apiUrl = document.getElementById(`dp_url_${index}`).value;
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
        data: {
            url: apiUrl,
            authKey: authKey,
            authValue: authValue
        }
    };

    ws.send(JSON.stringify(message));
}

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


function saveSettings() {
    showLoading('saveSettingsBtn', true);
    const formData = new URLSearchParams();
    const settingsToSave = ['destinationYear', 'destinationTimezoneSelect', 'presetCycleInterval', 'brightness', 'notificationVolume', 'timeTravelAnimationDuration', 'timeTravelAnimationInterval', 'animationStyleSelect', 'glitchEffectFrequency', 'malfunctionFrequency', 'presentTimezoneSelect', 'dataLinkTargetRow', 'dataLinkRefreshInterval', 'mqttBroker', 'mqttPort', 'mqttUser', 'mqttPassword'];
    settingsToSave.forEach(id => {
        const element = document.getElementById(id);
        if (element) {
            let key = id;
            if (id === 'destinationTimezoneSelect') key = 'destinationTimezoneIndex';
            if (id === 'presentTimezoneSelect') key = 'presentTimezoneIndex';
            if (id === 'animationStyleSelect') key = 'animationStyle';
            formData.append(key, element.value);
        }
    });
    ['timeTravelSoundToggle', 'timeTravelVolumeFade', 'displayFormat24h', 'dataLinkEnabled'].forEach(id => {
        formData.append(id, document.getElementById(id).checked);
    });
    ['lastTimeDepartedYear', 'lastTimeDepartedMonth', 'lastTimeDepartedDay', 'lastTimeDepartedHour', 'lastTimeDepartedMinute'].forEach(id => {
        formData.append(id, document.getElementById(id).textContent);
    });
    const [depHour, depMin] = document.getElementById('departureTime').value.split(':');
    formData.append('departureHour', depHour);
    formData.append('departureMinute', depMin);
    const [arrHour, arrMin] = document.getElementById('arrivalTime').value.split(':');
    formData.append('arrivalHour', arrHour);
    formData.append('arrivalMinute', arrMin);

    if (isDataLinkLoaded) {
        const numDataPoints = document.getElementById('numDataPoints').value;
        formData.append('numDataPoints', numDataPoints);
        for (let i = 0; i < numDataPoints; i++) {
            formData.append(`dp_dataSourceType_${i}`, document.getElementById(`dp_dataSourceType_${i}`).value === 'mqtt' ? 1 : 0);
            formData.append(`dp_displayMode_${i}`, document.getElementById(`dp_displayMode_${i}`).value);
            ['url', 'monthPath', 'dayPath', 'yearPath', 'timePath', 'prefix', 'suffix', 'icon', 'scrollSpeed', 'mqttTopic', 'yearPrefix', 'yearSuffix', 'scrollingText', 'authHeaderKey', 'authHeaderValue', 'httpMethod', 'requestBody'].forEach(field => {
                const el = document.getElementById(`dp_${field}_${i}`);
                if (el) formData.append(`dp_${field}_${i}`, el.value);
            });
            formData.append(`dp_apiExampleKey_${i}`, document.getElementById(`api_example_${i}`).value);
        }
    }

    fetch('/api/saveSettings', { method: 'POST', body: formData })
        .then(res => res.text()).then(text => {
            showMessage(text, 'success');
            setSettingsChanged(false);
            const duration = parseInt(document.getElementById('timeTravelAnimationDuration').value, 10);
            document.body.classList.add('time-travel-active');
            setTimeout(() => document.body.classList.remove('time-travel-active'), duration);
        }).catch(err => showMessage(`Error: ${err.message}`, 'error'))
        .finally(() => showLoading('saveSettingsBtn', false));
}

function fetchTime() {
    fetch('/api/time').then(res => res.json()).then(data => {
        document.getElementById('timeSyncStatus').textContent = data.timeSynchronized ? 'Yes' : 'No';
        if (data.unixTime) updateHeaderClocks(new Date(data.unixTime * 1000));
    });
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
        const duration1 = 1440 - depTotalMins;
        bar1.style.left = `${(depTotalMins / 1440) * 100}%`;
        bar1.style.width = `${(duration1 / 1440) * 100}%`;

        bar2.style.left = '0%';
        bar2.style.width = `${(arrTotalMins / 1440) * 100}%`;
        bar2.style.display = 'block';
    } else {
        const sleepDuration = arrTotalMins - depTotalMins;
        bar1.style.left = `${(depTotalMins / 1440) * 100}%`;
        bar1.style.width = `${(sleepDuration / 1440) * 100}%`;
        bar2.style.display = 'none';
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
    if (!path || !obj) return '';
    try {
        return path.split(/[.\[\]]+/).filter(Boolean).reduce((o, k) => (o || {})[k], obj) || '';
    } catch (e) {
        return '';
    }
}

function updateDataPointStatus(index, isSuccess) {
    const indicator = document.getElementById(`dp_status_${index}`);
    if (indicator) {
        indicator.className = 'dp-status-indicator'; // Reset
        indicator.classList.add(isSuccess ? 'success' : 'error');
    }
    dataPointStatus[index] = isSuccess;
}

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
    setSettingsChanged(true);
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
        const fields = ['dataSourceType', 'displayMode', 'url', 'monthPath', 'dayPath', 'yearPath', 'timePath', 'prefix', 'suffix', 'icon', 'scrollSpeed', 'mqttTopic', 'yearPrefix', 'yearSuffix', 'scrollingText', 'authHeaderKey', 'authHeaderValue', 'httpMethod', 'requestBody', 'api_example'];
        fields.forEach(field => {
            const sourceEl = document.getElementById(`dp_${field}_${sourceIndex}`);
            const targetEl = document.getElementById(`dp_${field}_${targetIndex}`);
            if (sourceEl && targetEl) {
                targetEl.value = sourceEl.value;
                targetEl.dispatchEvent(new Event('change'));
            }
        });
        showMessage(`Data Point ${sourceIndex + 1} duplicated to Data Point ${targetIndex + 1}.`, 'success');
        setSettingsChanged(true);
    });
}


function testDataPoint(event) {
    const index = event.target.dataset.index;
    const button = event.target;
    
    const dataSource = document.getElementById(`dp_dataSourceType_${index}`).value;
    if (dataSource !== 'api') {
        showMessage('Test is only available for API data points.', 'error');
        return;
    }

    const apiUrl = document.getElementById(`dp_url_${index}`).value;
    const authKey = document.getElementById(`dp_authHeaderKey_${index}`).value;
    const authValue = document.getElementById(`dp_authHeaderValue_${index}`).value;

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
            authKey: authKey,
            authValue: authValue
        }
    };
    ws.send(JSON.stringify(message));
}