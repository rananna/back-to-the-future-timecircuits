// Global state variables for the entire application
let settingsChanged = false; // Tracks if any settings have been changed by the user
let timezoneOptions = []; // Stores the available timezone options fetched from the server
let isDataLinkLoaded = false; // Flag to check if the Data Link settings have been loaded
let anyInputInvalid = false; // Flag to track if there are any invalid inputs in the forms
let analyzedDataCache = {}; // Caches the JSON data analyzed from API responses
let apiExamples = {}; // Stores the API example templates fetched from the server
let dataPointStateCache = {}; // Caches the state of individual data points, like modified URLs
let lastFocusedApiExample = {}; // Tracks the last focused API example to manage URL modifications
let activeWizardTarget = null; // The currently active target for the API wizard mapping
let dataPointStatus = {}; // Stores the success/error status of each data point
let ws; // The WebSocket object for real-time communication
let weatherInterval; // The interval ID for fetching weather data periodically
let isLoading = true; // Flag to indicate if the initial data is still loading

/**
 * Initializes the WebSocket connection to the server.
 */
function initWebSocket() {
    // Create a new WebSocket connection to the server's /ws endpoint
    ws = new WebSocket('ws://' + window.location.host + '/ws');

    /**
     * Handles the successful opening of the WebSocket connection.
     */
    ws.onopen = function() {
        console.log("CLIENT_DEBUG: WebSocket connection established.");
        showMessage('Data Link channel open', 'success', 2000);
    };

    /**
     * Handles incoming messages from the WebSocket server.
     * @param {MessageEvent} event The message event from the server.
     */
    ws.onmessage = function(event) {
        console.log("CLIENT_DEBUG: WebSocket message received:", event.data);
        const msg = JSON.parse(event.data);

        // If the message is an API result, handle it
        if (msg.action === 'apiResult') {
            // Find the button that triggered the API analysis
            const button = document.querySelector('.analyze-api-btn.analyzing, .dp-test-btn.analyzing');
            if (button) {
                 // Re-enable the button and remove the analyzing state
                 button.disabled = false;
                 button.classList.remove('analyzing');
                 button.textContent = button.classList.contains('dp-test-btn') ? 'Test' : 'Analyze API';
                 const index = button.dataset.index;
                 // Update the status indicator for the data point
                 updateDataPointStatus(index, msg.status === 'success');

                 // If the API call was successful
                 if (msg.status === 'success') {
                    // Cache the response payload
                    analyzedDataCache[index] = msg.payload;
                    if (button.classList.contains('analyze-api-btn')) {
                        // Display the API wizard results
                        const resultsContainer = document.getElementById(`wizard_results_${index}`);
                        displayApiWizardResults(index, msg.payload);
                    } else {
                        // Show a success message for the test
                        showMessage(`Data Point ${parseInt(index) + 1} test successful.`, 'success');
                    }
                    // Update the marquee preview with the new data
                    updateMarqueePreview(index);
                 } else {
                    // If there was an error, show an error message
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
                // Trigger an event to update any associated UI elements (like value spans)
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
        } else if (msg.action === 'uploadProgress') {
            const progressBar = document.getElementById(`${msg.type}-progress-bar`);
            const statusMessage = document.getElementById(`${msg.type}-status-message`);
            if (progressBar) {
                progressBar.style.width = `${msg.progress}%`;
            }
            if (statusMessage) {
                statusMessage.textContent = msg.message || `${msg.progress}%`;
            }
        } else if (msg.action === 'uploadError') {
            const statusMessage = document.getElementById(`${msg.type}-status-message`);
            if (statusMessage) {
                statusMessage.textContent = `Error: ${msg.message}`;
            }
            showMessage(`Upload failed: ${msg.message}`, 'error');
        }
    };

    /**
     * Handles the closing of the WebSocket connection.
     */
    ws.onclose = function() {
        console.log("CLIENT_DEBUG: WebSocket connection closed. Attempting to reconnect...");
        showMessage('Data Link channel closed. Retrying...', 'error', 3000);
        // Attempt to reconnect after 3 seconds
        setTimeout(initWebSocket, 3000);
    };

    /**
     * Handles any errors that occur with the WebSocket connection.
     * @param {Event} err The error event.
     */
    ws.onerror = function(err) {
        console.error('CLIENT_DEBUG: WebSocket error:', err);
    };
}

/**
 * Checks if the server is ready to accept requests.
 * @param {number} retries The number of times to retry checking.
 * @param {number} delay The delay between retries in milliseconds.
 * @returns {Promise<boolean>} A promise that resolves to true if the server is ready, false otherwise.
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
    // If the settings are already loaded, do nothing
    if (isDataLinkLoaded) return;
    showMessage('Loading Data Link settings...', 'info');
    // Fetch the settings from the server
    fetch('/api/settings/datalink').then(res => res.ok ? res.json() : Promise.reject('Failed to load'))
        .then(datalink => {
            // Apply the loaded settings to the UI
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
    // Get the preset details from the form
    const name = document.getElementById('presetName').value;
    const date = document.getElementById('presetDate').value;
    const time = document.getElementById('presetTime').value;
    // Validate the inputs
    if (!name || !date || !time) {
        showMessage('Preset name, date, and time are required.', 'error');
        return;
    }
    // Format the preset value
    const [year, month, day] = date.split('-');
    const [hour, minute] = time.split(':');
    const value = `${year}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}-${String(hour).padStart(2, '0')}-${String(minute).padStart(2, '0')}`;

    // Send the new preset to the server
    fetch('/api/addPreset', { method: 'POST', body: new URLSearchParams({ name, value }) })
        .then(res => {
            if (!res.ok) {
                throw new Error('Failed to save preset.');
            }
            return res.text();
        })
        .then(text => {
            showMessage(text, 'success');

            // Add the new preset to the dropdown list
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

            // Reset the preset form
            resetPresetForm();
        })
        .catch(err => showMessage(`Error: ${err.message}`, 'error'));
}

/**
 * Updates an existing custom preset.
 */
function updatePreset() {
    // Get the original preset name
    const originalName = document.getElementById('presetDateSelect').options[document.getElementById('presetDateSelect').selectedIndex].text;
    // Get the new preset details from the form
    const newName = document.getElementById('presetName').value;
    const date = document.getElementById('presetDate').value;
    const time = document.getElementById('presetTime').value;

    // Validate the inputs
    if (!newName || !date || !time) {
        showMessage('Preset name, date, and time are required.', 'error');
        return;
    }
    // Format the preset value
    const [year, month, day] = date.split('-');
    const [hour, minute] = time.split(':');
    const value = `${year}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}-${String(hour).padStart(2, '0')}-${String(minute).padStart(2, '0')}`;

    // Send the updated preset to the server
    fetch('/api/updatePreset', { method: 'POST', body: new URLSearchParams({ name: originalName, newName: newName, value: value }) })
        .then(res => res.text()).then(text => {
            showMessage(text, 'success');
            // Refresh the presets list and reset the form
            fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
            resetPresetForm();
        });
}

/**
 * Deletes the selected custom preset.
 */
function deletePreset() {
    // Get the name of the preset to delete
    const name = document.getElementById('presetDateSelect').options[document.getElementById('presetDateSelect').selectedIndex].text;
    // Confirm the deletion with the user
    if (confirm(`Are you sure you want to delete the preset "${name}"?`)) {
        // Send the delete request to the server
        fetch('/api/deletePreset', { method: 'POST', body: new URLSearchParams({ name }) })
            .then(res => res.text()).then(text => {
                showMessage(text, 'success');
                // Refresh the presets list and reset the form
                fetch('/api/getPresets').then(res => res.json()).then(populatePresetsSelect);
                resetPresetForm();
            });
    }
}

/**
 * Starts the API wizard to analyze a URL.
 * @param {Event} event The click event from the "Analyze API" button.
 */
function startApiWizard(event) {
    // Get the index of the data point
    const index = event.target.getAttribute('data-index');
    // Get the processed URL for the data point
    const apiUrl = getProcessedUrl(index);
    // Get the authentication headers
    const authKey = document.getElementById(`dp_authHeaderKey_${index}`).value;
    const authValue = document.getElementById(`dp_authHeaderValue_${index}`).value;
    const button = event.target;

    console.log(`CLIENT_DEBUG: Starting API Wizard for index ${index}. URL: ${apiUrl}`);

    // Validate the URL
    if (!apiUrl) {
        showMessage('Please enter an API URL first.', 'error');
        return;
    }

    // Check if the WebSocket is open
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        showMessage('Data Link channel is not open. Please wait.', 'error');
        return;
    }

    // Show a loading indicator
    const resultsContainer = document.getElementById(`wizard_results_${index}`);
    resultsContainer.innerHTML = '<span class="loading-spinner"></span> Analyzing...';
    button.disabled = true;
    button.classList.add('analyzing');

    // Create the message to send to the server
    const message = {
        action: "testApi",
        data: {
            url: apiUrl,
            authKey: authKey,
            authValue: authValue
        }
    };

    // Send the message via WebSocket
    ws.send(JSON.stringify(message));
}

/**
 * Saves all the settings to the server.
 */
function saveSettings() {
    // Helper functions to safely get values from DOM elements.
    const getEl = (id) => document.getElementById(id);
    const getValue = (id, def = '') => { const el = getEl(id); return el ? el.value : def; };
    const getIntValue = (id, def = 0) => parseInt(getValue(id, String(def)), 10);
    const getChecked = (id, def = false) => { const el = getEl(id); return el ? el.checked : def; };
    const getText = (id, def = '0') => { const el = getEl(id); return el ? el.textContent : def; };
    const getIntFromText = (id, def = 0) => parseInt(getText(id, String(def)), 10);

    // --- START: Input Validation ---
    const validateNumericInput = (id, label, isInteger = true, min = -Infinity, max = Infinity) => {
        const input = getEl(id);
        if (!input) return isInteger ? 0 : 0.0; // Return a default if element doesn't exist
        const value = input.value;
        const numValue = isInteger ? parseInt(value, 10) : parseFloat(value);
        
        if (value.trim() !== '' && (isNaN(numValue) || numValue < min || numValue > max)) {
            showMessage(`${label} must be a valid number between ${min} and ${max}.`, 'error');
            input.classList.add('invalid-input');
            input.focus();
            return null; // Indicates validation failure
        }
        input.classList.remove('invalid-input');
        return isNaN(numValue) ? (isInteger ? 0 : 0.0) : numValue;
    };

    const settings = {};
    settings.destinationYear = validateNumericInput('destinationYear', 'Destination Year', true, 1000, 9999);
    if (settings.destinationYear === null) return;
    
    settings.mqttPort = validateNumericInput('mqttPort', 'MQTT Port', true, 1, 65535);
    if (settings.mqttPort === null) return;

    document.querySelectorAll('.invalid-input').forEach(el => el.classList.remove('invalid-input'));
    // --- END: Input Validation ---

    showLoading('saveSettingsBtn', true);
    console.log("CLIENT_DEBUG: 'Engage Time Circuits' button clicked. Starting save process.");
    
    // Time Circuits & Temporal Settings
    settings.destinationTimezoneIndex = getIntValue('destinationTimezoneSelect');
    settings.presentTimezoneIndex = getIntValue('presentTimezoneSelect');
    settings.lastTimeDepartedYear = getIntFromText('lastTimeDepartedYear');
    settings.lastTimeDepartedMonth = getIntFromText('lastTimeDepartedMonth');
    settings.lastTimeDepartedDay = getIntFromText('lastTimeDepartedDay');
    settings.lastTimeDepartedHour = getIntFromText('lastTimeDepartedHour');
    settings.lastTimeDepartedMinute = getIntFromText('lastTimeDepartedMinute');

    const [depHour, depMin] = getValue('departureTime', '00:00').split(':');
    settings.departureHour = parseInt(depHour, 10);
    settings.departureMinute = parseInt(depMin, 10);

    const [arrHour, arrMin] = getValue('arrivalTime', '00:00').split(':');
    settings.arrivalHour = parseInt(arrHour, 10);
    settings.arrivalMinute = parseInt(arrMin, 10);

    settings.brightness = getIntValue('brightness', 5);
    settings.notificationVolume = getIntValue('notificationVolume', 15);
    settings.timeTravelAnimationDuration = getIntValue('timeTravelAnimationDuration', 4000);
    settings.timeTravelAnimationInterval = getIntValue('timeTravelAnimationInterval', 15);
    settings.animationStyle = getIntValue('animationStyleSelect', 0);
    settings.glitchEffectFrequency = getIntValue('glitchEffectFrequency', 0);
    settings.malfunctionFrequency = getIntValue('malfunctionFrequency', 0);
    settings.presetCycleInterval = getIntValue('presetCycleInterval', 10);

    settings.timeTravelSoundToggle = getChecked('timeTravelSoundToggle');
    settings.displayFormat24h = getChecked('displayFormat24h');

    // Data Link, Weather & Stock Ticker Settings
    settings.dataLinkEnabled = getChecked('dataLinkEnabled');
    settings.dataLinkRefreshInterval = getIntValue('dataLinkRefreshInterval', 10);
    settings.mqttBroker = getValue('mqttBroker');
    settings.mqttUser = getValue('mqttUser');
    settings.mqttPassword = getValue('mqttPassword');

    settings.weatherModeEnabled = getChecked('weatherModeEnabled');
    settings.cityName = getValue('cityName');
    settings.useMetricUnits = getChecked('useMetricUnits');
    
    settings.stockTickerModeEnabled = getChecked('stockTickerModeEnabled');
    settings.alphaVantageApiKey = getValue('alphaVantageApiKey');

    if (settings.stockTickerModeEnabled && !settings.alphaVantageApiKey) {
        showMessage('FMP API Key is required for Stock Ticker Mode.', 'error');
        showLoading('saveSettingsBtn', false);
        return;
    }

    settings.stockRow1_symbol = getValue('stockRow1_symbol');
    settings.stockRow2_symbol = getValue('stockRow2_symbol');
    settings.stockRow3_symbol = getValue('stockRow3_symbol');

    if (settings.dataLinkEnabled) {
        const numDataPoints = getIntValue('numDataPoints', 0);
        settings.numDataPoints = numDataPoints;
        settings.dataPoints = [];
        for (let i = 0; i < numDataPoints; i++) {
            const point = {};
            const sourceValue = getValue(`dp_dataSourceType_${i}`, 'api');
            point.dataSourceType = sourceValue === 'mqtt' ? 1 : (sourceValue === 'ha' ? 2 : 0);
            point.displayMode = getIntValue(`dp_displayMode_${i}`, 0);
            point.url = getValue(`dp_url_${i}`);
            point.monthPath = getValue(`dp_monthPath_${i}`);
            point.dayPath = getValue(`dp_dayPath_${i}`);
            point.yearPath = getValue(`dp_yearPath_${i}`);
            point.timePath = getValue(`dp_timePath_${i}`);
            point.prefix = getValue(`dp_prefix_${i}`);
            point.suffix = getValue(`dp_suffix_${i}`);
            point.icon = getValue(`dp_icon_${i}`);
            point.scrollSpeed = getIntValue(`dp_scrollSpeed_${i}`, 150);
            point.mqttTopic = getValue(`dp_mqttTopic_${i}`);
            point.yearPrefix = getValue(`dp_yearPrefix_${i}`);
            point.yearSuffix = getValue(`dp_yearSuffix_${i}`);
            point.scrollingText = getValue(`dp_scrollingText_${i}`);
            point.authHeaderKey = getValue(`dp_authHeaderKey_${i}`);
            point.authHeaderValue = getValue(`dp_authHeaderValue_${i}`);
            point.apiExampleKey = getValue(`api_example_${i}`);
            settings.dataPoints.push(point);
        }
    } else {
        settings.numDataPoints = 0;
        settings.dataPoints = [];
    }

    fetch('/api/saveSettings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(settings)
    })
    .then(res => {
        if (!res.ok) throw new Error(`Save failed with status: ${res.status}`);
        return res.text();
    })
    .then(text => {
        showMessage(text, 'success');
        setSettingsChanged(false);
        // The animation is now triggered server-side, so we just do a quick UI flash.
        document.body.classList.add('time-travel-active');
        setTimeout(() => document.body.classList.remove('time-travel-active'), 2000);
    })
    .catch(err => {
        console.error("CLIENT_DEBUG: An error occurred during the save/animation process.", err);
        showMessage(`Error: ${err.message}`, 'error');
    })
    .finally(() => {
        showLoading('saveSettingsBtn', false);
    });
}

