"""Config flow for Back to the Future Time Circuits."""
from __future__ import annotations

from typing import Any

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.config_entries import ConfigFlow, ConfigFlowResult, OptionsFlow
from homeassistant.core import callback
from homeassistant.helpers import config_validation as cv

from .const import DOMAIN

RADIO_STATION_SCHEMA = vol.Schema(
    {
        vol.Required("name"): str,
        vol.Required("url"): cv.url,
    }
)


class BttfTimeCircuitsOptionsFlow(OptionsFlow):
    """Handle options for BTTF Time Circuits."""

    def __init__(self, config_entry: config_entries.ConfigEntry) -> None:
        """Initialize options flow."""
        self.config_entry = config_entry

    async def async_step_init(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Manage the radio stations."""
        return self.async_show_menu(
            step_id="init",
            menu_options=["add_station", "edit_station", "remove_station"],
        )

    async def async_step_add_station(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Handle adding a radio station."""
        if user_input is not None:
            stations = list(self.config_entry.options.get("radio_stations", []))
            stations.append(user_input)
            return self.async_create_entry(
                title="", data={"radio_stations": stations}
            )

        return self.async_show_form(
            step_id="add_station", data_schema=RADIO_STATION_SCHEMA
        )

    async def async_step_edit_station(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Handle editing a radio station."""
        stations = self.config_entry.options.get("radio_stations", [])
        station_names = [station["name"] for station in stations]

        if not station_names:
            return self.async_abort(reason="no_stations")

        if user_input is not None:
            # This is a multi-step process, first we select the station to edit
            if "station_to_edit" in user_input:
                self.station_to_edit = user_input["station_to_edit"]
                station_data = next(
                    (s for s in stations if s["name"] == self.station_to_edit), None
                )
                return self.async_show_form(
                    step_id="edit_station_details",
                    data_schema=vol.Schema(
                        {
                            vol.Required("name", default=station_data["name"]): str,
                            vol.Required("url", default=station_data["url"]): cv.url,
                        }
                    ),
                )
            # This is the second step, where we save the changes
            else:
                updated_stations = []
                for station in stations:
                    if station["name"] == self.station_to_edit:
                        updated_stations.append(user_input)
                    else:
                        updated_stations.append(station)
                return self.async_create_entry(
                    title="", data={"radio_stations": updated_stations}
                )

        return self.async_show_form(
            step_id="edit_station",
            data_schema=vol.Schema(
                {vol.Required("station_to_edit"): vol.In(station_names)}
            ),
        )

    async def async_step_edit_station_details(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Handle editing a radio station - part 2."""
        stations = list(self.config_entry.options.get("radio_stations", []))
        updated_stations = []
        for station in stations:
            if station["name"] == self.station_to_edit:
                updated_stations.append(user_input)
            else:
                updated_stations.append(station)
        return self.async_create_entry(
            title="", data={"radio_stations": updated_stations}
        )

    async def async_step_remove_station(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Handle removing a radio station."""
        stations = self.config_entry.options.get("radio_stations", [])
        station_names = [station["name"] for station in stations]

        if not station_names:
            return self.async_abort(reason="no_stations")

        if user_input is not None:
            stations_to_keep = [
                s for s in stations if s["name"] not in user_input["stations_to_remove"]
            ]
            return self.async_create_entry(
                title="", data={"radio_stations": stations_to_keep}
            )

        return self.async_show_form(
            step_id="remove_station",
            data_schema=vol.Schema(
                {vol.Required("stations_to_remove"): cv.multi_select(station_names)}
            ),
        )


@config_entries.HANDLERS.register(DOMAIN)
class BttfTimeCircuitsConfigFlow(ConfigFlow):
    """Handle a config flow for BTTF Time Circuits."""

    VERSION = 1

    @staticmethod
    @callback
    def async_get_options_flow(
        config_entry: config_entries.ConfigEntry,
    ) -> BttfTimeCircuitsOptionsFlow:
        """Get the options flow for this handler."""
        return BttfTimeCircuitsOptionsFlow(config_entry)

    async def async_step_user(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """Handle the initial step."""
        if user_input is not None:
            device_id = user_input["device_id"]

            existing_entry = await self.async_set_unique_id(device_id)
            if existing_entry:
                self.hass.config_entries.async_update_entry(
                    existing_entry, data=user_input
                )
                await self.hass.config_entries.async_reload(existing_entry.entry_id)
                return self.async_abort(reason="reconfigure_successful")

            return self.async_create_entry(
                title=f"BTTF Time Circuits {device_id}",
                data={"device_id": device_id},
            )

        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema({vol.Required("device_id"): str}),
        )
