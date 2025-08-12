let settingsChanged = false;
let timezoneOptions = [];
let isDataLinkLoaded = false;
let anyInputInvalid = false;

const apiExamples = {
    '': { name: '-- Select an Example --', url: '' },
    'weather_temp': { name: 'Weather: Temperature', url: 'https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current_weather=true' },
    'weather_wind': { name: 'Weather: Wind Speed', url: 'https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current_weather=true' },
    'stock_tsla': { name: 'Stock: Tesla (Requires Key)', url: 'https://finnhub.io/api/v1/quote?symbol=TSLA&token=YOUR_API_KEY' },
    'stock_aapl': { name: 'Stock: Apple (Requires Key)', url: 'https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=AAPL&apikey=YOUR_API_KEY' },
    'btc': { name: 'Crypto: Bitcoin Price', url: 'https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd' },
    'space': { name: 'Fun: People in Space', url: 'http://api.open-notify.org/astros.json' },
    'cat_fact': { name: 'Fun: Random Cat Fact', url: 'https://catfact.ninja/fact' },
    'joke': { name: 'Fun: Get a Random Joke', url: 'https://official-joke-api.appspot.com/random_joke' },
    'bored': { name: 'Fun: Get an Activity Idea', url: 'https://bored-api.appbrewery.com/random' },
};


document.addEventListener('DOMContentLoaded', () => {
    function waitForServerReady() {
        fetch('/api/isReady')
            .then(response => {
                if (response.ok) {
                    console.log("Server is ready. Initializing UI.");
                    initializeUI();
                } else {
                    setTimeout(waitForServerReady, 1000);
                }
            })
            .catch(error => {
                setTimeout(waitForServerReady, 1000);
            });
    }
    waitForServerReady();
});

function initializeUI() {
    const initialEndpoints = [
        '/api/settings/timecircuits',
        '/api/settings/temporal',
        '/api/settings/datalink',
        '/api/timezones',
        '/api/getPresets',
        '/api/getTheme'
    ];
    const promises = initialEndpoints.map(url => fetch(url).then(res => {
        if (!res.ok) return Promise.reject(new Error(`Request to ${url} failed`));
        if (url.endsWith('Theme')) return res.text();
        return res.json();
    }));

    Promise.all(promises)
        .then(results => {
            const [timecircuits, temporal, datalink, timezones, presets, theme] = results;
            
            document.body.className = theme;
            
            populateTimezoneSelects(timezones);
            populatePresetsSelect(presets);
            
            applySettings(timecircuits, temporal, datalink);
            document.querySelector('.header-circuits').classList.add('visible');
            fetchTime();
            setInterval(fetchTime, 1000);
            attachEventListeners();
            
            showMessage('System Online', 'success');
        })
        .catch(error => {
            console.error("Failed during essential initialization:", error);
            showMessage(`Critical error loading settings: ${error.message}. Please refresh.`, 'error');
        });
}

function loadDataLinkSettings() {
    if (isDataLinkLoaded) return;
    console.log("Fetching Data Link settings on-demand...");
    showMessage('Loading Data Link settings...', 'info');
    fetch('/api/settings/datalink')
        .then(res => res.ok ? res.json() : Promise.reject('Failed to load'))
        .then(datalink => {
            console.log("Data Link settings fetched successfully.");
            applyDataLinkSettings(datalink);
            isDataLinkLoaded = true;
            showMessage('Data Link settings loaded.', 'success');
        })
        .catch(error => {
            console.error("Failed to load Data Link settings:", error);
            showMessage(`Error loading Data Link: ${error.message}`, 'error');
        });
}

