"""Button platform for the Back to the Future Time Circuits integration."""
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
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

_LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True)
class BTTFTimeCircuitsButtonEntityDescription(ButtonEntityDescription):
    """A class that describes BTTF Time Circuits button entities."""


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
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits buttons."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    entities = []
    for description in BUTTONS:
        if description.key == "favorite_radio_station":
            entities.append(BTTFTimeCircuitsFavoriteButton(device, description))
        else:
            entities.append(BTTFTimeCircuitsMqttButton(device, description))
    async_add_entities(entities)


class BTTFTimeCircuitsMqttButton(BTTFTimeCircuitsEntity, ButtonEntity):
    """Representation of a standard BTTF Time Circuits MQTT Button."""

    entity_description: BTTFTimeCircuitsButtonEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsButtonEntityDescription,
    ) -> None:
        """Initialize the button."""
        self.entity_description = description
        super().__init__(device)

    async def async_press(self) -> None:
        """Handle the button press."""
        command_topic = (
            f"{self.device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, "PRESS", 1, False)


class BTTFTimeCircuitsFavoriteButton(BTTFTimeCircuitsEntity, ButtonEntity):
    """Representation of the Favorite Radio Station button."""

    entity_description: BTTFTimeCircuitsButtonEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsButtonEntityDescription,
    ) -> None:
        """Initialize the button."""
        self.entity_description = description
        super().__init__(device)

    async def async_press(self) -> None:
        """Handle the button press by calling the media_player service."""
        # Find the media_player entity associated with this device
        entity_registry = self.hass.helpers.entity_registry.async_get(self.hass)
        media_player_entity_id = entity_registry.async_get_entity_id(
            "media_player", DOMAIN, f"{DOMAIN}_{self._device.device_id}_media_player"
        )

        if media_player_entity_id:
            await self.hass.services.async_call(
                "media_player",
                "favorite_radio_station",
                {"entity_id": media_player_entity_id},
                blocking=True,
            )