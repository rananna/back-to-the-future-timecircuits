"""Text platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import json
import logging
from dataclasses import dataclass

from homeassistant.components import mqtt
from homeassistant.components.text import (
    TextEntity,
    TextEntityDescription,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

_LOGGER = logging.getLogger(__name__)


@dataclass
class BTTFTimeCircuitsTextEntityDescription(TextEntityDescription):
    """A class that describes BTTF Time Circuits text entities."""


TEXTS: tuple[BTTFTimeCircuitsTextEntityDescription, ...] = (
    BTTFTimeCircuitsTextEntityDescription(
        key="dest_year",
        name="Destination Year",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="dest_month",
        name="Destination Month",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="dest_day",
        name="Destination Day",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="dest_time",
        name="Destination Time",
        icon="mdi:clock",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="pres_year",
        name="Present Year",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="pres_month",
        name="Present Month",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="pres_day",
        name="Present Day",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="pres_time",
        name="Present Time",
        icon="mdi:clock",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="last_year",
        name="Last Departed Year",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="last_month",
        name="Last Departed Month",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="last_day",
        name="Last Departed Day",
        icon="mdi:calendar",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="last_time",
        name="Last Departed Time",
        icon="mdi:clock",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="status_text",
        name="Status Text",
        icon="mdi:information",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits text entities."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    async_add_entities(
        BTTFTimeCircuitsText(device, description) for description in TEXTS
    )


class BTTFTimeCircuitsText(BTTFTimeCircuitsEntity, TextEntity):
    """Representation of a BTTF Time Circuits Text entity."""

    entity_description: BTTFTimeCircuitsTextEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsTextEntityDescription,
    ) -> None:
        """Initialize the text entity."""
        self.entity_description = description
        super().__init__(device)
        self._attr_native_value = None

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        state_topic = f"{self._device.base_topic}/{self.entity_description.key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            self._attr_native_value = msg.payload
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_set_value(self, value: str) -> None:
        """Update the current value."""
        command_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, value, 1, False)
        # Optimistically update the state
        self._attr_native_value = value
        self.async_write_ha_state()