"""Number platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import json
from dataclasses import dataclass

from homeassistant.components import mqtt
from homeassistant.components.number import (
    NumberEntity,
    NumberEntityDescription,
    NumberMode,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity


@dataclass(frozen=True)
class BTTFTimeCircuitsNumberEntityDescription(NumberEntityDescription):
    """A class that describes BTTF Time Circuits number entities."""


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits numbers."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    @callback
    def async_discover(
        payload: str,
    ) -> None:
        """Discover and add a BTTF Time Circuits number."""
        try:
            config = json.loads(payload)
        except json.JSONDecodeError:
            return

        entity_description = BTTFTimeCircuitsNumberEntityDescription(
            key=config["key"],
            name=config.get("name"),
            icon=config.get("icon"),
            native_min_value=config.get("native_min_value"),
            native_max_value=config.get("native_max_value"),
            native_step=config.get("native_step"),
            mode=config.get("mode"),
            native_unit_of_measurement=config.get("native_unit_of_measurement"),
        )

        async_add_entities([BTTFTimeCircuitsNumber(device, entity_description, config)])

    await mqtt.async_subscribe(
        hass,
        f"{device.base_topic}/number/+/config",
        lambda msg: async_discover(msg.payload),
        0,
    )


class BTTFTimeCircuitsNumber(BTTFTimeCircuitsEntity, NumberEntity):
    """Representation of a BTTF Time Circuits Number."""

    entity_description: BTTFTimeCircuitsNumberEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsNumberEntityDescription,
        config: dict,
    ) -> None:
        """Initialize the number."""
        self.entity_description = description
        self._config = config
        super().__init__(device)

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        state_topic = self._config.get("state_topic")

        if not state_topic:
            return

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            try:
                self._attr_native_value = float(msg.payload)
                self.async_write_ha_state()
            except ValueError:
                # Ignore non-numeric payloads
                pass

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_set_native_value(self, value: float) -> None:
        """Update the current value."""
        command_topic = self._config.get("command_topic")
        if command_topic:
            await mqtt.async_publish(self.hass, command_topic, str(int(value)), 1, False)