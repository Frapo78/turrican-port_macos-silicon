#import <AppKit/AppKit.h>

#include <string>

// Prompt only when no disk was provided and no known disk was found.
// The original (extended) ADF is user-supplied, never distributed in the app.
std::string macos_select_adf() {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];

    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setTitle:@"Select Turrican II disk"];
    [panel setMessage:@"Choose your original Turrican II extended ADF disk image."];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    [panel setAllowsMultipleSelection:NO];
    [panel setAllowedFileTypes:@[@"adf"]];

    if ([panel runModal] != NSModalResponseOK) return {};
    NSURL* url = [[panel URLs] firstObject];
    const char* path = url ? [url fileSystemRepresentation] : nullptr;
    return path ? std::string(path) : std::string();
  }
}
