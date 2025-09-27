"""MediaPlayer platform for the Back to the Future Time Circuits integration."""
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
from homeassistant.helpers import entity_platform, storage
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

_LOGGER = logging.getLogger(__name__)

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

STORAGE_KEY = f"{DOMAIN}_favorites"
STORAGE_VERSION = 1


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
    """Set up the BTTF Time Circuits media player."""
    _LOGGER.debug("media_player.async_setup_entry")
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]
    async_add_entities([BTTFTimeCircuitsMediaPlayer(device, MEDIA_PLAYER_DESCRIPTION)])

    platform = entity_platform.async_get_current_platform()
    platform.async_register_entity_service(
        "favorite_radio_station",
        {},
        "async_favorite_radio_station",
    )
    platform.async_register_entity_service(
        "clear_favorite_radio_stations",
        {},
        "async_clear_favorite_radio_stations",
    )


class BTTFTimeCircuitsMediaPlayer(BTTFTimeCircuitsEntity, MediaPlayerEntity):
    """Representation of a BTTF Time Circuits Media Player."""

    entity_description: MediaPlayerEntityDescription

    _attr_supported_features = SUPPORTED_FEATURES
    _attr_source_list = SOUND_EFFECTS

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: MediaPlayerEntityDescription,
    ) -> None:
        """Initialize the media player."""
        _LOGGER.debug(
            f"BTTFTimeCircuitsMediaPlayer.__init__ for device: {device.device_id}"
        )
        self.entity_description = description

        # BTTFTimeCircuitsEntity.__init__ doesn't call super(), which breaks the MRO
        # chain and prevents MediaPlayerEntity.__init__ from being called.
        # To fix this without modifying the base class, we explicitly call both initializers.
        BTTFTimeCircuitsEntity.__init__(self, device)
        MediaPlayerEntity.__init__(self)

        self._attr_volume_level = 0.5  # Default volume
        self._attr_state = "idle"
        self._attr_media_content_id = None
        self._attr_media_content_type = None
        self._attr_media_title = None

        self._favorites_store: storage.Store | None = None
        self._favorite_radio_stations: list[str] = []

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()

        self._favorites_store = storage.Store(self.hass, STORAGE_VERSION, STORAGE_KEY)
        favorites = await self._favorites_store.async_load()
        if favorites:
            self._favorite_radio_stations = favorites.get("radio_stations", [])
        self._update_source_list()

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

        @callback
        def favorite_station_pressed(msg: mqtt.ReceiveMessage) -> None:
            """Handle favorite station button press."""
            self.hass.async_create_task(self.async_favorite_radio_station())

        await mqtt.async_subscribe(
            self.hass, f"{self._device.base_topic}/audio/state", audio_state_received, 1
        )
        await mqtt.async_subscribe(
            self.hass,
            f"{self._device.base_topic}/volume/state",
            volume_state_received,
            1,
        )
        await mqtt.async_subscribe(
            self.hass,
            f"{self._device.base_topic}/favorite_radio_station/command",
            favorite_station_pressed,
            1,
        )

    def _update_source_list(self) -> None:
        """Update the source list with favorites."""
        self._attr_source_list = SOUND_EFFECTS + self._favorite_radio_stations
        self.async_write_ha_state()

    async def async_favorite_radio_station(self, **kwargs: Any) -> None:
        """Favorite the current radio station."""
        assert self._favorites_store
        if (
            self._attr_media_content_type in [MediaType.URL, MediaType.MUSIC]
            and self._attr_media_content_id
            and self._attr_media_content_id not in self._favorite_radio_stations
        ):
            self._favorite_radio_stations.append(self._attr_media_content_id)
            await self._favorites_store.async_save(
                {"radio_stations": self._favorite_radio_stations}
            )
            self._update_source_list()

    async def async_clear_favorite_radio_stations(self, **kwargs: Any) -> None:
        """Clear all favorite radio stations."""
        assert self._favorites_store
        self._favorite_radio_stations = []
        await self._favorites_store.async_save({"radio_stations": []})
        self._update_source_list()

    async def async_set_volume_level(self, volume: float) -> None:
        """Set the volume level."""
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

    async def async_play_media(
        self, media_type: MediaType | str, media_id: str, **kwargs: Any
    ) -> None:
        """Play a piece of media."""
        self._attr_media_content_id = media_id
        self._attr_media_content_type = media_type
        self._attr_media_title = media_id

        # Case 1: Sound effect selected via sound mode list
        if media_type == "sound":
            self._attr_media_title = f"Sound: {media_id}"
            command_topic = f"{self._device.base_topic}/play_sound/command"
            await mqtt.async_publish(self.hass, command_topic, media_id, 1, False)
            return

        # Case 2: TTS from Home Assistant
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

        # Case 3: Radio Stream URL
        if media_type == MediaType.URL or media_type == MediaType.MUSIC:
            self._attr_media_title = f"Radio: {media_id}"
            command_topic = f"{self._device.base_topic}/radio/command"
            await mqtt.async_publish(self.hass, command_topic, media_id, 1, False)
            return

    async def async_select_source(self, source: str) -> None:
        """Select a source to play."""
        if source in self._favorite_radio_stations:
            await self.async_play_media(MediaType.URL, source)
        elif source in SOUND_EFFECTS:
            await self.async_play_media("sound", source)
        else:
            _LOGGER.warning(f"Unknown source selected: {source}")

    async def async_select_sound(self, sound: str):
        """Play a sound effect."""
        await self.async_play_media("sound", sound)