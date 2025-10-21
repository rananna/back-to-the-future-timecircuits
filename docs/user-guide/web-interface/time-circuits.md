# Time Circuits Tab

This is the main screen for setting the time displays. It directly controls the "Destination Time" (top row) and "Last Time Departed" (bottom row) displays.

#### **Destination Time & Year**
This section controls the *top* display row.
- **Time Zone**: Use this dropdown to select the time zone for the destination time. This is useful for accurately setting times in different parts of the world.
- **YEAR**: Enter the four-digit destination year. The clock will instantly update the header display to reflect this year, using the current month, day, and time.

#### **Last Time Departed & Presets**
This section controls the *bottom* display row.

- **Static Time Display**: The text at the top of this section shows the full date and time that is currently set for the "Last Time Departed" display.

- **Famous & Custom Time Jumps**: This dropdown contains a list of dates from the movies and any custom presets you have saved. Selecting an option from this list will immediately update the "Last Time Departed" display.

- **Add/Edit Presets**: This form allows you to create, edit, and delete your own custom presets.
    - **To add a new preset**: Fill in the "Preset Name," "Date," and "Time" fields and click **"Add to Presets"**.
    - **To edit a preset**: Select a custom preset from the dropdown. The form will populate with its details. Make your changes and click **"Update Preset"**.
    - **To delete a preset**: Select a custom preset from the dropdown and click **"Delete Selected Preset"**.
    - **To create a new one after editing**: Click **"+ Create New Preset"** to clear the form.

- **Cycle Presets Every (min, 0=Off)**: This slider sets an interval in minutes for the clock to automatically cycle through all available presets (both famous and custom). Setting it to `0` disables this feature.
