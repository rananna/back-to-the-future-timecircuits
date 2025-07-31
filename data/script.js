// final_ui/data/script.js
// Global variable to track if any input is invalid
let anyInputInvalid = false;
let settingsChanged = false;
let presetCycleTimer = null;
let timezoneOptions = [];

function openTab(evt, tabName) {
    var i, tabcontent, tablinks;
    tabcontent = document.getElementsByClassName("tab-content");
    for (i = 0; i < tabcontent.length; i++) {
        tabcontent[i].style.display = "none";
    }
    tablinks = document.getElementsByClassName("tab-link");
    for (i = 0; i < tablinks.length; i++) {
        tablinks[i].className = tablinks[i].className.replace(" active", "");
    }
    document.getElementById(tabName).style.display = "block";
    evt.currentTarget.className += " active";
}

function setSettingsChanged(isChanged) {
    settingsChanged = isChanged;
    updateSaveButtonState();
}

// Function now adds/removes a class for glowing effect
function updateSaveButtonState() {
    const saveBtn = document.getElementById('saveSettingsBtn');
    if (anyInputInvalid) {
        saveBtn.disabled = true;
        saveBtn.classList.remove('needs-save');
    } else {
        saveBtn.disabled = !settingsChanged;
        if (settingsChanged) {
            saveBtn.classList.add('needs-save');
        } else {
            saveBtn.classList.remove('needs-save');
        }
    }
}

function showFeedback(elementId) {
    const el = document.getElementById(elementId);
    if(el) {
        // Find the closest label and then its feedback-check span
        const feedbackEl = el.closest('label, .slider-container, .setting-group').querySelector('.feedback-check'); // Added .setting-group for broader search
        if (feedbackEl) {
            feedbackEl.classList.remove('show');
            void feedbackEl.offsetWidth; // Trigger reflow to restart animation
            feedbackEl.classList.add('show');
            feedbackEl.addEventListener('animationend', () => {
                feedbackEl.classList.remove('show');
            }, { once: true });
        }
    }
}

function setupPresetCycler() {
    if (presetCycleTimer) {
        clearInterval(presetCycleTimer);
    }
    const intervalMinutes = parseInt(document.getElementById('presetCycleInterval').value, 10);
    if (intervalMinutes > 0) {
        const intervalMillis = intervalMinutes * 60 * 1000;
        presetCycleTimer = setInterval(() => {
            const select = document.getElementById('presetDateSelect');
            if (!select || select.options.length <= 1) return;

            let nextIndex = select.selectedIndex + 1;
            if (nextIndex >= select.options.length || !select.options[nextIndex].parentElement || select.options[nextIndex].parentElement.tagName !== 'OPTGROUP') {
                nextIndex = 1;
            }
            select.selectedIndex = nextIndex;
            select.dispatchEvent(new Event('change'));
        }, intervalMillis);
    }
}


