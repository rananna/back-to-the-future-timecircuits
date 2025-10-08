"""
Number platform for the Back to the Future Time Circuits integration.

This platform creates number entities (sliders) that allow the user to control
settings like brightness and refresh intervals on the Time Circuits device.
"""
from __future__ import annotations

import json
import logging
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

_LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True)
class BTTFTimeCircuitsNumberEntityDescription(NumberEntityDescription):
    """
    A class that describes BTTF Time Circuits number entities.

    This extends the standard NumberEntityDescription to include any
    custom properties needed for the integration's number entities.
    """


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
        key="stock_refresh",
        name="Stock Refresh",
        icon="mdi:chart-line",
        native_min_value=1,
        native_max_value=60,
        native_step=1,
        native_unit_of_measurement="min",
        mode=NumberMode.SLIDER,
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    Set up the BTTF Time Circuits number entities from a config entry.

    Args:
        hass: The Home Assistant instance.
        config_entry: The configuration entry for the integration.
        async_add_entities: A callback function to add the entities.
    """
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    async_add_entities(
        BTTFTimeCircuitsNumber(device, description) for description in NUMBERS
    )


class BTTFTimeCircuitsNumber(BTTFTimeCircuitsEntity, NumberEntity):
    """Representation of a BTTF Time Circuits Number entity."""

    entity_description: BTTFTimeCircuitsNumberEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsNumberEntityDescription,
    ) -> None:
        """
        Initialize the number entity.

        Args:
            device: The BTTFTimeCircuitsDevice instance.
            description: The entity description for the number entity.
        """
        self.entity_description = description
        super().__init__(device)

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events when the entity is added to Home Assistant."""
        await super().async_added_to_hass()
        state_topic = f"{self._device.base_topic}/{self.entity_description.key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages for the number's state."""
            try:
                self._attr_native_value = float(msg.payload)
                self.async_write_ha_state()
            except ValueError:
                _LOGGER.warning(
                    "Received non-numeric state for number on topic %s: %s",
                    msg.topic,
                    msg.payload,
                )

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_set_native_value(self, value: float) -> None:
        """
        Update the current value of the number entity.

        This is called when the user changes the value of the slider in the
        Home Assistant UI. It publishes the new value to the corresponding
        MQTT command topic on the device.

        Args:
            value: The new value to set.
        """
        command_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, str(int(value)), 1, False)