/**
 * Fetches the current time from the server.
 */
function fetchTime() {
    fetch('/api/time').then(res => res.json()).then(data => {
        // Update the time sync status and header clocks
        document.getElementById('timeSyncStatus').textContent = data.timeSynchronized ? 'Yes' : 'No';
        if (data.unixTime) updateHeaderClocks(new Date(data.unixTime * 1000));
    });
}

/**
 * Triggers a refresh of the weather data.
 */
function refreshWeatherData() {
    // Show a fetching indicator
    const preview = document.getElementById('weatherPreview');
    preview.textContent = 'Fetching...';

    // Get the city name from the input
    const city = document.getElementById('cityName').value;
    if (!city) {
        showMessage('Please enter a city name.', 'error');
        preview.textContent = 'Enter a city';
        return;
    }

    // Send the refresh request to the server
    fetch('/api/weather/refresh', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ cityName: city })
    })
    .then(res => {
        if (res.ok) {
            showMessage('Weather refresh triggered. Please wait a moment.', 'info');
            // Fetch the new weather data after a delay
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
 * @param {number} code The weather code from the API.
 * @returns {string} The 2-character weather icon.
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
    // If weather mode is not enabled, hide the weather display
    if (!document.getElementById('weatherModeEnabled').checked) {
        document.getElementById('weatherDisplay').style.display = 'none';
        return;
    }

    // Show a loading spinner
    const weatherDisplay = document.getElementById('weatherDisplay');
    const loadingSpinner = weatherDisplay.querySelector('.loading-spinner-container');
    const preview = document.getElementById('weatherPreview');
    loadingSpinner.style.display = 'block';

    // Fetch the weather data
    fetch('/api/weather')
        .then(res => {
            if (res.ok) {
                return res.json();
            }
            return Promise.reject('Weather data not ready');
        })
        .then(data => {
            // Get the units based on the user's preference
            const isMetric = document.getElementById('useMetricUnits').checked;
            const tempUnit = isMetric ? '°C' : '°F';
            const speedUnit = isMetric ? ' km/h' : ' mph';

            // Update the weather display with the fetched data
            weatherDisplay.style.display = 'grid';
            document.getElementById('weatherTemp').textContent = `${data.temperature.toFixed(1)}${tempUnit}`;
            document.getElementById('weatherFeelsLike').textContent = `${data.apparentTemperature.toFixed(1)}${tempUnit}`;
            document.getElementById('weatherHumidity').textContent = `${data.humidity}%`;
            document.getElementById('weatherWind').textContent = `${data.windSpeed.toFixed(1)}${speedUnit}`;
            document.getElementById('weatherHighLow').textContent = `${data.dailyHigh.toFixed(0)}° / ${data.dailyLow.toFixed(0)}°`;

            const city = document.getElementById('cityName').value;
            preview.textContent = `Live data for ${city}: ${data.temperature.toFixed(1)}${tempUnit}`;

            // Build the hourly forecast display
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
            // If there's an error, show '--' in the weather display
            weatherDisplay.style.display = 'grid';
            ['weatherTemp', 'weatherFeelsLike', 'weatherHumidity', 'weatherWind', 'weatherHighLow'].forEach(id => {
                document.getElementById(id).textContent = '--';
            });
            document.getElementById('hourlyForecastContainer').innerHTML = ''; // Clear hourly on error
            preview.textContent = 'Data not available. Check city name.';
        })
        .finally(() => {
            // Hide the loading spinner
            loadingSpinner.style.display = 'none';
        });
}

/**
 * Gets a value from a JSON object using a dot-notation path.
 * @param {object} obj The JSON object.
 * @param {string} path The path to the value (e.g., "data.items[0].name").
 * @returns {*} The value at the specified path, or null if not found.
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
 * @param {number} index The index of the data point.
 * @returns {string} The processed URL.
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
 * @param {Event} event The click event from the "Test" button.
 */
function testDataPoint(event) {
    // Get the index of the data point
    const index = event.target.dataset.index;
    const button = event.target;

    // Only test API data points
    const dataSource = document.getElementById(`dp_dataSourceType_${index}`).value;
    if (dataSource !== 'api') {
        showMessage('Test is only available for API data points.', 'error');
        return;
    }

    // Get the URL and auth headers
    const apiUrl = getProcessedUrl(index);
    const authKey = document.getElementById(`dp_authHeaderKey_${index}`).value;
    const authValue = document.getElementById(`dp_authHeaderValue_${index}`).value;

    // Validate the URL
    if (!apiUrl) {
        showMessage('Please enter an API URL first.', 'error');
        return;
    }

    // Check if the WebSocket is open
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        showMessage('Data Link channel is not open. Please wait.', 'error');
        return;
    }

    // Show a loading indicator on the button
    button.disabled = true;
    button.classList.add('analyzing');
    button.innerHTML = '<span class="loading-spinner"></span>';

    // Create the message to send to the server
    const message = {
        action: "testApi",
        data: {
            url: apiUrl,
            authKey: authKey,
            authValue: authValue
        }
    };
    // Send the message via WebSocket
    ws.send(JSON.stringify(message));
}

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
        showMessage('Please enter your FMP API key.', 'error');
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
        data: {
            symbol: symbol,
            apiKey: apiKey,
            rowIndex: index
        }
    };
    console.log("CLIENT_DEBUG: Sending testStock message:", message);
    ws.send(JSON.stringify(message));
}

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

