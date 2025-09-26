"""MediaPlayer platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import json
from typing import Any

from homeassistant.components import mqtt
from homeassistant.components.media_player import (
    MediaPlayerDeviceClass,
    MediaPlayerEntity,
    MediaPlayerEntityFeature,
    MediaType,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

SUPPORTED_FEATURES = (
    MediaPlayerEntityFeature.PLAY_MEDIA
    | MediaPlayerEntityFeature.STOP
    | MediaPlayerEntityFeature.VOLUME_SET
    | MediaPlayerEntityFeature.SELECT_SOURCE
)

SOUND_EFFECTS = [
    "ALARM_SOUND",
    "ARRIVAL_THUD",
    "CONFIRM_ON",
    "EASTER_EGG",
    "REBOOT_SOUND",
    "REMINDER_ALERT",
    "TIME_TRAVEL_FAIL",
]


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits media player."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]
    async_add_entities([BTTFTimeCircuitsMediaPlayer(device)])


class BTTFTimeCircuitsMediaPlayer(BTTFTimeCircuitsEntity, MediaPlayerEntity):
    """Representation of a BTTF Time Circuits Media Player."""

    _attr_has_entity_name = True
    _attr_name = "Speaker"
    _attr_device_class = MediaPlayerDeviceClass.SPEAKER
    _attr_supported_features = SUPPORTED_FEATURES
    _attr_source_list = SOUND_EFFECTS

    def __init__(self, device: BTTFTimeCircuitsDevice) -> None:
        """Initialize the media player."""
        self.entity_description = None  # No entity description for this one
        super().__init__(device)
        self._attr_unique_id = f"{DOMAIN}_{self._device.device_id}_media_player"
        self._attr_volume_level = 0.5  # Default volume
        self._attr_state = "idle"

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()

        @callback
        def audio_state_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages for audio state."""
            self._attr_state = "playing" if msg.payload == "PLAYING" else "idle"
            self.async_write_ha_state()

        @callback
        def volume_state_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages for volume state."""
            try:
                # Convert device's 0-21 scale to HA's 0-1 scale
                self._attr_volume_level = float(msg.payload) / 21.0
                self.async_write_ha_state()
            except (ValueError, TypeError):
                pass

        await mqtt.async_subscribe(
            self.hass, f"{self._device.base_topic}/audio/state", audio_state_received, 1
        )
        await mqtt.async_subscribe(
            self.hass, f"{self._device.base_topic}/volume/state", volume_state_received, 1
        )

    async def async_set_volume_level(self, volume: float) -> None:
        """Set the volume level."""
        # Convert HA's 0-1 scale to device's 0-21 scale
        device_volume = int(round(volume * 21.0))
        command_topic = f"{self._device.base_topic}/volume/command"
        await mqtt.async_publish(self.hass, command_topic, str(device_volume), 1, False)

    async def async_media_stop(self) -> None:
        """Stop the media player."""
        command_topic = f"{self._device.base_topic}/radio/command"
        await mqtt.async_publish(self.hass, command_topic, "stop", 1, False)

    async def async_play_media(
        self, media_type: MediaType | str, media_id: str, **kwargs: Any
    ) -> None:
        """Play a piece of media."""
        # Case 1: Sound effect selected via sound mode list
        if media_type == "sound":
            command_topic = f"{self._device.base_topic}/play_sound/command"
            await mqtt.async_publish(self.hass, command_topic, media_id, 1, False)
            return

        # Case 2: TTS from Home Assistant
        if media_type.startswith("audio/"):
            # The media_id is the URL from the TTS service
            tts_payload = {
                "url": media_id,
                "volume": int(self._attr_volume_level * 100),
            }
            command_topic = f"{self._device.base_topic}/tts/command"
            await mqtt.async_publish(
                self.hass, command_topic, json.dumps(tts_payload), 1, False
            )
            return

        # Case 3: Radio Stream URL
        if media_type == MediaType.URL or media_type == MediaType.MUSIC:
            command_topic = f"{self._device.base_topic}/radio/command"
            await mqtt.async_publish(self.hass, command_topic, media_id, 1, False)
            return

    async def async_select_source(self, source: str) -> None:
        """Select a source to play."""
        await self.async_play_media("sound", source)