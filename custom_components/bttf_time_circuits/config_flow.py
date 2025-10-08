"""Config flow for the Back to the Future Time Circuits integration."""
from __future__ import annotations

from typing import Any

import voluptuous as vol
from homeassistant import config_entries
from homeassistant.config_entries import ConfigFlow, ConfigFlowResult, OptionsFlow
from homeassistant.core import callback
from homeassistant.data_entry_flow import FlowResult
from homeassistant.helpers import config_validation as cv

from .const import DOMAIN

RECONFIGURE_SCHEMA = vol.Schema({vol.Required("device_id"): str})

RADIO_STATION_SCHEMA = vol.Schema(
    {
        vol.Required("name"): str,
        vol.Required("url"): cv.url,
    }
)


class BttfTimeCircuitsOptionsFlow(OptionsFlow):
    """
    Handle an options flow for BTTF Time Circuits.

    This flow allows users to configure options for the integration after it has
    been set up, such as managing a list of favorite radio stations.
    """

    def __init__(self, config_entry: config_entries.ConfigEntry) -> None:
        """
        Initialize the options flow.

        Args:
            config_entry: The configuration entry for which to show options.
        """
        self.config_entry = config_entry

    async def async_step_init(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """
        Manage the options flow.

        This is the initial step of the options flow, presenting a menu
        of actions to the user.

        Args:
            user_input: User-provided input.

        Returns:
            The result of the step.
        """
        return self.async_show_menu(
            step_id="init",
            menu_options=["add_station", "edit_station", "remove_station"],
        )

    async def async_step_add_station(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """
        Handle adding a radio station.

        This step shows a form to add a new radio station and saves it to the
        integration's options.

        Args:
            user_input: The user-provided data for the new station.

        Returns:
            The result of the step.
        """
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
        """
        Handle editing a radio station (step 1: selection).

        This step presents a dropdown of existing radio stations for the user
        to choose which one to edit.

        Args:
            user_input: The user's selection.

        Returns:
            The result of the step.
        """
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
        """
        Handle editing a radio station (step 2: editing details).

        This step shows a form pre-filled with the selected station's details
        and saves the updated information.

        Args:
            user_input: The updated station details from the user.

        Returns:
            The result of the step.
        """
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
        """
        Handle removing one or more radio stations.

        This step shows a multi-select list of existing stations and removes
        the selected ones from the configuration.

        Args:
            user_input: The user's selection of stations to remove.

        Returns:
            The result of the step.
        """
        stations = self.config_entry.options.get("radio_stations", [])
        station_names = [station["name"] for station in stations]

        if not station_names:
            return self.async_abort(reason="no_stations")

        if user_input is not None:
            stations_to_keep = [
                s
                for s in stations
                if s["name"] not in user_input["stations_to_remove"]
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
class BttfTimeCircuitsConfigFlow(ConfigFlow, domain=DOMAIN):
    """
    Handle a config flow for BTTF Time Circuits.

    This class manages the setup and reconfiguration of the integration.
    """

    VERSION = 2

    entry: config_entries.ConfigEntry | None

    @staticmethod
    @callback
    def async_get_options_flow(
        config_entry: config_entries.ConfigEntry,
    ) -> BttfTimeCircuitsOptionsFlow:
        """
        Get the options flow for this handler.

        Args:
            config_entry: The configuration entry.

        Returns:
            An instance of the options flow handler.
        """
        return BttfTimeCircuitsOptionsFlow(config_entry)

    async def async_step_reconfigure(
        self, user_input: dict[str, Any] | None = None
    ) -> FlowResult:
        """
        Handle a reconfiguration flow.

        This is triggered when an existing entry needs to be reconfigured,
        for example, to change the device ID.

        Args:
            user_input: The user-provided data.

        Returns:
            The result of the flow.
        """
        self.entry = self.hass.config_entries.async_get_entry(
            self.context["entry_id"]
        )

        if self.entry is None:
            return self.async_abort(reason="reconfigure_failed")

        if user_input is not None:
            new_data = self.entry.data.copy()
            new_data["device_id"] = user_input["device_id"]

            self.hass.config_entries.async_update_entry(
                self.entry,
                data=new_data,
            )
            await self.hass.config_entries.async_reload(self.entry.entry_id)
            return self.async_abort(reason="reconfigure_successful")

        return self.async_show_form(
            step_id="reconfigure",
            data_schema=RECONFIGURE_SCHEMA,
        )

    async def async_step_user(
        self, user_input: dict[str, Any] | None = None
    ) -> ConfigFlowResult:
        """
        Handle the initial user setup step.

        This step is called when a user initiates the setup flow. It prompts
        for the device ID and creates a new configuration entry.

        Args:
            user_input: The user-provided data.

        Returns:
            The result of the flow.
        """
        if user_input is not None:
            device_id = user_input["device_id"]

            existing_entry = await self.async_set_unique_id(device_id)
            if existing_entry:
                self.context["entry_id"] = existing_entry.entry_id
                return await self.async_step_reconfigure()

            return self.async_create_entry(
                title=f"BTTF Time Circuits {device_id}",
                data={"device_id": device_id},
            )

        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema({vol.Required("device_id"): str}),
        )
