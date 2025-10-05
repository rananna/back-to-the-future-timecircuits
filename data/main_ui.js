// Global state for the animation preview interval
let animationPreviewInterval = null;

/**
 * Initializes the UI when the DOM is fully loaded.
 */
document.addEventListener('DOMContentLoaded', async () => {
    // Check if the server is ready before initializing the UI
    const isReady = await checkServerReady();
    if (isReady) {
        initializeUI();
    } else {
        // If the server is not ready, display an error message
        document.body.innerHTML = '<div class="container"><h1>Connection Failed</h1><p>Could not connect to the Time Circuits device. Please check the connection and refresh the page.</p></div>';
        showMessage('Could not connect to device.', 'error', 10000);
    }
});

/**
 * Initializes the main UI components and fetches initial data.
 */
async function initializeUI() {
    try {
        // Define the API endpoints to fetch initial data from
        const initialEndpoints = [
            '/api/settings/BTTF_TC', '/api/settings/temporal',
            '/api/settings/datalink', '/api/timezones',
            '/api/getPresets', '/api/getTheme'
        ];
        // Fetch all the initial data in parallel
        const promises = initialEndpoints.map(url => fetch(url).then(res => {
            if (!res.ok) return Promise.reject(new Error(`Request to ${url} failed`));
            return url.endsWith('Theme') ? res.text() : res.json();
        }));

        // Wait for all promises to resolve
        const [timecircuits, temporal, datalink, timezones, presets, theme] = await Promise.all(promises);

        // Apply the fetched theme to the UI
        document.body.className = theme.trim();
        // Populate the timezone and preset dropdowns
        populateTimezoneSelects(timezones);
        populatePresetsSelect(presets);
        // Apply the fetched settings to the UI
        await applySettings(timecircuits, temporal, datalink);
        // Make the header clocks visible
        document.querySelector('.header-circuits').classList.add('visible');

        // Initialize the WebSocket connection
        initWebSocket();

        // Populate radio stations
        populateRadioStations();
        populateSequences();

        // Start fetching real-time data
        fetchTime();
        setInterval(fetchTime, 1000); // Fetch time every second
        fetchWeatherData();
        weatherInterval = setInterval(fetchWeatherData, 300000); // Fetch weather every 5 minutes
        fetchSystemStatus();
        setInterval(fetchSystemStatus, 5000); // Fetch system status every 5 seconds
        setInterval(updateStockStatus, 60000); // Update stock status every minute
        // Attach all the event listeners to the UI elements
        attachEventListeners();
        
        showMessage('System Online', 'success');

    } catch (error) {
        console.error("CLIENT_DEBUG: Failed during essential initialization:", error);
        showMessage(`Critical error loading settings: ${error.message}. Please refresh.`, 'error');
    } finally {
        // Set the loading flag to false
        isLoading = false;
    }
}

/**
/**
 * Populates the sequence select dropdown with data from the server.
 */
async function populateSequences() {
    try {
        const response = await fetch('/api/sequences');
        if (!response.ok) {
            throw new Error('Failed to fetch sequences');
        }
        const sequences = await response.json();
        const select = document.getElementById('sequenceSelect');
        select.innerHTML = '<option value="">-- Select a Sequence --</option>'; // Clear existing options

        if (sequences && sequences.length > 0) {
            sequences.forEach(sequence => {
                const option = document.createElement('option');
                option.textContent = sequence.name;
                // Store the raw JSON payload string in the value attribute
                option.value = sequence.payload;
                select.appendChild(option);
            });
        }
    } catch (error) {
        console.error("CLIENT_DEBUG: Failed to populate sequences:", error);
        showMessage('Could not load sequences.', 'error');
    }
}

/**
 * Populates the radio station table with data from the server.
 */
async function populateRadioStations() {
    try {
        const response = await fetch('/api/radio_stations');
        if (!response.ok) {
            throw new Error('Failed to fetch radio stations');
        }
        const stations = await response.json();
        const tableBody = document.getElementById('stationsTable').querySelector('tbody');
        tableBody.innerHTML = ''; // Clear existing rows

        if (stations && stations.length > 0) {
            stations.forEach((station, index) => {
                const row = tableBody.insertRow();
                row.innerHTML = `
                    <td>${station.name}</td>
                    <td>${station.url}</td>
                    <td class="actions">
                        <button class="action-button play-station-btn" data-url="${station.url}">Play</button>
                        <button class="action-button edit-station-btn" data-index="${index}" data-name="${station.name}" data-url="${station.url}">Edit</button>
                        <button class="delete-button delete-station-btn" data-index="${index}">Delete</button>
                    </td>
                `;
            });
        }
        // Re-attach event listeners for the new buttons
        attachStationButtonListeners();
    } catch (error) {
        console.error("CLIENT_DEBUG: Failed to populate radio stations:", error);
        showMessage('Could not load radio stations.', 'error');
    }
}

/**
 * Populates the timezone select dropdowns with data from the server.
 * @param {object} data The timezone data from the server.
 */
function populateTimezoneSelects(data) {
    // Clear the global array and selectors
    timezoneOptions = [];
    const presentSelect = document.getElementById('presentTimezoneSelect');
    const destSelect = document.getElementById('destinationTimezoneSelect');

    if (presentSelect) presentSelect.innerHTML = '';
    if (destSelect) destSelect.innerHTML = '';

    // First, populate the global timezoneOptions array. This is used by other functions
    // like formatDateTimeInTimezone to look up timezone data by index.
    for (const region in data) {
        data[region].forEach(tz => {
            timezoneOptions[tz.value] = tz;
        });
    }

    // Populate Present and Destination dropdowns. They use the index as the value.
    for (const region in data) {
        const optgroup = document.createElement('optgroup');
        optgroup.label = region;
        data[region].forEach(tz => {
            const option = document.createElement('option');
            option.value = tz.value; // Use the index
            option.textContent = tz.text;
            optgroup.appendChild(option);
        });
        if (presentSelect) presentSelect.appendChild(optgroup.cloneNode(true));
        if (destSelect) destSelect.appendChild(optgroup.cloneNode(true));
    }

}

