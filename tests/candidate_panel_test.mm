#import "../src/candidate_panel.h"
#include <cassert>
#include <iostream>
int main() {
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyProhibited];
        auto panel=[[TailKanaCandidatePanel alloc] initWithContentRect:NSMakeRect(0,0,500,100)
            styleMask:0 backing:NSBackingStoreBuffered defer:YES];
        assert([panel isKindOfClass:[NSPanel class]]);
        assert(panel.styleMask & NSWindowStyleMaskNonactivatingPanel);
        assert(!panel.canBecomeKeyWindow && !panel.canBecomeMainWindow);
        assert(!panel.hidesOnDeactivate && panel.becomesKeyOnlyIfNeeded && panel.worksWhenModal);
        assert(!panel.hasShadow);
        auto content=[[NSView alloc] initWithFrame:NSMakeRect(0,0,500,100)];
        auto child=[[NSView alloc] initWithFrame:NSMakeRect(0,0,400,80)];
        content.focusRingType=NSFocusRingTypeExterior;
        child.focusRingType=NSFocusRingTypeExterior;
        [content addSubview:child];panel.contentView=content;
        assert(content.focusRingType==NSFocusRingTypeNone && child.focusRingType==NSFocusRingTypeNone);
        auto lateChild=[[NSView alloc] initWithFrame:NSMakeRect(0,0,100,30)];
        lateChild.focusRingType=NSFocusRingTypeExterior;
        [content addSubview:lateChild];[panel makeFirstResponder:lateChild];
        assert(lateChild.focusRingType==NSFocusRingTypeNone);
        assert(tailKanaCandidateNativeShadow(0,YES));
        for(NSInteger appearance=1;appearance<=6;++appearance)assert(!tailKanaCandidateNativeShadow(appearance,YES));
        assert(!tailKanaCandidateNativeShadow(0,NO));
        assert(!panel.isVisible);
        [panel release];
        [lateChild release];[child release];[content release];
        std::cout<<"PASS: candidate panel is nonactivating, cannot take keyboard/main focus, remains available while inactive\n";
    }
}
