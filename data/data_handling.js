// data_handling.js

// Global state variables for data management
let timezoneOptions = [];
let analyzedDataCache = {};
let apiExamples = {};
let dataPointStateCache = {};
let lastFocusedApiExample = {};
let dataPointStatus = {};
let ws; // WebSocket object

/**
 * Initializes the WebSocket connection to the server.
 */
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
        } else if (msg.action === 'stateUpdate') {
            const el = document.getElementById(msg.key);
            if (el) {
                if (el.type === 'checkbox') {
                    el.checked = msg.value;
                } else {
                    el.value = msg.value;
                }
                el.dispatchEvent(new Event('input'));
                el.dispatchEvent(new Event('change'));
            }
        } else if (msg.action === 'stockTestResult') {
            console.log("CLIENT_DEBUG: Received stockTestResult:", msg);
            const button = document.querySelector(`.fetch-stock-btn[data-index="${msg.rowIndex}"].analyzing`);
             if (button) {
                 button.disabled = false;
                 button.classList.remove('analyzing');
                 button.textContent = 'Fetch';
             }
            updateStockPreview(msg.status, msg.payload, msg.rowIndex);
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

/**
 * Checks if the server is ready to accept requests.
 */
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

/**
 * Loads the Data Link settings from the server.
 */
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

/**
 * Adds a new custom preset.
 */
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

/**
 * Updates an existing custom preset.
 */
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

/**
 * Deletes the selected custom preset.
 */
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

/**
 * Starts the API wizard to analyze a URL.
 */
function startApiWizard(event) {
    const index = event.target.getAttribute('data-index');
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
        data: { url: apiUrl, authKey: authKey, authValue: authValue }
    };
    ws.send(JSON.stringify(message));
}

/**
 * Saves all the settings to the server.
 */
