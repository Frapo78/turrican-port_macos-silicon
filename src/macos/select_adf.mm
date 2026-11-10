#import <AppKit/AppKit.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

// The game requires an old-style UAE extended ADF, not a conventional ADF.
// Checking before persistence avoids repeatedly loading an unusable image.
bool is_extended_adf(NSURL* url) {
  NSError* error = nil;
  NSFileHandle* handle = [NSFileHandle fileHandleForReadingFromURL:url error:&error];
  if (!handle) return false;
  NSData* header = [handle readDataOfLength:8];
  [handle closeFile];
  return [header length] == 8 && std::memcmp([header bytes], "UAE--ADF", 8) == 0;
}

void show_import_error(NSString* summary, NSString* explanation) {
  NSAlert* alert = [[NSAlert alloc] init];
  [alert setAlertStyle:NSAlertStyleWarning];
  [alert setMessageText:summary];
  [alert setInformativeText:explanation ?: @""];
  [alert addButtonWithTitle:@"OK"];
  [alert runModal];
}

std::string path_string(NSURL* url) {
  const char* path = url ? [url fileSystemRepresentation] : nullptr;
  return path ? std::string(path) : std::string();
}

}  // namespace

// Import once to ~/Library/Application Support/Turrican II/T2.adf.
// Deliberately NOT placed in the .app bundle: Finder-launched apps can live
// in read-only locations and mutating a signed bundle invalidates its seal.
// An app upgrade must not delete the user's original, imported disk image.
std::string macos_load_or_import_adf() {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];

    NSFileManager* files = [NSFileManager defaultManager];
    NSError* error = nil;
    NSURL* support = [files URLForDirectory:NSApplicationSupportDirectory
                                  inDomain:NSUserDomainMask
                         appropriateForURL:nil
                                    create:YES
                                     error:&error];
    if (!support) {
      show_import_error(@"Cannot open app data directory",
                        [error localizedDescription]);
      return {};
    }
    NSURL* app_dir = [support URLByAppendingPathComponent:@"Turrican II"
                                            isDirectory:YES];
    if (![files createDirectoryAtURL:app_dir
         withIntermediateDirectories:YES
                          attributes:nil
                               error:&error]) {
      show_import_error(@"Cannot create Turrican II data directory",
                        [error localizedDescription]);
      return {};
    }

    NSURL* disk = [app_dir URLByAppendingPathComponent:@"T2.adf"];
    if (is_extended_adf(disk)) return path_string(disk);

    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setTitle:@"Import Turrican II disk"];
    [panel setMessage:@"Select your original Turrican II extended ADF. "
                       "The app will store its own copy and won't ask again."];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    [panel setAllowsMultipleSelection:NO];
    [panel setAllowedFileTypes:@[@"adf"]];
    [panel setPrompt:@"Import"];

    for (;;) {
      if ([panel runModal] != NSModalResponseOK) return {};
      NSURL* selected = [[panel URLs] firstObject];
      if (!selected || !is_extended_adf(selected)) {
        show_import_error(@"Invalid disk image",
                          @"Turrican II requires an original extended ADF "
                           "(UAE--ADF), not a standard ADF.");
        continue;
      }

      // Stage the copy in the same directory, then move into place. A failed
      // or interrupted copy is not mistaken for a successfully imported disk.
      NSURL* temp = [app_dir URLByAppendingPathComponent:@"T2.adf.importing"];
      if ([files fileExistsAtPath:[temp path]]) [files removeItemAtURL:temp error:nil];

      error = nil;
      if (![files copyItemAtURL:selected toURL:temp error:&error]) {
        show_import_error(@"Cannot import Turrican II disk",
                          [error localizedDescription]);
        continue;
      }
      if (!is_extended_adf(temp)) {
        [files removeItemAtURL:temp error:nil];
        show_import_error(@"Invalid imported disk",
                          @"The disk copy did not pass validation. "
                           "Please select a valid extended ADF.");
        continue;
      }

      // An invalid previous import may exist. Do not overwrite a valid
      // import, which would already have been returned above.
      if ([files fileExistsAtPath:[disk path]]) {
        error = nil;
        if (![files removeItemAtURL:disk error:&error]) {
          [files removeItemAtURL:temp error:nil];
          show_import_error(@"Cannot replace invalid disk",
                            [error localizedDescription]);
          continue;
        }
      }
      error = nil;
      if (![files moveItemAtURL:temp toURL:disk error:&error]) {
        [files removeItemAtURL:temp error:nil];
        show_import_error(@"Cannot save imported disk",
                          [error localizedDescription]);
        continue;
      }
      return path_string(disk);
    }
  }
}
