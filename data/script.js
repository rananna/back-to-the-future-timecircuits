let settingsChanged = false;
let timezoneOptions = [];
let isDataLinkLoaded = false;
let anyInputInvalid = false;

const apiTemplates = {
    nasdaq: { url: 'https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=NDAQ&apikey=YOUR_API_KEY', label: 'NASDAQ', jsonPath: 'Global Quote.05. price', icon: 'STOCK', format: '%L | %V', isLiveData: false, liveDataTag: '', scrollSpeed: 150 },
    sp500: { url: 'https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=SPY&apikey=YOUR_API_KEY', label: 'S&P500', jsonPath: 'Global Quote.05. price', icon: 'STOCK', format: '%L | %V', isLiveData: false, liveDataTag: '', scrollSpeed: 150 },
    tsx: { url: 'https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol=XIU.TRT&apikey=YOUR_API_KEY', label: 'TSX', jsonPath: 'Global Quote.05. price', icon: 'STOCK', format: '%L | %V', isLiveData: false, liveDataTag: '', scrollSpeed: 150 },
    usdcad: { url: 'https://www.alphavantage.co/query?function=CURRENCY_EXCHANGE_RATE&from_currency=USD&to_currency=CAD&apikey=YOUR_API_KEY', label: 'USDCAD', jsonPath: 'Realtime Currency Exchange Rate.5. Exchange Rate', icon: 'MONEY', format: '%L | %V', isLiveData: false, liveDataTag: '', scrollSpeed: 200 },
    crypto: { url: 'https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd', label: 'BTC', jsonPath: 'bitcoin.usd', icon: 'BTC', format: '%L | $%V', isLiveData: false, liveDataTag: '', scrollSpeed: 150 },
    windSpeed: { url: 'INTERNAL', label: 'WIND', jsonPath: 'speed', icon: 'WIND', format: '%L | %V MPH', isLiveData: true, liveDataTag: 'WIND_SPEED', scrollSpeed: 150 },
    feelsLike: { url: 'https://api.openweathermap.org/data/2.5/weather?lat=YOUR_LAT&lon=YOUR_LON&units=metric&appid=YOUR_API_KEY', label: 'FEELS', jsonPath: 'main.feels_like', icon: 'CLOUD', format: '%L | %V C', isLiveData: false, liveDataTag: '', scrollSpeed: 150 },
    humidity: { url: 'https://api.openweathermap.org/data/2.5/weather?lat=YOUR_LAT&lon=YOUR_LON&units=metric&appid=YOUR_API_KEY', label: 'HUMD', jsonPath: 'main.humidity', icon: 'RAIN', format: '%L | %V PCT', isLiveData: false, liveDataTag: '', scrollSpeed: 150 },
    uvIndex: { url: 'https://api.openweathermap.org/data/2.5/uvi?lat=YOUR_LAT&lon=YOUR_LON&appid=YOUR_API_KEY', label: 'UV', jsonPath: 'value', icon: 'SUN', format: '%L | %V', isLiveData: false, liveDataTag: '', scrollSpeed: 150 },
    aqi: { url: 'https://api.openweathermap.org/data/2.5/air_pollution?lat=YOUR_LAT&lon=YOUR_LON&appid=YOUR_API_KEY', label: 'AQI', jsonPath: 'list[0].main.aqi', icon: 'ALERT', format: '%L | %V', isLiveData: false, liveDataTag: '', scrollSpeed: 150 },
    youtube: { url: 'https://www.googleapis.com/youtube/v3/channels?part=statistics&id=YOUR_CHANNEL_ID&key=YOUR_API_KEY', label: 'SUBS', jsonPath: 'items[0].statistics.subscriberCount', icon: 'UP', format: '%L | %V', isLiveData: false, liveDataTag: '', scrollSpeed: 250 },
    space: { url: 'http://api.open-notify.org/astros.json', label: 'ASTRO', jsonPath: 'number', icon: 'WIFI', format: '%L | %V IN SPACE', isLiveData: false, liveDataTag: '', scrollSpeed: 150 },
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
    const promises = initialEndpoints.map(url => fetch(url).then(res => res.ok ? (url.endsWith('Theme') ? res.text() : res.json()) : Promise.reject(new Error(`Request to ${url} failed`))));

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

function applySettings(timecircuits, temporal, datalink) {
    if(timecircuits) {
        document.getElementById('destinationYear').value = timecircuits.destinationYear;
        document.getElementById('destinationTimezoneSelect').value = timecircuits.destinationTimezoneIndex;
        document.getElementById('lastTimeDepartedYear').textContent = timecircuits.lastTimeDepartedYear;
        document.getElementById('lastTimeDepartedMonth').textContent = timecircuits.lastTimeDepartedMonth;
        document.getElementById('lastTimeDepartedDay').textContent = timecircuits.lastTimeDepartedDay;
        document.getElementById('lastTimeDepartedHour').textContent = timecircuits.lastTimeDepartedHour;
        document.getElementById('lastTimeDepartedMinute').textContent = timecircuits.lastTimeDepartedMinute;
        document.getElementById('presentTimezoneSelect').value = timecircuits.presentTimezoneIndex;
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
    if (datalink.openWeatherMapApiKey) {
        document.getElementById('openWeatherMapApiKey').value = datalink.openWeatherMapApiKey;
    }
    if (datalink.alphaVantageApiKey) {
        document.getElementById('alphaVantageApiKey').value = datalink.alphaVantageApiKey;
    }
    if (datalink.youtubeApiKey) {
        document.getElementById('youtubeApiKey').value = datalink.youtubeApiKey;
    }
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
                document.getElementById(`dp_url_${i}`).value = point.url;
                document.getElementById(`dp_label_${i}`).value = point.label;
                document.getElementById(`dp_path_${i}`).value = point.jsonPath;
                document.getElementById(`dp_format_${i}`).value = point.format;
                document.getElementById(`dp_icon_${i}`).value = point.icon;
                const scrollSlider = document.getElementById(`dp_scrollSpeed_${i}`);
                scrollSlider.value = point.scrollSpeed;
                scrollSlider.dispatchEvent(new Event('input'));
                document.getElementById(`dp_isLiveData_${i}`).value = point.isLiveData;
                document.getElementById(`dp_liveDataTag_${i}`).value = point.liveDataTag;
                document.getElementById(`api_fields_${i}`).style.display = point.isLiveData ? 'none' : 'block';
            });
        }
    });
}

