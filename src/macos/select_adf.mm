#import <AppKit/AppKit.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

// Keep this detection in sync with Disk::load in src/amiga/disk.cpp.
// A normal 880 KiB ADF is supported by the disk reader, as are both UAE
// extended ADF variants. The previous importer accidentally rejected two
// of these three formats solely because they did not start with "UAE--ADF".
enum class AdfFormat { unsupported, standard, extended_old, extended_new };

AdfFormat detect_adf_format(NSURL* url) {
  if (!url) return AdfFormat::unsupported;
  NSFileManager* files = [NSFileManager defaultManager];
  NSError* error = nil;
  NSDictionary* attrs = [files attributesOfItemAtPath:[url path] error:&error];
  if (!attrs) return AdfFormat::unsupported;
  const unsigned long long bytes = [attrs[NSFileSize] unsignedLongLongValue];

  // Exact size of a standard double-density, 80-track Amiga ADF.
  if (bytes == 901120ULL) return AdfFormat::standard;
  if (bytes < 12) return AdfFormat::unsupported;

  NSFileHandle* handle = [NSFileHandle fileHandleForReadingFromURL:url error:&error];
  if (!handle) return AdfFormat::unsupported;
  NSData* data = [handle readDataOfLength:12];
  [handle closeFile];
  if ([data length] < 12) return AdfFormat::unsupported;
  const auto* header = static_cast<const uint8_t*>([data bytes]);

  // Original extended ADF: 8-byte magic + 160 x 4-byte track headers.
  // The disk reader tolerates a truncated final track; do not reject it.
  if (std::memcmp(header, "UAE--ADF", 8) == 0 &&
      bytes >= 8ULL + 160ULL * 4ULL)
    return AdfFormat::extended_old;

  // New extended ADF: 12-byte header and N 12-byte track descriptors.
  if (std::memcmp(header, "UAE-1ADF", 8) == 0) {
    const unsigned tracks = (unsigned(header[10]) << 8) | unsigned(header[11]);
    if (tracks > 0 && bytes >= 12ULL + 12ULL * tracks)
      return AdfFormat::extended_new;
  }
  return AdfFormat::unsupported;
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
// Do not write inside the .app bundle: it might be read-only and modifying a
// signed bundle breaks its signature. This file persists across updates.
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
    if (detect_adf_format(disk) != AdfFormat::unsupported)
      return path_string(disk);

    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setTitle:@"Import Turrican II disk"];
    [panel setMessage:@"Choose your Turrican II ADF disk image. "
                       "Standard ADF and UAE extended ADF are accepted. "
                       "The file will be copied into the app data directory."];
    [panel setCanChooseFiles:YES];
    [panel setCanChooseDirectories:NO];
    [panel setAllowsMultipleSelection:NO];
    [panel setAllowedFileTypes:@[@"adf"]];
    [panel setPrompt:@"Import"];

    for (;;) {
      if ([panel runModal] != NSModalResponseOK) return {};
      NSURL* selected = [[panel URLs] firstObject];
      if (detect_adf_format(selected) == AdfFormat::unsupported) {
        show_import_error(@"Unsupported disk format",
                          @"Choose a standard 880 KiB ADF or a UAE extended "
                           "ADF (UAE--ADF / UAE-1ADF). Other disk formats "
                           "would require additional support in the game engine.");
        continue;
      }

      // Stage the file and verify the COPY before replacing the imported
      // image. An interrupted import must not look like a successful one.
      NSURL* temp = [app_dir URLByAppendingPathComponent:@"T2.adf.importing"];
      if ([files fileExistsAtPath:[temp path]])
        [files removeItemAtURL:temp error:nil];

      error = nil;
      if (![files copyItemAtURL:selected toURL:temp error:&error]) {
        show_import_error(@"Cannot import Turrican II disk",
                          [error localizedDescription]);
        continue;
      }
      if (detect_adf_format(temp) == AdfFormat::unsupported) {
        [files removeItemAtURL:temp error:nil];
        show_import_error(@"Imported disk could not be verified",
                          @"The copied file is not a recognized ADF image. "
                           "Your original file has not been modified.");
        continue;
      }

      // An unrecognized previous import may exist; never replace a
      // recognized persisted disk without an explicit re-import action.
      if ([files fileExistsAtPath:[disk path]]) {
        error = nil;
        if (![files removeItemAtURL:disk error:&error]) {
          [files removeItemAtURL:temp error:nil];
          show_import_error(@"Cannot replace unsupported disk",
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
      std::fprintf(stderr, "Imported Turrican II disk to %s\n",
                   path_string(disk).c_str());
      return path_string(disk);
    }
  }
}
