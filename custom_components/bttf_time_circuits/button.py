"""Button platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import json
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


@dataclass(frozen=True)
class BTTFTimeCircuitsButtonEntityDescription(ButtonEntityDescription):
    """A class that describes BTTF Time Circuits button entities."""


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits buttons."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    @callback
    def async_discover(
        payload: str,
    ) -> None:
        """Discover and add a BTTF Time Circuits button."""
        try:
            config = json.loads(payload)
        except json.JSONDecodeError:
            return

        entity_description = BTTFTimeCircuitsButtonEntityDescription(
            key=config["key"],
            name=config.get("name"),
            icon=config.get("icon"),
            device_class=config.get("device_class"),
        )

        # The 'favorite_radio_station' button is a special case that calls a service
        # instead of publishing to an MQTT topic.
        if config["key"] == "favorite_radio_station":
            entity = BTTFTimeCircuitsFavoriteButton(device, entity_description)
        else:
            entity = BTTFTimeCircuitsMqttButton(device, entity_description, config)

        async_add_entities([entity])

    await mqtt.async_subscribe(
        hass,
        f"{device.base_topic}/button/+/config",
        lambda msg: async_discover(msg.payload),
        0,
    )


class BTTFTimeCircuitsMqttButton(BTTFTimeCircuitsEntity, ButtonEntity):
    """Representation of a standard BTTF Time Circuits MQTT Button."""

    entity_description: BTTFTimeCircuitsButtonEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsButtonEntityDescription,
        config: dict,
    ) -> None:
        """Initialize the button."""
        self.entity_description = description
        self._config = config
        super().__init__(device)

    async def async_press(self) -> None:
        """Handle the button press."""
        command_topic = self._config.get("command_topic")
        if command_topic:
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