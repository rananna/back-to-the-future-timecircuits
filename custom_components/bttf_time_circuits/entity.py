"""Base entity for the Back to the Future Time Circuits integration."""
from __future__ import annotations

import logging

from homeassistant.helpers.entity import Entity

from . import BTTFTimeCircuitsDevice
from .const import DOMAIN

_LOGGER = logging.getLogger(__name__)


class BTTFTimeCircuitsEntity(Entity):
    """Base class for all BTTF Time Circuits entities."""

    _attr_has_entity_name = True

    def __init__(self, device: BTTFTimeCircuitsDevice) -> None:
        """Initialize the entity."""
        super().__init__()
        _LOGGER.debug("BTTFTimeCircuitsEntity.__init__")
        self._device = device
        self._attr_device_info = device.device_info
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