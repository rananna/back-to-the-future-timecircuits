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


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits text entities."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]

    @callback
    def async_discover(
        payload: str,
    ) -> None:
        """Discover and add a BTTF Time Circuits text entity."""
        try:
            config = json.loads(payload)
        except json.JSONDecodeError:
            _LOGGER.warning("Received malformed JSON for text discovery: %s", payload)
            return

        entity_description = BTTFTimeCircuitsTextEntityDescription(
            key=config["key"],
            name=config.get("name"),
            icon=config.get("icon"),
        )

        async_add_entities([BTTFTimeCircuitsText(device, entity_description, config)])

    await mqtt.async_subscribe(
        hass,
        f"{device.base_topic}/text/+/config",
        lambda msg: async_discover(msg.payload),
        0,
    )


class BTTFTimeCircuitsText(BTTFTimeCircuitsEntity, TextEntity):
    """Representation of a BTTF Time Circuits Text entity."""

    entity_description: BTTFTimeCircuitsTextEntityDescription

    def __init__(
        self,
        device: BTTFTimeCircuitsDevice,
        description: BTTFTimeCircuitsTextEntityDescription,
        config: dict,
    ) -> None:
        """Initialize the text entity."""
        self.entity_description = description
        self._config = config
        super().__init__(device)
        self._attr_native_value = None

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        state_topic = self._config.get("state_topic")

        if not state_topic:
            return

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            self._attr_native_value = msg.payload
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_set_value(self, value: str) -> None:
        """Update the current value."""
        command_topic = self._config.get("command_topic")
        if command_topic:
            await mqtt.async_publish(self.hass, command_topic, value, 1, False)
        # Optimistically update the state
        self._attr_native_value = value
        self.async_write_ha_state()