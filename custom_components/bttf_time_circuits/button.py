"""
Button platform for the Back to the Future Time Circuits integration.

This platform creates various button entities in Home Assistant, allowing users
to trigger actions on the Time Circuits device, such as rebooting, syncing time,
or playing a favorite radio station.
"""
from __future__ import annotations

import json
import logging
from dataclasses import dataclass

from homeassistant.components import mqtt
from homeassistant.components.button import (
    ButtonDeviceClass,
    ButtonEntity,
    ButtonEntityDescription,
)
from homeassistant.components.media_player import MediaType
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers import entity_registry as er
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.util import dt as dt_util

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

_LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True)
class BTTFTimeCircuitsButtonEntityDescription(ButtonEntityDescription):
    """
    A class that describes BTTF Time Circuits button entities.

    This extends the standard ButtonEntityDescription to include any
    custom properties needed for the integration's buttons.
    """


BUTTONS: tuple[BTTFTimeCircuitsButtonEntityDescription, ...] = (
    BTTFTimeCircuitsButtonEntityDescription(
        key="time_travel",
        name="Time Travel",
        icon="mdi:creation",
        device_class=ButtonDeviceClass.RESTART,
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="favorite_radio_station",
        name="Favorite Radio Station",
        icon="mdi:star",
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="reboot_device",
        name="Reboot Device",
        icon="mdi:restart",
        device_class=ButtonDeviceClass.RESTART,
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="force_ntp_sync",
        name="Force NTP Sync",
        icon="mdi:timer-sync-outline",
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="factory_reset",
        name="Factory Reset",
        icon="mdi:delete-restore",
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="weather_refresh",
        name="Refresh Weather Data",
        icon="mdi:refresh",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    Set up the BTTF Time Circuits buttons from a config entry.

    This function is called by Home Assistant to create and add the button
    entities defined in the BUTTONS tuple. It distinguishes between standard
    MQTT buttons and the special favorite radio station button.

    Args:
        hass: The Home Assistant instance.
        config_entry: The configuration entry for the integration.
        async_add_entities: A callback function to add the entities.
    """
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    entities = []
    for description in BUTTONS:
        if description.key == "favorite_radio_station":
            entities.append(BTTFTimeCircuitsFavoriteButton(device, description))
        else:
            entities.append(BTTFTimeCircuitsMqttButton(device, description))
    async_add_entities(entities)


class BTTFTimeCircuitsMqttButton(BTTFTimeCircuitsEntity, ButtonEntity):
    """
    Representation of a standard BTTF Time Circuits MQTT Button.

    This entity sends a simple "PRESS" payload to a specific MQTT command
    topic when the button is pressed in Home Assistant.
    """

    entity_description: BTTFTimeCircuitsButtonEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsButtonEntityDescription,
    ) -> None:
        """
        Initialize the MQTT button.

        Args:
            device: The BTTFTimeCircuitsDevice instance.
            description: The entity description for the button.
        """
        self.entity_description = description
        super().__init__(device)

    async def async_press(self) -> None:
        """
        Handle the button press.

        This method is called when the user presses the button in the
        Home Assistant UI. It publishes a "PRESS" message to the
        corresponding MQTT command topic on the device. It includes a special
        case for the "time_travel" key to maintain backward compatibility.
        """
        # The "time_travel" button entity is now an alias for "trigger_animation"
        # to ensure it works with the latest firmware, which expects the
        # "trigger_animation" command. This change is made here to avoid
        # altering the entity's unique ID in Home Assistant, which would
        # create a breaking change for users.
        if self.entity_description.key == "time_travel":
            command_topic = f"{self._device.base_topic}/trigger_animation/command"
        else:
            command_topic = (
                f"{self._device.base_topic}/{self.entity_description.key}/command"
            )
        await mqtt.async_publish(self.hass, command_topic, "PRESS", 1, False)


class BTTFTimeCircuitsFavoriteButton(BTTFTimeCircuitsEntity, ButtonEntity):
    """
    Representation of the Favorite Radio Station button.

    This button is a special case that does not send a direct MQTT command.
    Instead, it calls the `media_player.play_media` service for the device's
    media_player entity, which in turn handles the logic to play the
    configured favorite radio station.
    """

    entity_description: BTTFTimeCircuitsButtonEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsButtonEntityDescription,
    ) -> None:
        """
        Initialize the Favorite Radio Station button.

        Args:
            device: The BTTFTimeCircuitsDevice instance.
            description: The entity description for the button.
        """
        self.entity_description = description
        super().__init__(device)

    async def async_press(self) -> None:
        """
        Handle the button press by toggling the media_player state.

        This finds the associated media_player entity for this device. If the
        favorite radio station is currently playing, it calls the `media_stop`
        service. Otherwise, it calls the `play_media` service to start it.
        """
        # Find the media_player entity associated with this device
        entity_registry = er.async_get(self.hass)
        media_player_entity_id = entity_registry.async_get_entity_id(
            "media_player", DOMAIN, f"{DOMAIN}_{self._device.device_id}_media_player"
        )

        if not media_player_entity_id:
            return

        # Get the current state of the media player
        media_player_state = self.hass.states.get(media_player_entity_id)

        # Check if the radio is currently playing the favorite station
        is_playing_favorite = (
            media_player_state is not None
            and media_player_state.state == "playing"
            and media_player_state.attributes.get("media_content_id")
            == "Favorite Radio Station"
            and media_player_state.attributes.get("media_content_type")
            == MediaType.CHANNEL
        )

        if is_playing_favorite:
            # If it's playing, stop it
            await self.hass.services.async_call(
                "media_player",
                "media_stop",
                {"entity_id": media_player_entity_id},
                blocking=True,
            )
        else:
            # If it's not playing, start it
            await self.hass.services.async_call(
                "media_player",
                "play_media",
                {
                    "entity_id": media_player_entity_id,
                    "media_content_id": "Favorite Radio Station",
                    "media_content_type": MediaType.CHANNEL,
                },
                blocking=True,
            )