document.addEventListener('DOMContentLoaded', (event) => {
    fetchTimezones().then(() => {
        fetchAndApplyPresets().then(() => {
            fetchSettings().then(() => {
                document.querySelector('.header-circuits').classList.add('visible');
                fetchTime(); // Initial time fetch
                setInterval(fetchTime, 1000); // Fetch time every second
            });
        });
    });

    // Updated sliders array to include timeTravelAnimationDuration
    const sliders = [
        { id: 'brightness', valueSpanId: 'brightnessValue' },
        { id: 'notificationVolume', valueSpanId: 'volumeValue' },
        { id: 'timeTravelAnimationInterval', valueSpanId: 'timeTravelAnimationIntervalValue' },
        { id: 'presetCycleInterval', valueSpanId: 'presetCycleIntervalValue' },
        { id: 'timeTravelAnimationDuration', valueSpanId: 'timeTravelAnimationDurationValue' } // NEW
    ];

    sliders.forEach(sliderInfo => {
        const sliderElement = document.getElementById(sliderInfo.id);
        if (sliderElement) {
            sliderElement.addEventListener('input', (e) => {
                const valueSpanElement = document.getElementById(sliderInfo.valueSpanId);
                if (valueSpanElement) valueSpanElement.textContent = e.target.value;
                setSettingsChanged(true);
                // NEW: Call handlePreview for sliders that support it live
                if (document.getElementById('livePreviewToggle').checked) {
                    if (sliderInfo.id === 'brightness' || sliderInfo.id === 'notificationVolume' || sliderInfo.id === 'timeTravelAnimationDuration') { // Include duration here
                        handlePreview(sliderInfo.id, e.target.value);
                    }
                }
            });
        }
    });

    document.getElementById('destinationYear').addEventListener('input', (e) => {
        setSettingsChanged(true);
        if (document.getElementById('livePreviewToggle').checked) {
            handlePreview('destinationYear', e.target.value);
            showFeedback('destinationYear');
        }
    });

    document.getElementById('destinationTimezoneSelect').addEventListener('change', (e) => {
        setSettingsChanged(true);
        if (document.getElementById('livePreviewToggle').checked) {
            handlePreview('destinationTimezoneIndex', e.target.value);
            showFeedback('destinationTimezoneSelect');
        }
    });

    document.getElementById('presentTimezoneSelect').addEventListener('change', (e) => {
        setSettingsChanged(true);
        if (document.getElementById('livePreviewToggle').checked) {
            handlePreview('presentTimezoneIndex', e.target.value);
            showFeedback('presentTimezoneSelect');
        }
    });


    document.getElementById('departureTime').addEventListener('input', () => {
        setSettingsChanged(true);
        updateSleepScheduleVisual();
    });
    document.getElementById('arrivalTime').addEventListener('input', () => {
        setSettingsChanged(true);
        updateSleepScheduleVisual();
    });

    const presetCycleSlider = document.getElementById('presetCycleInterval');
    presetCycleSlider.addEventListener('change', setupPresetCycler);

    document.getElementById('powerOfLoveBtn').addEventListener('click', () => {
        fetch('/api/toggleGreatScottSound', { method: 'POST' })
            .then(response => response.json())
            .then(data => {
                document.getElementById('powerOfLoveBtn').classList.toggle('button-active', data.state);
                setSettingsChanged(true);
            });
    });

    document.getElementById('displayFormat24h').addEventListener('change', (e) => {
        setSettingsChanged(true);
        fetchTime(); // To update header clocks immediately with new format
        handlePreview('displayFormat24h', e.target.checked); // Live preview for 24h format
        showFeedback('displayFormat24h');
    });

    document.getElementById('timeTravelSoundToggle').addEventListener('change', (e) => {
        setSettingsChanged(true);
        showFeedback('timeTravelSoundToggle');
    });

    const animationStyleSelect = document.getElementById('animationStyleSelect');
    if (animationStyleSelect) {
        animationStyleSelect.addEventListener('change', (e) => {
            setSettingsChanged(true);
            if (document.getElementById('livePreviewToggle').checked) {
                handlePreview('animationStyle', e.target.value);
            }
            showFeedback('animationStyleSelect');
        });
    }


    document.querySelectorAll('.container input, .container select').forEach(element => {
        element.addEventListener('input', (e) => {
            setSettingsChanged(true);
            if(e.target.type === 'number') {
                validateInput(e.target);
            }
        });
    });

    document.getElementById('syncNtpBtn').addEventListener('click', syncNtp);

    document.getElementById('presetDateSelect').addEventListener('change', (e) => {
        const selectedOption = e.target.options[e.target.selectedIndex];
        const actions = document.getElementById('presetActions');
        if (selectedOption.parentElement.label === 'Custom Jumps') {
            actions.classList.remove('hidden');
        } else {
            actions.classList.add('hidden');
        }

        const selectedValue = e.target.value;
        if (!selectedValue) return;

        const parts = selectedValue.split('-');
        const [year, month, day, hour, minute] = parts;

        document.getElementById('lastTimeDepartedYear').textContent = year;
        document.getElementById('lastTimeDepartedMonth').textContent = month;
        document.getElementById('lastTimeDepartedDay').textContent = day;
        document.getElementById('lastTimeDepartedHour').textContent = hour;
        document.getElementById('lastTimeDepartedMinute').textContent = minute;

        updateLastDepartedDisplay();
        setSettingsChanged(true);
        if (document.getElementById('livePreviewToggle').checked) {
            fetch(`/api/setLastDeparted?value=${selectedValue}`, { method: 'POST' })
                .then(response => {
                    if (response.ok) {
                        fetchTime();
                        showFeedback('presetDateSelect');
                    } else {
                        response.text().then(text => console.error(`Preview error for setLastDeparted: ${text}`));
                    }
                })
                .catch(error => console.error('Error in live preview setLastDeparted:', error));
        }
    });

    setupIncrementerButtons();
    validateAllNumberInputs();
});

function handlePreview(settingName, value) {
    if (!document.getElementById('livePreviewToggle').checked) return;

    fetch(`/api/previewSetting?setting=${settingName}&value=${value}`)
        .then(response => {
            if (!response.ok) {
                response.text().then(text => {
                    console.error(`Preview error for ${settingName}: ${text}`);
                    showMessage(`Live preview failed for ${settingName}: ${text}`, 'error');
                });
            } else {
                if (settingName !== 'brightness' && settingName !== 'notificationVolume' && settingName !== 'timeTravelAnimationDuration' && settingName !== 'displayFormat24h' && settingName !== 'animationStyle') {
                     showFeedback(settingName);
                }
                if (settingName === 'displayFormat24h' || settingName === 'destinationYear' || settingName === 'destinationTimezoneIndex' || settingName === 'presentTimezoneIndex') {
                    fetchTime();
                }
            }
        })
        .catch(error => {
            console.error('Error in handlePreview:', error)
            showMessage(`Live preview connection error for ${settingName}.`, 'error');
        });
}


