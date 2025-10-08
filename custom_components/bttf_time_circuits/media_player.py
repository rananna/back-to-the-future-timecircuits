"""
MediaPlayer platform for the Back to the Future Time Circuits integration.

This platform creates a media_player entity for the Time Circuits device,
which allows for playing sound effects, internet radio streams, and handling
Text-to-Speech (TTS) announcements.
"""
from __future__ import annotations

import json
import logging
from typing import Any

from homeassistant.components import mqtt
from homeassistant.components.media_player import (
    MediaPlayerDeviceClass,
    MediaPlayerEntity,
    MediaPlayerEntityDescription,
    MediaPlayerEntityFeature,
    MediaType,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN
from .device import BTTFTimeCircuitsDevice
from .entity import BTTFTimeCircuitsEntity

_LOGGER = logging.getLogger(__name__)

SUPPORTED_FEATURES = (
    MediaPlayerEntityFeature.PLAY
    | MediaPlayerEntityFeature.PLAY_MEDIA
    | MediaPlayerEntityFeature.STOP
    | MediaPlayerEntityFeature.VOLUME_SET
    | MediaPlayerEntityFeature.SELECT_SOURCE
)

SOUND_EFFECTS = [
    "ACCELERATION",
    "arrival_chime",
    "electric_sparks",
    "engine_rev",
    "flux_capacitor_power_on",
    "hum",
    "keypad_beeps",
    "lock_on",
    "relay_activation",
    "sys_beep",
    "time_travel",
]

# The new source list, combining sound effects and the single favorite station.
SOURCE_LIST = SOUND_EFFECTS + ["Favorite Radio Station"]


MEDIA_PLAYER_DESCRIPTION = MediaPlayerEntityDescription(
    key="media_player",
    name="Speaker",
    device_class=MediaPlayerDeviceClass.SPEAKER,
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    Set up the BTTF Time Circuits media player from a config entry.

    Args:
        hass: The Home Assistant instance.
        config_entry: The configuration entry for the integration.
        async_add_entities: A callback function to add the entities.
    """
    _LOGGER.debug("media_player.async_setup_entry")
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]
    async_add_entities(
        [BTTFTimeCircuitsMediaPlayer(device, MEDIA_PLAYER_DESCRIPTION)]
    )


class BTTFTimeCircuitsMediaPlayer(BTTFTimeCircuitsEntity, MediaPlayerEntity):
    """
    Representation of a BTTF Time Circuits Media Player.

    This entity handles all audio-related interactions with the device.
    """

    entity_description: MediaPlayerEntityDescription

    _attr_supported_features = SUPPORTED_FEATURES
    _attr_source_list = SOURCE_LIST

    _attr_should_poll = False

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: MediaPlayerEntityDescription,
    ) -> None:
        """
        Initialize the media player.

        Args:
            device: The BTTFTimeCircuitsDevice instance.
            description: The entity description for the media player.
        """
        _LOGGER.debug(
            f"BTTFTimeCircuitsMediaPlayer.__init__ for device: {device.device_id}"
        )
        self.entity_description = description
        super().__init__(device)
        self._attr_volume_level = 0.5  # Default volume
        self._attr_state = "idle"
        self._attr_media_content_id = None
        self._attr_media_content_type = None
        self._attr_media_title = None

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events when the entity is added to Home Assistant."""
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
            self.hass,
            f"{self._device.base_topic}/volume/state",
            volume_state_received,
            1,
        )

    async def async_set_volume_level(self, volume: float) -> None:
        """
        Set the volume level.

        Args:
            volume: The volume level (0.0 to 1.0).
        """
        # Convert HA's 0-1 scale to device's 0-21 scale
        device_volume = int(round(volume * 21.0))
        command_topic = f"{self._device.base_topic}/volume/command"
        await mqtt.async_publish(self.hass, command_topic, str(device_volume), 1, False)

    async def async_media_stop(self) -> None:
        """Stop the media player."""
        self._attr_media_content_id = None
        self._attr_media_content_type = None
        self._attr_media_title = None
        command_topic = f"{self._device.base_topic}/radio/command"
        await mqtt.async_publish(self.hass, command_topic, "stop", 1, False)

    async def async_media_play(self) -> None:
        """Play the last media."""
        if self._attr_media_content_type and self._attr_media_content_id:
            await self.async_play_media(
                media_type=self._attr_media_content_type,
                media_id=self._attr_media_content_id,
            )

    async def async_play_media(
        self, media_type: MediaType | str, media_id: str, **kwargs: Any
    ) -> None:
        """
        Play a piece of media.

        This is the central command for playing audio. It handles different
        media types by publishing the appropriate MQTT payload to the device.

        Args:
            media_type: The type of media to play (e.g., music, sound, channel).
            media_id: The identifier for the media (e.g., a URL or sound name).
            **kwargs: Additional arguments.
        """
        self._attr_media_content_id = media_id
        self._attr_media_content_type = media_type
        self._attr_media_title = media_id

        # Case 1: Sound effect. Home Assistant may send this as `media_type: music`
        # when a sound is selected from the media browser, so we check if the
        # `media_id` is in our list of known sound effects.
        if media_type == "sound" or (
            media_type == MediaType.MUSIC and media_id in SOUND_EFFECTS
        ):
            self._attr_media_title = f"Sound: {media_id}"
            command_topic = f"{self._device.base_topic}/play_sound/command"
            await mqtt.async_publish(self.hass, command_topic, media_id, 1, False)
            return

        # Case 2: Favorite Radio Station. This handles `media_type: channel`
        # which is used when selecting from the source list.
        if media_type == MediaType.CHANNEL and media_id == "Favorite Radio Station":
            self._attr_media_title = "Favorite Radio Station"
            command_topic = f"{self._device.base_topic}/radio/command"
            await mqtt.async_publish(
                self.hass, command_topic, "play_favorite_radio", 1, False
            )
            return

        # Case 3: TTS from Home Assistant. This handles `media_type: audio/...`
        if media_type.startswith("audio/"):
            self._attr_media_title = "TTS"
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

        # Case 4: Direct Radio Stream URL (e.g., from a script)
        if media_type == MediaType.URL or media_type == MediaType.MUSIC:
            self._attr_media_title = f"Radio: {media_id}"
            command_topic = f"{self._device.base_topic}/radio/command"
            await mqtt.async_publish(self.hass, command_topic, media_id, 1, False)
            return

    async def async_select_source(self, source: str) -> None:
        """
        Select a source to play from the source list.

        Args:
            source: The name of the source selected by the user.
        """
        if source == "Favorite Radio Station":
            await self.async_play_media(MediaType.CHANNEL, source)
        elif source in SOUND_EFFECTS:
            await self.async_play_media("sound", source)
        else:
            _LOGGER.warning(f"Unknown source selected: {source}")