function saveSettings() {
    showLoading('saveSettingsBtn', true);
    console.log("CLIENT_DEBUG: 'Engage Time Circuits' button clicked. Starting save process.");
    const settings = {};

    settings.destinationYear = parseInt(document.getElementById('destinationYear').value, 10);
    settings.destinationTimezoneIndex = parseInt(document.getElementById('destinationTimezoneSelect').value, 10);
    settings.presentTimezoneIndex = parseInt(document.getElementById('presentTimezoneSelect').value, 10);
    settings.lastTimeDepartedYear = parseInt(document.getElementById('lastTimeDepartedYear').textContent, 10);
    settings.lastTimeDepartedMonth = parseInt(document.getElementById('lastTimeDepartedMonth').textContent, 10);
    settings.lastTimeDepartedDay = parseInt(document.getElementById('lastTimeDepartedDay').textContent, 10);
    settings.lastTimeDepartedHour = parseInt(document.getElementById('lastTimeDepartedHour').textContent, 10);
    settings.lastTimeDepartedMinute = parseInt(document.getElementById('lastTimeDepartedMinute').textContent, 10);
    const [depHour, depMin] = document.getElementById('departureTime').value.split(':');
    settings.departureHour = parseInt(depHour, 10);
    settings.departureMinute = parseInt(depMin, 10);
    const [arrHour, arrMin] = document.getElementById('arrivalTime').value.split(':');
    settings.arrivalHour = parseInt(arrHour, 10);
    settings.arrivalMinute = parseInt(arrMin, 10);
    settings.brightness = parseInt(document.getElementById('brightness').value, 10);
    settings.notificationVolume = parseInt(document.getElementById('notificationVolume').value, 10);
    settings.timeTravelAnimationDuration = parseInt(document.getElementById('timeTravelAnimationDuration').value, 10);
    settings.timeTravelAnimationInterval = parseInt(document.getElementById('timeTravelAnimationInterval').value, 10);
    settings.animationStyle = parseInt(document.getElementById('animationStyleSelect').value, 10);
    settings.glitchEffectFrequency = parseInt(document.getElementById('glitchEffectFrequency').value, 10);
    settings.malfunctionFrequency = parseInt(document.getElementById('malfunctionFrequency').value, 10);
    settings.presetCycleInterval = parseInt(document.getElementById('presetCycleInterval').value, 10);
    settings.timeTravelSoundToggle = document.getElementById('timeTravelSoundToggle').checked;
    settings.timeTravelVolumeFade = document.getElementById('timeTravelVolumeFade').checked;
    settings.displayFormat24h = document.getElementById('displayFormat24h').checked;
    settings.dataLinkEnabled = document.getElementById('dataLinkEnabled').checked;
    settings.dataLinkRefreshInterval = parseInt(document.getElementById('dataLinkRefreshInterval').value, 10);
    settings.mqttBroker = document.getElementById('mqttBroker').value;
    settings.mqttPort = parseInt(document.getElementById('mqttPort').value, 10);
    settings.mqttUser = document.getElementById('mqttUser').value;
    settings.mqttPassword = document.getElementById('mqttPassword').value;
    settings.weatherModeEnabled = document.getElementById('weatherModeEnabled').checked;
    settings.cityName = document.getElementById('cityName').value;
    settings.useMetricUnits = document.getElementById('useMetricUnits').checked;
    settings.stockTickerModeEnabled = document.getElementById('stockTickerModeEnabled').checked;
    settings.alphaVantageApiKey = document.getElementById('alphaVantageApiKey').value;

    if (settings.stockTickerModeEnabled && !settings.alphaVantageApiKey) {
        showMessage('Alpha Vantage API Key is required for Stock Ticker Mode.', 'error');
        showLoading('saveSettingsBtn', false);
        return;
    }

    settings.stockRow1_symbol = document.getElementById('stockRow1_symbol').value;
    settings.stockRow2_symbol = document.getElementById('stockRow2_symbol').value;
    settings.stockRow3_symbol = document.getElementById('stockRow3_symbol').value;

    const numDataPoints = parseInt(document.getElementById('numDataPoints').value, 10);
    settings.numDataPoints = numDataPoints;
    settings.dataPoints = [];
    for (let i = 0; i < numDataPoints; i++) {
        const point = {};
        const sourceValue = document.getElementById(`dp_dataSourceType_${i}`).value;
        if (sourceValue === 'mqtt') {
            point.dataSourceType = 1;
        } else if (sourceValue === 'ha') {
            point.dataSourceType = 2;
        } else {
            point.dataSourceType = 0; // api
        }
        point.displayMode = parseInt(document.getElementById(`dp_displayMode_${i}`).value, 10);
        point.url = document.getElementById(`dp_url_${i}`).value;
        point.monthPath = document.getElementById(`dp_monthPath_${i}`).value;
        point.dayPath = document.getElementById(`dp_dayPath_${i}`).value;
        point.yearPath = document.getElementById(`dp_yearPath_${i}`).value;
        point.timePath = document.getElementById(`dp_timePath_${i}`).value;
        point.prefix = document.getElementById(`dp_prefix_${i}`).value;
        point.suffix = document.getElementById(`dp_suffix_${i}`).value;
        point.icon = document.getElementById(`dp_icon_${i}`).value;
        point.scrollSpeed = parseInt(document.getElementById(`dp_scrollSpeed_${i}`).value, 10);
        point.mqttTopic = document.getElementById(`dp_mqttTopic_${i}`).value;
        point.yearPrefix = document.getElementById(`dp_yearPrefix_${i}`).value;
        point.yearSuffix = document.getElementById(`dp_yearSuffix_${i}`).value;
        point.scrollingText = document.getElementById(`dp_scrollingText_${i}`).value;
        point.authHeaderKey = document.getElementById(`dp_authHeaderKey_${i}`).value;
        point.authHeaderValue = document.getElementById(`dp_authHeaderValue_${i}`).value;
        point.apiExampleKey = document.getElementById(`api_example_${i}`).value;
        settings.dataPoints.push(point);
    }

    fetch('/api/saveSettings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
    })
    .then(res => {
        if (!res.ok) {
            console.error(`CLIENT_DEBUG: /api/saveSettings call failed with status: ${res.status}`);
            throw new Error(`Save failed with status: ${res.status}`);
        }
        return res.text();
    })
    .then(text => {
        console.log(`CLIENT_DEBUG: /api/saveSettings successful. Response: ${text}`);
        showMessage(text, 'success');
        setSettingsChanged(false);
        console.log("CLIENT_DEBUG: Triggering /api/triggerAnimation...");
        return fetch('/api/triggerAnimation', { method: 'POST' });
    })
    .then(res => {
        if (!res.ok) {
            console.error(`CLIENT_DEBUG: /api/triggerAnimation call failed with status: ${res.status}`);
            throw new Error('Failed to trigger animation');
        }
        console.log("CLIENT_DEBUG: /api/triggerAnimation successful.");
        const duration = settings.timeTravelAnimationDuration;
        document.body.classList.add('time-travel-active');
        setTimeout(() => document.body.classList.remove('time-travel-active'), duration);
    })
    .catch(err => {
        console.error("CLIENT_DEBUG: An error occurred during the save/animation process.", err);
        showMessage(`Error: ${err.message}`, 'error')
    })
    .finally(() => {
        console.log("CLIENT_DEBUG: Save process finished.");
        showLoading('saveSettingsBtn', false)
    });
}

/**
 * Fetches the current time from the server.
 */
function fetchTime() {
    fetch('/api/time').then(res => res.json()).then(data => {
        document.getElementById('timeSyncStatus').textContent = data.timeSynchronized ? 'Yes' : 'No';
        if (data.unixTime) updateHeaderClocks(new Date(data.unixTime * 1000));
    });
}

/**
 * Triggers a refresh of the weather data.
 */
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
            setTimeout(fetchWeatherData, 3000);
        } else {
            throw new Error('Failed to trigger refresh.');
        }
    })
    .catch(err => {
        showMessage(`Error: ${err.message}`, 'error');
        preview.textContent = 'Error';
    });
}

/**
 * Gets a 2-character icon for a given weather code.
 */
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