/**
 * Handles the firmware file upload.
 * @param {Event} event The submit event from the firmware upload form.
 */
function handleFirmwareUpload(event) {
    event.preventDefault();
    const form = event.target;
    const fileInput = form.querySelector('input[type="file"]');
    const file = fileInput.files[0];
    if (!file) {
        showMessage('Please select a firmware file to upload.', 'error');
        return;
    }
    const formData = new FormData();
    formData.append(file.name, file);

    const progressBar = document.getElementById('firmware-progress-bar');
    const statusMessage = document.getElementById('firmware-status-message');
    progressBar.style.width = '0%';
    statusMessage.textContent = 'Uploading firmware...';

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/update', true);

    // --- START: MODIFICATION - Add Authentication Header ---
    xhr.setRequestHeader("X-Auth-Password", "1.21gigawatts");
    // --- END: MODIFICATION ---

    xhr.upload.onprogress = function(event) {
        if (event.lengthComputable) {
            const percentComplete = (event.loaded / event.total) * 100;
            progressBar.style.width = percentComplete + '%';
            statusMessage.textContent = `Uploading firmware... ${Math.round(percentComplete)}%`;
        }
    };

    xhr.onload = function() {
        if (xhr.status === 200) {
            statusMessage.textContent = 'Firmware update successful! Device is rebooting.';
            showMessage('Firmware update successful! Device is rebooting.', 'success', 10000);
        } else {
            statusMessage.textContent = `Firmware update failed. Status: ${xhr.status}`;
            showMessage(`Firmware update failed. Error: ${xhr.responseText}`, 'error');
        }
    };

    xhr.onerror = function() {
        statusMessage.textContent = 'Firmware update error.';
        showMessage('Firmware update error.', 'error');
    };

    xhr.send(formData);
}

