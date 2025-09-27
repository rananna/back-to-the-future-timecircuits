"""The Back to the Future Time Circuits device."""
from __future__ import annotations

from typing import TYPE_CHECKING

from homeassistant.components import mqtt
from homeassistant.components.media_player import DOMAIN as MEDIA_PLAYER_DOMAIN
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers import entity_registry as er
from homeassistant.helpers.device_registry import DeviceInfo
import asyncio
from datetime import datetime

from homeassistant.util import dt as dt_util

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

    async def _async_set_time(self, prefix: str, dt_obj) -> None:
        """Set a time display (destination, present, or last departed)."""
        # Month: JAN, FEB, etc.
        month = dt_obj.strftime("%b").upper()
        # Day: 01-31
        day = dt_obj.strftime("%d")
        # Year: 4 digits
        year = dt_obj.strftime("%Y")
        # Time: HHMM
        time = dt_obj.strftime("%H%M")

        data = {
            f"{prefix}_month": month,
            f"{prefix}_day": day,
            f"{prefix}_year": year,
            f"{prefix}_time": time,
        }

        for key, value in data.items():
            command_topic = f"{self.base_topic}/{key}/command"
            await mqtt.async_publish(self.hass, command_topic, str(value), 1, False)

    async def async_handle_set_destination_time(self, call: ServiceCall) -> None:
        """Handle the set_destination_time service call."""
        if dt_obj := call.data.get("datetime"):
            # Ensure datetime is timezone-aware
            aware_dt = dt_util.as_local(dt_obj)
            await self._async_set_time("dest", aware_dt)

    async def async_handle_set_present_time(self, call: ServiceCall) -> None:
        """Handle the set_present_time service call."""
        if dt_obj := call.data.get("datetime"):
            aware_dt = dt_util.as_local(dt_obj)
            await self._async_set_time("pres", aware_dt)

    async def async_handle_set_last_departed_time(self, call: ServiceCall) -> None:
        """Handle the set_last_departed_time service call."""
        if dt_obj := call.data.get("datetime"):
            aware_dt = dt_util.as_local(dt_obj)
            await self._async_set_time("last", aware_dt)

    async def _async_get_present_time_as_datetime(self) -> datetime | None:
        """Read the present time text entities and return a datetime object."""
        try:
            month_str = self.hass.states.get(f"text.bttf_time_circuits_{self.device_id}_pres_month").state
            day_str = self.hass.states.get(f"text.bttf_time_circuits_{self.device_id}_pres_day").state
            year_str = self.hass.states.get(f"text.bttf_time_circuits_{self.device_id}_pres_year").state
            time_str = self.hass.states.get(f"text.bttf_time_circuits_{self.device_id}_pres_time").state

            if not all([month_str, day_str, year_str, time_str]):
                return None

            month = datetime.strptime(month_str, "%b").month
            day = int(day_str)
            year = int(year_str)
            hour = int(time_str[:2])
            minute = int(time_str[2:])

            return datetime(year, month, day, hour, minute)
        except (AttributeError, ValueError, TypeError):
            # Handle cases where entities don't exist or have invalid state
            return None

    async def async_handle_time_travel(self, call: ServiceCall) -> None:
        """Handle the time_travel service call."""
        destination_dt = call.data.get("datetime")
        if not destination_dt:
            return

        # 1. Get the current present time, this will become the last departed time
        last_departed_dt = await self._async_get_present_time_as_datetime()
        if not last_departed_dt:
            # Fallback to now() if we can't read the display
            last_departed_dt = dt_util.now()

        # 2. Set the destination and last departed displays
        aware_dest_dt = dt_util.as_local(destination_dt)
        aware_last_dt = dt_util.as_local(last_departed_dt)

        await self._async_set_time("dest", aware_dest_dt)
        await self._async_set_time("last", aware_last_dt)

        # Give the device a moment to update displays before animation
        await asyncio.sleep(0.5)

        # 3. Trigger the time travel animation
        command_topic = f"{self.base_topic}/time_travel/command"
        await mqtt.async_publish(self.hass, command_topic, "PRESS", 1, False)