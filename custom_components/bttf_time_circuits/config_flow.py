"""Config flow for Back to the Future Time Circuits."""
from __future__ import annotations

import json
from typing import Any

import voluptuous as vol
from homeassistant.config_entries import ConfigFlow, ConfigFlowResult
from homeassistant.helpers.typing import DiscoveryInfoType

from .const import DOMAIN


class BttfTimeCircuitsConfigFlow(ConfigFlow, domain=DOMAIN):
    """Handle a config flow for BTTF Time Circuits."""

    VERSION = 1

    async def async_step_user(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Handle the initial step."""
        if user_input is not None:
            device_id = user_input["device_id"]
            await self.async_set_unique_id(device_id)
            self._abort_if_unique_id_configured()
            return self.async_create_entry(
                title=f"BTTF Time Circuits {device_id}",
                data={"device_id": device_id},
            )

        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema({vol.Required("device_id"): str}),
        )
