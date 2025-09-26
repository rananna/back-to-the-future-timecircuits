"""Notify platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import asyncio
from typing import Any

from homeassistant.components import mqtt
from homeassistant.components.notify import NotifyEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits notify entity."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]
    async_add_entities([BTTFTimeCircuitsNotifyEntity(device)])


class BTTFTimeCircuitsNotifyEntity(BTTFTimeCircuitsEntity, NotifyEntity):
    """Implementation of a notify entity for the BTTF Time Circuits."""

    def __init__(self, device: BTTFTimeCircuitsDevice) -> None:
        """Initialize the entity."""
        super().__init__(device)
        self._attr_name = f"{device.device_id} Time Circuits Message"
        self._attr_icon = "mdi:message-text"

    async def async_send_message(self, message: str = "", **kwargs: Any) -> None:
        """Send a message to the Time Circuits display."""
        data = kwargs.get("data") or {}
        sound_effect = data.get("sound_effect")
        duration = data.get("duration", 10)

        lines = message.split("\\n")
        line1 = lines[0] if len(lines) > 0 else ""
        line2 = lines[1] if len(lines) > 1 else ""
        line3 = lines[2] if len(lines) > 2 else ""

        base_topic = self._device.base_topic

        # 1. Publish messages
        await mqtt.async_publish(
            self.hass, f"{base_topic}/override_line_1/command", line1, 1, False
        )
        await mqtt.async_publish(
            self.hass, f"{base_topic}/override_line_2/command", line2, 1, False
        )
        await mqtt.async_publish(
            self.hass, f"{base_topic}/override_line_3/command", line3, 1, False
        )

        # 2. Play sound
        if sound_effect:
            await mqtt.async_publish(
                self.hass,
                f"{base_topic}/play_sound/command",
                sound_effect,
                1,
                False,
            )

        # 3. Turn on override
        await mqtt.async_publish(
            self.hass, f"{base_topic}/override/command", "ON", 1, False
        )

        # 4. Wait
        await asyncio.sleep(duration)

        # 5. Turn off override
        await mqtt.async_publish(
            self.hass, f"{base_topic}/override/command", "OFF", 1, False
        )