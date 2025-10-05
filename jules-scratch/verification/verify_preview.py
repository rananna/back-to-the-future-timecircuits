from playwright.sync_api import sync_playwright, expect, Page
import os

def verify_animation_preview(page: Page):
    """
    Verifies that clicking the 'Preview' button for an animation style
    sends the command to the device.
    """
    # 1. Navigate to the local index.html file.
    # We use os.path.abspath to get the full path to the file.
    file_path = os.path.abspath('data/index.html')
    page.goto(f'file://{file_path}')

    # 2. Click the 'Temporal Controls' tab to reveal the animation settings.
    temporal_controls_tab = page.get_by_role("button", name="Temporal Controls")
    expect(temporal_controls_tab).to_be_visible()
    temporal_controls_tab.click()

    # 3. Select a specific animation style.
    animation_select = page.locator('#animationStyleSelect')
    expect(animation_select).to_be_visible()
    # Select by value, '4' corresponds to 'Tornado Flicker'
    animation_select.select_option('4')

    # 4. Click the 'Preview' button.
    preview_button = page.locator('#previewAnimationBtn')
    expect(preview_button).to_be_visible()
    preview_button.click()

    # 5. Assert that the confirmation message appears.
    # This confirms our JavaScript change was successful.
    success_message = page.locator('#messageBanner')
    expect(success_message).to_have_text('Previewing animation on device...')
    expect(success_message).to_be_visible()

    # 6. Take a screenshot for visual confirmation.
    page.screenshot(path="jules-scratch/verification/verification.png")

def main():
    with sync_playwright() as p:
        browser = p.chromium.launch(headless=True)
        page = browser.new_page()
        verify_animation_preview(page)
        browser.close()

if __name__ == "__main__":
    main()