// Test-only capture of this application's own candidate renderer.
static WKWebView *tailkanaWebView(NSView *view) {
    if ([view isKindOfClass:[WKWebView class]]) return (WKWebView *)view;
    for (NSView *child in view.subviews) {
        if (WKWebView *found = tailkanaWebView(child)) return found;
    }
    return nil;
}
static void tailkanaCapture(NSWindow *window) {
    const char *directory = getenv("TAILKANA_PREVIEW_CAPTURE");
    if (!directory) return;
    NSString *path = [NSString stringWithUTF8String:directory];
    NSDictionary *state = @{@"visible": @(window.isVisible), @"frame": NSStringFromRect(window.frame)};
    NSData *json = [NSJSONSerialization dataWithJSONObject:state options:0 error:nil];
    [json writeToFile:[path stringByAppendingPathComponent:@"panel-window.json"] atomically:YES];
    WKWebView *view = tailkanaWebView(window.contentView);
    if (!view) return;
    [view takeSnapshotWithConfiguration:nil completionHandler:^(NSImage *snapshot, NSError *error) {
        if (error || !snapshot) {
            [error.description writeToFile:[path stringByAppendingPathComponent:@"panel-snapshot-error.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil];
            return;
        }
        NSBitmapImageRep *bitmap = [NSBitmapImageRep imageRepWithData:snapshot.TIFFRepresentation];
        NSData *png = [bitmap representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
        [png writeToFile:[path stringByAppendingPathComponent:@"panel.png"] atomically:YES];
    }];
}