/**
 * Populates the preset select dropdown with custom presets from the server.
 * @param {Array} data The array of custom presets.
 */
function populatePresetsSelect(data) {
    const select = document.getElementById('presetDateSelect');
    // Remove any existing custom presets
    let customGroup = select.querySelector('optgroup[label="Custom Time Jumps"]');
    if (customGroup) customGroup.remove();
    // If there are custom presets, add them to the dropdown
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

/**
 * Updates the "Last Time Departed" display.
 * @param {number} year The year.
 * @param {number} month The month.
 * @param {number} day The day.
 * @param {number} hour The hour.
 * @param {number} minute The minute.
 */
function updateLastDepartedDisplay(year, month, day, hour, minute) {
    // Check if 24-hour format is enabled
    const is24h = document.getElementById('displayFormat24h').checked;
    let displayHour = parseInt(hour, 10);
    let ampm = '';
    // Convert to 12-hour format if necessary
    if (!is24h) {
        ampm = displayHour >= 12 ? 'PM' : 'AM';
        if (displayHour > 12) displayHour -= 12;
        if (displayHour === 0) displayHour = 12;
    }
    // Format the date and time strings
    const monthStr = String(month).padStart(2, '0');
    const dayStr = String(day).padStart(2, '0');
    const hourStr = String(displayHour).padStart(2, '0');
    const minuteStr = String(minute).padStart(2, '0');
    // Update the display
    document.getElementById('lastTimeDepartedDisplay').textContent = `${monthStr}/${dayStr}/${year} ${hourStr}:${minuteStr} ${ampm}`.trim();
    // Store the values in hidden elements for later use
    document.getElementById('lastTimeDepartedYear').textContent = year;
    document.getElementById('lastTimeDepartedMonth').textContent = month;
    document.getElementById('lastTimeDepartedDay').textContent = day;
    document.getElementById('lastTimeDepartedHour').textContent = hour;
    document.getElementById('lastTimeDepartedMinute').textContent = minute;
}

/**
 * Applies the fetched settings to the UI.
 * @param {object} timecircuits The Time Circuits settings.
 * @param {object} temporal The temporal settings.
 * @param {object} datalink The Data Link settings.
 */
async function applySettings(timecircuits, temporal, datalink) {
    // Apply Time Circuits settings
    if (timecircuits) {
        document.getElementById('destinationYear').value = timecircuits.destinationYear;
        document.getElementById('destinationTimezoneSelect').value = timecircuits.destinationTimezoneIndex;
        document.getElementById('presentTimezoneSelect').value = timecircuits.presentTimezoneIndex;
    }
    // Apply temporal settings
    if (temporal) {
        document.getElementById('departureTime').value = `${String(temporal.departureHour).padStart(2, '0')}:${String(temporal.departureMinute).padStart(2, '0')}`;
        document.getElementById('arrivalTime').value = `${String(temporal.arrivalHour).padStart(2, '0')}:${String(temporal.arrivalMinute).padStart(2, '0')}`;
        ['brightness', 'notificationVolume', 'timeTravelAnimationDuration', 'timeTravelAnimationInterval', 'presetCycleInterval'].forEach(id => {
            const slider = document.getElementById(id);
            if (slider) {
                slider.value = temporal[id];
                const valueSpan = document.getElementById(`${id}Value`);
                if (valueSpan) valueSpan.textContent = temporal[id];
            }
        });
        ['timeTravelSoundToggle', 'displayFormat24h'].forEach(id => {
            document.getElementById(id).checked = temporal[id];
        });
    }

    // Update the Last Departed display
    if (timecircuits) {
        updateLastDepartedDisplay(timecircuits.lastTimeDepartedYear, timecircuits.lastTimeDepartedMonth, timecircuits.lastTimeDepartedDay, timecircuits.lastTimeDepartedHour, timecircuits.lastTimeDepartedMinute);
    }
    
    // Apply Data Link settings
    if (datalink) {
        await applyDataLinkSettings(datalink);
        isDataLinkLoaded = true;
    }
    // Update the sleep visualizer
    updateSleepVisual();
}

/**
 * Applies the fetched Data Link settings to the UI.
 * @param {object} datalink The Data Link settings.
 */
async function applyDataLinkSettings(datalink) {
    // DMS_NORMAL_CLOCK = 0, DMS_STOCK_TICKER = 1, DMS_WEATHER = 2, DMS_DATA_LINK = 3
    const displayMode = datalink.displayMode;

    // Set the checked state of the toggles based on the displayMode
    document.getElementById('stockTickerModeEnabled').checked = (displayMode === 1);
    document.getElementById('weatherModeEnabled').checked = (displayMode === 2);
    document.getElementById('dataLinkEnabled').checked = (displayMode === 3);

    // Show/hide the corresponding settings containers
    document.getElementById('stockTickerSettingsContainer').style.display = (displayMode === 1) ? 'block' : 'none';
    document.getElementById('weatherSettingsContainer').style.display = (displayMode === 2) ? 'block' : 'none';
    document.getElementById('dataLinkSettingsContainer').style.display = (displayMode === 3) ? 'block' : 'none';

    // Disable the other mode groups to prevent multiple selections
    document.getElementById('stockTickerGroup').classList.toggle('disabled', displayMode === 2 || displayMode === 3);
    document.getElementById('weatherModeGroup').classList.toggle('disabled', displayMode === 1 || displayMode === 3);
    document.getElementById('dataLinkGroup').classList.toggle('disabled', displayMode === 1 || displayMode === 2);

    document.getElementById('cityName').value = datalink.cityName || '';
    document.getElementById('useMetricUnits').checked = datalink.useMetricUnits;

    document.getElementById('financialModelingPrepApiKey').value = datalink.financialModelingPrepApiKey || '';
    document.getElementById('stockRefreshInterval').value = datalink.stockRefreshInterval || 2;

    if (displayMode === 1) { // If stock ticker mode is enabled
        loadStockAssets();
        updateStockStatus();
    }
    
    document.getElementById('mqttBroker').value = datalink.mqttBroker || '';
    document.getElementById('mqttPort').value = datalink.mqttPort || 1883;
    document.getElementById('mqttUser').value = datalink.mqttUser || '';
    document.getElementById('mqttPassword').value = datalink.mqttPassword || '';

    // Sanitize numDataPoints to prevent errors from invalid server data
    const numPoints = Number(datalink.numDataPoints) || 0;
    document.getElementById('numDataPoints').value = numPoints;
    document.getElementById('numDataPointsValue').textContent = numPoints;

    // Update the UI for each data point
    await updateDataPointsUI(numPoints);
    if (datalink.dataPoints) {
        for (let i = 0; i < numPoints; i++) {
            const point = datalink.dataPoints[i];
            if (!point) continue;

            let dataSourceValue = 'mqtt'; // Default
            if (point.dataSourceType === 1) {
                dataSourceValue = 'ha';
            } else if (point.dataSourceType === 2) {
                dataSourceValue = 'static';
            }
            document.getElementById(`dp_dataSourceType_${i}`).value = dataSourceValue;
            document.getElementById(`dp_scrollSpeed_${i}`).value = point.scrollSpeed || 150;
            document.getElementById(`dp_scrollSpeed_${i}Value`).textContent = point.scrollSpeed || 150;
            document.getElementById(`dp_mqttTopic_${i}`).value = point.mqttTopic || '';
            document.getElementById(`dp_scrollingText_${i}`).value = point.scrollingText || '';
            document.getElementById(`dp_prefixText_${i}`).value = point.prefixText || '';
            document.getElementById(`dp_suffixText_${i}`).value = point.suffixText || '';

            // Trigger change events to update the UI
            document.getElementById(`dp_dataSourceType_${i}`).dispatchEvent(new Event('change'));
            // Marquee preview is disabled, so no call to updateMarqueePreview is needed.
        }
    }
}

/**
 * Updates the radio control button and status display based on the current radio state.
 * @param {string} status The current status of the radio ('stopped', 'connecting', 'playing', 'error').
 * @param {string} [message] An optional message, typically for errors.
 */
function handleDisplayModeChange(changedCheckboxId) {
    const checkboxes = {
        weatherModeEnabled: 'weatherSettingsContainer',
        dataLinkEnabled: 'dataLinkSettingsContainer',
        stockTickerModeEnabled: 'stockTickerSettingsContainer'
    };
    const groupIds = {
        weatherModeEnabled: 'weatherModeGroup',
        dataLinkEnabled: 'dataLinkGroup',
        stockTickerModeEnabled: 'stockTickerGroup'
    };
    const changedCheckbox = document.getElementById(changedCheckboxId);
    const isChecked = changedCheckbox.checked;

    // If a checkbox is checked, uncheck others and manage containers/groups
    if (isChecked) {
        for (const id in checkboxes) {
            if (id !== changedCheckboxId) {
                document.getElementById(id).checked = false;
                document.getElementById(checkboxes[id]).style.display = 'none';
                document.getElementById(groupIds[id]).classList.add('disabled');
            }
        }
        document.getElementById(checkboxes[changedCheckboxId]).style.display = 'block';
        document.getElementById(groupIds[changedCheckboxId]).classList.remove('disabled');

        // Special actions for specific modes when enabled
        if (changedCheckboxId === 'weatherModeEnabled' && document.getElementById('cityName').value) {
            lookupCity();
        } else if (changedCheckboxId === 'stockTickerModeEnabled') {
            loadStockAssets();
            updateStockStatus();
        }
    } else {
        // If a checkbox is unchecked, just hide its container and enable all groups for re-selection
        document.getElementById(checkboxes[changedCheckboxId]).style.display = 'none';
        for (const id in groupIds) {
            document.getElementById(groupIds[id]).classList.remove('disabled');
        }
    }

    if (!isLoading) {
        setSettingsChanged(true);
    }
}

function updateRadioControls(status, message = '') {
    const controlBtn = document.getElementById('radioControlBtn');
    const statusDisplay = document.getElementById('radioStatus');

    if (!controlBtn || !statusDisplay) return;

    controlBtn.disabled = false;
    controlBtn.innerHTML = 'Play'; // Default

    switch (status) {
        case 'stopped':
            controlBtn.textContent = 'Play';
            controlBtn.dataset.state = 'stopped';
            statusDisplay.textContent = 'Stopped';
            statusDisplay.className = 'radio-status-display';
            break;
        case 'connecting':
            controlBtn.innerHTML = '<span class="button-spinner"></span>';
            controlBtn.dataset.state = 'connecting';
            controlBtn.disabled = true;
            statusDisplay.textContent = 'Connecting...';
            statusDisplay.className = 'radio-status-display info';
            break;
        case 'playing':
            controlBtn.textContent = 'Stop';
            controlBtn.dataset.state = 'playing';
            statusDisplay.textContent = 'Playing';
            statusDisplay.className = 'radio-status-display success';
            break;
        case 'error':
            controlBtn.textContent = 'Play';
            controlBtn.dataset.state = 'stopped'; // Allow retry
            statusDisplay.textContent = `Error: ${message || 'Unknown'}`;
            statusDisplay.className = 'radio-status-display error';
            break;
    }
}

/**
 * Attaches all the necessary event listeners to the UI elements.
 */
function attachEventListeners() {
    // Header clocks click to scroll to settings
    document.getElementById('header-dest').onclick = () => scrollToSettings('TimeCircuits', 'destinationTimeSettings');
    document.getElementById('header-pres').onclick = () => scrollToSettings('System', 'presentTimeSettings');
    document.getElementById('header-last').onclick = () => scrollToSettings('TimeCircuits', 'lastDepartedSettings');
    // "Great Scott!" button
    document.getElementById('greatScottBtn').onclick = () => fetch('/api/greatScott', { method: 'POST' });
    // "Engage Time Circuits" button
    document.getElementById('saveSettingsBtn').onclick = saveSettings;
    // Timezone and destination year inputs
    ['destinationTimezoneSelect', 'presentTimezoneSelect'].forEach(id => {
        document.getElementById(id).onchange = () => {
            if (!isLoading) setSettingsChanged(true);
            updateHeaderClocks(new Date());
        };
    });
    document.getElementById('destinationYear').oninput = () => updateHeaderClocks(new Date());

    // Preset selection and management
    document.getElementById('presetDateSelect').onchange = handlePresetSelectionChange;
    document.getElementById('savePresetBtn').onclick = handleSavePreset;
    document.getElementById('deletePresetBtn').onclick = deletePreset;
    document.getElementById('newPresetBtn').onclick = resetPresetForm;
    // Weather lookup and refresh buttons
    document.getElementById('lookupCityBtn').onclick = lookupCity;
    document.getElementById('refreshWeatherBtn').onclick = () => {
        const lat = document.getElementById('weatherLatitude').value;
        const lon = document.getElementById('weatherLongitude').value;
        if (lat && lon) {
            triggerWeatherRefresh(parseFloat(lat), parseFloat(lon));
        } else {
            showMessage('Please lookup a city to get coordinates first.', 'error');
        }
    };
    // 24-hour format toggle
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

    // Weather, Data Link, and Stock Ticker mode toggles
    document.getElementById('weatherModeEnabled').onchange = () => handleDisplayModeChange('weatherModeEnabled');
    document.getElementById('dataLinkEnabled').onchange = () => handleDisplayModeChange('dataLinkEnabled');
    document.getElementById('stockTickerModeEnabled').onchange = () => handleDisplayModeChange('stockTickerModeEnabled');

    // Use event delegation for the stock fetch buttons. This ensures the click event
    // is handled even if the buttons are added to the DOM after the initial page load.
    document.getElementById('addAssetBtn').onclick = addStockAsset;

    // Number of data points slider
    document.getElementById('notificationVolume').addEventListener('input', (e) => {
        document.getElementById('notificationVolumeValue').textContent = e.target.value;
    });

    document.getElementById('numDataPoints').oninput = (e) => {
        document.getElementById('numDataPointsValue').textContent = e.target.value;
        updateDataPointsUI(parseInt(e.target.value, 10));
        if (!isLoading) setSettingsChanged(true);
    };
    // General input change listeners to track settings changes
    document.querySelectorAll('input, select').forEach(el => {
        el.addEventListener('change', () => {
            if (!isLoading) setSettingsChanged(true);
        });
        el.addEventListener('input', (e) => {
            if (!isLoading) {
                const valueSpan = document.getElementById(`${e.target.id}Value`);
                if (valueSpan) valueSpan.textContent = e.target.value;
                setSettingsChanged(true);
            }
        });
    });
    // Reset to defaults button
    document.getElementById('resetDefaultsBtn').onclick = () => {
        if (confirm("Are you sure? This will reset all settings to their defaults.")) {
            fetch('/api/resetSettings', { method: 'POST' })
                .then(res => res.text()).then(text => {
                    showMessage(text, 'success');
                    setTimeout(() => window.location.reload(), 1500);
                });
        }
    };
    // Sync time button
    document.getElementById('syncNtpBtn').onclick = () => {
        fetch('/api/syncTime', { method: 'POST' }).then(res => res.text()).then(text => showMessage(text, 'info'));
    };
    // Theme selector
    document.querySelectorAll('.theme-option').forEach(el => {
        el.onclick = () => {
            const theme = el.getAttribute('data-theme');
            document.body.className = theme;
            fetch('/api/setTheme', { method: 'POST', body: new URLSearchParams({ theme }) });
        };
    });
    // Sleep schedule inputs
    document.getElementById('departureTime').onchange = updateSleepVisual;
    document.getElementById('arrivalTime').onchange = updateSleepVisual;

    // Keyboard listener to deselect the wizard target
    document.addEventListener('keydown', (e) => {
        if (e.key === "Escape" && activeWizardTarget) {
            activeWizardTarget.classList.remove('is-wizard-target');
            activeWizardTarget = null;
            showMessage('Wizard target deselected.', 'info', 2000);
        }
    });

    // Firmware upload form
    document.getElementById('firmware-upload-form').onsubmit = handleFirmwareUpload;

    // Radio control button
    document.getElementById('radioControlBtn').onclick = (e) => {
        ws.send(JSON.stringify({ action: 'stop_radio' }));
    };

    // Station form modal listeners
    document.getElementById('addStationBtn').onclick = openStationForm;
    document.querySelector('#stationFormContainer .close-button').onclick = closeStationForm;
    document.getElementById('saveStationBtn').onclick = saveStation;

    // Run sequence button
    document.getElementById('runSequenceBtn').onclick = () => {
        const select = document.getElementById('sequenceSelect');
        const sequencePayload = select.value;
        if (sequencePayload && ws) {
            ws.send(JSON.stringify({ action: 'run_sequence', payload: sequencePayload }));
            showMessage(`Running sequence: ${select.options[select.selectedIndex].text}`, 'success');
        } else if (!sequencePayload) {
            showMessage('Please select a sequence to run.', 'error');
        }
    };
}

/**
 * Attaches event listeners to the dynamically created station buttons.
 */
function attachStationButtonListeners() {
    document.querySelectorAll('.play-station-btn').forEach(btn => {
        btn.onclick = (e) => {
            const url = e.target.dataset.url;
            if (url) {
                ws.send(JSON.stringify({ action: 'play_radio', url: url }));
            }
        };
    });

    document.querySelectorAll('.edit-station-btn').forEach(btn => {
        btn.onclick = (e) => {
            const index = e.target.dataset.index;
            const name = e.target.dataset.name;
            const url = e.target.dataset.url;
            openStationForm(index, name, url);
        };
    });

    document.querySelectorAll('.delete-station-btn').forEach(btn => {
        btn.onclick = (e) => {
            const index = e.target.dataset.index;
            if (confirm('Are you sure you want to delete this station?')) {
                deleteStation(index);
            }
        };
    });
}

/**
 * Opens the station form modal, optionally pre-filling it for editing.
 * @param {number} [index=-1] The index of the station to edit.
 * @param {string} [name=''] The name of the station.
 * @param {string} [url=''] The URL of the station.
 */
function openStationForm(index = -1, name = '', url = '') {
    document.getElementById('stationFormTitle').textContent = index === -1 ? 'Add Station' : 'Edit Station';
    document.getElementById('stationIndex').value = index;
    document.getElementById('stationName').value = name;
    document.getElementById('stationURL').value = url;
    document.getElementById('stationFormContainer').style.display = 'block';
}

/**
 * Closes the station form modal.
 */
function closeStationForm() {
    document.getElementById('stationFormContainer').style.display = 'none';
}

/**
 * Saves a station (either new or existing) to the server.
 */
async function saveStation() {
    const name = document.getElementById('stationName').value;
    const url = document.getElementById('stationURL').value;
    const index = parseInt(document.getElementById('stationIndex').value, 10);

    if (!name || !url) {
        showMessage('Name and URL are required.', 'error');
        return;
    }

    try {
        const response = await fetch('/api/station/save', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ name, url, index })
        });
        if (response.ok) {
            showMessage('Station saved successfully.', 'success');
            closeStationForm();
            // The backend will broadcast an update, so we don't need to call populateRadioStations() here.
        } else {
            throw new Error('Failed to save station');
        }
    } catch (error) {
        console.error("CLIENT_DEBUG: Failed to save station:", error);
        showMessage('Error saving station.', 'error');
    }
}

