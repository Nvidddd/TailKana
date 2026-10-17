// SPDX-License-Identifier: GPL-3.0-or-later
#import <AppKit/AppKit.h>

// The transparent WKWebView includes space for the CSS shadow. A native focus
// ring or window shadow would outline that larger rectangle a second time.
static void tailKanaDisableCandidateFocusRings(NSView *view) {
    view.focusRingType = NSFocusRingTypeNone;
    for (NSView *child in view.subviews) tailKanaDisableCandidateFocusRings(child);
}
static BOOL tailKanaCandidateNativeShadow(NSInteger appearance, BOOL configured) {
    return appearance == 0 && configured;
}

// Candidate controls must receive mouse events while the client application
// keeps keyboard focus. Use the same public NSPanel policy as Squirrel.
@interface TailKanaCandidatePanel : NSPanel
@end
@implementation TailKanaCandidatePanel
- (instancetype)initWithContentRect:(NSRect)rect
                         styleMask:(NSWindowStyleMask)style
                           backing:(NSBackingStoreType)backing
                             defer:(BOOL)defer {
    self = [super initWithContentRect:rect
                          styleMask:style | NSWindowStyleMaskNonactivatingPanel
                            backing:backing defer:defer];
    if (self) {
        self.hidesOnDeactivate = NO;
        self.becomesKeyOnlyIfNeeded = YES;
        self.worksWhenModal = YES;
        self.hasShadow = NO;
    }
    return self;
}
- (BOOL)canBecomeKeyWindow { return NO; }
- (BOOL)canBecomeMainWindow { return NO; }
- (void)setContentView:(NSView *)view {
    [super setContentView:view];
    tailKanaDisableCandidateFocusRings(view);
}
- (BOOL)makeFirstResponder:(NSResponder *)responder {
    // WebKit can add its internal focusable view after initial setup.
    if ([responder isKindOfClass:[NSView class]])
        tailKanaDisableCandidateFocusRings((NSView *)responder);
    return [super makeFirstResponder:responder];
}
@end
