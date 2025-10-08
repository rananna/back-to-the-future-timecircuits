"""
Select platform for the Back to the Future Time Circuits integration.

This platform creates various `select` entities (dropdown menus) in Home
Assistant, allowing users to choose from a list of options for settings like
the display mode or the default animation sequence.
"""
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
    """
    A class that describes BTTF Time Circuits select entities.

    This extends the standard SelectEntityDescription to include any
    custom properties needed for the integration's select entities.
    """


ANIMATION_OPTIONS = [
    "Intruder Alert",
    "Time Travel",
    "System Boot",
    "Party Mode",
    "Countdown",
    "Knight Rider",
    "Lightning",
    "Loading",
    "Error",
    "Flux Capacitor Charge-Up",
    "Tachyons Detected",
    "Data Stream",
    "Wormhole Collapse",
    "All Displays Random",
    "Time Travel Tunnel",
    "Fire Trails",
    "Sparkle Reveal",
    "Sequential Flicker",
    "Random Flicker",
    "Counting Up",
    "Wave Flicker",
    "Tornado Flicker",
    "Capacitor Charge-Up",
    "Digital Rain",
    "Waveform Collapse",
    "Timeline Skim",
    "Temporal Desync",
    "Glitchy Jump-Cut",
    "Plasma Warm-Up",
    "Time Warp Streaks",
    "Character Scanline",
    "Focus In",
    "Code Breaker",
    "Temporal Paradox",
    "Digit Cascade",
    "Electric Surge",
    "Flip-Disc Display",
    "Interference Pattern",
    "Randomize All",
]

SELECTS: tuple[BTTFTimeCircuitsSelectEntityDescription, ...] = (
    BTTFTimeCircuitsSelectEntityDescription(
        key="display_mode",
        name="Display Mode",
        icon="mdi:television-guide",
        options=["Normal Clock", "Stock Ticker", "Weather", "Data Link"],
    ),
    BTTFTimeCircuitsSelectEntityDescription(
        key="animation_sequence",
        name="Default Animation Sequence",
        icon="mdi:play-box-outline",
        options=ANIMATION_OPTIONS,
    ),
    BTTFTimeCircuitsSelectEntityDescription(
        key="sequencer",
        name="Run Animation",
        icon="mdi:movie-play-outline",
        options=["Select a sequence"] + ANIMATION_OPTIONS,
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    Set up the BTTF Time Circuits select entities from a config entry.

    Args:
        hass: The Home Assistant instance.
        config_entry: The configuration entry for the integration.
        async_add_entities: A callback function to add the entities.
    """
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    entities = []
    for description in SELECTS:
        if description.key == "sequencer":
            entities.append(BTTFTimeCircuitsSequencerSelect(device, description))
        else:
            entities.append(BTTFTimeCircuitsSelect(device, description))
    async_add_entities(entities)


class BTTFTimeCircuitsSelect(BTTFTimeCircuitsEntity, SelectEntity):
    """Representation of a standard BTTF Time Circuits Select entity."""

    entity_description: BTTFTimeCircuitsSelectEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsSelectEntityDescription,
    ) -> None:
        """
        Initialize the select entity.

        Args:
            device: The BTTFTimeCircuitsDevice instance.
            description: The entity description for the select entity.
        """
        self.entity_description = description
        super().__init__(device)
        self._attr_current_option = None

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events when the entity is added to Home Assistant."""
        await super().async_added_to_hass()
        state_topic = f"{self._device.base_topic}/{self.entity_description.key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages for the select's state."""
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
        """
        Change the selected option.

        This is called when the user selects a new option in the UI. It
        publishes the new option to the corresponding MQTT command topic.

        Args:
            option: The new option selected by the user.
        """
        command_topic = (
            f"{self._device.base_topic}/{self.entity_description.key}/command"
        )
        await mqtt.async_publish(self.hass, command_topic, option, 1, False)


class BTTFTimeCircuitsSequencerSelect(BTTFTimeCircuitsSelect):
    """
    Representation of a "run-once" sequencer select entity.

    This select entity is designed to trigger an action rather than set a
    persistent state. When an option is selected, it sends the command to the
    device and then immediately resets itself to the default placeholder option.
    """

    async def async_added_to_hass(self) -> None:
        """Set the initial state of the dropdown to the placeholder."""
        self._attr_current_option = self.entity_description.options[0]
        self.async_write_ha_state()

    async def async_select_option(self, option: str) -> None:
        """
        Change the selected option and trigger the sequencer.

        This sends the selected animation name to the device and then resets
        the dropdown to the placeholder value after a short delay to provide
        visual feedback to the user.

        Args:
            option: The animation sequence to run.
        """
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