function setupIncrementerButtons() {
    document.querySelectorAll('.incrementer-btn').forEach(button => {
        button.addEventListener('click', (e) => {
            const targetId = e.target.dataset.target;
            const action = e.target.dataset.action;
            const targetSlider = document.getElementById(targetId);

            if (targetSlider) {
                let currentValue = parseInt(targetSlider.value, 10);
                const step = parseInt(targetSlider.step, 10) || 1;

                if (action === 'increment') {
                    currentValue += step;
                } else {
                    currentValue -= step;
                }

                const min = parseInt(targetSlider.min, 10);
                const max = parseInt(targetSlider.max, 10);

                if (currentValue < min) currentValue = min;
                if (currentValue > max) currentValue = max;

                targetSlider.value = currentValue;

                targetSlider.dispatchEvent(new Event('input', { bubbles: true }));
            }
        });
    });
}

function addOrUpdatePreset() {
    const editingValue = document.getElementById('editingPresetValue').value;
    if (editingValue) {
        updatePreset(editingValue);
    } else {
        addPreset();
    }
}

function addPreset() {
    const name = document.getElementById('presetName').value;
    const date = document.getElementById('presetDate').value;
    const time = document.getElementById('presetTime').value;

    if (!name || !date || !time) {
        showMessage('All preset fields are required!', 'error');
        return;
    }

    const [year, month, day] = date.split('-');
    const [hour, minute] = time.split(':');
    const value = `${year}-${month}-${day}-${hour}-${minute}`;

    const formData = new FormData();
    formData.append('name', name);
    formData.append('value', value);

    fetch('/api/addPreset', { method: 'POST', body: new URLSearchParams(formData) })
        .then(response => {
            if (!response.ok) {
                response.text().then(text => showMessage(`Error: ${text}`, 'error'));
                throw new Error('Network response was not ok');
            }
            return response.text();
        })
        .then(data => {
            showMessage(data, 'success');
            resetPresetForm();
            fetchAndApplyPresets();
        })
        .catch(error => console.error('Error adding preset:', error));
}

function editPreset() {
    const select = document.getElementById('presetDateSelect');
    const selectedOption = select.options[select.selectedIndex];

    if (!selectedOption || !selectedOption.value || !selectedOption.parentElement || selectedOption.parentElement.label !== 'Custom Jumps') {
        showMessage('Please select a custom preset to edit.', 'error');
        return;
    }

    const name = selectedOption.textContent;
    const value = selectedOption.value;

    const [year, month, day, hour, minute] = value.split('-');

    document.getElementById('presetName').value = name;
    document.getElementById('presetDate').value = `${year}-${month}-${day}`;
    document.getElementById('presetTime').value = `${hour}:${minute}`;
    document.getElementById('editingPresetValue').value = value;

    const btn = document.getElementById('addPresetBtn');
    btn.textContent = 'Update Preset';
    showMessage('Editing preset. Click "Update Preset" to save changes.', 'success');
}

function updatePreset(originalValue) {
    const newName = document.getElementById('presetName').value;
    const newDate = document.getElementById('presetDate').value;
    const newTime = document.getElementById('presetTime').value;

    if (!newName || !newDate || !newTime) {
        showMessage('All preset fields are required!', 'error');
        return;
    }

    const [year, month, day] = newDate.split('-');
    const [hour, minute] = newTime.split(':');
    const newValue = `${year}-${month}-${day}-${hour}-${minute}`;

    const formData = new FormData();
    formData.append('originalValue', originalValue);
    formData.append('newName', newName);
    formData.append('newValue', newValue);

    fetch('/api/updatePreset', { method: 'POST', body: new URLSearchParams(formData) })
        .then(response => response.text())
        .then(data => {
            showMessage(data, 'success');
            resetPresetForm();
            fetchAndApplyPresets();
        })
        .catch(error => showMessage('Error updating preset', 'error'));
}


function resetPresetForm() {
    document.getElementById('presetName').value = '';
    document.getElementById('presetDate').value = '';
    document.getElementById('presetTime').value = '';
    document.getElementById('editingPresetValue').value = '';
    document.getElementById('addPresetBtn').textContent = 'Add to Presets';
}

function deletePreset() {
    const select = document.getElementById('presetDateSelect');
    const selectedOption = select.options[select.selectedIndex];

    if (!selectedOption || !selectedOption.value || !selectedOption.parentElement || selectedOption.parentElement.label !== 'Custom Jumps') {
        showMessage('Please select a custom preset to delete.', 'error');
        return;
    }

    const presetValue = selectedOption.value;
    showCustomConfirm(`Delete preset "${selectedOption.textContent}"?`, () => {
        const formData = new FormData();
        formData.append('value', presetValue);

        fetch('/api/deletePreset', { method: 'POST', body: new URLSearchParams(formData) })
            .then(response => {
                if (!response.ok) throw new Error('Network response was not ok');
                return response.text();
            })
            .then(data => {
                showMessage(data, 'success');
                fetchAndApplyPresets().then(fetchSettings);
            })
            .catch(error => showMessage('Error deleting preset!', 'error'));
    });
}

