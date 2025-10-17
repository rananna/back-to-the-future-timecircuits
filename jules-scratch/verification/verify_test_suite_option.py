from playwright.sync_api import sync_playwright, expect
import os

def run(playwright):
    browser = playwright.chromium.launch(headless=True)
    page = browser.new_page()

    # Navigate to the local web server
    page.goto('http://localhost:8000/index.html')

    # Wait for the UI to be initialized by looking for a known element
    temporal_controls_tab = page.locator('a[href="#temporal-controls"]')
    expect(temporal_controls_tab).to_be_visible(timeout=10000) # Wait up to 10 seconds

    # Click the "Temporal Controls" tab
    temporal_controls_tab.click()

    # Take a screenshot of the animation sequence dropdown
    sequence_dropdown = page.locator('#animationSequence')
    expect(sequence_dropdown).to_be_visible() # Ensure the dropdown is visible before screenshot

    # Click the dropdown to show the options
    sequence_dropdown.click()

    # Wait for the options to be populated
    page.wait_for_timeout(500) # A small delay to ensure options are rendered

    # Take a screenshot of the entire page to show the open dropdown
    page.screenshot(path='jules-scratch/verification/verification.png')

    browser.close()

with sync_playwright() as playwright:
    run(playwright)