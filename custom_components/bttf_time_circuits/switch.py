"""Switch platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import json
import logging
from dataclasses import dataclass
from typing import Any

from homeassistant.components import mqtt
from homeassistant.components.switch import (
    SwitchEntity,
    SwitchEntityDescription,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

_LOGGER = logging.getLogger(__name__)


@dataclass
class BTTFTimeCircuitsSwitchEntityDescription(SwitchEntityDescription):
    """A class that describes BTTF Time Circuits switch entities."""

    # For display mode switches, this is the payload to check for 'on' state
    on_payload: str | None = None
    # The MQTT topic key, if different from the entity key
    mqtt_key: str | None = None


SWITCHES: tuple[BTTFTimeCircuitsSwitchEntityDescription, ...] = ()


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits switches."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    entities = []
    for description in SWITCHES:
        if description.on_payload:
            entities.append(BTTFTimeCircuitsDisplayModeSwitch(device, description))
        else:
            entities.append(BTTFTimeCircuitsMqttSwitch(device, description))
    async_add_entities(entities)


class BTTFTimeCircuitsMqttSwitch(BTTFTimeCircuitsEntity, SwitchEntity):
    """Representation of a standard BTTF Time Circuits MQTT Switch."""

    entity_description: BTTFTimeCircuitsSwitchEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsSwitchEntityDescription,
    ) -> None:
        """Initialize the switch."""
        self.entity_description = description
        super().__init__(device)
        self._attr_is_on = False

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        state_topic = f"{self._device.base_topic}/{self.entity_description.key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            self._attr_is_on = msg.payload == "ON"
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn the switch on."""
        command_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, "ON", 1, False)

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn the switch off."""
        command_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, "OFF", 1, False)


class BTTFTimeCircuitsDisplayModeSwitch(BTTFTimeCircuitsEntity, SwitchEntity):
    """Representation of a BTTF Time Circuits Display Mode Switch."""

    entity_description: BTTFTimeCircuitsSwitchEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsSwitchEntityDescription,
    ) -> None:
        """Initialize the display mode switch."""
        self.entity_description = description
        super().__init__(device)
        self._attr_is_on = False

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        mqtt_key = self.entity_description.mqtt_key or self.entity_description.key
        state_topic = f"{self._device.base_topic}/{mqtt_key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            self._attr_is_on = msg.payload == self.entity_description.on_payload
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn the switch on."""
        mqtt_key = self.entity_description.mqtt_key or self.entity_description.key
        command_topic = f"{self._device.base_topic}/{mqtt_key}/command"
        await mqtt.async_publish(
            self.hass, command_topic, self.entity_description.on_payload, 1, False
        )

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn the switch off by reverting to Normal Clock mode."""
        mqtt_key = self.entity_description.mqtt_key or self.entity_description.key
        command_topic = f"{self._device.base_topic}/{mqtt_key}/command"
        await mqtt.async_publish(self.hass, command_topic, "Normal Clock", 1, False)