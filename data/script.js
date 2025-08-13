let settingsChanged = false;
let timezoneOptions = [];
let isDataLinkLoaded = false;
let anyInputInvalid = false;
let analyzedDataCache = {};

const apiExamples = {
    '': { name: '-- Select an Example --', url: '' },
    'stock_aapl_price': { name: 'Stock: Apple Price', url: 'https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=AAPL&apikey=YOUR_API_KEY' },
    'stock_aapl_change': { name: 'Stock: Apple Change %', url: 'https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=AAPL&apikey=YOUR_API_KEY' },
    'crypto_btc_price': { name: 'Crypto: Bitcoin Price', url: 'https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd' },
    'crypto_eth_change': { name: 'Crypto: Ethereum Change %', url: 'https://api.coingecko.com/api/v3/simple/price?ids=ethereum&vs_currencies=usd&include_24hr_change=true' },
    'weather_temp': { name: 'Weather: Temperature (°F)', url: 'https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current=temperature_2m&temperature_unit=fahrenheit' },
    'weather_feels_like': { name: 'Weather: Feels Like (°F)', url: 'https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current=apparent_temperature&temperature_unit=fahrenheit' },
    'weather_humidity': { name: 'Weather: Humidity', url: 'https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current=relative_humidity_2m' },
    'weather_wind_speed': { name: 'Weather: Wind Speed', url: 'https://api.open-meteo.com/v1/forecast?latitude=40.71&longitude=-74.01&current=wind_speed_10m&wind_speed_unit=mph' },
    'space_iss_pos': { name: 'Space: ISS Position', url: 'http://api.open-notify.org/iss-now.json' },
    'space_astros': { name: 'Space: People in Space', url: 'http://api.open-notify.org/astros.json' },
    'space_mars_sol': { name: 'Space: Mars Rover Sol', url: 'https://api.maas2.apollorion.com/' },
    'space_sun_dist': { name: 'Space: Sun Distance', url: 'https://api.le-systeme-solaire.net/rest/bodies/soleil' },
    'util_ip': { name: 'Utility: Public IP', url: 'http://ip-api.com/json' },
    'util_network_info': { name: 'Utility: Network Info', url: 'http://ip-api.com/json' },
    'util_day_of_year': { name: 'Utility: Day of Year', url: 'http://worldtimeapi.org/api/ip' },
    'util_github_commits': { name: 'Utility: GitHub Commits', url: 'https://api.github.com/repos/octocat/Hello-World/commits' },
    'fun_yt_subs': { name: 'Fun: YouTube Subscribers', url: 'https://www.googleapis.com/youtube/v3/channels?part=statistics&id=UC_x5XG1OV2P6uZZ5FSM9Ttw&key=YOUR_API_KEY' },
    'fun_twitch_viewers': { name: 'Fun: Twitch Viewers', url: 'https://api.twitch.tv/helix/streams?user_login=shroud' },
    'fun_holiday_countdown': { name: 'Fun: Holiday Countdown (see note)', url: 'http://worldtimeapi.org/api/ip' },
    'fun_game_users': { name: 'Fun: Game Server Users', url: 'https://api.steampowered.com/ISteamUserStats/GetNumberOfCurrentPlayers/v1/?appid=730' }
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
        '/api/settings/timecircuits', '/api/settings/temporal',
        '/api/settings/datalink', '/api/timezones',
        '/api/getPresets', '/api/getTheme'
    ];
    const promises = initialEndpoints.map(url => fetch(url).then(res => {
        if (!res.ok) return Promise.reject(new Error(`Request to ${url} failed`));
        return url.endsWith('Theme') ? res.text() : res.json();
    }));

    Promise.all(promises).then(results => {
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
    }).catch(error => {
        console.error("Failed during essential initialization:", error);
        showMessage(`Critical error loading settings: ${error.message}. Please refresh.`, 'error');
    });
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
            console.error("Failed to load Data Link settings:", error);
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
        ampm = displayHour >= 12 ? ' PM' : ' AM';
        if (displayHour > 12) displayHour -= 12;
        if (displayHour === 0) displayHour = 12;
    }
    const monthStr = String(month).padStart(2, '0');
    const dayStr = String(day).padStart(2, '0');
    const hourStr = String(displayHour).padStart(2, '0');
    const minuteStr = String(minute).padStart(2, '0');
    document.getElementById('lastTimeDepartedDisplay').textContent = `${monthStr}/${dayStr}/${year} ${hourStr}:${minuteStr}${ampm}`;
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
        updateLastDepartedDisplay(timecircuits.lastTimeDepartedYear, timecircuits.lastTimeDepartedMonth, timecircuits.lastTimeDepartedDay, timecircuits.lastTimeDepartedHour, timecircuits.lastTimeDepartedMinute);
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
                document.getElementById(`dp_dataSourceType_${i}`).dispatchEvent(new Event('change'));
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
        const [hour, minute, second] = formatted.time.split(' ')[0].split(':');
        const ampm = formatted.time.split(' ').length > 1 ? formatted.time.split(' ')[1] : '';
        const [monthNum, day, year] = formatted.date.split('/');
        const setContent = (id, text) => { document.getElementById(id).textContent = text; };
        setContent(`header-${prefix}-month`, months[parseInt(monthNum, 10) - 1] || '---');
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
                const block = document.createElement('div');
                block.className = 'setting-group data-point-block';
                block.innerHTML = `
                    <h4>Data Point ${i + 1}</h4>
                    <label for="dp_dataSourceType_${i}">Data Source:</label>
                    <select id="dp_dataSourceType_${i}" class="data-source-select" data-index="${i}">
                        <option value="api">Web API (HTTP)</option>
                        <option value="mqtt">MQTT Broker</option>
                    </select>

                    <div id="dp_api_container_${i}">
                        <label for="api_example_${i}">API Examples (optional):</label>
                        <select id="api_example_${i}" class="api-example-select" data-index="${i}"></select>
                        <label for="dp_url_${i}">API URL:</label>
                        <input type="text" id="dp_url_${i}" placeholder="http://api.example.com/data.json">
                        <button class="analyze-api-btn" data-index="${i}">Analyze API</button>
                        <div class="api-wizard-results" id="wizard_results_${i}"></div>
                    </div>
                    
                    <div id="dp_mqtt_container_${i}" style="display: none;">
                        <label for="dp_mqttTopic_${i}">MQTT Topic:</label>
                        <input type="text" id="dp_mqttTopic_${i}" placeholder="e.g., /home/livingroom/temperature">
                    </div>

                    <div class="time-circuit-row">
                        <label for="dp_monthPath_${i}" class="time-circuit-label">MONTH</label>
                        <input type="text" id="dp_monthPath_${i}" class="time-circuit-input">
                    </div>
                    <div class="time-circuit-row">
                        <label for="dp_dayPath_${i}" class="time-circuit-label">DAY</label>
                        <input type="text" id="dp_dayPath_${i}" class="time-circuit-input">
                        <select id="dp_icon_${i}" style="width: 100px; margin-left: 10px;">
                            <option value="">Icon</option><option value="SUN">Sun</option><option value="CLOUD">Cloud</option><option value="RAIN">Rain</option><option value="SNOW">Snow</option><option value="STORM">Storm</option><option value="WIND">Wind</option><option value="UP">Up</option><option value="DOWN">Down</option><option value="EQUAL">Equal</option><option value="WIFI">WiFi</option><option value="HOME">Home</option><option value="WORK">Work</option><option value="CAR">Car</option><option value="BIKE">Bike</option><option value="RUN">Run</option><option value="HEART">Heart</option><option value="MONEY">Money</option><option value="CLOCK">Clock</option><option value="CAL">Calendar</option>
                        </select>
                    </div>
                    <div class="time-circuit-row">
                        <label for="dp_yearPath_${i}" class="time-circuit-label">YEAR</label>
                        <input type="text" id="dp_yearPath_${i}" class="time-circuit-input">
                    </div>

                    <div class="time-format-group">
                        <div class="time-circuit-row">
                            <label for="dp_timePath_${i}" class="time-circuit-label">TIME</label>
                            <input type="text" id="dp_timePath_${i}" class="time-circuit-input">
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

                    <label for="dp_scrollSpeed_${i}">Scroll Speed (ms/char): <span id="dp_scrollSpeed_${i}Value">150</span></label>
                    <input type="range" id="dp_scrollSpeed_${i}" min="50" max="500" step="10" value="150">
                `;
                container.appendChild(block);
            }
            populateApiExampleDropdowns();
            attachDataPointEventListeners();
        }
        resolve();
    });
}

