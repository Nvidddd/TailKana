void WebviewCandidateWindow::set_tailkana_appearance(int value) const {
    HoverableWindow *window = unwrap_webview_handle<HoverableWindow>(w_->window());
    // 1–3: transparent glass; 4–6: solid; 0 restores the other engine's style.
    if (value < 0 || value > 6 || window.tailKanaAppearance == value) return;
    window.tailKanaAppearance = value;
    NSInteger scheme = value == 0 ? 0 : (value - 1) % 3 + 1;
    window.visualView.appearance = (scheme == 0 || scheme == 3) ? nil : [NSAppearance appearanceNamed:scheme == 2 ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua];
    window.tailKanaApplyingBlur = YES;
    set_native_blur(value == 0 ? static_cast<blur_t>(window.configuredBlur) : blur_t::blur);
    set_native_shadow(window.configuredShadow);
    window.tailKanaApplyingBlur = NO;
}
