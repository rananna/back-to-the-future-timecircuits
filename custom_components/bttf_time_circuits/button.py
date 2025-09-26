"""Button platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

from dataclasses import dataclass

from homeassistant.components import mqtt
from homeassistant.components.button import (
    ButtonDeviceClass,
    ButtonEntity,
    ButtonEntityDescription,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

@dataclass
class BTTFTimeCircuitsButtonEntityDescription(ButtonEntityDescription):
    """A class that describes BTTF Time Circuits button entities."""

BUTTONS: tuple[BTTFTimeCircuitsButtonEntityDescription, ...] = (
    BTTFTimeCircuitsButtonEntityDescription(
        key="trigger_animation",
        name="Trigger Animation",
        icon="mdi:movie-play",
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="reboot_device",
        name="Reboot Device",
        icon="mdi:restart",
        device_class=ButtonDeviceClass.RESTART,
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="force_ntp_sync",
        name="Force NTP Sync",
        icon="mdi:timer-sync-outline",
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="factory_reset",
        name="Factory Reset",
        icon="mdi:delete-restore",
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="save_all_settings",
        name="Save All Settings",
        icon="mdi:content-save-all-outline",
    ),
    BTTFTimeCircuitsButtonEntityDescription(
        key="refresh_weather_data",
        name="Refresh Weather Data",
        icon="mdi:refresh",
    ),
)


async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits buttons."""
    device_id = "BTTF_TC_123456"  # Placeholder
    entities = [BTTFTimeCircuitsButton(device_id, description) for description in BUTTONS]
    async_add_entities(entities)


class BTTFTimeCircuitsButton(BTTFTimeCircuitsEntity, ButtonEntity):
    """Representation of a BTTF Time Circuits Button."""

    entity_description: BTTFTimeCircuitsButtonEntityDescription

    def __init__(self, device_id: str, description: BTTFTimeCircuitsButtonEntityDescription) -> None:
        """Initialize the button."""
        super().__init__(device_id)
        self.entity_description = description
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_{description.key}"

    async def async_press(self) -> None:
        """Handle the button press."""
        command_topic = f"BTTF_TC/{self._device_id}/{self.entity_description.key}/command"
        await mqtt.async_publish(self.hass, command_topic, "PRESS", 1, False)