function populateTimezoneSelects(data) {
    timezoneOptions = [];
    const presentSelect = document.getElementById('presentTimezoneSelect');
    const destinationSelect = document.getElementById('destinationTimezoneSelect');
    presentSelect.innerHTML = '';
    destinationSelect.innerHTML = '';
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
        presentSelect.appendChild(optgroup.cloneNode(true));
        destinationSelect.appendChild(optgroup.cloneNode(true));
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
        ampm = displayHour >= 12 ? ' PM' : ' AM';
        if (displayHour > 12) displayHour -= 12;
        if (displayHour === 0) displayHour = 12;
    }

    const monthStr = String(month).padStart(2, '0');
    const dayStr = String(day).padStart(2, '0');
    const hourStr = String(displayHour).padStart(2, '0');
    const minuteStr = String(minute).padStart(2, '0');

    const formattedDate = `${monthStr}/${dayStr}/${year}`;
    const formattedTime = `${hourStr}:${minuteStr}${ampm}`;
    document.getElementById('lastTimeDepartedDisplay').textContent = `${formattedDate} ${formattedTime}`;

    document.getElementById('lastTimeDepartedYear').textContent = year;
    document.getElementById('lastTimeDepartedMonth').textContent = month;
    document.getElementById('lastTimeDepartedDay').textContent = day;
    document.getElementById('lastTimeDepartedHour').textContent = hour;
    document.getElementById('lastTimeDepartedMinute').textContent = minute;

    const presetValue = `${year}-${monthStr}-${dayStr}-${String(hour).padStart(2, '0')}-${minuteStr}`;
    const presetSelect = document.getElementById('presetDateSelect');
    let found = false;
    for (const option of presetSelect.options) {
        if (option.value === presetValue) {
            presetSelect.value = presetValue;
            found = true;
            break;
        }
    }
    if (!found) {
        presetSelect.value = "";
    }
    
    const selectedOption = presetSelect.options[presetSelect.selectedIndex];
    const isCustom = selectedOption && selectedOption.parentElement.label === 'Custom Time Jumps';
    document.getElementById('presetActions').classList.toggle('hidden', !isCustom);
}

function applySettings(timecircuits, temporal, datalink) {
    if(timecircuits) {
        document.getElementById('destinationYear').value = timecircuits.destinationYear;
        document.getElementById('destinationTimezoneSelect').value = timecircuits.destinationTimezoneIndex;
        document.getElementById('presentTimezoneSelect').value = timecircuits.presentTimezoneIndex;
        updateLastDepartedDisplay(
            timecircuits.lastTimeDepartedYear,
            timecircuits.lastTimeDepartedMonth,
            timecircuits.lastTimeDepartedDay,
            timecircuits.lastTimeDepartedHour,
            timecircuits.lastTimeDepartedMinute
        );
    }
    if(temporal) {
        const depHour = String(temporal.departureHour).padStart(2, '0');
        const depMin = String(temporal.departureMinute).padStart(2, '0');
        document.getElementById('departureTime').value = `${depHour}:${depMin}`;
        const arrHour = String(temporal.arrivalHour).padStart(2, '0');
        const arrMin = String(temporal.arrivalMinute).padStart(2, '0');
        document.getElementById('arrivalTime').value = `${arrHour}:${arrMin}`;
        
        ['brightness', 'notificationVolume', 'timeTravelAnimationDuration', 'timeTravelAnimationInterval', 'presetCycleInterval', 'glitchEffectFrequency', 'malfunctionFrequency'].forEach(id => {
            const slider = document.getElementById(id);
            if (slider) {
                slider.value = temporal[id];
                const valueSpan = document.getElementById(`${id}Value`);
                if (valueSpan) valueSpan.textContent = temporal[id];
            }
        });

        ['timeTravelSoundToggle', 'timeTravelVolumeFade', 'displayFormat24h'].forEach(id => {
            const toggle = document.getElementById(id);
            if (toggle) toggle.checked = temporal[id];
        });
        document.getElementById('animationStyleSelect').value = temporal.animationStyle;
    }
    if (datalink) {
        applyDataLinkSettings(datalink);
        isDataLinkLoaded = true;
    }
    updateSleepVisual();
}