function clearPresets() {
    fetch('/api/clearPresets', { method: 'POST' })
        .then(response => {
            if (!response.ok) throw new Error('Network response was not ok');
            return response.text();
        })
        .then(data => {
            showMessage(data, 'success');
            fetchAndApplyPresets().then(fetchSettings);
        })
        .catch(error => showMessage('Error clearing presets!', 'error'));
}

function syncNtp() {
    const btn = document.getElementById('syncNtpBtn');
    showLoading('syncNtpBtn', true);
    fetch('/api/syncNtp', { method: 'POST' })
        .then(response => response.text())
        .then(data => {
            showMessage(data, 'success');
            setTimeout(fetchTime, 1000);
        })
        .catch(error => showMessage('Error requesting NTP sync!', 'error'))
        .finally(() => showLoading('syncNtpBtn', false));
}

function fetchAndApplyPresets() {
    return fetch('/api/getPresets')
        .then(response => response.json())
        .then(presets => {
            const select = document.getElementById('presetDateSelect');
            const existingGroup = select.querySelector('optgroup[label="Custom Jumps"]');
            if (existingGroup) {
                existingGroup.remove();
            }

            if (presets.length > 0) {
                const optgroup = document.createElement('optgroup');
                optgroup.label = 'Custom Jumps';
                presets.forEach(preset => {
                    const option = document.createElement('option');
                    option.value = preset.value;
                    option.textContent = preset.name;
                    optgroup.appendChild(option);
                });
                select.appendChild(optgroup);
            }
        })
        .catch(error => showMessage('Error loading custom presets!', 'error'));
}

function updateLastDepartedDisplay() {
    const year = document.getElementById('lastTimeDepartedYear').textContent;
    const month = parseInt(document.getElementById('lastTimeDepartedMonth').textContent, 10);
    const day = document.getElementById('lastTimeDepartedDay').textContent;
    let hour = parseInt(document.getElementById('lastTimeDepartedHour').textContent, 10);
    const minute = parseInt(document.getElementById('lastTimeDepartedMinute').textContent, 10);

    if(!year || !month || !day || isNaN(hour) || isNaN(minute)) return;

    const months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
    const monthStr = months[month - 1];

    const hourStr = hour.toString().padStart(2, '0');
    const displayString = `${monthStr} ${day} ${year} ${hourStr}:${minute.toString().padStart(2, '0')}`;
    document.getElementById('lastTimeDepartedDisplay').textContent = displayString;
}

function updateHeaderClocks(presentTimeRaw, destinationTimeHeader, lastDepartedTimeRaw) {
    const months = ["JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"];
    const is24h = document.getElementById('displayFormat24h').checked;

    const populateHeaderRow = (prefix, formattedTimeInfo, yearOverride = null) => {
        const timeParts = formattedTimeInfo.time.split(' ');
        const [hour, minute, second] = timeParts[0].split(':');
        const ampm = timeParts.length > 1 ? timeParts[1] : '';

        const dateParts = formattedTimeInfo.date.split('/');
        const monthNum = parseInt(dateParts[0], 10);
        const day = dateParts[1];
        const year = dateParts[2];

        document.getElementById(`header-${prefix}-month`).textContent = months[monthNum - 1];
        document.getElementById(`header-${prefix}-day`).textContent = day;
        document.getElementById(`header-${prefix}-year`).textContent = yearOverride || year;

        document.getElementById(`header-${prefix}-hour`).textContent = hour;
        document.getElementById(`header-${prefix}-minute`).textContent = minute;
        document.getElementById(`header-${prefix}-second`).textContent = second;

        const ampmTextElement = document.getElementById(`header-${prefix}-ampm`);
        if (ampmTextElement) {
            if (is24h) {
                ampmTextElement.textContent = '';
            } else {
                ampmTextElement.textContent = ampm;
            }
        }
    };

    const presentTzIndex = parseInt(document.getElementById('presentTimezoneSelect').value, 10) || 0;
    const presentUnixTimestamp = presentTimeRaw.getTime() / 1000;
    const formattedPresentTime = formatDateTimeInTimezone(presentUnixTimestamp, presentTzIndex, is24h);
    populateHeaderRow('pres', formattedPresentTime);

    const destTzIndex = parseInt(document.getElementById('destinationTimezoneSelect').value, 10) || 0;
    const destinationTimeWithYear = new Date(presentTimeRaw.getTime());
    destinationTimeWithYear.setFullYear(document.getElementById('destinationYear').value);

    const formattedDestTimeForDisplay = formatDateTimeInTimezone(destinationTimeWithYear.getTime() / 1000, destTzIndex, is24h);
    populateHeaderRow('dest', formattedDestTimeForDisplay, document.getElementById('destinationYear').value);


    const lastYear = document.getElementById('lastTimeDepartedYear').textContent;
    const lastMonth = parseInt(document.getElementById('lastTimeDepartedMonth').textContent, 10) - 1;
    const lastDay = document.getElementById('lastTimeDepartedDay').textContent;
    const lastHour = document.getElementById('lastTimeDepartedHour').textContent;
    const lastMinute = parseInt(document.getElementById('lastTimeDepartedMinute').textContent, 10);
    const lastDepartedTime = new Date(lastYear, lastMonth, lastDay, lastHour, lastMinute);

    const lastDepartedUnixTimestamp = lastDepartedTime.getTime() / 1000;

    const formattedLastDepartedTime = formatDateTimeInTimezone(lastDepartedUnixTimestamp, presentTzIndex, is24h);
    populateHeaderRow('last', formattedLastDepartedTime);
}