function populateApiExampleDropdowns() {
    document.querySelectorAll('.api-example-select').forEach(select => {
        Object.keys(apiExamples).forEach(key => {
            const option = document.createElement('option');
            option.value = key;
            option.textContent = apiExamples[key].name;
            select.appendChild(option);
        });
    });
}

function attachDataPointEventListeners() {
    document.querySelectorAll('.data-source-select').forEach(select => {
        select.onchange = (e) => {
            const index = e.target.dataset.index;
            document.getElementById(`dp_api_container_${index}`).style.display = e.target.value === 'api' ? 'block' : 'none';
            document.getElementById(`dp_mqtt_container_${index}`).style.display = e.target.value === 'mqtt' ? 'block' : 'none';
        };
    });
    document.querySelectorAll('.analyze-api-btn').forEach(btn => btn.onclick = startApiWizard);
    document.querySelectorAll('.api-example-select').forEach(select => {
        select.onchange = (e) => {
            const index = e.target.dataset.index;
            document.getElementById(`dp_url_${index}`).value = apiExamples[e.target.value].url;
        };
    });
}

function startApiWizard(event) {
    const index = event.target.getAttribute('data-index');
    const url = document.getElementById(`dp_url_${index}`).value;
    if (!url) {
        showMessage('Please enter an API URL first.', 'error');
        return;
    }
    const resultsContainer = document.getElementById(`wizard_results_${index}`);
    resultsContainer.innerHTML = '<span class="loading-spinner"></span> Analyzing...';
    fetch('/api/testDataPoint', { method: 'POST', body: new URLSearchParams({ url }) })
        .then(res => res.json()).then(data => {
            if (data.success) {
                analyzedDataCache[index] = data.value;
                displayApiWizardResults(index, data.value);
            } else {
                showMessage(`Error: ${data.error}`, 'error');
                resultsContainer.innerHTML = `<span class="error-text">${data.error}</span>`;
            }
        }).catch(err => {
            showMessage(`Network Error: ${err.message}`, 'error');
            resultsContainer.innerHTML = `<span class="error-text">Network error.</span>`;
        });
}

