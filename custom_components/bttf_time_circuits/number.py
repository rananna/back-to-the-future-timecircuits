"""Number platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

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

from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

@dataclass
class BTTFTimeCircuitsNumberEntityDescription(NumberEntityDescription):
    """A class that describes BTTF Time Circuits number entities."""

NUMBERS: tuple[BTTFTimeCircuitsNumberEntityDescription, ...] = (
    BTTFTimeCircuitsNumberEntityDescription(
        key="brightness",
        name="Brightness",
        icon="mdi:brightness-6",
        native_min_value=0,
        native_max_value=7,
        native_step=1,
        mode=NumberMode.SLIDER,
    ),
    BTTFTimeCircuitsNumberEntityDescription(
        key="volume",
        name="Volume",
        icon="mdi:volume-high",
        native_min_value=0,
        native_max_value=21,
        native_step=1,
        mode=NumberMode.SLIDER,
    ),
    BTTFTimeCircuitsNumberEntityDescription(
        key="animation_interval",
        name="Animation Interval",
        icon="mdi:clock-in",
        native_unit_of_measurement="min",
        native_min_value=0,
        native_max_value=120,
        native_step=1,
        mode=NumberMode.BOX,
    ),
    BTTFTimeCircuitsNumberEntityDescription(
        key="animation_duration",
        name="Animation Duration",
        icon="mdi:movie-filter",
        native_unit_of_measurement="ms",
        native_min_value=1000,
        native_max_value=10000,
        native_step=100,
        mode=NumberMode.BOX,
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits numbers."""
    device_id = "BTTF_TC_123456" # Placeholder
    entities = [BTTFTimeCircuitsNumber(device_id, description) for description in NUMBERS]
    async_add_entities(entities)


class BTTFTimeCircuitsNumber(BTTFTimeCircuitsEntity, NumberEntity):
    """Representation of a BTTF Time Circuits Number."""

    entity_description: BTTFTimeCircuitsNumberEntityDescription

    def __init__(self, device_id: str, description: BTTFTimeCircuitsNumberEntityDescription) -> None:
        """Initialize the number."""
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
            try:
                self._attr_native_value = float(msg.payload)
                self.async_write_ha_state()
            except ValueError:
                # Ignore non-numeric payloads
                pass

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_set_native_value(self, value: float) -> None:
        """Update the current value."""
        command_topic = f"BTTF_TC/{self._device_id}/{self.entity_description.key}/command"
        await mqtt.async_publish(self.hass, command_topic, str(int(value)), 1, False)