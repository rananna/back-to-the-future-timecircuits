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
from homeassistant.util import dt as dt_util

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

_LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True)
class BTTFTimeCircuitsButtonEntityDescription(ButtonEntityDescription):
    """A class that describes BTTF Time Circuits button entities."""

    datetime_str: str | None = None


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
        key="set_destination_1955",
        name="Set Destination to 1955",
        icon="mdi:calendar-arrow-left",
        datetime_str="1955-11-05 06:00:00",
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="set_destination_2015",
        name="Set Destination to 2015",
        icon="mdi:calendar-arrow-right",
        datetime_str="2015-10-21 07:28:00",
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="set_destination_1885",
        name="Set Destination to 1885",
        icon="mdi:calendar-arrow-left",
        datetime_str="1885-09-02 08:00:00",
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
        elif description.datetime_str:
            entities.append(BTTFTimeCircuitsFamousDateButton(device, description))
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


class BTTFTimeCircuitsFamousDateButton(BTTFTimeCircuitsEntity, ButtonEntity):
    """Representation of a Famous Date button."""

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
        """Handle the button press by calling the set_destination_time service."""
        if self.entity_description.datetime_str:
            dt_obj = dt_util.parse_datetime(self.entity_description.datetime_str)
            await self.hass.services.async_call(
                DOMAIN,
                "set_destination_time",
                {"datetime": dt_obj},
                target={"device_id": self._device.device_id},
                blocking=True,
            )