function attachEventListeners() {
    document.getElementById('saveSettingsBtn').onclick = saveSettings;
    document.querySelectorAll('.tab-link').forEach(btn => btn.onclick = (e) => {
        const tabName = e.target.getAttribute('data-tab');
        openTab(e, tabName);
        if (tabName === 'DataLink' && !isDataLinkLoaded) loadDataLinkSettings();
    });
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
    document.getElementById('apiTemplateSelector').onchange = (e) => {
        if (e.target.value) applyApiTemplate(e.target.value);
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
                block.innerHTML = `<h4>Data Point ${i + 1}</h4><input type="hidden" id="dp_isLiveData_${i}" value="false"><input type="hidden" id="dp_liveDataTag_${i}" value=""><div id="api_fields_${i}"><label for="dp_url_${i}">API URL:</label><input type="text" id="dp_url_${i}" placeholder="http://..."><label for="dp_path_${i}">JSON Path:</label><input type="text" id="dp_path_${i}" placeholder="data.value[0]"></div><div class="preset-date-inputs"><div style="width:100%"><label for="dp_label_${i}">Label (4 chars):</label><input type="text" id="dp_label_${i}" maxlength="4"></div><div style="width:100%"><label for="dp_icon_${i}">Icon:</label><input type="text" id="dp_icon_${i}" placeholder="e.g., SUN, BTC"></div></div><label for="dp_format_${i}">Format (%L, %V, |):</label><input type="text" id="dp_format_${i}" placeholder="%L | %V"><hr><label for="dp_scrollSpeed_${i}">Scroll Speed (ms/step): <span id="dp_scrollSpeed_val_${i}">150</span></label><input type="range" id="dp_scrollSpeed_${i}" min="50" max="500" step="10" value="150"><button class="test-api-btn" data-index="${i}">Test API</button>`;
                container.appendChild(block);
            }
            document.querySelectorAll('.test-api-btn').forEach(btn => btn.onclick = testApi);
        }
        resolve();
    });
}

function applyApiTemplate(templateName) {
    const template = apiTemplates[templateName];
    if (template && document.getElementById('dp_url_0')) {
        document.getElementById('dp_url_0').value = template.url;
        document.getElementById('dp_label_0').value = template.label;
        document.getElementById('dp_path_0').value = template.jsonPath;
        document.getElementById('dp_icon_0').value = template.icon;
        document.getElementById('dp_format_0').value = template.format;
        document.getElementById('dp_isLiveData_0').value = template.isLiveData;
        document.getElementById('dp_liveDataTag_0').value = template.liveDataTag;
        document.getElementById('api_fields_0').style.display = template.isLiveData ? 'none' : 'block';
        const scrollSlider = document.getElementById('dp_scrollSpeed_0');
        scrollSlider.value = template.scrollSpeed;
        scrollSlider.dispatchEvent(new Event('input'));
        showMessage(`Template "${templateName}" applied to Data Point 1.`, 'success');
        setSettingsChanged(true);
    }
}

