"""Base entity for the Back to the Future Time Circuits integration."""
from __future__ import annotations

from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity import Entity

from .const import DOMAIN


class BTTFTimeCircuitsEntity(Entity):
    """Base class for all BTTF Time Circuits entities."""

    _attr_has_entity_name = True

    def __init__(self, device_id: str) -> None:
        """Initialize the entity."""
        self._device_id = device_id
        self._attr_device_info = DeviceInfo(
            identifiers={(DOMAIN, self._device_id)},
            name="Time Circuits",
            manufacturer="dmadison",
            model="ESP32",
            sw_version="1.0.0",  # This will be updated by the update entity later
        )
        # Construct the unique ID from the domain, device ID, and entity key
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_{self.entity_description.key}"

    @property
    def available(self) -> bool:
        """Return True if the device is available."""
        # This will be improved later with a coordinator to track online status
        return True