/**
 * Deletes a station from the server.
 * @param {number} index The index of the station to delete.
 */
async function deleteStation(index) {
    try {
        const response = await fetch('/api/station/delete', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ index })
        });
        if (response.ok) {
            showMessage('Station deleted successfully.', 'success');
             // The backend will broadcast an update, so we don't need to call populateRadioStations() here.
        } else {
            throw new Error('Failed to delete station');
        }
    } catch (error) {
        console.error("CLIENT_DEBUG: Failed to delete station:", error);
        showMessage('Error deleting station.', 'error');
    }
}

/**
 * Handles the change event of the preset select dropdown.
 */
function handlePresetSelectionChange(event) {
    applySelectedPreset(event);

    const select = event.target;
    if (!select.value) {
        resetPresetForm();
        return;
    }
    const selectedOption = select.options[select.selectedIndex];
    const isCustomPreset = selectedOption.parentElement.label === 'Custom Time Jumps';

    // If a custom preset is selected, show the edit form
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
        // Otherwise, show the "add new" form
        resetPresetForm(false);
    }
}

/**
 * Handles the click event of the save/update preset button.
 */
function handleSavePreset() {
    const select = document.getElementById('presetDateSelect');
    const selectedOption = select.options[select.selectedIndex];
    const isCustomPreset = selectedOption.parentElement.label === 'Custom Time Jumps';

    // If a custom preset is selected, update it, otherwise add a new one
    if (isCustomPreset) {
        updatePreset();
    } else {
        addPreset();
    }
}

