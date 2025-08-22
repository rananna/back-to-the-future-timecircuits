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
    // Show a loading indicator on the save button
    showLoading('saveSettingsBtn', true);
    console.log("CLIENT_DEBUG: 'Engage Time Circuits' button clicked. Starting save process.");

    // Create a single object to hold all settings
    const settings = {};

    // Gather all the settings from the UI
    // Time Circuits & Temporal Settings
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

    // Data Link & Weather Settings
    settings.dataLinkEnabled = document.getElementById('dataLinkEnabled').checked;
    settings.dataLinkRefreshInterval = parseInt(document.getElementById('dataLinkRefreshInterval').value, 10);
    settings.mqttBroker = document.getElementById('mqttBroker').value;
    settings.mqttPort = parseInt(document.getElementById('mqttPort').value, 10);
    settings.mqttUser = document.getElementById('mqttUser').value;
    settings.mqttPassword = document.getElementById('mqttPassword').value;

    settings.weatherModeEnabled = document.getElementById('weatherModeEnabled').checked;
    settings.cityName = document.getElementById('cityName').value;
    settings.useMetricUnits = document.getElementById('useMetricUnits').checked;

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

    // Send the settings to the server
    fetch('/api/saveSettings', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
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

        // Now, trigger the animation in a separate call
        console.log("CLIENT_DEBUG: Triggering /api/triggerAnimation...");
        return fetch('/api/triggerAnimation', { method: 'POST' });
    })
    .then(res => {
        if (!res.ok) {
            console.error(`CLIENT_DEBUG: /api/triggerAnimation call failed with status: ${res.status}`);
            throw new Error('Failed to trigger animation');
        }
        console.log("CLIENT_DEBUG: /api/triggerAnimation successful.");
        // Add a class to the body to trigger a flashing animation
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
        // Hide the loading indicator on the save button
        showLoading('saveSettingsBtn', false)
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