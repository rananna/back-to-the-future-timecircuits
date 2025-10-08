"""
Text platform for the Back to the Future Time Circuits integration.

This platform creates various `text` entities that correspond to the individual
segments of the Time Circuits display (e.g., Destination Year, Present Month).
This allows users to directly set the text on any part of the display.
"""
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
    """
    A class that describes BTTF Time Circuits text entities.

    This extends the standard TextEntityDescription to include any
    custom properties needed for the integration's text entities.
    """


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
        key="override_line_1",
        name="Override Line 1",
        icon="mdi:format-text",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="override_line_2",
        name="Override Line 2",
        icon="mdi:format-text",
    ),
    BTTFTimeCircuitsTextEntityDescription(
        key="override_line_3",
        name="Override Line 3",
        icon="mdi:format-text",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    Set up the BTTF Time Circuits text entities from a config entry.

    Args:
        hass: The Home Assistant instance.
        config_entry: The configuration entry for the integration.
        async_add_entities: A callback function to add the entities.
    """
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
        """
        Initialize the text entity.

        Args:
            device: The BTTFTimeCircuitsDevice instance.
            description: The entity description for the text entity.
        """
        self.entity_description = description
        super().__init__(device)
        self._attr_native_value = None

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events when the entity is added to Home Assistant."""
        await super().async_added_to_hass()
        state_topic = f"{self._device.base_topic}/{self.entity_description.key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages for the text entity's state."""
            self._attr_native_value = msg.payload
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_set_value(self, value: str) -> None:
        """
        Update the current value of the text entity.

        This is called when the user sets a new value in the Home Assistant UI.
        It publishes the new value to the corresponding MQTT command topic.

        Args:
            value: The new text value to set.
        """
        command_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, value, 1, False)
        # Optimistically update the state
        self._attr_native_value = value
        self.async_write_ha_state()