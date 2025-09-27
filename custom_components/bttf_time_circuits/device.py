"""The Back to the Future Time Circuits device."""
from __future__ import annotations

from typing import TYPE_CHECKING

from homeassistant.components import mqtt
from homeassistant.components.media_player import DOMAIN as MEDIA_PLAYER_DOMAIN
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers import entity_registry as er
from homeassistant.helpers.device_registry import DeviceInfo

from .const import DOMAIN

if TYPE_CHECKING:
    from .media_player import BTTFTimeCircuitsMediaPlayer


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

    async def async_discover_entities(self) -> None:
        """Send a discovery message to the device."""
        command_topic = f"{self.base_topic}/discover/command"
        await mqtt.async_publish(self.hass, command_topic, "ON", 1, False)

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