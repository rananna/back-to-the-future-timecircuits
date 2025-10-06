from playwright.sync_api import sync_playwright, Page, expect

def verify_animation_dropdown(page: Page):
    """
    This script verifies that the animation sequence dropdown is present
    in the 'Temporal Controls' tab of the web UI.
    """
    # 1. Arrange: Go to the locally served web page.
    # The server is running from the 'data' directory, so index.html is at the root.
    page.goto("http://localhost:8000/index.html")

    # 2. Act: Click on the 'Temporal Controls' tab to reveal the settings.
    # The tab link has data-tab="Temporal".
    temporal_tab_link = page.get_by_role("button", name="Temporal Controls")
    expect(temporal_tab_link).to_be_visible()
    temporal_tab_link.click()

    # 3. Assert: Check that the animation sequence dropdown is now visible.
    # The dropdown has the ID 'sequenceSelect'.
    animation_sequence_dropdown = page.locator("#sequenceSelect")
    expect(animation_sequence_dropdown).to_be_visible()

    # 4. Screenshot: Capture the state of the 'Temporal Controls' tab for visual verification.
    temporal_settings_container = page.locator("#Temporal")
    temporal_settings_container.screenshot(path="jules-scratch/verification/verification.png")

# Boilerplate to run the verification
if __name__ == "__main__":
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        try:
            verify_animation_dropdown(page)
            print("Playwright script executed successfully.")
        except Exception as e:
            print(f"Playwright script failed: {e}")
        finally:
            browser.close()