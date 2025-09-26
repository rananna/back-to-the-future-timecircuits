"""Config flow for Back to the Future Time Circuits."""
from __future__ import annotations

import voluptuous as vol

from homeassistant.config_entries import ConfigFlow
from homeassistant.helpers.typing import DiscoveryInfoType

from .const import DOMAIN


class BTTFTimeCircuitsConfigFlow(ConfigFlow, domain=DOMAIN):
    """Handle a config flow for BTTF Time Circuits."""

    VERSION = 1

    async def async_step_user(self, user_input=None):
        """Handle the initial step."""
        if self._async_current_entries():
            return self.async_abort(reason="single_instance_allowed")

        return self.async_create_entry(title="BTTF Time Circuits", data={})

    async def async_step_mqtt(self, discovery_info: DiscoveryInfoType) -> dict:
        """Handle MQTT discovery."""
        # This flow is triggered by MQTT discovery, but we don't need to do
        # anything with the discovery info because the integration will be
        # set up to listen to all devices that are discovered on the
        # BTTF_TC topic.
        #
        # We just need to make sure that a config entry exists.
        if self._async_current_entries():
            return self.async_abort(reason="single_instance_allowed")

        return self.async_create_entry(title="BTTF Time Circuits", data={})