let settingsChanged = false;
let timezoneOptions = [];
let isDataLinkLoaded = false;
let anyInputInvalid = false;
let analyzedDataCache = {};
let selectedPathInfo = { index: null, path: null };

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
                document.getElementById(`dp_scroll_enabled_${i}`).checked = point.scrollEnabled || false;

                const pauseSlider = document.getElementById(`dp_pauseDuration_${i}`);
                if (pauseSlider) {
                    pauseSlider.value = point.pauseDuration || 5;
                    document.getElementById(`dp_pauseDuration_${i}Value`).textContent = pauseSlider.value;
                }

                ['month', 'day', 'year', 'time'].forEach(segment => {
                    const configDiv = document.getElementById(`config_${i}_${segment}`);
                    const mode = point[`${segment}Mode`] || 'static';
                    const value = point[`${segment}Value`] || '';

                    configDiv.querySelector(`.mode-btn[data-mode="${mode}"]`).classList.add('active');
                    configDiv.querySelector(`.mode-btn[data-mode="${mode === 'static' ? 'api' : 'static'}"]`).classList.remove('active');
                    document.getElementById(`dp_input_${i}_${segment}`).value = value;
                });

                if (point.icon) {
                    const iconSelect = document.querySelector(`#config_${i}_day .icon-select`);
                    iconSelect.value = point.icon;
                    if (point.icon) {
                        const textInput = document.getElementById(`dp_input_${i}_day`);
                        textInput.disabled = true;
                        textInput.value = '';
                    }
                }

                handleScrollingToggle({ target: document.getElementById(`dp_scroll_enabled_${i}`) });
                updateLivePreview({ target: document.getElementById(`dp_input_${i}_month`) });
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
                block.innerHTML = `
                    <h4>Data Point ${i + 1}</h4>
                    <div class="api-wizard-main">
                        <label for="api_example_${i}">API Examples (optional):</label>
                        <select id="api_example_${i}" class="api-example-select" data-index="${i}"></select>
                        <label for="dp_url_${i}">API URL:</label>
                        <input type="text" id="dp_url_${i}" placeholder="http://api.example.com/data.json">
                        <button class="analyze-api-btn" data-index="${i}">Analyze API</button>
                    </div>
                    <div class="segmented-form-container" id="form_container_${i}">
                        <div class="api-wizard-results" id="wizard_results_${i}"></div>
                        <div class="segmented-inputs-column">
                            ${generateSegmentInput(i, 'month', 'MONTH (3)')}
                            ${generateSegmentInput(i, 'day', 'DAY (2)')}
                            ${generateSegmentInput(i, 'year', 'YEAR (4)')}
                            ${generateSegmentInput(i, 'time', 'TIME (4)')}
                        </div>
                        <div class="scrolling-toggle">
                            <label for="dp_scroll_enabled_${i}" class="toggle-label">Enable Scrolling:</label>
                            <label class="toggle-switch small">
                                <input type="checkbox" id="dp_scroll_enabled_${i}" data-index="${i}">
                                <span class="slider round"></span>
                            </label>
                        </div>
                        <hr>
                        <label for="dp_pauseDuration_${i}">Pause Duration (sec): <span id="dp_pauseDuration_${i}Value">5</span></label>
                        <div class="slider-container">
                             <input type="range" id="dp_pauseDuration_${i}" min="1" max="30" value="5" data-index="${i}">
                        </div>
                    </div>
                    <div class="live-preview-container">
                        <label>Live Preview:</label>
                        <div class="live-preview" id="live_preview_${i}">
                            <div class="segment month"></div>
                            <div class="segment day"></div>
                            <div class="segment year"></div>
                            <div class="segment time"></div>
                        </div>
                    </div>
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
        for (const key in apiExamples) {
            const option = document.createElement('option');
            option.value = key;
            option.textContent = apiExamples[key].name;
            select.appendChild(option);
        }
    });
}

function generateSegmentInput(index, segment, label) {
    let iconDropdown = '';
    if (segment === 'day') {
        iconDropdown = `
            <select class="icon-select" data-index="${index}" data-segment="${segment}">
                <option value="">-- No Icon --</option>
                <option value="SUN">SUN</option>
                <option value="CLOUD">CLOUD</option>
                <option value="WIFI">WIFI</option>
                <option value="BTC">BTC</option>
                <option value="USD">USD</option>
                <option value="EUR">EUR</option>
                <option value="HEART">HEART</option>
                <option value="STAR">STAR</option>
                <option value="TEMP">TEMP</option>
                <option value="WIND">WIND</option>
                <option value="RISE">RISE</option>
                <option value="FALL">FALL</option>
                <option value="MAIL">MAIL</option>
                <option value="USER">USER</option>
            </select>
        `;
    }
    return `
        <div class="segment-config" id="config_${index}_${segment}">
            <label>${label}</label>
            <div class="mode-toggle">
                <button class="mode-btn active" data-mode="static" data-index="${index}" data-segment="${segment}">Txt</button>
                <button class="mode-btn" data-mode="api" data-index="${index}" data-segment="${segment}">API</button>
            </div>
            <input type="text" class="segment-input" id="dp_input_${index}_${segment}" data-index="${index}" data-segment="${segment}">
            ${iconDropdown}
        </div>
    `;
}

function attachDataPointEventListeners() {
    document.querySelectorAll('.analyze-api-btn').forEach(btn => btn.onclick = startApiWizard);
    document.querySelectorAll('.api-example-select').forEach(select => {
        select.onchange = (e) => {
            const index = e.target.dataset.index;
            const url = apiExamples[e.target.value].url;
            document.getElementById(`dp_url_${index}`).value = url;
        };
    });
    document.querySelectorAll('.mode-btn').forEach(btn => btn.onclick = toggleSegmentMode);

    document.querySelectorAll('.segment-input').forEach(input => {
        input.oninput = (e) => {
            if (e.target.dataset.segment === 'day') {
                const { index } = e.target.dataset;
                const iconSelect = document.getElementById(`config_${index}_day`).querySelector('.icon-select');
                if (iconSelect) iconSelect.value = '';
            }
            updateLivePreview(e);
        };
    });

    document.querySelectorAll('.icon-select').forEach(select => {
        select.onchange = (e) => {
            const { index } = e.target.dataset;
            const textInput = document.getElementById(`dp_input_${index}_day`);
            if (e.target.value) {
                textInput.value = '';
                textInput.disabled = true;
            } else {
                textInput.disabled = false;
            }
            updateLivePreview(e);
        };
    });

    document.querySelectorAll('input[type="checkbox"][id^="dp_scroll_enabled_"]').forEach(checkbox => {
        checkbox.onchange = handleScrollingToggle;
    });

    document.querySelectorAll('input[id^="dp_pauseDuration_"]').forEach(slider => {
        slider.oninput = (e) => {
            document.getElementById(`${e.target.id}Value`).textContent = e.target.value;
            setSettingsChanged(true);
        };
    });
}

function toggleSegmentMode(event) {
    const { index, segment, mode } = event.target.dataset;
    const configDiv = document.getElementById(`config_${index}_${segment}`);
    const input = document.getElementById(`dp_input_${index}_${segment}`);

    if (mode === 'api' && selectedPathInfo.index == index && selectedPathInfo.path) {
        configDiv.querySelectorAll('.mode-btn').forEach(btn => btn.classList.remove('active'));
        event.target.classList.add('active');
        input.value = `$.${selectedPathInfo.path}`;
        clearSelectedPath();
    } else if (mode === 'api') {
        showMessage('Please select a value from the API analysis results first.', 'error');
    } else {
        configDiv.querySelectorAll('.mode-btn').forEach(btn => btn.classList.remove('active'));
        event.target.classList.add('active');
    }

    updateLivePreview(event);
}

function handleScrollingToggle(event) {
    const { index } = event.target.dataset;
    const isChecked = event.target.checked;
    const yearConfig = document.getElementById(`config_${index}_year`);
    const timeConfig = document.getElementById(`config_${index}_time`);

    if (isChecked) {
        yearConfig.querySelector('label').textContent = 'SCROLLING TEXT (8+)';
        timeConfig.style.display = 'none';
    } else {
        yearConfig.querySelector('label').textContent = 'YEAR (4)';
        timeConfig.style.display = 'flex';
    }
    updateLivePreview(event);
}

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
        body: new URLSearchParams({ url, path: '' })
    })
    .then(res => res.json())
    .then(data => {
        if (data.success) {
            analyzedDataCache[index] = data.value;
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
    container.innerHTML = '<strong>Click the data point you want to use:</strong>';
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
                li.innerHTML = `<button class="wizard-clickable-item" data-index="${index}" data-path="${currentPath}"><span class="wizard-key">${currentPath}:</span> <span class="wizard-value">"${String(value)}"</span></button>`;
            }
            list.appendChild(li);
        }
        return list.childNodes;
    };

    buildList(jsonData);
    container.appendChild(list);
    container.querySelectorAll('.wizard-clickable-item').forEach(btn => btn.onclick = selectApiWizardValue);
}

function selectApiWizardValue(event) {
    const { index, path } = event.currentTarget.dataset;
    selectedPathInfo = { index: parseInt(index), path: path };

    const container = document.getElementById(`form_container_${index}`);
    container.classList.add('path-selected');

    const resultsContainer = document.getElementById(`wizard_results_${index}`);
    resultsContainer.innerHTML = `<p class="wizard-selection"><strong>Path Selected:</strong> \`${path}\`<br>Now click the glowing 'API' button for the segment you want to assign this to.</p>`;
}

