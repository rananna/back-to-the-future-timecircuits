"""The Back to the Future Time Circuits integration."""
from __future__ import annotations

import json
import logging

from homeassistant.components import mqtt
from homeassistant.components.mqtt import ReceiveMessage
from homeassistant.components.media_player import DOMAIN as MEDIA_PLAYER_DOMAIN
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, ServiceCall, callback
from homeassistant.helpers import device_registry as dr, entity_registry as er
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.typing import ConfigType

from .const import DOMAIN
from .media_player import BTTFTimeCircuitsMediaPlayer

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

SERVICE_SET_STATUS_DISPLAY_FIELDS = [
    "destination_month",
    "destination_day",
    "destination_year",
    "destination_time",
    "present_month",
    "present_day",
    "present_year",
    "present_time",
    "last_departed_month",
    "last_departed_day",
    "last_departed_year",
    "last_departed_time",
]


class BTTFTimeCircuitsDevice:
    """A wrapper for a BTTF Time Circuits device."""

    device_info: DeviceInfo

    def __init__(self, hass: HomeAssistant, device_id: str) -> None:
        """Initialize the device."""
        self.hass = hass
        self.device_id = device_id
        self.base_topic = f"bttf_time_circuits/{device_id}"
        self.device_info = {
            "identifiers": {(DOMAIN, self.device_id)},
            "name": "Time Circuits",
            "manufacturer": "rananna",
            "model": "ESP32",
            "sw_version": "1.0.0",
        }

    async def async_handle_set_status_display(self, call: ServiceCall) -> None:
        """Handle the set_status_display service call."""
        for key in SERVICE_SET_STATUS_DISPLAY_FIELDS:
            if key in call.data:
                value = call.data[key]
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

    async def _async_get_media_player_entity(
        self,
    ) -> BTTFTimeCircuitsMediaPlayer | None:
        """Get the media_player entity for this device."""
        ent_reg = er.async_get(self.hass)
        entity_id = ent_reg.async_get_entity_id(
            MEDIA_PLAYER_DOMAIN, DOMAIN, f"bttf_time_circuits_{self.device_id}_media_player"
        )
        if entity_id:
            return self.hass.data[MEDIA_PLAYER_DOMAIN].get_entity(entity_id)
        return None

    async def async_handle_favorite_radio_station(self, call: ServiceCall) -> None:
        """Handle the favorite_radio_station service call."""
        if entity := await self._async_get_media_player_entity():
            await entity.async_favorite_radio_station()

    async def async_handle_clear_favorite_radio_stations(
        self, call: ServiceCall
    ) -> None:
        """Handle the clear_favorite_radio_stations service call."""
        if entity := await self._async_get_media_player_entity():
            await entity.async_clear_favorite_radio_stations()


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

    @callback
    async def async_handle_devices_cmnd(msg: ReceiveMessage):
        """Handle the devices command."""
        try:
            payload = json.loads(msg.payload)
        except json.JSONDecodeError:
            return

        if payload.get("action") == "discover":
            await mqtt.async_publish(
                hass,
                "bttf_time_circuits/devices/all/cmnd/discover",
                '{"action":"discover"}',
                1,
                False,
            )

    unsubscribe = await mqtt.async_subscribe(
        hass,
        "bttf_time_circuits/cmnd/DEVICES",
        async_handle_devices_cmnd,
        1,
    )
    entry.async_on_unload(unsubscribe)

    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """Unload a config entry."""
    if unloaded := await hass.config_entries.async_unload_platforms(entry, PLATFORMS):
        hass.data[DOMAIN].pop(entry.entry_id)
    return unloaded