/**
 * Resets the preset form to its default state.
 * @param {boolean} resetDropdown Whether to also reset the dropdown selection.
 */
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

/**
 * Applies the selected preset to the "Last Time Departed" display.
 * @param {Event} event The change event from the preset select dropdown.
 */
function applySelectedPreset(event) {
    const select = event.target;
    if (!select.value) return;
    const [year, month, day, hour, minute] = select.value.split('-');
    updateLastDepartedDisplay(year, month, day, hour, minute);
    showMessage(`Last Time Departed set to: ${select.options[select.selectedIndex].text}`, 'info');
    if (!isLoading) setSettingsChanged(true);
    updateHeaderClocks(new Date());
}

/**
 * Scrolls to a specific settings group in a tab.
 * @param {string} tabName The name of the tab to switch to.
 * @param {string} elementId The ID of the element to scroll to.
 */
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


/**
 * Sets the `settingsChanged` flag and enables/disables the save button.
 * @param {boolean} isChanged Whether the settings have changed.
 */
function setSettingsChanged(isChanged) {
    settingsChanged = isChanged;
    document.getElementById('saveSettingsBtn').disabled = !isChanged;
    if (isChanged) {
        document.getElementById('saveSettingsBtn').classList.add('needs-save');
    } else {
        document.getElementById('saveSettingsBtn').classList.remove('needs-save');
    }
}