function applySelectedPreset(event) {
    const select = event.target;
    const value = select.value;
    const selectedOption = select.options[select.selectedIndex];
    const isCustom = selectedOption.parentElement.label === 'Custom Time Jumps';

    document.getElementById('presetActions').classList.toggle('hidden', !isCustom);

    if (!value) return;
    const [year, month, day, hour, minute] = value.split('-');
    
    document.getElementById('destinationYear').value = year;
    document.getElementById('lastTimeDepartedYear').textContent = year;
    document.getElementById('lastTimeDepartedMonth').textContent = month;
    document.getElementById('lastTimeDepartedDay').textContent = day;
    document.getElementById('lastTimeDepartedHour').textContent = hour;
    document.getElementById('lastTimeDepartedMinute').textContent = minute;
    
    const is24h = document.getElementById('displayFormat24h').checked;
    let displayHour = parseInt(hour, 10);
    let ampm = '';
    if (!is24h) {
        ampm = displayHour >= 12 ? ' PM' : ' AM';
        if (displayHour > 12) displayHour -= 12;
        if (displayHour === 0) displayHour = 12;
    }
    const formattedDate = `${month.padStart(2, '0')}/${day.padStart(2, '0')}/${year}`;
    const formattedTime = `${String(displayHour).padStart(2, '0')}:${minute.padStart(2, '0')}${ampm}`;
    document.getElementById('lastTimeDepartedDisplay').textContent = `${formattedDate} ${formattedTime}`;
    
    if (isCustom) {
        document.getElementById('presetName').value = selectedOption.textContent;
        document.getElementById('presetDate').value = `${year}-${month.padStart(2,'0')}-${day.padStart(2,'0')}`;
        document.getElementById('presetTime').value = `${hour.padStart(2,'0')}:${minute.padStart(2,'0')}`;
    }

    showMessage(`Time Circuits set to: ${selectedOption.text}`, 'info');
    setSettingsChanged(true);
    updateHeaderClocks(new Date());
}

function saveSettings() {
    showLoading('saveSettingsBtn', true);
    const formData = new URLSearchParams();
    
    formData.append('openWeatherMapApiKey', document.getElementById('openWeatherMapApiKey').value);
    formData.append('alphaVantageApiKey', document.getElementById('alphaVantageApiKey').value);
    formData.append('youtubeApiKey', document.getElementById('youtubeApiKey').value);

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
            formData.append(`dp_format_${i}`, document.getElementById(`dp_format_${i}`).value);
            formData.append(`dp_icon_${i}`, document.getElementById(`dp_icon_${i}`).value);
            formData.append(`dp_scrollSpeed_${i}`, document.getElementById(`dp_scrollSpeed_${i}`).value);
            formData.append(`dp_isLiveData_${i}`, document.getElementById(`dp_isLiveData_${i}`).value);
            formData.append(`dp_liveDataTag_${i}`, document.getElementById(`dp_liveDataTag_${i}`).value);
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

function testApi(event) {
    const index = event.target.getAttribute('data-index');
    const url = document.getElementById(`dp_url_${index}`).value;
    const path = document.getElementById(`dp_path_${index}`).value;
    if (!url || !path) {
        showMessage('URL and JSON Path are required to test.', 'error');
        return;
    }
    showMessage('Testing API...', 'info');
    fetch('/api/testDataPoint', { method: 'POST', body: new URLSearchParams({ url, path }) })
        .then(res => res.json())
        .then(data => {
            if (data.success) {
                showMessage(`Success! Got value: ${data.value}`, 'success');
            } else {
                showMessage(`Error: ${data.error}`, 'error');
            }
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
    let startPercent, widthPercent;
    if (depTotalMins < arrTotalMins) {
        startPercent = (depTotalMins / 1440) * 100;
        widthPercent = ((arrTotalMins - depTotalMins) / 1440) * 100;
    } else {
        const toMidnight = 1440 - depTotalMins;
        const afterMidnight = arrTotalMins;
        startPercent = (depTotalMins / 1440) * 100;
        widthPercent = ((toMidnight + afterMidnight) / 1440) * 100;
    }
    bar.style.left = `${startPercent}%`;
    bar.style.width = `${widthPercent}%`;
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