/**
 * Fetches the current weather data from the server.
 */
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
        .then(res => {
            if (res.ok) {
                return res.json();
            }
            return Promise.reject('Weather data not ready');
        })
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

            const city = document.getElementById('cityName').value;
            preview.textContent = `Live data for ${city}: ${data.temperature.toFixed(1)}${tempUnit}`;

            const hourlyContainer = document.getElementById('hourlyForecastContainer');
            hourlyContainer.innerHTML = '';
            const now = new Date();
            let currentHour = now.getHours();

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
        .finally(() => {
            loadingSpinner.style.display = 'none';
        });
}

/**
 * Gets a value from a JSON object using a dot-notation path.
 */
function getValueFromPath(obj, path) {
    if (!path || !obj) return null;
    try {
        const value = path.split(/[.\[\]]+/).filter(Boolean).reduce((o, k) => (o || {})[k], obj);
        return value !== undefined ? value : null;
    } catch (e) {
        return null;
    }
}

/**
 * Gets the processed URL for a data point, replacing "YOUR_API_KEY" if necessary.
 */
function getProcessedUrl(index) {
    let apiUrl = document.getElementById(`dp_url_${index}`).value;
    const authValue = document.getElementById(`dp_authHeaderValue_${index}`).value;
    if (apiUrl.includes('YOUR_API_KEY') && authValue) {
        return apiUrl.replace('YOUR_API_KEY', authValue);
    }
    return apiUrl;
}

/**
 * Tests a single data point.
 */
function testDataPoint(event) {
    const index = event.target.dataset.index;
    const button = event.target;
    const dataSource = document.getElementById(`dp_dataSourceType_${index}`).value;
    if (dataSource !== 'api') {
        showMessage('Test is only available for API data points.', 'error');
        return;
    }
    const apiUrl = getProcessedUrl(index);
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
        data: { url: apiUrl, authKey: authKey, authValue: authValue }
    };
    ws.send(JSON.stringify(message));
}

/**
 * Fetches a stock quote.
 */
function fetchStockQuote(event) {
    console.log("CLIENT_DEBUG: fetchStockQuote called.");
    const index = event.target.dataset.index;
    const symbol = document.getElementById(`stockRow${parseInt(index) + 1}_symbol`).value;
    const apiKey = document.getElementById('alphaVantageApiKey').value;
    const button = event.target;

    console.log(`CLIENT_DEBUG: Stock Fetch Request - Index: ${index}, Symbol: ${symbol}, API Key Present: ${!!apiKey}`);

    if (!symbol) {
        showMessage('Please enter a stock symbol.', 'error');
        return;
    }
    if (!apiKey) {
        showMessage('Please enter your Alpha Vantage API key.', 'error');
        return;
    }
     if (!ws || ws.readyState !== WebSocket.OPEN) {
        showMessage('Data Link channel is not open. Please wait.', 'error');
        console.error("CLIENT_DEBUG: WebSocket is not open. State: " + (ws ? ws.readyState : "undefined"));
        return;
    }

    button.disabled = true;
    button.classList.add('analyzing');
    button.innerHTML = '<span class="loading-spinner"></span>';

    const message = {
        action: "testStock",
        data: { symbol: symbol, apiKey: apiKey, rowIndex: index }
    };
    console.log("CLIENT_DEBUG: Sending testStock message:", message);
    ws.send(JSON.stringify(message));
}

/**
 * Updates the stock preview UI.
 */
function updateStockPreview(status, payload, rowIndex) {
    console.log(`CLIENT_DEBUG: updateStockPreview - Status: ${status}, RowIndex: ${rowIndex}`, payload);
    const previewContainer = document.getElementById(`stock_preview_${rowIndex}`);
    const priceEl = previewContainer.querySelector('.stock-price');
    const changeEl = previewContainer.querySelector('.stock-change');
    const quote = Array.isArray(payload) ? payload[0] : payload;

    if (status === 'success' && quote && typeof quote === 'object' && !quote["Error Message"]) {
        const price = parseFloat(quote.price).toFixed(2);
        priceEl.textContent = `$${price}`;
        if (quote.changesPercentage !== undefined) {
            const change = parseFloat(quote.changesPercentage).toFixed(2);
            changeEl.textContent = `${change}%`;
            changeEl.classList.remove('positive', 'negative');
            if (change > 0) {
                changeEl.classList.add('positive');
            } else if (change < 0) {
                changeEl.classList.add('negative');
            }
        } else {
            changeEl.textContent = '--';
            changeEl.classList.remove('positive', 'negative');
        }
        console.log(`CLIENT_DEBUG: Stock UI updated for row ${rowIndex} - Price: ${price}`);
    } else {
        priceEl.textContent = 'Error';
        changeEl.textContent = '--';
        changeEl.classList.remove('positive', 'negative');
        let errorMsg = 'Failed to fetch stock data.';
        if (typeof payload === 'string') {
            errorMsg = payload;
        } else if (payload && payload['Error Message']) {
            errorMsg = payload['Error Message'];
        } else if (payload && payload['Note']) {
            errorMsg = payload['Note'];
        }
        showMessage(errorMsg, 'error');
        console.error(`CLIENT_DEBUG: Stock fetch error for row ${rowIndex}:`, errorMsg);
    }
}