/**
 * Updates the header clocks with the current time.
 * @param {Date} presentTimeRaw The current time.
 */
function updateHeaderClocks(presentTimeRaw) {
    const months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
    const is24h = document.getElementById('displayFormat24h').checked;

    // Helper function to populate a single header row
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

    // Populate the "Present Time" and "Destination Time" rows
    const presentUnixTimestamp = presentTimeRaw.getTime() / 1000;
    populateHeaderRow('pres', presentUnixTimestamp);

    const destYearInput = document.getElementById('destinationYear');
    if (destYearInput && destYearInput.value) {
        const destinationTime = new Date(presentTimeRaw.getTime());
        destinationTime.setFullYear(parseInt(destYearInput.value, 10));
        populateHeaderRow('dest', destinationTime.getTime() / 1000, destYearInput.value);
    }

    // Populate the "Last Time Departed" row
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

    // Update the current time marker in the sleep visualizer
    const now = new Date();
    const totalMinutes = now.getHours() * 60 + now.getMinutes();
    document.getElementById('currentTimeMarker').style.left = `${(totalMinutes / 1440) * 100}%`;
}

/**
 * Formats a Unix timestamp into a date and time string for a specific timezone.
 * @param {number} unixTimestamp The Unix timestamp.
 * @param {number} timezoneIndex The index of the timezone in the `timezoneOptions` array.
 * @param {boolean} is24HourFormat Whether to use 24-hour format.
 * @returns {object|null} An object with `time` and `date` strings, or null if an error occurs.
 */
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

