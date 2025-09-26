"""The Back to the Future Time Circuits integration."""
from __future__ import annotations

import voluptuous as vol
from homeassistant.components import mqtt
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers import config_validation as cv
from homeassistant.helpers.typing import ConfigType

from .const import DOMAIN

PLATFORMS: list[str] = [
    "button",
    "media_player",
    "notify",
    "number",
    "sensor",
    "switch",
    "text",
    "time",
    "update",
]

# Service definitions
SERVICE_SET_STATUS_DISPLAY = "set_status_display"
SERVICE_RUN_SEQUENCE = "run_sequence"

# Define the schema for the set_status_display service
SET_STATUS_DISPLAY_SCHEMA = vol.Schema(
    {
        vol.Optional("destination_month"): cv.string,
        vol.Optional("destination_day"): cv.string,
        vol.Optional("destination_year"): cv.string,
        vol.Optional("destination_time"): cv.string,
        vol.Optional("present_month"): cv.string,
        vol.Optional("present_day"): cv.string,
        vol.Optional("present_year"): cv.string,
        vol.Optional("present_time"): cv.string,
        vol.Optional("last_departed_month"): cv.string,
        vol.Optional("last_departed_day"): cv.string,
        vol.Optional("last_departed_year"): cv.string,
        vol.Optional("last_departed_time"): cv.string,
    }
)

# Define the schema for the run_sequence service
RUN_SEQUENCE_SCHEMA = vol.Schema(
    {
        vol.Required("sequence"): cv.string,  # JSON string
    }
)


async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    """Set up the BTTF Time Circuits component."""
    hass.data.setdefault(DOMAIN, {})
    return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up BTTF Time Circuits from a config entry."""
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)

    # Placeholder for device ID
    device_id = "BTTF_TC_123456"
    base_topic = f"BTTF_TC/{device_id}"

    async def async_handle_set_status_display(call: ServiceCall) -> None:
        """Handle the set_status_display service call."""
        for key, value in call.data.items():
            topic_key = (
                key.replace("destination_", "dest_")
                .replace("present_", "pres_")
                .replace("last_departed_", "last_")
            )
            command_topic = f"{base_topic}/{topic_key}/command"
            await mqtt.async_publish(hass, command_topic, str(value), 1, False)

    async def async_handle_run_sequence(call: ServiceCall) -> None:
        """Handle the run_sequence service call."""
        sequence_json = call.data.get("sequence")
        command_topic = f"{base_topic}/sequencer/command"
        await mqtt.async_publish(hass, command_topic, sequence_json, 1, False)

    hass.services.async_register(
        DOMAIN,
        SERVICE_SET_STATUS_DISPLAY,
        async_handle_set_status_display,
        schema=SET_STATUS_DISPLAY_SCHEMA,
    )
    hass.services.async_register(
        DOMAIN,
        SERVICE_RUN_SEQUENCE,
        async_handle_run_sequence,
        schema=RUN_SEQUENCE_SCHEMA,
    )

    # Set up listeners to remove services when the config entry is unloaded.
    entry.async_on_unload(
        lambda: hass.services.async_remove(DOMAIN, SERVICE_SET_STATUS_DISPLAY)
    )
    entry.async_on_unload(
        lambda: hass.services.async_remove(DOMAIN, SERVICE_RUN_SEQUENCE)
    )

    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    # Service removal is handled by the listeners set up in async_setup_entry.
    return await hass.config_entries.async_unload_platforms(entry, PLATFORMS)