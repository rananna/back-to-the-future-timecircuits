// Global state variables for the entire application
let settingsChanged = false; // Tracks if any settings have been changed by the user
let timezoneOptions = []; // Stores the available timezone options fetched from the server
let isDataLinkLoaded = false; // Flag to check if the Data Link settings have been loaded
let isManualRefresh = false; // Flag to track if a manual weather refresh is in progress
let anyInputInvalid = false; // Flag to track if there are any invalid inputs in the forms
let dataPointStatus = {}; // Stores the success/error status of each data point
let dataPointStateCache = {}; // Cache for data point UI state to prevent re-rendering
let ws; // The WebSocket object for real-time communication
let weatherInterval; // The interval ID for fetching weather data periodically
let isLoading = true; // Flag to indicate if the initial data is still loading
let correctedCityName = ''; // Stores the validated city name from the geocoding API

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

        if (msg.action === 'presetUpdate') {
            const presetSelect = document.getElementById('presetDateSelect');
            if (presetSelect) {
                let optionFound = false;
                for (let i = 0; i < presetSelect.options.length; i++) {
                    if (presetSelect.options[i].value === msg.value) {
                        presetSelect.selectedIndex = i;
                        optionFound = true;
                        break;
                    }
                }

                if (optionFound) {
                    presetSelect.dispatchEvent(new Event('change', { bubbles: true }));
                } else {
                    console.error(`Preset value from server not found in dropdown: ${msg.value}`);
                }

                showMessage(`Preset cycled to: ${msg.name}`, 'info');
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
        } else if (msg.action === 'weatherUpdate') {
            console.log("CLIENT_DEBUG: Received weatherUpdate:", msg.data);
            updateWeatherUI(msg.data);
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
        } else if (msg.action === 'animationComplete') {
            console.log("CLIENT_DEBUG: Animation complete message received. Re-enabling save button.");
            showLoading('saveSettingsBtn', false);
        } else if (msg.action === 'stockUpdate') {
            console.log('CLIENT_DEBUG: Received stock update from server. Refreshing status.');
            updateStockStatus();
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
 * Asynchronously performs geocoding for a city name if it has changed.
 * @param {string} cityName The name of the city to geocode.
 * @returns {Promise<object|null>} A promise that resolves with an object containing latitude and longitude, or null if geocoding fails.
 */
async function geocodeCityIfNeeded(cityName) {
    // If city name hasn't changed since last successful geocoding, resolve immediately.
    const weatherLatitude = document.getElementById('weatherLatitude');
    const weatherLongitude = document.getElementById('weatherLongitude');

    if (cityName.toLowerCase() === correctedCityName.toLowerCase() && weatherLatitude.value && weatherLongitude.value) {
        return {
            latitude: parseFloat(weatherLatitude.value),
            longitude: parseFloat(weatherLongitude.value)
        };
    }

    // If city name is empty, resolve with nulls, which will be handled by the backend.
    if (!cityName || cityName.length < 2) {
        return { latitude: 0.0, longitude: 0.0 };
    }

    showMessage('Validating city name...', 'info', 10000); // Show for longer

    try {
        const response = await fetch(`https://geocoding-api.open-meteo.com/v1/search?name=${encodeURIComponent(cityName)}&count=1&language=en&format=json`);
        if (!response.ok) {
            throw new Error(`Geocoding API failed with status: ${response.status}`);
        }
        const data = await response.json();

        if (data && data.results && data.results.length > 0) {
            const location = data.results[0];
            correctedCityName = getDescriptiveLocationName(location);
            weatherLatitude.value = location.latitude;
            weatherLongitude.value = location.longitude;
            showMessage(`City validated: ${correctedCityName}`, 'success');
            return { latitude: location.latitude, longitude: location.longitude };
        } else {
            showMessage('City not found. Weather location will not be updated.', 'error');
            return null; // Indicates geocoding failure
        }
    } catch (err) {
        console.error("CLIENT_DEBUG: Geocoding API error:", err);
        showMessage('Error verifying city name. Weather location will not be updated.', 'error');
        return null; // Indicates geocoding failure
    }
}


/**
 * Saves all the settings to the server.
 */
async function saveSettings() {
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
    
    // --- Geocode city name if weather mode is enabled ---
    if (getChecked('weatherModeEnabled')) {
        const cityName = getValue('cityName');
        const location = await geocodeCityIfNeeded(cityName);

        if (location === null) {
            // Geocoding failed, halt the save.
            showLoading('saveSettingsBtn', false);
            return;
        }
        // These will be added to the settings object later.
    }

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
    settings.latitude = parseFloat(getValue('weatherLatitude', '0.0'));
    settings.longitude = parseFloat(getValue('weatherLongitude', '0.0'));
    settings.useMetricUnits = getChecked('useMetricUnits');
    
    settings.stockTickerModeEnabled = getChecked('stockTickerModeEnabled');
    settings.financialModelingPrepApiKey = getValue('financialModelingPrepApiKey');
    settings.stockRefreshInterval = getIntValue('stockRefreshInterval', 2);

    if (settings.stockTickerModeEnabled && !settings.financialModelingPrepApiKey) {
        showMessage('FMP API Key is required for Stock Ticker Mode.', 'error');
        getEl('financialModelingPrepApiKey').classList.add('invalid-input');
        getEl('financialModelingPrepApiKey').focus();
        showLoading('saveSettingsBtn', false);
        return;
    }

    const stockAssets = [];
    document.querySelectorAll('#stockAssetList .asset-item').forEach(item => {
        stockAssets.push({
            symbol: item.dataset.symbol,
            type: parseInt(item.dataset.type, 10),
            timezone: item.dataset.timezone
        });
    });
    settings.stockAssets = stockAssets;

    if (settings.dataLinkEnabled) {
        const numDataPoints = getIntValue('numDataPoints', 0);
        settings.numDataPoints = numDataPoints;
        settings.dataPoints = [];
        for (let i = 0; i < numDataPoints; i++) {
            const point = {};
            const sourceValue = getValue(`dp_dataSourceType_${i}`, 'mqtt');
            if (sourceValue === 'ha') {
                point.dataSourceType = 1;
            } else if (sourceValue === 'static') {
                point.dataSourceType = 2;
            } else { // mqtt
                point.dataSourceType = 0;
            }
            point.displayMode = getIntValue(`dp_displayMode_${i}`, 0);
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
 * Creates a descriptive string for a location object.
 * @param {object} location The location object from the geocoding API.
 * @returns {string} A descriptive location name.
 */
function getDescriptiveLocationName(location) {
    let nameParts = [location.name];
    if (location.admin1) nameParts.push(location.admin1);
    if (location.country) nameParts.push(location.country);
    return nameParts.join(', ');
}

/**
 * Looks up a city name using the geocoding API.
 */
function lookupCity() {
    const cityInput = document.getElementById('cityName');
    const lookupButton = document.getElementById('lookupCityBtn');
    const city = cityInput.value.trim();

    if (!city || city.length < 2) {
        showMessage('Please enter a city name (at least 2 characters).', 'error');
        return;
    }

    const preview = document.getElementById('weatherPreview');
    preview.textContent = 'Verifying city...';
    const loadingSpinner = document.querySelector('#weatherSettingsContainer .loading-spinner-container');

    // UX Improvement: Disable inputs and show spinner
    cityInput.disabled = true;
    lookupButton.disabled = true;
    loadingSpinner.style.display = 'block';

    fetch(`https://geocoding-api.open-meteo.com/v1/search?name=${encodeURIComponent(city)}&count=10&language=en&format=json`)
        .then(response => response.json())
        .then(data => {
            if (data && data.results && data.results.length > 0) {
                if (data.results.length === 1) {
                    handleLocationSelection({ currentTarget: { dataset: { location: JSON.stringify(data.results[0]) } } }, true);
                } else {
                    showLocationModal(data.results);
                }
            } else {
                correctedCityName = '';
                preview.textContent = 'City not found.';
                showMessage('City not found. Please check the name and try again.', 'error');
            }
        })
        .catch(err => {
            console.error("CLIENT_DEBUG: Geocoding API error:", err);
            correctedCityName = '';
            preview.textContent = 'Could not verify city.';
            showMessage('Error verifying city name. Check connection or browser console.', 'error');
        })
        .finally(() => {
            // UX Improvement: Re-enable inputs and hide spinner
            cityInput.disabled = false;
            lookupButton.disabled = false;
            loadingSpinner.style.display = 'none';
        });
}


/**
 * Sends the validated city name to the server to trigger a weather data refresh.
 * @param {string} validatedCity The validated and formatted city name from the geocoding API.
 */
function triggerWeatherRefresh(latitude, longitude) {
    const preview = document.getElementById('weatherPreview');
    preview.textContent = 'Fetching...';

    let payload = {};
    if (latitude !== null && longitude !== null) {
        payload = { latitude, longitude };
    } else {
        payload = { cityName: correctedCityName };
    }

    fetch('/api/weather/refresh', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(payload)
    })
    .then(res => {
        if (res.ok) {
            showMessage('Weather refresh triggered. Please wait a moment.', 'info');
            setTimeout(fetchWeatherData, 3000);
        } else {
            return res.text().then(text => { throw new Error(text || 'Failed to trigger refresh.') });
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
function updateWeatherUI(data) {
    const isMetric = document.getElementById('useMetricUnits').checked;
    const tempUnit = isMetric ? '°C' : '°F';
    const speedUnit = isMetric ? ' km/h' : ' mph';

    const setContent = (id, value) => {
        const el = document.getElementById(id);
        if (el) el.textContent = value;
    };

    const formatTime = (unixTimestamp) => {
        if (!unixTimestamp) return '--:--';
        const date = new Date(unixTimestamp * 1000);
        return date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
    };

    // Update main display
    setContent('weatherIcon', getWeatherIcon(data.weatherCode));
    setContent('weatherTemp', `${data.temperature.toFixed(1)}${tempUnit}`);
    setContent('weatherFeelsLike', `${data.apparentTemperature.toFixed(1)}${tempUnit}`);
    setContent('weatherHumidity', `${data.humidity}%`);
    setContent('weatherWind', `${data.windSpeed.toFixed(1)}${speedUnit}`);
    setContent('weatherHighLow', `${data.dailyHigh.toFixed(0)}° / ${data.dailyLow.toFixed(0)}°`);
    setContent('weatherSunriseSunset', `${formatTime(data.sunrise)} / ${formatTime(data.sunset)}`);
    setContent('weatherPrecipChance', `${data.precipitationProbability}%`);
    setContent('weatherMaxWind', `${data.maxWindSpeed.toFixed(1)}${speedUnit}`);
    setContent('weatherTomorrowHighLow', `${data.tomorrowHigh.toFixed(0)}° / ${data.tomorrowLow.toFixed(0)}°`);
    setContent('weatherTomorrowCode', getWeatherIcon(data.tomorrowWeatherCode));

    // Update preview text
    const displayCity = correctedCityName || document.getElementById('cityName').value;
    document.getElementById('weatherPreview').textContent = `Live data for ${displayCity}: ${data.temperature.toFixed(1)}${tempUnit}`;

    // Update hidden inputs for saving, only if the new data is valid
    if (data.latitude !== 0 && data.longitude !== 0) {
        document.getElementById('weatherLatitude').value = data.latitude;
        document.getElementById('weatherLongitude').value = data.longitude;
    }

    // Build the hourly forecast display
    const hourlyContainer = document.getElementById('hourlyForecastContainer');
    hourlyContainer.innerHTML = '';
    if (data.hourly) {
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
    }

    document.querySelector('#weatherDisplay .loading-spinner-container').style.display = 'none';
}


/**
 * Fetches the current weather data from the server.
 */
function fetchWeatherData() {
    if (!document.getElementById('weatherModeEnabled').checked) {
        document.getElementById('weatherDisplay').style.display = 'none';
        return;
    }

    const loadingSpinner = document.getElementById('weatherDisplay').querySelector('.loading-spinner-container');
    loadingSpinner.style.display = 'block';

    fetch('/api/weather')
        .then(res => {
            if (res.ok) return res.json();
            return res.json().then(errorData => Promise.reject(errorData));
        })
        .then(data => {
            updateWeatherUI(data);
        })
        .catch(error => {
            console.warn("CLIENT_DEBUG: Could not fetch weather data:", error);
            const preview = document.getElementById('weatherPreview');
            preview.textContent = error.reason || 'Data not available. Check city name.';

            // --- START: MODIFICATION - Do not clear UI on fetch error ---
            // The following lines that clear the UI have been removed to prevent
            // a failed periodic fetch from wiping out valid data received via WebSocket.
            // A non-intrusive error message is now shown in the 'weatherPreview' element instead.
            //
            // const weatherFields = ['weatherIcon', 'weatherTemp', 'weatherFeelsLike', 'weatherHumidity', 'weatherWind', 'weatherCode', 'weatherHighLow', 'weatherSunriseSunset', 'weatherPrecipChance', 'weatherMaxWind', 'weatherTomorrowHighLow', 'weatherTomorrowCode', 'weatherLatitudeDisplay', 'weatherLongitudeDisplay'];
            // weatherFields.forEach(id => {
            //     const el = document.getElementById(id);
            //     if (el) el.textContent = '--';
            // });
            // document.getElementById('hourlyForecastContainer').innerHTML = '';
            // document.getElementById('weatherApiUrl').value = '';
            // --- END: MODIFICATION ---

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

function updateStockPreview(status, payload, rowIndex) {
    console.log(`CLIENT_DEBUG: updateStockPreview - Status: ${status}, RowIndex: ${rowIndex}`, payload);
    const previewContainer = document.getElementById(`stock_preview_${rowIndex}`);
    if (!previewContainer) {
        console.warn(`CLIENT_DEBUG: Stock preview container not found for rowIndex: ${rowIndex}`);
        showMessage('Could not update stock preview.', 'error');
        return;
    }
    const priceEl = previewContainer.querySelector('.stock-price');
    const changeEl = previewContainer.querySelector('.stock-change');

    // Make sure elements exist before trying to set their properties
    if (!priceEl || !changeEl) {
        console.error(`CLIENT_DEBUG: Preview elements not found for rowIndex: ${rowIndex}`);
        return;
    }

    const quote = Array.isArray(payload) ? payload[0] : payload;

    if (status === 'success' && quote && typeof quote === 'object' && !quote["Error Message"]) {
        const price = parseFloat(quote.price).toFixed(2);
        
        priceEl.textContent = `$${price}`;
        
        if (quote.changesPercentage !== undefined && !isNaN(parseFloat(quote.changesPercentage))) {
            const change = parseFloat(quote.changesPercentage);
            changeEl.textContent = `${change.toFixed(2)}%`;
            changeEl.classList.remove('positive', 'negative', 'error-text');
            if (change > 0) {
                changeEl.classList.add('positive');
            } else if (change < 0) {
                changeEl.classList.add('negative');
            }
        } else {
            changeEl.textContent = '--';
            changeEl.classList.remove('positive', 'negative', 'error-text');
        }
        console.log(`CLIENT_DEBUG: Stock UI updated for row ${rowIndex} - Price: ${price}`);
    } else {
        let errorMsg = 'Failed to fetch stock data.';
        if (typeof payload === 'string') {
            errorMsg = payload;
        } else if (payload && payload['Error Message']) {
            errorMsg = payload['Error Message'];
        } else if (payload && payload['Note']) {
            errorMsg = payload['Note'];
        }

        priceEl.textContent = 'Error';
        changeEl.textContent = errorMsg;
        changeEl.classList.add('error-text');
        changeEl.classList.remove('positive', 'negative');

        // Also show a general message at the top, but the specific error is in the preview
        showMessage('Stock data error. See details below.', 'error');
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

async function loadStockAssets() {
    try {
        const response = await fetch('/api/stocks');
        const data = await response.json();
        if (!response.ok) {
            // Use the error message from the server if available
            throw new Error(data.error || 'Failed to fetch stock assets');
        }
        renderStockAssets(data);
    } catch (error) {
        console.error('Error loading stock assets:', error.message);
        const container = document.getElementById('stockAssetList');
        container.innerHTML = `<p class="error-text">Could not load stock assets: ${error.message}</p>`;
    }
}

function renderStockAssets(assets) {
    const container = document.getElementById('stockAssetList');
    container.innerHTML = ''; // Clear existing list

    if (!assets || assets.length === 0) {
        container.innerHTML = '<p>No assets are being tracked.</p>';
        return;
    }

    assets.forEach((asset, index) => {
        const simpleSymbol = getSimpleSymbol(asset.symbol);
        const assetContainerDiv = document.createElement('div');
        assetContainerDiv.className = 'asset-item-container';

        const assetDiv = document.createElement('div');
        assetDiv.className = 'asset-item';
        assetDiv.dataset.symbol = simpleSymbol;
        assetDiv.dataset.exchange = asset.exchange || '';
        assetDiv.dataset.name = asset.name || '';
        // --- START: MODIFICATION - Add default type and timezone to prevent undefined issues ---
        // These attributes are expected by the save function but not consistently provided by the backend.
        // Providing defaults ensures that saving doesn't fail due to undefined values.
        assetDiv.dataset.type = asset.type || 'stock';
        assetDiv.dataset.timezone = asset.timezone || 'America/New_York';
        // --- END: MODIFICATION ---
        assetDiv.setAttribute('draggable', 'true');

        const changeClass = asset.change_percent >= 0 ? 'positive' : 'negative';
        const price = asset.data_valid ? `$${asset.price.toFixed(2)}` : '--';
        const change = asset.data_valid ? `${asset.change_percent.toFixed(2)}%` : '--';

        assetDiv.innerHTML = `
            <span class="asset-symbol">${simpleSymbol}</span>
            <span class="asset-name">${asset.name || ''}</span>
            <span class="asset-price">${price}</span>
            <span class="asset-change ${changeClass}">${change}</span>
            <button class="remove-asset-btn" data-symbol="${simpleSymbol}">DELETE</button>
        `;
        assetContainerDiv.appendChild(assetDiv);

        const previewDiv = document.createElement('div');
        previewDiv.id = `stock_preview_${index}`;
        previewDiv.className = 'stock-preview';
        previewDiv.innerHTML = `<span class="stock-price"></span><span class="stock-change"></span>`;
        assetContainerDiv.appendChild(previewDiv);

        container.appendChild(assetContainerDiv);
    });

    // Add event listeners to the new remove and test buttons
    document.querySelectorAll('.remove-asset-btn').forEach(btn => {
        btn.onclick = removeStockAsset;
    });
}

function addAssetToDOM(asset) {
    const container = document.getElementById('stockAssetList');
    const placeholder = container.querySelector('p');
    if (placeholder) {
        placeholder.remove();
    }

    const simpleSymbol = getSimpleSymbol(asset.symbol);
    const assetContainerDiv = document.createElement('div');
    assetContainerDiv.className = 'asset-item-container';

    const assetDiv = document.createElement('div');
    assetDiv.className = 'asset-item';
    assetDiv.dataset.symbol = simpleSymbol;
    assetDiv.dataset.exchange = asset.exchange || '';
    assetDiv.dataset.name = asset.name || '';
    assetDiv.dataset.type = asset.type || 'stock';
    assetDiv.dataset.timezone = asset.timezone || 'America/New_York';
    assetDiv.setAttribute('draggable', 'true');

    const price = asset.price ? `$${asset.price.toFixed(2)}` : '--';
    const change = asset.change_percent ? `${asset.change_percent.toFixed(2)}%` : '--';
    const changeClass = asset.change_percent >= 0 ? 'positive' : 'negative';

    assetDiv.innerHTML = `
        <span class="asset-symbol">${simpleSymbol}</span>
        <span class="asset-name">${asset.name || 'Loading...'}</span>
        <span class="asset-price">${price}</span>
        <span class="asset-change ${changeClass}">${change}</span>
        <button class="remove-asset-btn" data-symbol="${simpleSymbol}">DELETE</button>
    `;
    assetContainerDiv.appendChild(assetDiv);

    const previewDiv = document.createElement('div');
    previewDiv.id = `stock_preview_${container.children.length}`;
    previewDiv.className = 'stock-preview';
    previewDiv.innerHTML = `<span class="stock-price"></span><span class="stock-change"></span>`;
    assetContainerDiv.appendChild(previewDiv);

    container.appendChild(assetContainerDiv);

    // Re-attach event listener for the new remove button
    assetDiv.querySelector('.remove-asset-btn').onclick = removeStockAsset;
}

async function addStockAsset() {
    const input = document.getElementById('addAssetInput');
    const symbol = input.value.trim().toUpperCase();
    if (!symbol) return;

    // --- START: MODIFICATION - Optimistic UI Update ---
    // Add the asset to the DOM immediately with a "Loading..." state.
    const tempAsset = {
        symbol: symbol,
        name: 'Loading...',
        price: 0,
        change_percent: 0,
        data_valid: false
    };
    addAssetToDOM(tempAsset);
    input.value = '';
    // --- END: MODIFICATION ---

    try {
        const apiKey = document.getElementById('financialModelingPrepApiKey').value;
        if (!apiKey) {
            showMessage('Please enter your Financial Modeling Prep API key.', 'error');
            // If API key is missing, remove the temporary element.
            const tempElement = document.querySelector(`.asset-item[data-symbol='${symbol}']`);
            if (tempElement) tempElement.parentElement.remove();
            return;
        }

        const response = await fetch('/api/stocks/add', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ symbol })
        });

        const result = await response.json();
        if (response.ok && result.status === 'success') {
            showMessage(`Asset ${symbol} added successfully. Fetching data...`, 'success');
            // Now, load the full list from the server to get all details.
            // This will replace the temporary element with the real one.
            await loadStockAssets();
        } else {
            // If adding fails, remove the temporary element and show error.
            const tempElement = document.querySelector(`.asset-item[data-symbol='${symbol}']`);
            if (tempElement) tempElement.parentElement.remove();
            throw new Error(result.message || 'Failed to add asset.');
        }
    } catch (error) {
        console.error('Error adding asset:', error);
        showMessage(`Error: ${error.message}`, 'error');
        // Ensure the temporary element is removed on any kind of error.
        const tempElement = document.querySelector(`.asset-item[data-symbol='${symbol}']`);
        if (tempElement) tempElement.parentElement.remove();
    }
}

async function removeStockAsset(event) {
    const symbol = event.target.dataset.symbol;
    if (!confirm(`Are you sure you want to remove ${symbol}?`)) return;

    try {
        const response = await fetch('/api/stocks/delete', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ symbol })
        });

        const result = await response.json();
        if (response.ok && result.status === 'success') {
            showMessage(`Asset ${symbol} removed.`, 'success');
            await loadStockAssets();
        } else {
            throw new Error(result.message || 'Failed to remove asset.');
        }
    } catch (error) {
        console.error('Error removing asset:', error);
        showMessage(`Error removing asset: ${error.message}`, 'error');
    }
}

function getDragAfterElement(container, y) {
    const draggableElements = [...container.querySelectorAll('.asset-item:not(.dragging)')];

    return draggableElements.reduce((closest, child) => {
        const box = child.getBoundingClientRect();
        const offset = y - box.top - box.height / 2;
        if (offset < 0 && offset > closest.offset) {
            return { offset: offset, element: child };
        } else {
            return closest;
        }
    }, { offset: Number.NEGATIVE_INFINITY }).element;
}

/**
 * Normalizes a stock symbol to a simple string.
 * The symbol can be a plain string (e.g., "AAPL") or a JSON string
 * (e.g., '{"symbol":"AAPL",...}').
 * @param {string} symbol The stock symbol string.
 * @returns {string} The simple symbol string (e.g., "AAPL").
 */
function getSimpleSymbol(symbol) {
    // If it's not a string or is empty, return it as is.
    if (typeof symbol !== 'string' || !symbol) {
        return symbol;
    }
    // If it starts with '{', assume it's a JSON string.
    if (symbol.startsWith('{')) {
        try {
            const parsed = JSON.parse(symbol);
            return parsed.symbol || symbol; // Fallback to original string if parsing works but .symbol is missing
        } catch (e) {
            // If parsing fails, it's a malformed string, but we can't do much.
            return symbol;
        }
    }
    // Otherwise, it's a simple symbol string.
    return symbol;
}

async function handleSymbolAutocomplete(e) {
    const query = e.target.value;
    const container = document.getElementById('symbolAutocompleteContainer');
    if (query.length < 1) {
        container.innerHTML = '';
        container.style.display = 'none';
        return;
    }

    try {
        const response = await fetch(`/api/stocks/search?q=${query}`);
        if (!response.ok) {
            throw new Error('Symbol search failed');
        }
        const results = await response.json();
        renderAutocompleteResults(results);
    } catch (error) {
        console.error('Error during symbol autocomplete:', error);
    }
}

function renderAutocompleteResults(results) {
    const container = document.getElementById('symbolAutocompleteContainer');
    container.innerHTML = '';
    if (results.length > 0) {
        container.style.display = 'block';
        results.forEach(result => {
            const resultDiv = document.createElement('div');
            resultDiv.className = 'autocomplete-item';
            resultDiv.textContent = `${result.symbol} - ${result.name}`;
            resultDiv.onclick = () => {
                document.getElementById('addAssetInput').value = result.symbol;
                container.innerHTML = '';
                container.style.display = 'none';
            };
            container.appendChild(resultDiv);
        });
    } else {
        container.style.display = 'none';
    }
}

async function updateStockStatus() {
    if (!document.getElementById('stockTickerModeEnabled').checked) {
        return;
    }

    try {
        const response = await fetch('/api/stocks/status');
        if (!response.ok) {
            const errorData = await response.json().catch(() => ({ error: 'Failed to fetch stock status' }));
            throw new Error(errorData.error || 'Failed to fetch stock status');
        }
        const status = await response.json();
        const guidanceEl = document.getElementById('stockRefreshGuidance');
        const apiUsageEl = document.querySelector('.stockApiUsage');
        const API_CALL_LIMIT = 250;

        // Update API usage count and guidance text
        if (apiUsageEl) {
            apiUsageEl.textContent = `API Calls Today: ${status.api_usage} / ${API_CALL_LIMIT}`;
        }

        if (guidanceEl) {
            const assets = status.assets || [];
            const numAssets = assets.length;
            if (numAssets > 0) {
                const TRADING_MINUTES_STOCK = 390; // 6.5 hours for NYSE (9:30am to 4:00pm)
                const TRADING_MINUTES_CRYPTO = 1440; // 24 hours

                let stockCount = 0;
                let cryptoCount = 0;

                assets.forEach(asset => {
                    const symbol = getSimpleSymbol(asset.symbol).toUpperCase();
                    if (symbol.endsWith('-USD') || symbol.endsWith('-EUR')) {
                        cryptoCount++;
                    } else {
                        stockCount++;
                    }
                });

                // Total weighted minutes = (number of stocks * stock trading minutes) + (number of cryptos * crypto trading minutes)
                const totalWeightedMinutes = (stockCount * TRADING_MINUTES_STOCK) + (cryptoCount * TRADING_MINUTES_CRYPTO);

                // Recommended interval = (Total weighted minutes) / (API call limit)
                // This ensures that over the course of a day, the total calls for all assets stay within the limit.
                const recommendedInterval = Math.ceil(totalWeightedMinutes / API_CALL_LIMIT);

                let guidanceText = `With ${stockCount} stock(s) and ${cryptoCount} crypto(s), a refresh interval of ~${recommendedInterval} minutes is recommended to stay under the ${API_CALL_LIMIT}/day limit.`;
                if (stockCount > 0 && cryptoCount > 0) {
                    guidanceText += ` (Calculated with ~6.5hr market days for stocks and 24hr for crypto.)`;
                } else if (stockCount > 0) {
                    guidanceText += ` (Calculated with ~6.5hr market days for stocks.)`;
                } else if (cryptoCount > 0) {
                    guidanceText += ` (Calculated with 24hr trading for crypto.)`;
                }
                guidanceEl.textContent = guidanceText;

            } else {
                guidanceEl.textContent = `Add assets to see refresh interval guidance. The free API limit is ${API_CALL_LIMIT} calls per day.`;
            }
        }


        // Update individual asset display
        status.assets.forEach(asset => {
            const simpleSymbol = getSimpleSymbol(asset.symbol);
            const assetDiv = document.querySelector(`.asset-item[data-symbol='${simpleSymbol}']`);
            if (assetDiv) {
                const priceEl = assetDiv.querySelector('.asset-price');
                const changeEl = assetDiv.querySelector('.asset-change');

                if (asset.data_valid) {
                    priceEl.textContent = `$${asset.price.toFixed(2)}`;
                    changeEl.textContent = `${asset.change_percent.toFixed(2)}%`;
                    changeEl.className = 'asset-change ' + (asset.change_percent >= 0 ? 'positive' : 'negative');
                } else {
                    priceEl.textContent = 'Error';
                    changeEl.textContent = asset.error_reason || '--';
                    changeEl.className = 'asset-change negative';
                }
            }
        });

    } catch (error) {
        console.error('Error updating stock status:', error.message);
        const apiUsageEl = document.getElementById('stockApiUsage');
        if (apiUsageEl) {
            apiUsageEl.textContent = 'Could not get status.';
        }
    }
}

/**
 * Shows a modal with a list of locations for the user to choose from.
 * @param {Array} locations An array of location objects from the geocoding API.
 */
function showLocationModal(locations) {
    const modal = document.getElementById('locationChoiceModal');
    const list = document.getElementById('locationList');
    const closeButton = modal.querySelector('.close-button');

    // Clear previous results
    list.innerHTML = '';

    // Populate the list with new results
    locations.forEach(location => {
        const li = document.createElement('li');
        // Construct a descriptive name, including admin regions if they exist
        let displayName = `${location.name}, ${location.country}`;
        const adminParts = [location.admin1, location.admin2, location.admin3, location.admin4].filter(Boolean);
        if (adminParts.length > 0) {
            displayName += ` (${adminParts.join(', ')})`;
        }
        li.textContent = displayName;
        // Store the full location object in a data attribute
        li.dataset.location = JSON.stringify(location);
        li.addEventListener('click', handleLocationSelection);
        list.appendChild(li);
    });

    // Function to close the modal
    const closeModal = () => {
        modal.style.display = 'none';
    };

    // Show the modal
    modal.style.display = 'block';

    // Add event listeners to close the modal
    closeButton.onclick = closeModal;
    window.onclick = function(event) {
        if (event.target == modal) {
            closeModal();
        }
    };
}

/**
 * Handles the user's selection from the location choice modal.
 * @param {Event} event The click event from the list item.
 */
function handleLocationSelection(event) {
    const li = event.currentTarget;
    const location = JSON.parse(li.dataset.location);
    const cityInput = document.getElementById('cityName');
    const modal = document.getElementById('locationChoiceModal');

    // Use the chosen location's name
    const bestMatch = getDescriptiveLocationName(location);
    correctedCityName = bestMatch;
    cityInput.value = bestMatch; // Update the input field

    // Populate the latitude and longitude fields
    document.getElementById('weatherLatitude').value = location.latitude.toFixed(4);
    document.getElementById('weatherLongitude').value = location.longitude.toFixed(4);


    if (!isLoading) setSettingsChanged(true);

    showMessage(`Selected: ${bestMatch}. Fetching weather...`, 'success');

    // Hide the modal
    modal.style.display = 'none';

    // Trigger the weather refresh with the selected location's coordinates
    triggerWeatherRefresh(location.latitude, location.longitude);
}