function showMessage(message, type = 'success', duration = 3000) {
    const banner = document.getElementById('messageBanner');
    if (!banner) return;
    banner.textContent = message;
    banner.className = 'message-banner ' + type;
    banner.style.visibility = 'visible';
    banner.style.opacity = '1';
    setTimeout(() => {
        banner.style.opacity = '0';
        setTimeout(() => {
            banner.style.visibility = 'hidden';
            banner.textContent = '';
            banner.className = 'message-banner';
        }, 500);
    }, duration);
}

function showLoading(buttonId, isLoading) {
    const button = document.getElementById(buttonId);
    if (!button) return;
    if (isLoading) {
        button.dataset.originalText = button.textContent;
        button.textContent = 'Saving...';
        button.disabled = true;
    } else {
        button.textContent = button.dataset.originalText || 'Submit';
        button.disabled = false;
    }
}

function validateInput(inputElement) {
    const validationMessageSpan = document.getElementById(inputElement.id + 'Validation');
    if (inputElement.type === 'number') {
        const numValue = parseInt(inputElement.value, 10);
        const min = parseInt(inputElement.min, 10);
        const max = parseInt(inputElement.max, 10);
        if (isNaN(numValue) || numValue < min || numValue > max) {
            inputElement.classList.add('invalid');
            if (validationMessageSpan) validationMessageSpan.textContent = `Value must be between ${min} and ${max}.`;
        } else {
            inputElement.classList.remove('invalid');
            if (validationMessageSpan) validationMessageSpan.textContent = '';
        }
    }
    validateAllNumberInputs();
}

function validateAllNumberInputs() {
    anyInputInvalid = false;
    const inputs = document.querySelectorAll('.container input[type="number"]');
    inputs.forEach(input => {
        if(input.classList.contains('invalid')){
            anyInputInvalid = true;
        }
    });
    updateSaveButtonState();
}

function applyTheme(themeIndex) {
    const themeClasses = ['theme-time-circuits', 'theme-outatime', 'theme-88mph', 'theme-plutonium-glow', 'theme-mr-fusion', 'theme-clock-tower'];
    document.body.classList.remove(...themeClasses);
    if(themeIndex >= 0 && themeIndex < themeClasses.length) {
        document.body.classList.add(themeClasses[themeIndex]);
    }
    setSettingsChanged(true);
}

function updateWifiStrengthIndicator(rssi) {
    const wifiIndicator = document.getElementById('wifiStrength');
    wifiIndicator.className = 'wifi-indicator';
    if (rssi >= -67) wifiIndicator.classList.add('strength-4');
    else if (rssi >= -70) wifiIndicator.classList.add('strength-3');
    else if (rssi >= -80) wifiIndicator.classList.add('strength-2');
    else wifiIndicator.classList.add('strength-1');
}

function fetchStatus() {
    fetch('/api/status')
        .then(response => response.json())
        .then(data => {
            updateWifiStrengthIndicator(data.rssi);
            document.getElementById('wifiSSID').textContent = data.ssid || 'N/A';
        })
        .catch(error => console.error('Error fetching status:', error));
}

