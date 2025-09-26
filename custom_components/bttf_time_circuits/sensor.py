"""Sensor platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import json
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

from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

# Define the sensor descriptions
@dataclass
class BTTFTimeCircuitsSensorEntityDescription(SensorEntityDescription):
    """A class that describes BTTF Time Circuits sensor entities."""
    state: Callable[[str], str] | None = None

SENSORS: tuple[BTTFTimeCircuitsSensorEntityDescription, ...] = (
    BTTFTimeCircuitsSensorEntityDescription(
        key="status",
        name="Status",
        icon="mdi:clock-outline",
        state=lambda value: value,
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="audio_stream_status",
        name="Audio Stream Status",
        icon="mdi:waveform",
        state=lambda value: value.lower(),
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits sensors."""
    # This function will be expanded later to discover devices
    # For now, we'll assume a single device for development
    device_id = "BTTF_TC_123456" # Placeholder

    entities = [BTTFTimeCircuitsSensor(device_id, description) for description in SENSORS]
    async_add_entities(entities)


class BTTFTimeCircuitsSensor(BTTFTimeCircuitsEntity, SensorEntity):
    """Representation of a BTTF Time Circuits Sensor."""

    entity_description: BTTFTimeCircuitsSensorEntityDescription

    def __init__(self, device_id: str, description: BTTFTimeCircuitsSensorEntityDescription) -> None:
        """Initialize the sensor."""
        super().__init__(device_id)
        self.entity_description = description
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_{description.key}"

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()

        state_topic = f"BTTF_TC/{self._device_id}/{self.entity_description.key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            if self.entity_description.state:
                self._attr_native_value = self.entity_description.state(msg.payload)
            else:
                self._attr_native_value = msg.payload
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

        if self.entity_description.key == "status":
            attributes_topic = f"BTTF_TC/{self._device_id}/status/attributes"
            @callback
            def attributes_received(msg: mqtt.ReceiveMessage) -> None:
                """Handle new MQTT attribute messages."""
                try:
                    self._attr_extra_state_attributes = json.loads(msg.payload)
                except json.JSONDecodeError:
                    pass # Ignore invalid JSON
                self.async_write_ha_state()

            await mqtt.async_subscribe(self.hass, attributes_topic, attributes_received, 1)