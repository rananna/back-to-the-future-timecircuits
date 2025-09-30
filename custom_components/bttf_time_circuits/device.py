"""The Back to the Future Time Circuits device."""
from __future__ import annotations

from typing import TYPE_CHECKING

from homeassistant.components import mqtt
from homeassistant.components.media_player import DOMAIN as MEDIA_PLAYER_DOMAIN
from homeassistant.core import HomeAssistant, ServiceCall
from homeassistant.helpers import entity_registry as er
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator
import asyncio
import json
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
    coordinator: DataUpdateCoordinator | None = None

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
        for key, value in call.data.items():
            if key in SERVICE_SET_STATUS_DISPLAY_FIELDS:
                topic_key = (
                    key.replace("destination_", "dest_")
                    .replace("present_", "pres_")
                    .replace("last_departed_", "last_")
                )
                command_topic = f"{self.base_topic}/{topic_key}/command"
                await mqtt.async_publish(self.hass, command_topic, str(value), 1, False)

    def _translate_sequence_command(self, command: dict) -> dict | None:
        """Translates a user-friendly sequence command into the firmware format."""

        # Mapping from user-friendly command to firmware command
        COMMAND_MAP = {
            "wait": "WAIT",
            "sound": "SOUND",
            "flash": "FLASH",
            "pulse": "PULSE",
            "fade_in": "FADE_IN",
            "fade_out": "FADE_OUT",
            "marquee": "MARQUEE",
        }

        # Mapping from user-friendly segment name to firmware segment index (0-3)
        SEGMENT_MAP = {
            "month": 0, "destination_month": 0, "present_month": 0, "last_departed_month": 0,
            "day": 1, "destination_day": 1, "present_day": 1, "last_departed_day": 1,
            "year": 2, "destination_year": 2, "present_year": 2, "last_departed_year": 2,
            "time": 3, "destination_time": 3, "present_time": 3, "last_departed_time": 3,
        }

        user_cmd = command.get("command")
        if not user_cmd or user_cmd.lower() not in COMMAND_MAP:
            # _LOGGER is defined in __init__.py, but we can't access it here.
            # Let's use the logging module directly.
            import logging
            logging.getLogger(__name__).warning("Skipping unknown sequence command: %s", user_cmd)
            return None

        firmware_cmd = {"command": COMMAND_MAP[user_cmd.lower()]}

        # Handle parameters
        if firmware_cmd["command"] in ["WAIT", "FADE_IN", "FADE_OUT"]:
            duration = command.get("duration", 1000)
            firmware_cmd["intParam"] = int(duration)

        elif firmware_cmd["command"] in ["SOUND", "MARQUEE"]:
            param = command.get("effect") or command.get("sound") or command.get("text")
            if not param:
                import logging
                logging.getLogger(__name__).warning("Command '%s' requires an 'effect' or 'text' parameter.", user_cmd)
                return None
            firmware_cmd["stringParam"] = str(param)

        elif firmware_cmd["command"] in ["FLASH", "PULSE"]:
            segment_name = command.get("segment")
            if not segment_name or segment_name.lower() not in SEGMENT_MAP:
                import logging
                logging.getLogger(__name__).warning("Command '%s' requires a valid 'segment' parameter.", user_cmd)
                return None

            firmware_cmd["targetSegment"] = SEGMENT_MAP[segment_name.lower()]
            duration = command.get("duration", 1000)
            firmware_cmd["intParam"] = int(duration)

        return firmware_cmd


    async def async_handle_run_sequence(self, call: ServiceCall) -> None:
        """Handle the run_sequence service call."""
        sequence = call.data.get("sequence")
        if not isinstance(sequence, list):
            # _LOGGER is defined in __init__.py, but we can't access it here.
            # Let's use the logging module directly.
            import logging
            logging.getLogger(__name__).error("The 'sequence' must be a list of commands.")
            return

        translated_commands = []
        for user_command in sequence:
            if not isinstance(user_command, dict):
                continue

            command_name = user_command.get("command")

            if command_name == "message":
                import logging
                logging.getLogger(__name__).warning(
                    "The 'message' command is not supported in a sequence. "
                    "Use the 'text.set_value' or 'bttf_time_circuits.set_status_display' "
                    "service to set display text directly."
                )
                continue

            if command_name == "delay":
                user_command["command"] = "wait"

            translated = self._translate_sequence_command(user_command)
            if translated:
                translated_commands.append(translated)

        if not translated_commands:
            import logging
            logging.getLogger(__name__).warning("Sequence contained no valid commands to execute.")
            return

        target_row = call.data.get("target_row", 2)
        wrapped_sequence = [
            {
                "targetRow": target_row,
                "commands": translated_commands,
            }
        ]
        payload = json.dumps(wrapped_sequence)

        command_topic = f"{self.base_topic}/sequencer/command"
        await mqtt.async_publish(self.hass, command_topic, payload, 1, False)

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