function fetchTime() {
    fetchStatus();
    fetch('/api/time')
        .then(response => response.json())
        .then(data => {
            document.getElementById('timeSyncStatus').textContent = data.timeSynchronized ? 'Yes' : 'No';
            document.getElementById('lastSyncTime').textContent = data.lastSyncTime;
            document.getElementById('lastNtpServer').textContent = data.lastNtpServer;
            const is24h = document.getElementById('displayFormat24h').checked;

            if (data.unixTime && timezoneOptions.length > 0) {
                const presentTzIndex = parseInt(document.getElementById('presentTimezoneSelect').value, 10) || 0;
                const presentTime = new Date(data.unixTime * 1000);

                const destinationTimeHeader = new Date(data.unixTime * 1000);
                destinationTimeHeader.setFullYear(document.getElementById('destinationYear').value);

                const lastYear = document.getElementById('lastTimeDepartedYear').textContent;
                const lastMonth = parseInt(document.getElementById('lastTimeDepartedMonth').textContent, 10) - 1;
                const lastDay = document.getElementById('lastTimeDepartedDay').textContent;
                const lastHour = document.getElementById('lastTimeDepartedHour').textContent;
                const lastMinute = parseInt(document.getElementById('lastTimeDepartedMinute').textContent, 10);
                const lastDepartedTime = new Date(lastYear, lastMonth, lastDay, lastHour, lastMinute);

                updateHeaderClocks(presentTime, destinationTimeHeader, lastDepartedTime);

                const currentMinutes = presentTime.getHours() * 60 + presentTime.getMinutes();
                const totalDayMinutes = 24 * 60;
                const markerPosition = (currentMinutes / totalDayMinutes) * 100;
                document.getElementById('currentTimeMarker').style.left = `${markerPosition}%`;
            }
        })
        .catch(error => console.error('Error fetching time:', error));
}

function fetchTimezones() {
    return fetch('/api/timezones')
        .then(response => response.json())
        .then(data => {
            timezoneOptions = [];
            const presentSelect = document.getElementById('presentTimezoneSelect');
            const destinationSelect = document.getElementById('destinationTimezoneSelect');
            [presentSelect, destinationSelect].forEach(sel => { if(sel) sel.innerHTML = ''; });
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
                if(presentSelect) presentSelect.appendChild(optgroup.cloneNode(true));
                if(destinationSelect) destinationSelect.appendChild(optgroup.cloneNode(true));
            }
        })
        .catch(error => showMessage('Error loading time zones!', 'error'));
}

function formatDateTimeInTimezone(unixTimestamp, timezoneIndex, is24HourFormat) {
    if (!timezoneOptions || !timezoneOptions[timezoneIndex]) return { time: "--:--:--", date: "--/--/----" };
    const tzIANA = timezoneOptions[timezoneIndex].ianaTzName;
    const dateObj = new Date(unixTimestamp * 1000);
    const timeOptions = { hour: '2-digit', minute: '2-digit', second: '2-digit', hour12: !is24HourFormat, timeZone: tzIANA };
    const dateOptions = { year: 'numeric', month: '2-digit', day: '2-digit', timeZone: tzIANA };
    try {
        return {
            time: dateObj.toLocaleTimeString('en-US', timeOptions),
            date: dateObj.toLocaleDateString('en-US', dateOptions)
        };
    } catch (e) {
        console.error("Error formatting time for timezone", tzIANA, e);
        return { time: "Error", date: "Error" };
    }
}

function fetchSettings() {
    return fetch('/api/settings')
        .then(response => response.json())
        .then(data => {
            document.getElementById('destinationYear').value = data.destinationYear;

            document.getElementById('lastTimeDepartedHour').textContent = data.lastTimeDepartedHour.toString().padStart(2, '0');
            document.getElementById('lastTimeDepartedMinute').textContent = data.lastTimeDepartedMinute.toString().padStart(2, '0');
            document.getElementById('lastTimeDepartedMonth').textContent = data.lastTimeDepartedMonth.toString().padStart(2, '0');
            document.getElementById('lastTimeDepartedDay').textContent = data.lastTimeDepartedDay.toString().padStart(2, '0');
            document.getElementById('lastTimeDepartedYear').textContent = data.lastTimeDepartedYear;
            updateLastDepartedDisplay();

            const depHour = data.departureHour.toString().padStart(2, '0');
            const depMin = data.departureMinute.toString().padStart(2, '0');
            document.getElementById('departureTime').value = `${depHour}:${depMin}`;

            const arrHour = data.arrivalHour.toString().padStart(2, '0');
            const arrMin = data.arrivalMinute.toString().padStart(2, '0');
            document.getElementById('arrivalTime').value = `${arrHour}:${arrMin}`;

            ['brightness', 'notificationVolume', 'timeTravelAnimationInterval', 'presetCycleInterval', 'timeTravelAnimationDuration'].forEach(id => {
                const element = document.getElementById(id);
                if (element) {
                    element.value = data[id];
                    element.dispatchEvent(new Event('input'));
                }
            });

            ['timeTravelSoundToggle', 'displayFormat24h'].forEach(id => {
                const element = document.getElementById(id);
                if (element) {
                    element.checked = data[id];
                }
            });
            document.getElementById('powerOfLoveBtn').classList.toggle('button-active', data.greatScottSoundToggle);

            document.getElementById('presentTimezoneSelect').value = data.presentTimezoneIndex;
            document.getElementById('destinationTimezoneSelect').value = data.destinationTimezoneIndex;

            const animationStyleSelect = document.getElementById('animationStyleSelect');
            if (animationStyleSelect) {
                animationStyleSelect.value = data.animationStyle;
            }

            const ltdYear = data.lastTimeDepartedYear.toString().padStart(4, '0');
            const ltdMonth = data.lastTimeDepartedMonth.toString().padStart(2, '0');
            const ltdDay = data.lastTimeDepartedDay.toString().padStart(2, '0');
            const ltdHour = data.lastTimeDepartedHour.toString().padStart(2, '0');
            const ltdMinute = data.lastTimeDepartedMinute.toString().padStart(2, '0');
            const savedValue = `${ltdYear}-${ltdMonth}-${ltdDay}-${ltdHour}-${ltdMinute}`;
            const select = document.getElementById('presetDateSelect');
            let found = false;
            for (let i = 0; i < select.options.length; i++) {
                if (select.options[i].value === savedValue) {
                    select.selectedIndex = i;
                    found = true;
                    break;
                }
            }
            if(!found) select.selectedIndex = 0;
            select.dispatchEvent(new Event('change'));

            buildThemeSelector(data.theme);
            updateSleepScheduleVisual();
            validateAllNumberInputs();
            setupPresetCycler();

            setSettingsChanged(false);
        })
        .catch(error => showMessage('Error loading settings!', 'error'));
}

