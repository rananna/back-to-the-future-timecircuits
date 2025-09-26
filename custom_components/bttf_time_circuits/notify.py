"""Notify platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import asyncio

from homeassistant.components import mqtt
from homeassistant.components.notify import BaseNotificationService
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.typing import ConfigType, DiscoveryInfoType

from .const import DOMAIN

async def async_get_service(
    hass: HomeAssistant,
    config: ConfigType,
    discovery_info: DiscoveryInfoType | None = None,
) -> BTTFTimeCircuitsNotificationService | None:
    """Return the notification service."""
    # In a real implementation, we would get the device_id from discovery_info
    # or a coordinator. For now, we use a placeholder.
    device_id = "BTTF_TC_123456"
    return BTTFTimeCircuitsNotificationService(hass, device_id)


class BTTFTimeCircuitsNotificationService(BaseNotificationService):
    """Implementation of a notification service for the BTTF Time Circuits."""

    def __init__(self, hass: HomeAssistant, device_id: str) -> None:
        """Initialize the service."""
        self.hass = hass
        self._device_id = device_id

    async def async_send_message(self, message: str = "", **kwargs: Any) -> None:
        """Send a message to the Time Circuits display."""

        # Extract data from the service call
        data = kwargs.get("data") or {}
        sound_effect = data.get("sound_effect")
        duration = data.get("duration", 10) # Default to 10 seconds

        # Split the message into up to three lines
        lines = message.split("\\n")
        line1 = lines[0] if len(lines) > 0 else ""
        line2 = lines[1] if len(lines) > 1 else ""
        line3 = lines[2] if len(lines) > 2 else ""

        base_topic = f"BTTF_TC/{self._device_id}"

        # 1. Publish the messages to the override line topics
        await mqtt.async_publish(self.hass, f"{base_topic}/override_line_1/command", line1, 1, False)
        await mqtt.async_publish(self.hass, f"{base_topic}/override_line_2/command", line2, 1, False)
        await mqtt.async_publish(self.hass, f"{base_topic}/override_line_3/command", line3, 1, False)

        # 2. Play a sound if one was provided
        if sound_effect:
            await mqtt.async_publish(self.hass, f"{base_topic}/play_sound/command", sound_effect, 1, False)

        # 3. Turn on the override switch to display the message
        await mqtt.async_publish(self.hass, f"{base_topic}/override/command", "ON", 1, False)

        # 4. Wait for the specified duration
        await asyncio.sleep(duration)

        # 5. Turn off the override switch to return to normal
        await mqtt.async_publish(self.hass, f"{base_topic}/override/command", "OFF", 1, False)