function applyDataLinkSettings(datalink) {
    document.getElementById('dataLinkEnabled').checked = datalink.dataLinkEnabled;
    document.getElementById('dataLinkEnabled').dispatchEvent(new Event('change'));
    document.getElementById('dataLinkTargetRow').value = datalink.dataLinkTargetRow;
    document.getElementById('dataLinkRefreshInterval').value = datalink.dataLinkRefreshInterval;
    document.getElementById('dataLinkRefreshIntervalValue').textContent = datalink.dataLinkRefreshInterval;
    document.getElementById('numDataPoints').value = datalink.numDataPoints;
    document.getElementById('numDataPointsValue').textContent = datalink.numDataPoints;
    updateDataPointsUI(datalink.numDataPoints).then(() => {
        if (datalink.dataPoints) {
            datalink.dataPoints.forEach((point, i) => {
                document.getElementById(`dp_url_${i}`).value = point.url || '';
                document.getElementById(`dp_label_${i}`).value = point.label || '';
                document.getElementById(`dp_path_${i}`).value = point.jsonPath || '';
                document.getElementById(`dp_prefix_${i}`).value = point.prefix || '';
                document.getElementById(`dp_suffix_${i}`).value = point.suffix || '';
                document.getElementById(`dp_format_${i}`).value = point.format || '';
                document.getElementById(`dp_icon_${i}`).value = point.icon || '';
                const scrollSlider = document.getElementById(`dp_scrollSpeed_${i}`);
                scrollSlider.value = point.scrollSpeed || 150;
                scrollSlider.dispatchEvent(new Event('input'));
            });
        }
    });
}

