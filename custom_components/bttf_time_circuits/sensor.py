"""Sensor platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import json
import logging
from collections.abc import Callable
from dataclasses import dataclass

from homeassistant.components import mqtt
from homeassistant.components.sensor import (
    SensorDeviceClass,
    SensorEntity,
    SensorEntityDescription,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.typing import ConfigType, DiscoveryInfoType

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

_LOGGER = logging.getLogger(__name__)

# Define the sensor descriptions
@dataclass(frozen=True)
class BTTFTimeCircuitsSensorEntityDescription(SensorEntityDescription):
    """A class that describes BTTF Time Circuits sensor entities."""


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits sensors."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    @callback
    def async_discover(
        payload: str,
    ) -> None:
        """Discover and add a BTTF Time Circuits sensor."""
        try:
            config = json.loads(payload)
        except json.JSONDecodeError:
            _LOGGER.warning("Received malformed JSON for sensor discovery: %s", payload)
            return

        entity_description = BTTFTimeCircuitsSensorEntityDescription(
            key=config["key"],
            name=config.get("name"),
            icon=config.get("icon"),
            device_class=config.get("device_class"),
        )

        async_add_entities(
            [BTTFTimeCircuitsSensor(device, entity_description, config)]
        )

    await mqtt.async_subscribe(
        hass,
        f"{device.base_topic}/sensor/+/config",
        lambda msg: async_discover(msg.payload),
        0,
    )


class BTTFTimeCircuitsSensor(BTTFTimeCircuitsEntity, SensorEntity):
    """Representation of a BTTF Time Circuits Sensor."""

    entity_description: BTTFTimeCircuitsSensorEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsSensorEntityDescription,
        config: dict,
    ) -> None:
        """Initialize the sensor."""
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
            self._attr_native_value = msg.payload
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

        attributes_topic = self._config.get("json_attributes_topic")
        if attributes_topic:

            @callback
            def attributes_received(msg: mqtt.ReceiveMessage) -> None:
                """Handle new MQTT attribute messages."""
                try:
                    self._attr_extra_state_attributes = json.loads(msg.payload)
                except json.JSONDecodeError:
                    _LOGGER.warning(
                        "Received malformed JSON for sensor attributes on topic %s: %s",
                        msg.topic,
                        msg.payload,
                    )
                self.async_write_ha_state()

            await mqtt.async_subscribe(
                self.hass, attributes_topic, attributes_received, 1
            )