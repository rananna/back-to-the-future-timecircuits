"""Config flow for Back to the Future Time Circuits."""
from __future__ import annotations

from typing import Any

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.config_entries import ConfigFlow, ConfigFlowResult

from .const import DOMAIN


@config_entries.HANDLERS.register(DOMAIN)
class BttfTimeCircuitsConfigFlow(ConfigFlow):
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