function clearSelectedPath() {
    if (selectedPathInfo.index !== null) {
        const container = document.getElementById(`form_container_${selectedPathInfo.index}`);
        container.classList.remove('path-selected');
    }
    selectedPathInfo = { index: null, path: null };
}

function updateLivePreview(event) {
    const { index } = event.target.dataset;
    const preview = document.getElementById(`live_preview_${index}`);

    ['month', 'day', 'year', 'time'].forEach(segment => {
        const configDiv = document.getElementById(`config_${index}_${segment}`);
        const input = document.getElementById(`dp_input_${index}_${segment}`);
        const previewSegment = preview.querySelector(`.segment.${segment}`);
        const mode = configDiv.querySelector('.mode-btn.active').dataset.mode;

        let displayValue = input.value;
        if (segment === 'day') {
            const iconSelect = configDiv.querySelector('.icon-select');
            if (iconSelect && iconSelect.value) {
                displayValue = iconSelect.value;
            }
        }

        if (mode === 'api') {
            displayValue = `{${displayValue.replace('$.', '')}}`;
        }

        const maxLength = previewSegment.className.includes('year') || previewSegment.className.includes('time') ? 4 : (previewSegment.className.includes('day') ? 2 : 3);
        previewSegment.textContent = displayValue.substring(0, maxLength);
    });

    setSettingsChanged(true);
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
            formData.append(`dp_url_${i}`, document.getElementById(`dp_url_${i}`).value);
            formData.append(`dp_scroll_enabled_${i}`, document.getElementById(`dp_scroll_enabled_${i}`).checked);
            formData.append(`dp_pauseDuration_${i}`, document.getElementById(`dp_pauseDuration_${i}`).value);

            ['month', 'day', 'year', 'time'].forEach(segment => {
                const configDiv = document.getElementById(`config_${i}_${segment}`);
                const mode = configDiv.querySelector('.mode-btn.active').dataset.mode;
                const value = document.getElementById(`dp_input_${i}_${segment}`).value;
                formData.append(`dp_${i}_${segment}_mode`, mode);
                formData.append(`dp_${i}_${segment}_value`, value);

                if (segment === 'day') {
                    const icon = configDiv.querySelector('.icon-select').value;
                    formData.append(`dp_${i}_icon`, icon);
                }
            });
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