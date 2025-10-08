"""
The Back to the Future Time Circuits integration.

This component provides a Home Assistant integration for the ESP32-based
Back to the Future Time Circuits clock replica. It sets up the necessary
platforms (sensors, switches, etc.), manages the device connection, and
registers custom services for interacting with the clock.
"""
from __future__ import annotations

import asyncio
import logging
from datetime import timedelta

from homeassistant.components import mqtt
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers import device_registry as dr
from homeassistant.helpers.typing import ConfigType
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator

from .const import DOMAIN
from .device import BTTFTimeCircuitsDevice

_LOGGER = logging.getLogger(__name__)

PLATFORMS: list[str] = [
    "button",
    "media_player",
    "notify",
    "number",
    "sensor",
    "select",
    "switch",
    "text",
    "update",
]


async def async_setup(hass: HomeAssistant, config: ConfigType) -> bool:
    """
    Set up the BTTF Time Circuits component.

    This is the entry point for the integration. It initializes the domain
    data dictionary in Home Assistant.

    Args:
        hass: The Home Assistant instance.
        config: The configuration for the component.

    Returns:
        True if the setup was successful.
    """
    _LOGGER.debug("async_setup")
    hass.data.setdefault(DOMAIN, {})
    return True


async def async_migrate_entry(hass: HomeAssistant, config_entry: ConfigEntry) -> bool:
    """
    Migrate an old config entry.

    This function is called by Home Assistant to migrate a configuration entry
    from an older version to the current version.

    Args:
        hass: The Home Assistant instance.
        config_entry: The configuration entry to migrate.

    Returns:
        True if the migration was successful.
    """
    _LOGGER.debug("Migrating from version %s", config_entry.version)

    if config_entry.version == 1:
        # No migration needed
        config_entry.version = 2
        hass.config_entries.async_update_entry(config_entry, data=config_entry.data)

    _LOGGER.info("Migration to version %s successful", config_entry.version)
    return True


async def options_update_listener(hass: HomeAssistant, entry: ConfigEntry) -> None:
    """
    Handle an options update.

    This is called when the user updates the configuration options for the
    integration. It reloads the config entry to apply the changes.

    Args:
        hass: The Home Assistant instance.
        entry: The configuration entry that was updated.
    """
    await hass.config_entries.async_reload(entry.entry_id)


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """
    Set up BTTF Time Circuits from a config entry.

    This function is called when a config entry is created or loaded. It
    initializes the device communication, sets up the platforms, and registers
    the custom services for the integration.

    Args:
        hass: The Home Assistant instance.
        entry: The configuration entry.

    Returns:
        True if the entry was set up successfully.
    """
    _LOGGER.debug("async_setup_entry")
    device_id = entry.data.get("device_id")

    if not device_id:
        _LOGGER.error("device_id is not set")
        return False

    device = BTTFTimeCircuitsDevice(hass, device_id)
    hass.data[DOMAIN][entry.entry_id] = device

    async def async_update_data():
        """Update data via MQTT."""
        command_topic = f"{device.base_topic}/command"
        await mqtt.async_publish(hass, command_topic, "STATE", 1, False)

    coordinator = DataUpdateCoordinator(
        hass,
        _LOGGER,
        name=f"{DOMAIN}_{device_id}_coordinator",
        update_method=async_update_data,
        update_interval=timedelta(seconds=60),
    )

    device.coordinator = coordinator
    await coordinator.async_config_entry_first_refresh()

    device_registry = dr.async_get(hass)
    device_registry.async_get_or_create(
        config_entry_id=entry.entry_id,
        **device.device_info,
    )

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
    hass.services.async_register(
        DOMAIN,
        "set_destination_time",
        device.async_handle_set_destination_time,
    )
    hass.services.async_register(
        DOMAIN,
        "set_present_time",
        device.async_handle_set_present_time,
    )
    hass.services.async_register(
        DOMAIN,
        "set_last_departed_time",
        device.async_handle_set_last_departed_time,
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
    entry.async_on_unload(
        lambda: hass.services.async_remove(DOMAIN, "set_destination_time")
    )
    entry.async_on_unload(
        lambda: hass.services.async_remove(DOMAIN, "set_present_time")
    )
    entry.async_on_unload(
        lambda: hass.services.async_remove(DOMAIN, "set_last_departed_time")
    )
    hass.services.async_register(
        DOMAIN,
        "time_travel",
        device.async_handle_time_travel,
    )
    entry.async_on_unload(
        lambda: hass.services.async_remove(DOMAIN, "time_travel")
    )
    entry.async_on_unload(entry.add_update_listener(options_update_listener))

    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """
    Unload a config entry.

    This function is called when a config entry is being removed or reloaded.
    It unloads the platforms and cleans up the resources associated with the
    config entry.

    Args:
        hass: The Home Assistant instance.
        entry: The configuration entry to unload.

    Returns:
        True if the entry was unloaded successfully.
    """
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][entry.entry_id]
    if device.coordinator:
        # You might want to cancel any ongoing updates
        device.coordinator.async_update_listeners = lambda: None

    if unloaded := await hass.config_entries.async_unload_platforms(entry, PLATFORMS):
        hass.data[DOMAIN].pop(entry.entry_id)

    return unloaded