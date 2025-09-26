"""Switch platform for the Back to the Future Time Circuits integration."""
from __future__ import annotations

from dataclasses import dataclass
from typing import Any

from homeassistant.components import mqtt
from homeassistant.components.switch import (
    SwitchEntity,
    SwitchEntityDescription,
)
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import DOMAIN
from .entity import BTTFTimeCircuitsEntity

@dataclass
class BTTFTimeCircuitsSwitchEntityDescription(SwitchEntityDescription):
    """A class that describes BTTF Time Circuits switch entities."""
    # For display mode switches, this is the payload to check for 'on' state
    on_payload: str | None = None

# Define all switch entities
SWITCHES: list[BTTFTimeCircuitsSwitchEntityDescription] = [
    BTTFTimeCircuitsSwitchEntityDescription(
        key="override_switch",
        name="Override Switch",
        icon="mdi:message-cog",
    ),
    BTTFTimeCircuitsSwitchEntityDescription(
        key="24h_format",
        name="24-Hour Format",
        icon="mdi:clock-time-twelve-outline",
    ),
    BTTFTimeCircuitsSwitchEntityDescription(
        key="temporal_echo",
        name="Temporal Echo Effect",
        icon="mdi:ghost",
    ),
    # Display Mode Switches
    BTTFTimeCircuitsSwitchEntityDescription(
        key="display_mode_stock",
        name="Stock Ticker Mode",
        icon="mdi:chart-line",
        on_payload="Stock Ticker",
    ),
    BTTFTimeCircuitsSwitchEntityDescription(
        key="display_mode_weather",
        name="Weather Mode",
        icon="mdi:weather-cloudy",
        on_payload="Weather",
    ),
    BTTFTimeCircuitsSwitchEntityDescription(
        key="display_mode_datalink",
        name="Data Link Mode",
        icon="mdi:link-variant",
        on_payload="Data Link",
    ),
]

# Add the 5 Data Point enabled switches
for i in range(5):
    SWITCHES.append(
        BTTFTimeCircuitsSwitchEntityDescription(
            key=f"datapoint_{i}_enabled",
            name=f"Data Point {i+1} Enabled",
            icon="mdi:toggle-switch",
        )
    )

async def async_setup_entry(
    hass: HomeAssistant,
    config_entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """Set up the BTTF Time Circuits switches."""
    device_id = "BTTF_TC_123456"  # Placeholder

    entities = []
    for description in SWITCHES:
        if description.on_payload:
            entities.append(BTTFTimeCircuitsDisplayModeSwitch(device_id, description))
        else:
            entities.append(BTTFTimeCircuitsMqttSwitch(device_id, description))

    async_add_entities(entities)


class BTTFTimeCircuitsMqttSwitch(BTTFTimeCircuitsEntity, SwitchEntity):
    """Representation of a standard BTTF Time Circuits MQTT Switch."""

    def __init__(self, device_id: str, description: BTTFTimeCircuitsSwitchEntityDescription) -> None:
        """Initialize the switch."""
        super().__init__(device_id)
        self.entity_description = description
        # The MQTT topic key is different from the entity key for override_switch
        self._mqtt_key = "override" if description.key == "override_switch" else description.key
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_{description.key}"
        self._attr_is_on = False

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        state_topic = f"BTTF_TC/{self._device_id}/{self._mqtt_key}/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            self._attr_is_on = msg.payload == "ON"
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn the switch on."""
        command_topic = f"BTTF_TC/{self._device_id}/{self._mqtt_key}/command"
        await mqtt.async_publish(self.hass, command_topic, "ON", 1, False)

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn the switch off."""
        command_topic = f"BTTF_TC/{self._device_id}/{self._mqtt_key}/command"
        await mqtt.async_publish(self.hass, command_topic, "OFF", 1, False)


class BTTFTimeCircuitsDisplayModeSwitch(BTTFTimeCircuitsEntity, SwitchEntity):
    """Representation of a BTTF Time Circuits Display Mode Switch."""

    entity_description: BTTFTimeCircuitsSwitchEntityDescription

    def __init__(self, device_id: str, description: BTTFTimeCircuitsSwitchEntityDescription) -> None:
        """Initialize the display mode switch."""
        super().__init__(device_id)
        self.entity_description = description
        self._attr_unique_id = f"{DOMAIN}_{self._device_id}_{description.key}"
        self._attr_is_on = False

    async def async_added_to_hass(self) -> None:
        """Subscribe to MQTT events."""
        await super().async_added_to_hass()
        state_topic = f"BTTF_TC/{self._device_id}/display_mode/state"

        @callback
        def message_received(msg: mqtt.ReceiveMessage) -> None:
            """Handle new MQTT messages."""
            self._attr_is_on = msg.payload == self.entity_description.on_payload
            self.async_write_ha_state()

        await mqtt.async_subscribe(self.hass, state_topic, message_received, 1)

    async def async_turn_on(self, **kwargs: Any) -> None:
        """Turn the switch on."""
        command_topic = f"BTTF_TC/{self._device_id}/display_mode/command"
        await mqtt.async_publish(self.hass, command_topic, self.entity_description.on_payload, 1, False)

    async def async_turn_off(self, **kwargs: Any) -> None:
        """Turn the switch off by reverting to Normal Clock mode."""
        command_topic = f"BTTF_TC/{self._device_id}/display_mode/command"
        await mqtt.async_publish(self.hass, command_topic, "Normal Clock", 1, False)