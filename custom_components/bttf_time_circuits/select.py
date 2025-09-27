"""Select platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import logging
from dataclasses import dataclass

from homeassistant.components import mqtt
from homeassistant.components.select import (
    SelectEntity,
    SelectEntityDescription,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

_LOGGER = logging.getLogger(__name__)


@dataclass(frozen=True)
class BTTFTimeCircuitsSelectEntityDescription(SelectEntityDescription):
    """A class that describes BTTF Time Circuits select entities."""


SELECTS: tuple[BTTFTimeCircuitsSelectEntityDescription, ...] = (
    BTTFTimeCircuitsSelectEntityDescription(
        key="display_mode",
        name="Display Mode",
        icon="mdi:television-guide",
        options=["Normal Clock", "Stock Ticker", "Weather", "Data Link"],
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits selects."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    async_add_entities(
        BTTFTimeCircuitsSelect(device, description) for description in SELECTS
    )


class BTTFTimeCircuitsSelect(BTTFTimeCircuitsEntity, SelectEntity):
    """Representation of a BTTF Time Circuits Select."""

    entity_description: BTTFTimeCircuitsSelectEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsSelectEntityDescription,
    ) -> None:
        """Initialize the select."""
        self.entity_description = description
        super().__init__(device)
        self._attr_current_option = None

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        state_topic = f"{self._device.base_topic}/{self.entity_description.key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            if msg.payload in self.entity_description.options:
                self._attr_current_option = msg.payload
                self.async_write_ha_state()
            else:
                _LOGGER.warning(
                    "Received invalid option for %s: %s",
                    self.entity_id,
                    msg.payload,
                )

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_select_option(self, option: str) -> None:
        """Change the selected option."""
        command_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, option, 1, False)