function attachEventListeners() {
    document.getElementById('header-dest').onclick = () => scrollToSettings('TimeCircuits', 'destinationTimeSettings');
    document.getElementById('header-pres').onclick = () => scrollToSettings('System', 'presentTimeSettings');
    document.getElementById('header-last').onclick = () => scrollToSettings('TimeCircuits', 'lastDepartedSettings');
    document.getElementById('greatScottBtn').onclick = () => {
        fetch('/api/greatScott', { method: 'POST' });
    };
    
    document.getElementById('saveSettingsBtn').onclick = saveSettings;
    document.querySelectorAll('.tab-link').forEach(btn => btn.onclick = (e) => {
        const tabName = e.target.getAttribute('data-tab');
        openTab(e, tabName);
        if (tabName === 'DataLink' && !isDataLinkLoaded) loadDataLinkSettings();
    });
    
    const timezoneChangeHandler = () => {
        setSettingsChanged(true);
        updateHeaderClocks(new Date());
    };
    document.getElementById('destinationTimezoneSelect').onchange = timezoneChangeHandler;
    document.getElementById('presentTimezoneSelect').onchange = timezoneChangeHandler;
    document.getElementById('destinationYear').oninput = () => updateHeaderClocks(new Date());

    document.getElementById('presetDateSelect').onchange = applySelectedPreset;
    document.getElementById('addPresetBtn').onclick = addPreset;
    document.getElementById('updatePresetBtn').onclick = updatePreset;
    document.getElementById('deletePresetBtn').onclick = deletePreset;

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
                .then(res => res.text())
                .then(text => {
                    showMessage(text, 'success');
                    setTimeout(() => window.location.reload(), 1500);
                });
        }
    };
    document.getElementById('syncNtpBtn').onclick = () => {
        fetch('/api/syncTime', { method: 'POST' })
            .then(res => res.text())
            .then(text => showMessage(text, 'info'));
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

function scrollToSettings(tabName, elementId) {
    const tabButton = document.querySelector(`.tab-link[data-tab='${tabName}']`);
    if (tabButton) {
        openTab({ currentTarget: tabButton }, tabName);
        setTimeout(() => {
            const element = document.getElementById(elementId);
            if (element) {
                element.scrollIntoView({ behavior: 'smooth', block: 'center' });
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
    const saveBtn = document.getElementById('saveSettingsBtn');
    saveBtn.disabled = !isChanged || anyInputInvalid;
}

function validateDataPointField(event) {
    const el = event.target;
    el.classList.remove('invalid');
    if (el.id.startsWith('dp_format_') && el.value && !el.value.includes('%V')) {
        el.classList.add('invalid');
    }
    anyInputInvalid = !!document.querySelector('.invalid');
    setSettingsChanged(true);
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
        const timeParts = formatted.time.split(' ');
        const [hour, minute, second] = timeParts[0].split(':');
        const ampm = timeParts.length > 1 ? timeParts[1] : '';
        const dateParts = formatted.date.split('/');
        const monthNum = parseInt(dateParts[0], 10);
        const day = dateParts[1];
        const year = dateParts[2];
        const setContent = (id, text) => {
            const el = document.getElementById(id);
            if (el) el.textContent = text;
        };
        setContent(`header-${prefix}-month`, months[monthNum - 1] || '---');
        setContent(`header-${prefix}-day`, day || '00');
        setContent(`header-${prefix}-year`, yearOverride || year || '0000');
        setContent(`header-${prefix}-hour`, hour || '00');
        setContent(`header-${prefix}-minute`, minute || '00');
        setContent(`header-${prefix}-second`, second || '00');
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
    const lastMonth = parseInt(document.getElementById('lastTimeDepartedMonth').textContent, 10) - 1;
    const lastDay = document.getElementById('lastTimeDepartedDay').textContent;
    const lastHour = document.getElementById('lastTimeDepartedHour').textContent;
    const lastMinute = document.getElementById('lastTimeDepartedMinute').textContent;
    if(lastYear && !isNaN(lastMonth) && lastDay && lastHour && lastMinute) {
        const lastDepartedTime = new Date(lastYear, lastMonth, lastDay, lastHour, lastMinute);
        populateHeaderRow('last', lastDepartedTime.getTime() / 1000, lastYear);
    }

    const now = new Date();
    const totalMinutes = now.getHours() * 60 + now.getMinutes();
    const percentOfDay = (totalMinutes / (24 * 60)) * 100;
    document.getElementById('currentTimeMarker').style.left = `${percentOfDay}%`;
}

function formatDateTimeInTimezone(unixTimestamp, timezoneIndex, is24HourFormat) {
    if (!timezoneOptions || timezoneIndex < 0 || !timezoneOptions[timezoneIndex]) return null;
    const tzIANA = timezoneOptions[timezoneIndex].ianaTzName;
    const dateObj = new Date(unixTimestamp * 1000);
    try {
        const timeOptions = { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: !is24HourFormat, timeZone: tzIANA };
        const dateOptions = { year: 'numeric', month: '2-digit', day: '2-digit', timeZone: tzIANA };
        return {
            time: dateObj.toLocaleTimeString('en-US', timeOptions),
            date: dateObj.toLocaleDateString('en-US', dateOptions)
        };
    } catch (e) {
        return { time: "Error", date: "Error" };
    }
}

function updateDataPointsUI(numPoints) {
    return new Promise((resolve) => {
        const container = document.getElementById('dataPointsConfigContainer');
        container.innerHTML = '';
        if (numPoints > 0) {
            for (let i = 0; i < numPoints; i++) {
                const block = document.createElement('div');
                block.className = 'setting-group data-point-block';
                block.innerHTML = `<h4>Data Point ${i + 1}</h4>
                    <label for="api_example_${i}">API Examples (optional):</label>
                    <select id="api_example_${i}" class="api-example-select" data-index="${i}"></select>

                    <label for="dp_url_${i}">API URL:</label>
                    <input type="text" id="dp_url_${i}" placeholder="http://api.example.com/data.json">
                    <button class="analyze-api-btn" data-index="${i}">Analyze API</button>
                    <div class="api-wizard-results" id="wizard_results_${i}"></div>
                    <div class="api-final-details" id="final_details_${i}" style="display:none;">
                        <input type="hidden" id="dp_path_${i}">
                        <div class="preset-date-inputs">
                            <div style="width:100%"><label for="dp_label_${i}">Label (4 chars):</label><input type="text" id="dp_label_${i}" maxlength="4" placeholder="DATA"></div>
                            <div style="width:100%"><label for="dp_icon_${i}">Icon:</label><input type="text" id="dp_icon_${i}" placeholder="e.g., SUN, BTC"></div>
                        </div>
                        <div class="preset-date-inputs">
                            <div style="width:100%"><label for="dp_prefix_${i}">Prefix:</label><input type="text" id="dp_prefix_${i}" maxlength="16" placeholder="e.g., $"></div>
                            <div style="width:100%"><label for="dp_suffix_${i}">Suffix:</label><input type="text" id="dp_suffix_${i}" maxlength="16" placeholder="e.g., USD"></div>
                        </div>
                        <label for="dp_format_${i}">Format (%L, %V, %P, %S, |):</label><input type="text" id="dp_format_${i}" value="%L | %P%V%S">
                        <hr>
                        <label for="dp_scrollSpeed_${i}">Scroll Speed (ms/step): <span id="dp_scrollSpeed_val_${i}">150</span></label>
                        <input type="range" id="dp_scrollSpeed_${i}" min="50" max="500" step="10" value="150" oninput="document.getElementById('dp_scrollSpeed_val_${i}').textContent=this.value">
                    </div>`;
                container.appendChild(block);
            }
            populateApiExampleDropdowns();
            document.querySelectorAll('.analyze-api-btn').forEach(btn => btn.onclick = startApiWizard);
        }
        resolve();
    });
}

function populateApiExampleDropdowns() {
    document.querySelectorAll('.api-example-select').forEach(select => {
        for (const key in apiExamples) {
            const option = document.createElement('option');
            option.value = key;
            option.textContent = apiExamples[key].name;
            select.appendChild(option);
        }
        select.onchange = (e) => {
            const index = e.target.dataset.index;
            const url = apiExamples[e.target.value].url;
            document.getElementById(`dp_url_${index}`).value = url;
        };
    });
}


function applySelectedPreset(event) {
    const select = event.target;
    const value = select.value;
    if (!value) return;

    const [year, month, day, hour, minute] = value.split('-');
    updateLastDepartedDisplay(year, month, day, hour, minute);

    const selectedOption = select.options[select.selectedIndex];
    const isCustom = selectedOption.parentElement.label === 'Custom Time Jumps';
    if (isCustom) {
        document.getElementById('presetName').value = selectedOption.textContent;
        document.getElementById('presetDate').value = `${year}-${String(month).padStart(2,'0')}-${String(day).padStart(2,'0')}`;
        document.getElementById('presetTime').value = `${String(hour).padStart(2,'0')}:${String(minute).padStart(2,'0')}`;
    }

    showMessage(`Last Time Departed set to: ${selectedOption.text}`, 'info');
    setSettingsChanged(true);
    updateHeaderClocks(new Date());
}

function saveSettings() {
    showLoading('saveSettingsBtn', true);
    const formData = new URLSearchParams();

    const settingsToSave = ['destinationYear', 'destinationTimezoneSelect', 'presetCycleInterval', 'brightness', 'notificationVolume', 'timeTravelAnimationDuration', 'timeTravelAnimationInterval', 'animationStyleSelect', 'glitchEffectFrequency', 'malfunctionFrequency', 'presentTimezoneSelect', 'dataLinkTargetRow', 'dataLinkRefreshInterval'];
    
    for (const id of settingsToSave) {
        const element = document.getElementById(id);
        if (element) {
            let key = id;
            if (id === 'destinationTimezoneSelect') key = 'destinationTimezoneIndex';
            if (id === 'presentTimezoneSelect') key = 'presentTimezoneIndex';
            if (id === 'animationStyleSelect') key = 'animationStyle';
            formData.append(key, element.value);
        } else {
            return;
        }
    }
    const togglesToSave = ['timeTravelSoundToggle', 'timeTravelVolumeFade', 'displayFormat24h', 'dataLinkEnabled'];
    togglesToSave.forEach(id => formData.append(id, document.getElementById(id).checked));
    formData.append('lastTimeDepartedYear', document.getElementById('lastTimeDepartedYear').textContent);
    formData.append('lastTimeDepartedMonth', document.getElementById('lastTimeDepartedMonth').textContent);
    formData.append('lastTimeDepartedDay', document.getElementById('lastTimeDepartedDay').textContent);
    formData.append('lastTimeDepartedHour', document.getElementById('lastTimeDepartedHour').textContent);
    formData.append('lastTimeDepartedMinute', document.getElementById('lastTimeDepartedMinute').textContent);
    const departureTime = document.getElementById('departureTime').value.split(':');
    formData.append('departureHour', departureTime[0]);
    formData.append('departureMinute', departureTime[1]);
    const arrivalTime = document.getElementById('arrivalTime').value.split(':');
    formData.append('arrivalHour', arrivalTime[0]);
    formData.append('arrivalMinute', arrivalTime[1]);
    if (isDataLinkLoaded) {
        const numDataPoints = document.getElementById('numDataPoints').value;
        formData.append('numDataPoints', numDataPoints);
        for (let i = 0; i < numDataPoints; i++) {
            if (!document.getElementById(`dp_url_${i}`)) break;
            formData.append(`dp_url_${i}`, document.getElementById(`dp_url_${i}`).value);
            formData.append(`dp_label_${i}`, document.getElementById(`dp_label_${i}`).value);
            formData.append(`dp_path_${i}`, document.getElementById(`dp_path_${i}`).value);
            formData.append(`dp_prefix_${i}`, document.getElementById(`dp_prefix_${i}`).value);
            formData.append(`dp_suffix_${i}`, document.getElementById(`dp_suffix_${i}`).value);
            formData.append(`dp_format_${i}`, document.getElementById(`dp_format_${i}`).value);
            formData.append(`dp_icon_${i}`, document.getElementById(`dp_icon_${i}`).value);
            formData.append(`dp_scrollSpeed_${i}`, document.getElementById(`dp_scrollSpeed_${i}`).value);
        }
    }
    fetch('/api/saveSettings', { method: 'POST', body: formData })
        .then(res => res.text())
        .then(text => {
            showMessage(text, 'success');
            setSettingsChanged(false);
            const duration = parseInt(document.getElementById('timeTravelAnimationDuration').value, 10);
            document.body.classList.add('time-travel-active');
            setTimeout(() => document.body.classList.remove('time-travel-active'), duration);
        })
        .catch(err => showMessage(`Error: ${err.message}`, 'error'))
        .finally(() => showLoading('saveSettingsBtn', false));
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
    const value = `${year}-${month}-${day}-${hour}-${minute}`;
    const formData = new URLSearchParams({ name, value });
    fetch('/api/addPreset', { method: 'POST', body: formData })
        .then(res => res.text())
        .then(text => {
            showMessage(text, 'success');
            fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
            document.getElementById('presetName').value = '';
            document.getElementById('presetDate').value = '';
            document.getElementById('presetTime').value = '';
        });
}

function updatePreset() {
    const name = document.getElementById('presetDateSelect').options[document.getElementById('presetDateSelect').selectedIndex].text;
    const date = document.getElementById('presetDate').value;
    const time = document.getElementById('presetTime').value;
    if (!name || !date || !time) {
        showMessage('Preset name, date, and time are required.', 'error');
        return;
    }
    const [year, month, day] = date.split('-');
    const [hour, minute] = time.split(':');
    const value = `${year}-${month}-${day}-${hour}-${minute}`;
    const formData = new URLSearchParams({ name, value });
    fetch('/api/updatePreset', { method: 'POST', body: formData })
        .then(res => res.text())
        .then(text => {
            showMessage(text, 'success');
            fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
        });
}

function deletePreset() {
    const name = document.getElementById('presetDateSelect').options[document.getElementById('presetDateSelect').selectedIndex].text;
    if (confirm(`Are you sure you want to delete the preset "${name}"?`)) {
        const formData = new URLSearchParams({ name });
        fetch('/api/deletePreset', { method: 'POST', body: formData })
            .then(res => res.text())
            .then(text => {
                showMessage(text, 'success');
                fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
                document.getElementById('presetActions').classList.add('hidden');
                document.getElementById('presetName').value = '';
                document.getElementById('presetDate').value = '';
                document.getElementById('presetTime').value = '';
            });
    }
}

function fetchTime() {
    fetch('/api/time')
        .then(res => res.json())
        .then(data => {
            const timeSyncStatusEl = document.getElementById('timeSyncStatus');
            if (timeSyncStatusEl) timeSyncStatusEl.textContent = data.timeSynchronized ? 'Yes' : 'No';
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
    const bar = document.getElementById('sleepScheduleBar');
    let bar2 = document.getElementById('sleepScheduleBar2');
    if (!bar2) {
        bar2 = document.createElement('div');
        bar2.id = 'sleepScheduleBar2';
        bar2.className = 'sleep-schedule-bar';
        document.querySelector('.sleep-schedule-visual').prepend(bar2);
    }

    if (arrTotalMins < depTotalMins) { // Normal overnight case
        const sleepDuration = (1440 - depTotalMins) + arrTotalMins;
        bar.style.left = `${(depTotalMins / 1440) * 100}%`;
        bar.style.width = `${(sleepDuration / 1440) * 100}%`;
        bar2.style.width = '0%';
    } else { // Same day sleep
        const sleepDuration = arrTotalMins - depTotalMins;
        bar.style.left = `${(depTotalMins / 1440) * 100}%`;
        bar.style.width = `${(sleepDuration / 1440) * 100}%`;
        bar2.style.width = '0%';
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

// --- NEW API WIZARD FUNCTIONS ---

function startApiWizard(event) {
    const index = event.target.getAttribute('data-index');
    const url = document.getElementById(`dp_url_${index}`).value;
    const resultsContainer = document.getElementById(`wizard_results_${index}`);
    
    if (!url) {
        showMessage('Please enter an API URL first.', 'error');
        return;
    }
    
    resultsContainer.innerHTML = '<span class="loading-spinner"></span> Analyzing...';
    
    fetch('/api/testDataPoint', { 
        method: 'POST', 
        body: new URLSearchParams({ url, path: '' }) // Send empty path to get full JSON
    })
    .then(res => res.json())
    .then(data => {
        if (data.success) {
            displayApiWizardResults(index, data.value);
        } else {
            showMessage(`Error: ${data.error}`, 'error');
            resultsContainer.innerHTML = `<span class="error-text">${data.error}</span>`;
        }
    })
    .catch(err => {
        showMessage(`Network Error: ${err.message}`, 'error');
        resultsContainer.innerHTML = `<span class="error-text">Network error.</span>`;
    });
}

function displayApiWizardResults(index, jsonData) {
    const container = document.getElementById(`wizard_results_${index}`);
    container.innerHTML = '<strong>Click the data point you want to display:</strong>';
    const list = document.createElement('ul');
    list.className = 'wizard-list';
    
    // Recursively build the list of clickable items
    const buildList = (obj, parentPath = '') => {
        for (const key in obj) {
            const currentPath = parentPath ? `${parentPath}.${key}` : key;
            const value = obj[key];
            const li = document.createElement('li');

            if (typeof value === 'object' && value !== null && !Array.isArray(value)) {
                li.textContent = `${key}:`;
                li.className = 'wizard-object-key';
                const subList = document.createElement('ul');
                buildList(value, currentPath).forEach(item => subList.appendChild(item));
                li.appendChild(subList);
            } else if (Array.isArray(value)) {
                 li.textContent = `${key}[...]:`;
                 li.className = 'wizard-object-key';
                 const subList = document.createElement('ul');
                 // For arrays, let's just show the first item's keys if it's an object
                 if (value.length > 0 && typeof value[0] === 'object' && value[0] !== null) {
                     buildList(value[0], `${currentPath}[0]`).forEach(item => subList.appendChild(item));
                 }
                 li.appendChild(subList);
            } else {
                li.innerHTML = `<span class="wizard-key">${key}:</span> <span class="wizard-value">"${String(value)}"</span>`;
                li.className = 'wizard-clickable-item';
                li.onclick = () => selectApiWizardValue(index, currentPath, String(value));
            }
            list.appendChild(li);
        }
        return list.childNodes;
    };
    
    buildList(jsonData);
    container.appendChild(list);
}

function selectApiWizardValue(index, path, value) {
    // Hide the wizard results
    const resultsContainer = document.getElementById(`wizard_results_${index}`);
    resultsContainer.innerHTML = `<p class="wizard-selection"><strong>Selected:</strong> ${path} <span class="wizard-change-btn">(Change)</span></p>`;
    resultsContainer.querySelector('.wizard-change-btn').onclick = (e) => {
        const btn = e.target.closest('.data-point-block').querySelector('.analyze-api-btn');
        btn.click();
    };

    // Populate the hidden path field
    document.getElementById(`dp_path_${index}`).value = path;

    // Show the final details form
    const finalDetails = document.getElementById(`final_details_${index}`);
    finalDetails.style.display = 'block';

    // Suggest a label based on the key
    const labelSuggestion = path.split('.').pop().replace(/\[\d+\]/,'').substring(0, 4).toUpperCase();
    document.getElementById(`dp_label_${index}`).value = labelSuggestion;

    showMessage(`Value "${value}" selected. Please confirm the label and format.`, 'success');
    setSettingsChanged(true);
}