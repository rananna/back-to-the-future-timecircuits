"""Update platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import asyncio
from typing import Any

import aiohttp
from homeassistant.components.update import (
    UpdateDeviceClass,
    UpdateEntity,
    UpdateEntityFeature,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

OTA_PASSWORD = "1.21gigawatts"
GITHUB_REPO = "dmadison/bttf-time-circuits-clock"
FIRMWARE_FILE_NAME = "firmware.bin" # Placeholder, will need to confirm actual filename

async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits update entity."""
    device_id = "BTTF_TC_123456"  # Placeholder
    async_add_entities([BTTFTimeCircuitsUpdateEntity(device_id)])


class BTTFTimeCircuitsUpdateEntity(BTTFTimeCircuitsEntity, UpdateEntity):
    """Representation of a BTTF Time Circuits Update Entity."""

    _attr_has_entity_name = True
    _attr_name = "Firmware"
    _attr_device_class = UpdateDeviceClass.FIRMWARE
    _attr_supported_features = (
        UpdateEntityFeature.INSTALL | UpdateEntityFeature.PROGRESS
    )

    def __init__(self, device_id: str) -> None:
        """Initialize the update entity."""
        super().__init__(device_id)
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_update"
        self._attr_title = "Time Circuits Firmware"
        self._attr_installed_version = "1.0.0" # Will be updated from device status
        self._attr_latest_version = "1.0.0"
        self._release_url = None
        self._firmware_url = None

    async def async_update(self) -> None:
        """Check for the latest version."""
        # In a real implementation, we would get the installed version from the device
        # via the status sensor's attributes. For now, we'll keep it static.

        url = f"https://api.github.com/repos/{GITHUB_REPO}/releases/latest"
        async with aiohttp.ClientSession() as session:
            async with session.get(url) as response:
                if response.status == 200:
                    data = await response.json()
                    self._attr_latest_version = data.get("tag_name", self._attr_installed_version)
                    self._release_url = data.get("html_url")

                    # Find the correct firmware asset
                    for asset in data.get("assets", []):
                        if asset.get("name") == FIRMWARE_FILE_NAME:
                            self._firmware_url = asset.get("browser_download_url")
                            break
                else:
                    # Could not fetch latest version, assume up-to-date
                    self._attr_latest_version = self._attr_installed_version

        self.async_write_ha_state()

    async def async_install(
        self, version: str | None, backup: bool, **kwargs: Any
    ) -> None:
        """Install the latest version."""
        if not self._firmware_url:
            return

        self._attr_in_progress = True
        self.async_write_ha_state()

        try:
            async with aiohttp.ClientSession() as session:
                # 1. Download the firmware
                async with session.get(self._firmware_url) as fw_response:
                    if fw_response.status != 200:
                        return # Failed to download

                    firmware_data = await fw_response.read()

                # 2. Upload to the device
                # In a real implementation, we'd get the device IP from a coordinator
                device_ip = "192.168.1.123" # Placeholder
                update_url = f"http://{device_ip}/update"
                headers = {"X-Auth-Password": OTA_PASSWORD}

                async with session.post(update_url, data=firmware_data, headers=headers, timeout=120) as update_response:
                    if update_response.status == 200:
                        # Success
                        await asyncio.sleep(15) # Give device time to reboot
                    else:
                        # Upload failed
                        pass

        finally:
            self._attr_in_progress = False
            self.async_write_ha_state()