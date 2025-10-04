"""Sensor platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import json
import logging
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

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

_LOGGER = logging.getLogger(__name__)


# Define the sensor descriptions
@dataclass(frozen=True, kw_only=True)
class BTTFTimeCircuitsSensorEntityDescription(SensorEntityDescription):
    """A class that describes BTTF Time Circuits sensor entities."""

    value_type: str | None = None


SENSORS: tuple[BTTFTimeCircuitsSensorEntityDescription, ...] = (
    BTTFTimeCircuitsSensorEntityDescription(
        key="dest_year",
        name="Destination Year",
        icon="mdi:calendar",
        value_type="int",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="dest_month",
        name="Destination Month",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="dest_day",
        name="Destination Day",
        icon="mdi:calendar",
        value_type="int",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="dest_time",
        name="Destination Time",
        icon="mdi:clock",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="pres_year",
        name="Present Year",
        icon="mdi:calendar",
        value_type="int",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="pres_month",
        name="Present Month",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="pres_day",
        name="Present Day",
        icon="mdi:calendar",
        value_type="int",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="pres_time",
        name="Present Time",
        icon="mdi:clock",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="last_year",
        name="Last Departed Year",
        icon="mdi:calendar",
        value_type="int",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="last_month",
        name="Last Departed Month",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="last_day",
        name="Last Departed Day",
        icon="mdi:calendar",
        value_type="int",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="last_time",
        name="Last Departed Time",
        icon="mdi:clock",
    ),
    BTTFTimeCircuitsSensorEntityDescription(
        key="status",
        name="Status",
        icon="mdi:information",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits sensors."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    async_add_entities(
        BTTFTimeCircuitsSensor(device, entity_description)
        for entity_description in SENSORS
    )


class BTTFTimeCircuitsSensor(BTTFTimeCircuitsEntity, SensorEntity):
    """Representation of a BTTF Time Circuits Sensor."""

    entity_description: BTTFTimeCircuitsSensorEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsSensorEntityDescription,
    ) -> None:
        """Initialize the sensor."""
        self.entity_description = description
        super().__init__(device)

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()

        state_topic = f"{self._device.base_topic}/{self.entity_description.key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            if self.entity_description.value_type == "int":
                try:
                    self._attr_native_value = int(msg.payload)
                except (ValueError, TypeError):
                    _LOGGER.warning(
                        "Received non-integer value for %s: %s",
                        self.entity_id,
                        msg.payload,
                    )
                    self._attr_native_value = None
            else:
                self._attr_native_value = msg.payload

            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

        attributes_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/attributes"
        )

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

        await mqtt.async_subscribe(self.hass, attributes_topic, attributes_received, 1)