/**
 * Handles the UI file upload.
 * @param {Event} event The submit event from the UI upload form.
 */
function handleUiUpload(event) {
    event.preventDefault();
    const form = event.target;
    const fileInput = form.querySelector('input[type="file"]');
    const files = fileInput.files;
    if (files.length === 0) {
        showMessage('Please select UI files to upload.', 'error');
        return;
    }

    const formData = new FormData();
    for (let i = 0; i < files.length; i++) {
        formData.append(files[i].name, files[i]);
    }
    
    const progressBar = document.getElementById('ui-progress-bar');
    const statusMessage = document.getElementById('ui-status-message');
    progressBar.style.width = '0%';
    statusMessage.textContent = 'Uploading UI files...';

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/upload-ui', true);

    xhr.upload.onprogress = function(event) {
        if (event.lengthComputable) {
            const percentComplete = (event.loaded / event.total) * 100;
            progressBar.style.width = percentComplete + '%';
            statusMessage.textContent = `Uploading UI files... ${Math.round(percentComplete)}%`;
        }
    };

    xhr.onload = function() {
        if (xhr.status === 200) {
            statusMessage.textContent = 'UI files uploaded successfully! Please refresh the page.';
            showMessage('UI files uploaded successfully! Please refresh the page.', 'success', 5000);
        } else {
            statusMessage.textContent = 'UI file upload failed.';
            showMessage('UI file upload failed.', 'error');
        }
    };

    xhr.onerror = function() {
        statusMessage.textContent = 'UI file upload error.';
        showMessage('UI file upload error.', 'error');
    };

    xhr.send(formData);
}