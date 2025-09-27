"""The Back to the Future Time Circuits integration."""
from __future__ import annotations

import logging

from homeassistant.components import mqtt
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers.typing import ConfigType

from .const import DOMAIN

_LOGGER = logging.getLogger(__name__)

PLATFORMS: list[str] = [
    "button",
    "media_player",
    "notify",
    "number",
    "sensor",
    "switch",
    "text",
    "update",
]


class BTTFTimeCircuitsDevice:
    """A wrapper for a BTTF Time Circuits device."""

    def __init__(self, hass: HomeAssistant, device_id: str) -> None:
        """Initialize the device."""
        self.hass = hass
        self.device_id = device_id
        self.base_topic = f"bttf_time_circuits/{device_id}"

    async def async_handle_set_status_display(self, call: ServiceCall) -> None:
        """Handle the set_status_display service call."""
        for key, value in call.data.items():
            topic_key = (
                key.replace("destination_", "dest_")
                .replace("present_", "pres_")
                .replace("last_departed_", "last_")
            )
            command_topic = f"{self.base_topic}/{topic_key}/command"
            await mqtt.async_publish(self.hass, command_topic, str(value), 1, False)

    async def async_handle_run_sequence(self, call: ServiceCall) -> None:
        """Handle the run_sequence service call."""
        sequence_json = call.data.get("sequence")
        command_topic = f"{self.base_topic}/sequencer/command"
        await mqtt.async_publish(self.hass, command_topic, sequence_json, 1, False)


async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    """Set up the BTTF Time Circuits component."""
    _LOGGER.debug("async_setup")
    hass.data.setdefault(DOMAIN, {})
    return True


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Set up BTTF Time Circuits from a config entry."""
    _LOGGER.debug("async_setup_entry")
    device_id = entry.data.get("device_id")

    if not device_id:
        _LOGGER.error("device_id is not set")
        return False

    device = BTTFTimeCircuitsDevice(hass, device_id)
    hass.data[DOMAIN][entry.entry_id] = device

    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)

    hass.services.async_register(
        DOMAIN,
        "set_status_display",
        device.async_handle_set_status_display,
    )
    hass.services.async_register(
        DOMAIN,
        "run_sequence",
        device.async_handle_run_sequence,
    )

    entry.async_on_unload(
        lambda: hass.services.async_remove(DOMAIN, "set_status_display")
    )
    entry.async_on_unload(lambda: hass.services.async_remove(DOMAIN, "run_sequence"))

    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    if unloaded := await hass.config_entries.async_unload_platforms(entry, PLATFORMS):
        hass.data[DOMAIN].pop(entry.entry_id)
    return unloaded