function saveSettings() {
    showLoading('saveSettingsBtn', true);
    const formData = new FormData();
    formData.append('destinationYear', document.getElementById('destinationYear').value);

    formData.append('destinationTimezoneIndex', document.getElementById('destinationTimezoneSelect').value);
    formData.append('lastTimeDepartedHour', document.getElementById('lastTimeDepartedHour').textContent);
    formData.append('lastTimeDepartedMinute', document.getElementById('lastTimeDepartedMinute').textContent);
    formData.append('lastTimeDepartedMonth', document.getElementById('lastTimeDepartedMonth').textContent);
    formData.append('lastTimeDepartedDay', document.getElementById('lastTimeDepartedDay').textContent);
    formData.append('lastTimeDepartedYear', document.getElementById('lastTimeDepartedYear').textContent);

    const departureTime = document.getElementById('departureTime').value.split(':');
    formData.append('departureHour', departureTime[0]);
    formData.append('departureMinute', departureTime[1]);

    const arrivalTime = document.getElementById('arrivalTime').value.split(':');
    formData.append('arrivalHour', arrivalTime[0]);
    formData.append('arrivalMinute', arrivalTime[1]);

    ['brightness', 'notificationVolume', 'timeTravelAnimationInterval', 'presetCycleInterval', 'timeTravelAnimationDuration'].forEach(id => {
        formData.append(id, document.getElementById(id).value);
    });
    ['timeTravelSoundToggle', 'displayFormat24h'].forEach(id => {
        formData.append(id, document.getElementById(id).checked);
    });
    formData.append('greatScottSoundToggle', document.getElementById('powerOfLoveBtn').classList.contains('button-active'));
    formData.append('theme', document.querySelector('.theme-swatch.selected').parentElement.dataset.themeIndex);
    formData.append('presentTimezoneIndex', document.getElementById('presentTimezoneSelect').value);

    formData.append('animationStyle', document.getElementById('animationStyleSelect').value);

    fetch('/api/saveSettings', { method: 'POST', body: new URLSearchParams(formData) })
        .then(response => response.text())
        .then(data => {
            showMessage(data, 'success');
            setSettingsChanged(false);
            const settingGroups = document.querySelectorAll('.setting-group');
            settingGroups.forEach(el => {
                el.classList.add('highlight-saved');
                el.addEventListener('animationend', () => {
                    el.classList.remove('highlight-saved');
                }, { once: true });
            });
            startTimeTravelSequence();
        })
        .catch(error => showMessage('Error saving settings!', 'error'))
        .finally(() => {
            showLoading('saveSettingsBtn', false);
        });
}

function startTimeTravelSequence() {
    fetch('/api/timeTravel');
    document.body.classList.add('time-travel-active');
    setTimeout(() => {
        document.body.classList.remove('time-travel-active');
    }, 3000);
}

function showCustomConfirm(msg, callback) {
    const confirmModal = document.getElementById('customConfirm');
    document.getElementById('confirmMsg').textContent = msg;
    confirmModal.style.display = 'block';
    document.getElementById('confirmYes').onclick = () => {
        callback();
        confirmModal.style.display = 'none';
    }
    document.getElementById('confirmNo').onclick = () => confirmModal.style.display = 'none';
}

