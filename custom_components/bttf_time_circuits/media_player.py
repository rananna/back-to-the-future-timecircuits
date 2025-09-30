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
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.storage import Store

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
    | MediaPlayerEntityFeature.CLEAR_PLAYLIST
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


class BTTFTimeCircuitsMediaPlayer(BTTFTimeCircuitsEntity, MediaPlayerEntity):
    """Representation of a BTTF Time Circuits Media Player."""

    entity_description: MediaPlayerEntityDescription

    _attr_supported_features = SUPPORTED_FEATURES
    _attr_source_list = SOUND_EFFECTS

    _attr_should_poll = False

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
        super().__init__(device)
        self._attr_volume_level = 0.5  # Default volume
        self._attr_state = "idle"
        self._attr_media_content_id = None
        self._attr_media_content_type = None
        self._attr_media_title = None

        self._radio_stations: list[dict[str, str]] = []
        self._favorites: list[dict[str, str]] = []
        self._store = Store(self.hass, STORAGE_VERSION, STORAGE_KEY)

    async def async_load_favorites(self) -> None:
        """Load favorites from the store."""
        if (stored_data := await self._store.async_load()) is None:
            return
        self._favorites = stored_data.get("favorites", [])
        self._update_radio_stations()

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()

        await self.async_load_favorites()

        self.async_on_remove(
            self.config_entry.add_update_listener(self._on_options_update)
        )

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

    async def _on_options_update(
        self, hass: HomeAssistant, entry: ConfigEntry
    ) -> None:
        """Handle options update."""
        self._update_radio_stations()
        self.async_write_ha_state()
        await self._publish_radio_stations()

    def _update_radio_stations(self) -> None:
        """Update the radio stations from config and favorites."""
        self._radio_stations = self.config_entry.options.get("radio_stations", [])

        # Combine configured stations and favorites, using URL as the unique key
        # This prevents duplicates and prioritizes configured stations over favorites
        combined_stations = {
            station["url"]: station for station in self._radio_stations
        }
        for fav in self._favorites:
            combined_stations.setdefault(fav["url"], fav)

        # Get the names for the source list
        radio_station_names = [
            station["name"] for station in combined_stations.values()
        ]

        self._attr_source_list = SOUND_EFFECTS + sorted(radio_station_names)

    async def _publish_radio_stations(self) -> None:
        """Publish the list of radio stations to MQTT."""
        command_topic = f"{self._device.base_topic}/radio_stations/command"
        payload = json.dumps(self._radio_stations)
        await mqtt.async_publish(self.hass, command_topic, payload, 1, True)

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
            all_stations = self._radio_stations + self._favorites
            station_name = next(
                (s["name"] for s in all_stations if s["url"] == media_id),
                media_id,  # Fallback to URL
            )
            self._attr_media_title = station_name
            command_topic = f"{self._device.base_topic}/radio/command"
            await mqtt.async_publish(self.hass, command_topic, media_id, 1, False)
            return

    async def async_select_source(self, source: str) -> None:
        """Select a source to play."""
        all_stations = self._radio_stations + self._favorites
        station_url = next(
            (s["url"] for s in all_stations if s["name"] == source), None
        )

        if station_url:
            await self.async_play_media(MediaType.URL, station_url)
        elif source in SOUND_EFFECTS:
            await self.async_play_media("sound", source)
        else:
            _LOGGER.warning(f"Unknown source selected: {source}")

    async def async_set_favorite(self, favorite: bool) -> None:
        """Set the current radio station as a favorite."""
        if self._attr_media_content_type not in [MediaType.URL, MediaType.MUSIC]:
            _LOGGER.warning(
                "Not a favorite-able media type: %s", self._attr_media_content_type
            )
            return

        media_url = self._attr_media_content_id
        media_name = self._attr_media_title

        is_currently_favorite = any(fav["url"] == media_url for fav in self._favorites)

        if favorite and not is_currently_favorite:
            # Add to favorites
            self._favorites.append({"name": media_name, "url": media_url})
        elif not favorite and is_currently_favorite:
            # Remove from favorites
            self._favorites = [
                fav for fav in self._favorites if fav["url"] != media_url
            ]
        else:
            # No change needed
            return

        await self._store.async_save({"favorites": self._favorites})
        self._update_radio_stations()
        self.async_write_ha_state()

    async def async_clear_playlist(self) -> None:
        """Clear all favorite radio stations."""
        self._favorites = []
        await self._store.async_save({"favorites": self._favorites})
        self._update_radio_stations()
        self.async_write_ha_state()
