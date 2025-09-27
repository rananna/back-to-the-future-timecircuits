"""The Back to the Future Time Circuits integration."""
from __future__ import annotations

import logging

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers import device_registry as dr
from homeassistant.helpers.typing import ConfigType

from .const import DOMAIN
from .device import BTTFTimeCircuitsDevice

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

    device_registry = dr.async_get(hass)
    device_registry.async_get_or_create(
        config_entry_id=entry.entry_id,
        **device.device_info,
    )

    await device.async_discover_entities()

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
    hass.services.async_register(
        DOMAIN,
        "favorite_radio_station",
        device.async_handle_favorite_radio_station,
    )
    hass.services.async_register(
        DOMAIN,
        "clear_favorite_radio_stations",
        device.async_handle_clear_favorite_radio_stations,
    )

    entry.async_on_unload(
        lambda: hass.services.async_remove(DOMAIN, "set_status_display")
    )
    entry.async_on_unload(lambda: hass.services.async_remove(DOMAIN, "run_sequence"))
    entry.async_on_unload(
        lambda: hass.services.async_remove(DOMAIN, "favorite_radio_station")
    )
    entry.async_on_unload(
        lambda: hass.services.async_remove(DOMAIN, "clear_favorite_radio_stations")
    )

    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    if unloaded := await hass.config_entries.async_unload_platforms(entry, PLATFORMS):
        hass.data[DOMAIN].pop(entry.entry_id)
    return unloaded