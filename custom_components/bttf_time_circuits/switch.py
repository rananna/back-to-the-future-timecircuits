"""
Switch platform for the Back to the Future Time Circuits integration.

This platform creates various switch entities that allow the user to toggle
settings on the Time Circuits device, such as enabling 24-hour format or
the display override mode.
"""
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
    """
    A class that describes BTTF Time Circuits switch entities.

    This extends the standard SwitchEntityDescription to include any
    custom properties needed for the integration's switch entities.
    """


SWITCHES: tuple[BTTFTimeCircuitsSwitchEntityDescription, ...] = (
    BTTFTimeCircuitsSwitchEntityDescription(
        key="override_switch",
        name="Override Switch",
        icon="mdi:television-shimmer",
    ),
    BTTFTimeCircuitsSwitchEntityDescription(
        key="24h_format",
        name="24h Format",
        icon="mdi:clock-time-twelve-outline",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    Set up the BTTF Time Circuits switch entities from a config entry.

    Args:
        hass: The Home Assistant instance.
        config_entry: The configuration entry for the integration.
        async_add_entities: A callback function to add the entities.
    """
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    entities = [
        BTTFTimeCircuitsMqttSwitch(device, description) for description in SWITCHES
    ]
    async_add_entities(entities)


class BTTFTimeCircuitsMqttSwitch(BTTFTimeCircuitsEntity, SwitchEntity):
    """Representation of a standard BTTF Time Circuits MQTT Switch."""

    entity_description: BTTFTimeCircuitsSwitchEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsSwitchEntityDescription,
    ) -> None:
        """
        Initialize the switch entity.

        Args:
            device: The BTTFTimeCircuitsDevice instance.
            description: The entity description for the switch.
        """
        self.entity_description = description
        super().__init__(device)
        self._attr_is_on = False

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events when the entity is added to Home Assistant."""
        await super().async_added_to_hass()
        state_topic = f"{self._device.base_topic}/{self.entity_description.key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages for the switch's state."""
            self._attr_is_on = msg.payload == "ON"
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_turn_on(self, **kwargs: Any) -> None:
        """
        Turn the switch on.

        This is called when the user toggles the switch to the 'on' state in
        the Home Assistant UI. It publishes "ON" to the corresponding MQTT
        command topic.

        Args:
            **kwargs: Additional arguments.
        """
        command_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, "ON", 1, False)

    async def async_turn_off(self, **kwargs: Any) -> None:
        """
        Turn the switch off.

        This is called when the user toggles the switch to the 'off' state in
        the Home Assistant UI. It publishes "OFF" to the corresponding MQTT
        command topic.

        Args:
            **kwargs: Additional arguments.
        """
        command_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, "OFF", 1, False)