/**
 * Updates the UI to show the specified number of data points without destroying existing ones.
 * @param {number} numPoints The number of data points to show.
 * @returns {Promise<void>} A promise that resolves when the UI is updated.
 */
/**
 * A single, comprehensive function to get the state of a data point from the UI.
 * This is the single source of truth for reading data point form data.
 * @param {number} index The index of the data point.
 * @param {boolean} forSave If true, formats the data for the backend (e.g., converts strings to numbers).
 * @returns {object|null} An object with the data point's data, or null if the point doesn't exist.
 */
function getUIDataPoint(index, forSave = false) {
    const getElValue = (id) => document.getElementById(id)?.value || '';
    if (!document.getElementById(`dp_dataSourceType_${index}`)) {
        return null; // Don't try to read data from a non-existent element
    }

    const dataSourceTypeStr = getElValue(`dp_dataSourceType_${index}`);

    if (forSave) {
        // Format for the C++ backend
        let dataSourceType;
        if (dataSourceTypeStr === 'ha') dataSourceType = 1;
        else if (dataSourceTypeStr === 'static') dataSourceType = 2;
        else dataSourceType = 0; // mqtt

        return {
            dataSourceType: dataSourceType,
            displayMode: 1, // Always scrolling
            scrollSpeed: parseInt(getElValue(`dp_scrollSpeed_${index}`), 10) || 150,
            mqttTopic: getElValue(`dp_mqttTopic_${index}`),
            scrollingText: getElValue(`dp_scrollingText_${index}`).toUpperCase(),
            prefixText: getElValue(`dp_prefixText_${index}`),
            suffixText: getElValue(`dp_suffixText_${index}`)
        };
    } else {
        // Raw values for UI state preservation
        return {
            dataSourceType: dataSourceTypeStr,
            displayMode: 'scrolling',
            scrollSpeed: getElValue(`dp_scrollSpeed_${index}`),
            mqttTopic: getElValue(`dp_mqttTopic_${index}`),
            scrollingText: getElValue(`dp_scrollingText_${index}`),
            prefixText: getElValue(`dp_prefixText_${index}`),
            suffixText: getElValue(`dp_suffixText_${index}`)
        };
    }
}

/**
 * Applies a data object to the UI fields for a specific data point.
 * @param {number} index The index of the data point.
 * @param {object} data The data object to apply.
 */
function applyDataPointToUI(index, data) {
    if (!data) return;

    const setElValue = (id, value) => {
        const el = document.getElementById(id);
        if (el) {
            el.value = value;
            const valueSpan = document.getElementById(`${id}Value`);
            if (valueSpan) valueSpan.textContent = value;
        }
    };

    // Restore the state of all UI fields from the provided data object.
    setElValue(`dp_dataSourceType_${index}`, data.dataSourceType || 'mqtt');
    setElValue(`dp_scrollSpeed_${index}`, data.scrollSpeed || 150);
    setElValue(`dp_mqttTopic_${index}`, data.mqttTopic || '');
    setElValue(`dp_scrollingText_${index}`, data.scrollingText || '');
    setElValue(`dp_prefixText_${index}`, data.prefixText || '');
    setElValue(`dp_suffixText_${index}`, data.suffixText || '');

    // Manually trigger a change event to ensure the correct containers are shown/hidden
    document.getElementById(`dp_dataSourceType_${index}`).dispatchEvent(new Event('change'));
}


/**
 * Updates the UI to show the specified number of data points using a "tear down and rebuild" strategy.
 * This approach guarantees UI consistency by avoiding complex DOM manipulations.
 * @param {number} numPoints The number of data points to show.
 */
const DP_HTML_TEMPLATE = (i) => `
    <div class="dp-header">
        <div class="dp-title-group">
            <span class="dp-status-indicator" id="dp_status_${i}"></span>
            <h4>Data Point ${i + 1}</h4>
        </div>
        <div class="dp-action-bar">
            <button type="button" class="action-button dp-clear-btn" data-index="${i}">Clear</button>
            <button type="button" class="action-button dp-dup-btn" data-index="${i}">Duplicate</button>
        </div>
    </div>
    <label for="dp_dataSourceType_${i}">Data Source:</label>
    <select id="dp_dataSourceType_${i}" class="data-source-select" data-index="${i}">
        <option value="mqtt">MQTT Push</option>
        <option value="ha">Home Assistant Push</option>
        <option value="static">Static Text</option>
    </select>

    <div id="dp_mqtt_container_${i}" class="dp-container">
        <label for="dp_mqttTopic_${i}">MQTT Topic:</label>
        <input type="text" id="dp_mqttTopic_${i}" placeholder="e.g., /home/livingroom/temperature">
    </div>

    <div id="dp_prefix_suffix_container_${i}" class="dp-container" style="display:none;">
        <label for="dp_prefixText_${i}">Prefix Text:</label>
        <input type="text" id="dp_prefixText_${i}" placeholder="e.g., TEMP:">
        <label for="dp_suffixText_${i}">Suffix Text:</label>
        <input type="text" id="dp_suffixText_${i}" placeholder="e.g., °C">
    </div>

    <input type="hidden" id="dp_displayMode_${i}" value="scrolling">

    <div id="scrolling_text_container_${i}" class="dp-container">
        <label for="dp_scrollingText_${i}">Scrolling Text:</label>
        <input type="text" id="dp_scrollingText_${i}" placeholder="Enter text or map a value...">
    </div>

    <label for="dp_scrollSpeed_${i}">Scroll Speed (ms/char): <span id="dp_scrollSpeed_${i}Value">150</span></label>
    <input type="range" id="dp_scrollSpeed_${i}" min="50" max="500" step="10" value="150">
    <input type="hidden" id="dp_api_example_${i}">
`;