function displayApiWizardResults(index, jsonData) {
    const container = document.getElementById(`wizard_results_${index}`);
    container.innerHTML = '<strong>Click a value to map it to a display:</strong>';
    const list = document.createElement('ul');
    list.className = 'wizard-list';
    const buildList = (obj, parentPath = '') => {
        for (const key in obj) {
            const currentPath = parentPath ? `${parentPath}.${key}` : key;
            const value = obj[key];
            const li = document.createElement('li');
            if (typeof value === 'object' && value !== null) {
                li.innerHTML = `<span class="wizard-key">${key}:</span>`;
                const subList = document.createElement('ul');
                buildList(value, currentPath).forEach(item => subList.appendChild(item));
                li.appendChild(subList);
            } else {
                li.innerHTML = `<span class="wizard-key">${currentPath}:</span> <span class="wizard-value">"${String(value)}"</span> 
                <button class="wizard-map-btn" data-path="${currentPath}" data-index="${index}" data-target="month">M</button>
                <button class="wizard-map-btn" data-path="${currentPath}" data-index="${index}" data-target="day">D</button>
                <button class="wizard-map-btn" data-path="${currentPath}" data-index="${index}" data-target="year">Y</button>
                <button class="wizard-map-btn" data-path="${currentPath}" data-index="${index}" data-target="time">T</button>`;
            }
            list.appendChild(li);
        }
        return list.childNodes;
    };
    buildList(jsonData).forEach(item => list.appendChild(item));
    container.appendChild(list);
    container.querySelectorAll('.wizard-map-btn').forEach(btn => btn.onclick = (e) => {
        const { path, index, target } = e.target.dataset;
        document.getElementById(`dp_${target}Path_${index}`).value = path;
        showMessage(`Mapped "${path}" to ${target.toUpperCase()}`, 'success', 2000);
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
            ['url', 'monthPath', 'dayPath', 'yearPath', 'timePath', 'prefix', 'suffix', 'icon', 'scrollSpeed', 'mqttTopic'].forEach(field => {
                formData.append(`dp_${field}_${i}`, document.getElementById(`dp_${field}_${i}`).value);
            });
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
    fetch('/api/addPreset', { method: 'POST', body: new URLSearchParams({ name, value }) })
        .then(res => res.text()).then(text => {
            showMessage(text, 'success');
            fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
            ['presetName', 'presetDate', 'presetTime'].forEach(id => document.getElementById(id).value = '');
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
    fetch('/api/updatePreset', { method: 'POST', body: new URLSearchParams({ name, value }) })
        .then(res => res.text()).then(text => {
            showMessage(text, 'success');
            fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
        });
}

function deletePreset() {
    const name = document.getElementById('presetDateSelect').options[document.getElementById('presetDateSelect').selectedIndex].text;
    if (confirm(`Are you sure you want to delete the preset "${name}"?`)) {
        fetch('/api/deletePreset', { method: 'POST', body: new URLSearchParams({ name }) })
            .then(res => res.text()).then(text => {
                showMessage(text, 'success');
                fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
                document.getElementById('presetActions').classList.add('hidden');
                ['presetName', 'presetDate', 'presetTime'].forEach(id => document.getElementById(id).value = '');
            });
    }
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
    const bar = document.getElementById('sleepScheduleBar');
    if (arrTotalMins < depTotalMins) {
        const sleepDuration = (1440 - depTotalMins) + arrTotalMins;
        bar.style.left = `${(depTotalMins / 1440) * 100}%`;
        bar.style.width = `${(sleepDuration / 1440) * 100}%`;
    } else {
        const sleepDuration = arrTotalMins - depTotalMins;
        bar.style.left = `${(depTotalMins / 1440) * 100}%`;
        bar.style.width = `${(sleepDuration / 1440) * 100}%`;
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