"""Select platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import logging
from dataclasses import dataclass
import asyncio

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
    BTTFTimeCircuitsSelectEntityDescription(
        key="sequencer",
        name="Sequencer",
        icon="mdi:movie-play-outline",
        options=[
            "Select a sequence",
            "Intruder Alert",
            "Time Travel",
            "Party Mode",
            "Countdown",
            "Knight Rider",
            "Cylon",
            "Lightning",
            "Loading",
            "Error",
            "Debug",
            "DebugEffects",
            "DebugLogic",
            "DebugParallelLogic",
            "DebugStress",
        ],
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits selects."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    entities = []
    for description in SELECTS:
        if description.key == "sequencer":
            entities.append(BTTFTimeCircuitsSequencerSelect(device, description))
        else:
            entities.append(BTTFTimeCircuitsSelect(device, description))
    async_add_entities(entities)


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


class BTTFTimeCircuitsSequencerSelect(BTTFTimeCircuitsSelect):
    """Representation of a BTTF Time Circuits Sequencer Select."""

    async def async_added_to_hass(self) -> None:
        """Set the initial state of the dropdown."""
        self._attr_current_option = self.entity_description.options[0]
        self.async_write_ha_state()

    async def async_select_option(self, option: str) -> None:
        """Change the selected option."""
        if option == self.entity_description.options[0]:
            return

        command_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, option, 1, False)

        # Provide visual feedback in the UI
        self._attr_current_option = option
        self.async_write_ha_state()

        # Reset the dropdown to the placeholder after a short delay
        await asyncio.sleep(1)
        self._attr_current_option = self.entity_description.options[0]
        self.async_write_ha_state()