function updateDataPointsUI(numPoints) {
    const container = document.getElementById('dataPointsConfigContainer');

    const currentState = [];
    const existingPointElements = container.querySelectorAll('.data-point-block');
    existingPointElements.forEach((block, i) => {
        const data = getUIDataPoint(i, false); // Get raw UI state
        if (data) {
            currentState.push(data);
        }
    });

    container.innerHTML = '';

    for (let i = 0; i < numPoints; i++) {
        const block = document.createElement('div');
        block.className = 'setting-group data-point-block collapsed';
        block.innerHTML = DP_HTML_TEMPLATE(i);
        container.appendChild(block);
    }

    attachDataPointEventListeners(container);

    for (let i = 0; i < numPoints; i++) {
        if (currentState[i]) {
            applyDataPointToUI(i, currentState[i]);
        }
    }

    // Trigger change events to ensure correct UI visibility
    container.querySelectorAll('.data-source-select, .display-mode-select').forEach(select => {
        if (select) {
            select.dispatchEvent(new Event('change', { 'bubbles': true }));
        }
    });
}

/**
 * Gets the display value for a data point field, resolving paths if possible.
 * @param {string} path The path or static value.
 * @param {string} placeholder The placeholder text if the value can't be resolved.
 * @param {number} index The index of the data point.
 * @returns {string} The resolved value or placeholder.
 */
/**
 * Attaches event listeners to the data point UI elements.
 * @param {Element} rootElement The root element to search for data point elements within.
 */
function attachDataPointEventListeners(rootElement = document) {
    // Data source and display mode selectors
    rootElement.querySelectorAll('.data-source-select').forEach(select => {
        select.onchange = (e) => {
            const index = e.currentTarget.dataset.index;
            const dataSource = document.getElementById(`dp_dataSourceType_${index}`).value;
            const scrollingTextInput = document.getElementById(`dp_scrollingText_${index}`);
            const scrollingTextContainer = document.getElementById(`scrolling_text_container_${index}`);
            const mqttContainer = document.getElementById(`dp_mqtt_container_${index}`);
            const prefixSuffixContainer = document.getElementById(`dp_prefix_suffix_container_${index}`);

            // Hide all by default
            mqttContainer.style.display = 'none';
            prefixSuffixContainer.style.display = 'none';
            scrollingTextContainer.style.display = 'none';

            if (dataSource === 'mqtt') {
                mqttContainer.style.display = 'block';
                prefixSuffixContainer.style.display = 'block';
                scrollingTextInput.placeholder = "Enter text or map a value...";
            } else if (dataSource === 'ha') {
                // For 'ha', all optional containers remain hidden
                scrollingTextInput.placeholder = "Enter text or map a value...";
            } else if (dataSource === 'static') {
                scrollingTextContainer.style.display = 'block';
                scrollingTextInput.placeholder = "e.g., 'MEETING AT 10' or 'GO TEAM'";
            }
        };
    });

    // Data point action buttons
    rootElement.querySelectorAll('.dp-clear-btn').forEach(btn => btn.onclick = clearDataPointFields);
    rootElement.querySelectorAll('.dp-dup-btn').forEach(btn => btn.onclick = duplicateDataPoint);

    // General input change listeners for data points
    rootElement.querySelectorAll('.data-point-block input, .data-point-block select, .data-point-block textarea').forEach(input => {
        input.addEventListener('input', (e) => {
            const indexMatch = e.target.id.match(/_(\d+)$/);
            if (!indexMatch) return;
            const index = indexMatch[1];
            
            // Handle the scroll speed slider value update
            if (e.target.id.startsWith('dp_scrollSpeed_')) {
                const valueSpan = document.getElementById(`dp_scrollSpeed_${index}Value`);
                if (valueSpan) {
                    valueSpan.textContent = e.target.value;
                }
            }

            if (e.target.id.startsWith('dp_dayPath_')) {
                document.getElementById(`dp_icon_${index}`).value = '';
            }

            if (e.target.id.includes('requestBody')) {
                validateJson(e.target);
            }
        });
    });

    // Accordion logic for data point blocks
    rootElement.querySelectorAll('.dp-header').forEach(header => {
        header.onclick = (e) => {
            // Don't collapse if a button inside the header was clicked
            if (e.target.tagName === 'BUTTON') return;
            const block = header.closest('.data-point-block');
            block.classList.toggle('collapsed');
        };
    });
}

/**
 * Validates if the text in a textarea is valid JSON.
 * @param {HTMLTextAreaElement} textarea The textarea to validate.
 * @returns {boolean} True if the JSON is valid, false otherwise.
 */
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

/**
 * Updates the sleep schedule visualizer.
 */
