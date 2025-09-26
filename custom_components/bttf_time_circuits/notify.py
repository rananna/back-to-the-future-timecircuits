"""Notify platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import asyncio
from typing import Any

from homeassistant.components import mqtt
from homeassistant.components.notify import BaseNotificationService
from homeassistant.core import HomeAssistant
from homeassistant.helpers.typing import ConfigType, DiscoveryInfoType

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN


async def async_get_service(
    hass: HomeAssistant,
    config: ConfigType,
    discovery_info: DiscoveryInfoType | None = None,
) -> BTTFTimeCircuitsNotificationService | None:
    """Return the notification service."""
    # The service is only available if there are any BTTF devices.
    if not any(
        isinstance(device, BTTFTimeCircuitsDevice)
        for device in hass.data.get(DOMAIN, {}).values()
    ):
        return None

    return BTTFTimeCircuitsNotificationService(hass)


class BTTFTimeCircuitsNotificationService(BaseNotificationService):
    """Implementation of a notification service for the BTTF Time Circuits."""

    def __init__(self, hass: HomeAssistant) -> None:
        """Initialize the service."""
        self.hass = hass

    @property
    def targets(self) -> dict[str, str]:
        """Return a dictionary of registered devices."""
        targets = {}
        for device in self.hass.data.get(DOMAIN, {}).values():
            if isinstance(device, BTTFTimeCircuitsDevice):
                targets[device.device_id] = device.device_id
        return targets

    async def async_send_message(self, message: str = "", **kwargs: Any) -> None:
        """Send a message to the Time Circuits display."""
        target_device_ids = kwargs.get("target") or self.targets.keys()
        if not target_device_ids:
            return

        data = kwargs.get("data") or {}
        sound_effect = data.get("sound_effect")
        duration = data.get("duration", 10)  # Default to 10 seconds

        lines = message.split("\\n")
        line1 = lines[0] if len(lines) > 0 else ""
        line2 = lines[1] if len(lines) > 1 else ""
        line3 = lines[2] if len(lines) > 2 else ""

        devices_to_notify = [
            device
            for device_id in target_device_ids
            if (
                device := next(
                    (
                        d
                        for d in self.hass.data[DOMAIN].values()
                        if isinstance(d, BTTFTimeCircuitsDevice)
                        and d.device_id == device_id
                    ),
                    None,
                )
            )
            is not None
        ]

        if not devices_to_notify:
            return

        for device in devices_to_notify:
            base_topic = device.base_topic
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
        for device in devices_to_notify:
            base_topic = device.base_topic
            await mqtt.async_publish(
                self.hass, f"{base_topic}/override/command", "OFF", 1, False
            )