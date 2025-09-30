from playwright.sync_api import sync_playwright, Page, expect
import pathlib

def run_verification(page: Page):
    # Get the absolute path to the index.html file
    file_path = pathlib.Path("data/index.html").resolve()

    # Go to the local HTML file
    page.goto(f"file://{file_path}")

    # Click the "Temporal Controls" tab
    page.get_by_role("button", name="Temporal Controls").click()

    # Select the "Sparkle Reveal" option from the dropdown
    animation_select = page.locator("#animationStyleSelect")
    animation_select.select_option(label="Sparkle Reveal")

    # Expect the dropdown to have the correct value
    expect(animation_select).to_have_value("28")

    # Take a screenshot of the dropdown area
    page.locator("#DisplaySound").screenshot(path="jules-scratch/verification/verification.png")

with sync_playwright() as p:
    browser = p.chromium.launch(headless=True)
    page = browser.new_page()
    run_verification(page)
    browser.close()