function updateSleepVisual() {
    const depTime = document.getElementById('departureTime').value; // This is now "Sleep Time"
    const arrTime = document.getElementById('arrivalTime').value;   // This is now "Wake Time"
    if (!depTime || !arrTime) return;

    const [depH, depM] = depTime.split(':').map(Number);
    const [arrH, arrM] = arrTime.split(':').map(Number);
    const depTotalMins = depH * 60 + depM; // Time sleep begins
    const arrTotalMins = arrH * 60 + arrM; // Time wake up happens

    const bar1 = document.getElementById('sleepScheduleBar');
    const bar2 = document.getElementById('sleepScheduleBar2');

    // Case 1: Awake time is one continuous block (e.g., wake at 7am, sleep at 10pm)
    if (arrTotalMins < depTotalMins) {
        const awakeDuration = depTotalMins - arrTotalMins;
        bar1.style.left = `${(arrTotalMins / 1440) * 100}%`;
        bar1.style.width = `${(awakeDuration / 1440) * 100}%`;
        bar2.style.display = 'none';
    } 
    // Case 2: Awake time is split into two blocks (e.g., sleep at 1am, wake at 9am)
    // The awake time is from midnight to 1am, and from 9am to midnight.
    else {
        const firstAwakeDuration = depTotalMins; // from 00:00 to sleep time
        bar1.style.left = '0%';
        bar1.style.width = `${(firstAwakeDuration / 1440) * 100}%`;
        
        const secondAwakeDuration = 1440 - arrTotalMins; // from wake time to 24:00
        bar2.style.left = `${(arrTotalMins / 1440) * 100}%`;
        bar2.style.width = `${(secondAwakeDuration / 1440) * 100}%`;
        bar2.style.display = 'block';
    }
}

/**
 * Shows or hides a loading spinner on a button.
 * @param {string} buttonId The ID of the button.
 * @param {boolean} isLoading Whether to show the loading spinner.
 */
function showLoading(buttonId, isLoading) {
    const button = document.getElementById(buttonId);
    if (!button) return;
    if (isLoading) {
        button.dataset.originalText = button.textContent;
        button.innerHTML = '<span class="loading-spinner"></span> Saving...';
        button.disabled = true;
    } else {
        button.textContent = button.dataset.originalText || 'Save & Engage Time Circuits';
        button.disabled = false;
    }
}

/**
 * Shows a message banner at the top of the page.
 * @param {string} message The message to show.
 * @param {string} type The type of message (info, success, error).
 * @param {number} duration The duration to show the message in milliseconds.
 */
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

/**
 * Updates the status indicator for a data point.
 * @param {number} index The index of the data point.
 * @param {boolean} isSuccess Whether the data point is successful.
 */
function updateDataPointStatus(index, isSuccess) {
    const indicator = document.getElementById(`dp_status_${index}`);
    if (indicator) {
        indicator.className = 'dp-status-indicator'; // Reset
        indicator.classList.add(isSuccess ? 'success' : 'error');
    }
    dataPointStatus[index] = isSuccess;
}

/**
 * Clears all the fields for a data point.
 * @param {Event} event The click event from the "Clear" button.
 */
function clearDataPointFields(event) {
    const index = event.currentTarget.dataset.index;
    const fields = ['mqttTopic', 'scrollingText', 'requestBody', 'url', 'authHeaderKey', 'authHeaderValue'];
    fields.forEach(field => {
        const el = document.getElementById(`dp_${field}_${index}`);
        if (el) el.value = '';
    });
    
    showMessage(`Data Point ${parseInt(index) + 1} fields cleared.`, 'info');
    if (!isLoading) setSettingsChanged(true);
}

/**
 * Duplicates a data point to a new slot.
 * @param {Event} event The click event from the "Duplicate" button.
 */
function duplicateDataPoint(event) {
    const sourceIndex = parseInt(event.currentTarget.dataset.index, 10);
    const numDataPoints = parseInt(document.getElementById('numDataPoints').value, 10);
    
    if (numDataPoints >= 5) {
        showMessage('Cannot duplicate, maximum number of data points reached.', 'error');
        return;
    }

    const targetIndex = numDataPoints;
    
    document.getElementById('numDataPoints').value = targetIndex + 1;
    document.getElementById('numDataPointsValue').textContent = targetIndex + 1;
    updateDataPointsUI(targetIndex + 1).then(() => {
        const fields = ['dataSourceType', 'url', 'scrollSpeed', 'mqttTopic', 'scrollingText', 'authHeaderKey', 'authHeaderValue', 'api_example'];
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

// The old getDataPointFromUI function is now replaced by the more comprehensive one above.
// This new version is used for saving state, while the one at the end of the file
// is used for preparing data to be sent to the backend. I will consolidate them.

/**
 * Gathers all the UI input values for a given data point for backend submission.
 * @param {number} index The index of the data point.
 * @returns {object} An object containing the data point's configuration formatted for the backend.
 */
// This function is no longer needed as its logic has been consolidated into getUIDataPoint

/**
 * Fetches the system status from the server.
 */
function fetchSystemStatus() {
    if (isLoading) return;
    fetch('/api/system/status')
        .then(res => {
            if (res.ok) {
                return res.json();
            }
            return Promise.reject('Failed to fetch system status');
        })
        .then(data => {
            // Update the system status display
            document.getElementById('freeMemory').textContent = `${(data.freeHeap / 1024).toFixed(1)} KB`;
            document.getElementById('wifiSignal').textContent = `${data.rssi} dBm`;
            
            const uptime = data.uptime;
            const days = Math.floor(uptime / 86400);
            const hours = Math.floor((uptime % 86400) / 3600);
            const minutes = Math.floor((uptime % 3600) / 60);
            document.getElementById('deviceUptime').textContent = `${days}d ${hours}h ${minutes}m`;
        })
        .catch(err => {
            console.warn("CLIENT_DEBUG: Could not fetch system status:", err);
            // If there's an error, show "Error"
            document.getElementById('freeMemory').textContent = 'Error';
            document.getElementById('wifiSignal').textContent = 'Error';
            document.getElementById('deviceUptime').textContent = 'Error';
        });
}

/**
 * Sets the loading state of a button.
 * @param {HTMLButtonElement} button The button to modify.
 * @param {boolean} isLoading Whether to show the loading spinner.
 */

function setButtonLoading(button, isLoading) {
    if (isLoading) {
        button.dataset.originalText = button.innerHTML;
        button.innerHTML = '<span class="loading-spinner"></span>';
        button.disabled = true;
    } else {
        button.innerHTML = button.dataset.originalText || 'Upload';
        button.disabled = false;
    }
}