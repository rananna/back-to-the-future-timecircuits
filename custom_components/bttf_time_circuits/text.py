"""Text platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

from dataclasses import dataclass

from homeassistant.components import mqtt
from homeassistant.components.text import (
    TextEntity,
    TextEntityDescription,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

@dataclass
class BTTFTimeCircuitsTextEntityDescription(TextEntityDescription):
    """A class that describes BTTF Time Circuits text entities."""

# Define all 22 text entities
TEXT_ENTITIES: list[BTTFTimeCircuitsTextEntityDescription] = []

# 12 Direct Display Segments
ROWS = {"dest": "Destination", "pres": "Present", "last": "Last Departed"}
SEGMENTS = {"month": "Month", "day": "Day", "year": "Year", "time": "Time"}
for row_key, row_name in ROWS.items():
    for seg_key, seg_name in SEGMENTS.items():
        TEXT_ENTITIES.append(
            BTTFTimeCircuitsTextEntityDescription(
                key=f"{row_key}_{seg_key}",
                name=f"{row_name} {seg_name}",
                icon="mdi:form-textbox",
            )
        )

# 5 Data Point Marquees
for i in range(5):
    TEXT_ENTITIES.append(
        BTTFTimeCircuitsTextEntityDescription(
            key=f"datapoint_{i}_marquee",
            name=f"Data Point {i+1} Marquee",
            icon="mdi:text-box-outline",
        )
    )

# 3 Override Message Lines
for i in range(1, 4):
    TEXT_ENTITIES.append(
        BTTFTimeCircuitsTextEntityDescription(
            key=f"override_line_{i}",
            name=f"Override Message Line {i}",
            icon="mdi:message-draw",
        )
    )

# Other Text Entities
TEXT_ENTITIES.extend([
    BTTFTimeCircuitsTextEntityDescription(
        key="tts_text",
        name="TTS Text",
        icon="mdi:text-to-speech",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="weather_city",
        name="Weather City",
        icon="mdi:city",
    ),
])


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits text entities."""
    device_id = "BTTF_TC_123456"  # Placeholder
    entities = [BTTFTimeCircuitsText(device_id, description) for description in TEXT_ENTITIES]
    async_add_entities(entities)


class BTTFTimeCircuitsText(BTTFTimeCircuitsEntity, TextEntity):
    """Representation of a BTTF Time Circuits Text entity."""

    entity_description: BTTFTimeCircuitsTextEntityDescription

    def __init__(self, device_id: str, description: BTTFTimeCircuitsTextEntityDescription) -> None:
        """Initialize the text entity."""
        super().__init__(device_id)
        self.entity_description = description
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_{description.key}"
        self._attr_native_value = None

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        state_topic = f"BTTF_TC/{self._device_id}/{self.entity_description.key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            self._attr_native_value = msg.payload
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_set_value(self, value: str) -> None:
        """Update the current value."""
        command_topic = f"BTTF_TC/{self._device_id}/{self.entity_description.key}/command"
        await mqtt.async_publish(self.hass, command_topic, value, 1, False)
        # Optimistically update the state
        self._attr_native_value = value
        self.async_write_ha_state()