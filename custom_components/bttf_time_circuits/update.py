"""Update platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import asyncio
from typing import Any

import aiohttp
from homeassistant.components import mqtt
from homeassistant.components.update import (
    UpdateDeviceClass,
    UpdateEntity,
    UpdateEntityFeature,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

OTA_PASSWORD = "1.21gigawatts"
GITHUB_REPO = "rananna/back-to-the-future-timecircuits"
FIRMWARE_FILE_NAME = "firmware.bin"  # Confirm this is the correct filename


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits update entity."""
    device: BTTFTimeCircuitsDevice = hass.data[DOMAIN][config_entry.entry_id]
    async_add_entities([BTTFTimeCircuitsUpdateEntity(device)])


class BTTFTimeCircuitsUpdateEntity(BTTFTimeCircuitsEntity, UpdateEntity):
    """Representation of a BTTF Time Circuits Update Entity."""

    _attr_has_entity_name = True
    _attr_name = "Firmware"
    _attr_device_class = UpdateDeviceClass.FIRMWARE
    _attr_supported_features = (
        UpdateEntityFeature.INSTALL | UpdateEntityFeature.PROGRESS
    )

    def __init__(self, device: BTTFTimeCircuitsDevice) -> None:
        """Initialize the update entity."""
        super().__init__(device)
        self._attr_unique_id = f"{DOMAIN}_{self._device.device_id}_update"
        self._attr_title = "Time Circuits Firmware"
        self._attr_installed_version = "0.0.0"  # Initial value
        self._attr_latest_version = "0.0.0"  # Initial value
        self._release_url = None
        self._firmware_url = None
        self._device_ip = None

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()

        @callback
        def sw_version_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages for software version."""
            self._attr_installed_version = msg.payload
            self.async_write_ha_state()

        @callback
        def ip_address_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages for IP address."""
            self._device_ip = msg.payload
            self.async_write_ha_state()

        await mqtt.async_subscribe(
            self.hass, f"{self._device.base_topic}/sw_version/state", sw_version_received, 1
        )
        await mqtt.async_subscribe(
            self.hass, f"{self._device.base_topic}/ip_address/state", ip_address_received, 1
        )

    async def async_update(self) -> None:
        """Check for the latest version."""
        url = f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest"
        async with aiohttp.ClientSession() as session:
            try:
                async with session.get(url, timeout=10) as response:
                    if response.status == 200:
                        data = await response.json()
                        self._attr_latest_version = data.get(
                            "tag_name", self._attr_installed_version
                        )
                        self._release_url = data.get("html_url")
                        for asset in data.get("assets", []):
                            if asset.get("name") == FIRMWARE_FILE_NAME:
                                self._firmware_url = asset.get("browser_download_url")
                                break
                    else:
                        self._attr_latest_version = self._attr_installed_version
            except (asyncio.TimeoutError, aiohttp.ClientError):
                self._attr_latest_version = self._attr_installed_version

        self.async_write_ha_state()

    async def async_install(
        self, version: str | None, backup: bool, **kwargs: Any
    ) -> None:
        """Install the latest version."""
        if not self._firmware_url or not self._device_ip:
            return

        self._attr_in_progress = True
        self.async_write_ha_state()

        try:
            async with aiohttp.ClientSession() as session:
                async with session.get(self._firmware_url) as fw_response:
                    if fw_response.status != 200:
                        return
                    firmware_data = await fw_response.read()

                update_url = f"http://{self._device_ip}/update"
                headers = {"X-Auth-Password": OTA_PASSWORD}
                async with session.post(
                    update_url, data=firmware_data, headers=headers, timeout=120
                ) as update_response:
                    if update_response.status == 200:
                        await asyncio.sleep(15)
        finally:
            self._attr_in_progress = False
            self.async_write_ha_state()