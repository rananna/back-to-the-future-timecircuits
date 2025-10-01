import os
import json
from playwright.sync_api import sync_playwright, expect

def run(playwright):
    """
    This script verifies that the 'DebugParallelLogic' sequence is correctly
    added to the sequencer dropdown in the web UI.
    """
    # Get the absolute path to the project's root directory
    base_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
    sequences_json_path = os.path.join(base_dir, 'data', 'sequences.json')

    # Use localhost where the python server is running
    html_file_url = "http://localhost:8000/index.html"

    browser = playwright.chromium.launch(headless=True)
    page = browser.new_page()

    # Mock the API response for /api/sequences
    def handle_route(route):
        """Intercepts the API call and returns the local JSON file."""
        # The URL in a server context will be http://localhost:8000/api/sequences
        if 'api/sequences' in route.request.url:
            print(f"Intercepted request to: {route.request.url}")
            with open(sequences_json_path, 'r') as f:
                sequences_data = json.load(f)
            route.fulfill(
                status=200,
                content_type='application/json',
                body=json.dumps(sequences_data)
            )
        else:
            route.continue_()

    # The route needs to match the full URL now
    page.route('http://localhost:8000/api/sequences', handle_route)

    # Navigate to the local HTML file via the server
    page.goto(html_file_url)

    # Find the sequencer dropdown using its ID
    sequencer_dropdown = page.locator("#sequencerSelect")

    # Wait for the dropdown to be populated by the mocked API response
    # We expect 19 options (18 from the file + the new one)
    expect(sequencer_dropdown.locator("option")).to_have_count(19, timeout=5000)

    # Click the dropdown to make the options visible
    sequencer_dropdown.click()

    # Check if the "Debug Parallel Logic" option is present and visible
    option_to_check = "Debug Parallel Logic"
    option_locator = sequencer_dropdown.locator(f"option:has-text('{option_to_check}')")
    expect(option_locator).to_be_visible()

    # Take a screenshot for visual verification
    screenshot_path = os.path.join(os.path.dirname(__file__), "verification.png")
    page.screenshot(path=screenshot_path)
    print(f"Screenshot saved to {screenshot_path}")

    browser.close()

with sync_playwright() as playwright:
    run(playwright)

print("Verification script finished and screenshot taken.")