function resetToDefaults() {
    showLoading('resetDefaultsBtn', true);
    fetch('/api/resetSettings', { method: 'POST' })
        .then(response => { if (!response.ok) throw new Error('Network response was not ok'); return response.text(); })
        .then(() => {
            showMessage('Settings reset to defaults!', 'success');
            fetchAndApplyPresets().then(fetchSettings);
        })
        .catch(error => showMessage('Error resetting settings!', 'error'))
        .finally(() => showLoading('resetDefaultsBtn', false));
}

function resetWifi() {
    showLoading('resetWifiBtn', true);
    fetch('/api/resetWifi', { method: 'POST' })
        .then(response => { if (!response.ok) throw new Error('Network response was not ok'); return response.text(); })
        .then(() => showMessage('WiFi credentials reset. Device restarting...', 'success', 10000))
        .catch(error => showMessage('Error resetting WiFi!', 'error'))
        .finally(() => showLoading('resetWifiBtn', false));
}

function testSound() {
    fetch('/api/testSound').catch(error => showMessage('Error testing sound!', 'error'));
}

function updateSleepScheduleVisual() {
    const container = document.getElementById('sleepScheduleVisualContainer');
    if (!container) return;

    const [depHour, depMin] = document.getElementById('departureTime').value.split(':').map(Number);
    const [arrHour, arrMin] = document.getElementById('arrivalTime').value.split(':').map(Number);

    document.getElementById('sleepTimeDisplayLeft').textContent = `${arrHour.toString().padStart(2, '0')}:${arrMin.toString().padStart(2, '0')}`;
    document.getElementById('sleepTimeDisplayRight').textContent = `${depHour.toString().padStart(2, '0')}:${depMin.toString().padStart(2, '0')}`;

    const existingBars = container.querySelectorAll('.sleep-schedule-bar');
    existingBars.forEach(bar => bar.remove());

    const depTotalMinutes = depHour * 60 + depMin;
    const arrTotalMinutes = arrHour * 60 + arrMin;
    const totalDayMinutes = 24 * 60;

    const createBar = (left, width) => {
        const bar = document.createElement('div');
        bar.className = 'sleep-schedule-bar';
        bar.style.left = `${left}%`;
        bar.style.width = `${width}%`;
        container.insertBefore(bar, container.firstChild);
    };

    if (depTotalMinutes < arrTotalMinutes) {
        const left = (depTotalMinutes / totalDayMinutes) * 100;
        const width = ((arrTotalMinutes - depTotalMinutes) / totalDayMinutes) * 100;
        if (width > 0) createBar(left, width);
    } else if (depTotalMinutes > arrTotalMinutes) {
        const left1 = (depTotalMinutes / totalDayMinutes) * 100;
        const width1 = ((totalDayMinutes - depTotalMinutes) / totalDayMinutes) * 100;
        if (width1 > 0) createBar(left1, width1);

        const left2 = 0;
        const width2 = (arrTotalMinutes / totalDayMinutes) * 100;
        if (width2 > 0) createBar(left2, width2);
    }
}


function buildThemeSelector(selectedTheme) {
    const themeSelector = document.getElementById('themeSelector');
    themeSelector.innerHTML = '';
    const themes = [
        { name: 'Time Circuits', color: '#00ff00' },
        { name: 'OUTATIME', color: '#ff0000' },
        { name: '88 MPH', color: '#00ffff' },
        { name: 'Plutonium Glow', color: '#a0ffa0' },
        { name: 'Mr. Fusion', color: '#ff8c00' },
        { name: 'Clock Tower', color: '#daa520' }
    ];
    themes.forEach((theme, index) => {
        const option = document.createElement('div');
        option.className = 'theme-option';
        option.dataset.themeIndex = index;
        const swatch = document.createElement('div');
        swatch.className = 'theme-swatch';
        swatch.style.backgroundColor = theme.color;
        const name = document.createElement('span');
        name.className = 'theme-name';
        name.textContent = theme.name;
        option.appendChild(swatch);
        option.appendChild(name);
        if (index === selectedTheme) swatch.classList.add('selected');
        option.onclick = () => {
            if (!swatch.classList.contains('selected')) {
                document.querySelector('.theme-swatch.selected').classList.remove('selected');
                swatch.classList.add('selected');
                applyTheme(index);
            }
        };
        themeSelector.appendChild(option);
    });
    applyTheme(selectedTheme);
}

function clearPreferences() {
    showCustomConfirm(
        'Are you sure you want to clear all saved settings from the device? This cannot be undone and will reset the device to its default configuration.',
        () => {
            fetch('/api/clearPreferences', { method: 'POST' })
                .then(response => {
                    if (!response.ok) {
                        throw new Error('Network response was not ok');
                    }
                    return response.text();
                })
                .then(text => {
                    showMessage(text, 'success', 5000);
                    setTimeout(() => {
                        fetchAndApplyPresets().then(fetchSettings);
                    }, 1000);
                })
                .catch(error => {
                    showMessage('Error clearing preferences!', 'error');
                    console.error('Error:', error);
                });
        }
    );
}