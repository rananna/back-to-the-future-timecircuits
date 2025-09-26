"""Base entity for the Back to the Future Time Circuits integration."""
from __future__ import annotations

from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity import Entity

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN


class BTTFTimeCircuitsEntity(Entity):
    """Base class for all BTTF Time Circuits entities."""

    _attr_has_entity_name = True

    def __init__(self, device: BTTFTimeCircuitsDevice) -> None:
        """Initialize the entity."""
        self._device = device
        self._attr_device_info = DeviceInfo(
            identifiers={(DOMAIN, self._device.device_id)},
            name="Time Circuits",
            manufacturer="dmadison",
            model="ESP32",
            sw_version="1.0.0",  # This will be updated by the update entity later
        )
        # Construct the unique ID for entities that have an entity description
        if hasattr(self, "entity_description") and self.entity_description:
            self._attr_unique_id = (
                f"{DOMAIN}_{self._device.device_id}_{self.entity_description.key}"
            )

    @property
    def available(self) -> bool:
        """Return True if the device is available."""
        # This will be improved later with a coordinator to track online status
        return True