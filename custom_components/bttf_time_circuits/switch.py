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


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits switches."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    @callback
    def async_discover(
        payload: str,
    ) -> None:
        """Discover and add a BTTF Time Circuits switch."""
        try:
            config = json.loads(payload)
        except json.JSONDecodeError:
            _LOGGER.warning("Received malformed JSON for switch discovery: %s", payload)
            return

        entity_description = BTTFTimeCircuitsSwitchEntityDescription(
            key=config["key"],
            name=config.get("name"),
            icon=config.get("icon"),
        )

        # Display mode switches have a specific on_payload
        if "on_payload" in config:
            entity = BTTFTimeCircuitsDisplayModeSwitch(
                device, entity_description, config
            )
        else:
            entity = BTTFTimeCircuitsMqttSwitch(device, entity_description, config)

        async_add_entities([entity])

    await mqtt.async_subscribe(
        hass,
        f"{device.base_topic}/switch/+/config",
        lambda msg: async_discover(msg.payload),
        0,
    )


class BTTFTimeCircuitsMqttSwitch(BTTFTimeCircuitsEntity, SwitchEntity):
    """Representation of a standard BTTF Time Circuits MQTT Switch."""

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsSwitchEntityDescription,
        config: dict,
    ) -> None:
        """Initialize the switch."""
        self.entity_description = description
        self._config = config
        super().__init__(device)
        self._attr_is_on = False

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        state_topic = self._config.get("state_topic")

        if not state_topic:
            return

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            self._attr_is_on = msg.payload == "ON"
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn the switch on."""
        command_topic = self._config.get("command_topic")
        if command_topic:
            await mqtt.async_publish(self.hass, command_topic, "ON", 1, False)

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn the switch off."""
        command_topic = self._config.get("command_topic")
        if command_topic:
            await mqtt.async_publish(self.hass, command_topic, "OFF", 1, False)


class BTTFTimeCircuitsDisplayModeSwitch(BTTFTimeCircuitsEntity, SwitchEntity):
    """Representation of a BTTF Time Circuits Display Mode Switch."""

    entity_description: BTTFTimeCircuitsSwitchEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsSwitchEntityDescription,
        config: dict,
    ) -> None:
        """Initialize the display mode switch."""
        self.entity_description = description
        self._config = config
        super().__init__(device)
        self._attr_is_on = False

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        state_topic = self._config.get("state_topic")

        if not state_topic:
            return

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            self._attr_is_on = msg.payload == self._config.get("on_payload")
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn the switch on."""
        command_topic = self._config.get("command_topic")
        on_payload = self._config.get("on_payload")
        if command_topic and on_payload:
            await mqtt.async_publish(self.hass, command_topic, on_payload, 1, False)

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn the switch off by reverting to Normal Clock mode."""
        command_topic = self._config.get("command_topic")
        if command_topic:
            await mqtt.async_publish(self.hass, command_topic, "Normal Clock", 1, False)