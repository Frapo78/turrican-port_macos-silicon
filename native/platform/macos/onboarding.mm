// SPDX-License-Identifier: GPL-3.0-or-later
#import <AppKit/AppKit.h>
#import <CommonCrypto/CommonDigest.h>
#include "assets/adf_probe.hpp"
#include <string>

namespace {
NSString* sha256_file(NSURL* file) {
  NSFileHandle* handle = [NSFileHandle fileHandleForReadingFromURL:file error:nil];
  if (!handle) return nil;
  CC_SHA256_CTX context;
  CC_SHA256_Init(&context);
  @try {
    while (true) {
      NSData* chunk = [handle readDataOfLength:64 * 1024];
      if (![chunk length]) break;
      CC_SHA256_Update(&context, [chunk bytes], (CC_LONG)[chunk length]);
    }
  } @catch (NSException* exception) {
    [handle closeFile];
    return nil;
  }
  [handle closeFile];
  unsigned char digest[CC_SHA256_DIGEST_LENGTH];
  CC_SHA256_Final(digest, &context);
  NSMutableString* hex = [NSMutableString string];
  for (unsigned char b : digest) [hex appendFormat:@"%02x", (unsigned)b];
  return hex;
}
void dialog(NSString* title, NSString* explanation) {
  NSAlert* a = [[NSAlert alloc] init];
  [a setMessageText:title];
  [a setInformativeText:explanation];
  [a addButtonWithTitle:@"OK"];
  [a runModal];
}
}

// One-time local *technical identification*, not legal proof of ownership.
// This native prototype intentionally does not extract copyrighted assets.
// After a successful onboarding, subsequent launches read only the JSON
// manifest; game loop never imports an ADF or invokes an Amiga emulator.
bool native_macos_onboard() {
  @autoreleasepool {
    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp activateIgnoringOtherApps:YES];
    NSFileManager* fs = [NSFileManager defaultManager];
    NSError* error = nil;
    NSURL* dir = [fs URLForDirectory:NSApplicationSupportDirectory
                            inDomain:NSUserDomainMask appropriateForURL:nil
                               create:YES error:&error];
    if (!dir) return false;
    NSURL* app = [dir URLByAppendingPathComponent:@"Turrican II Native" isDirectory:YES];
    if (![fs createDirectoryAtURL:app withIntermediateDirectories:YES
                       attributes:nil error:&error]) return false;
    NSURL* manifest = [app URLByAppendingPathComponent:@"edition.json"];
    NSData* previous = [NSData dataWithContentsOfURL:manifest];
    id parsed = previous ? [NSJSONSerialization JSONObjectWithData:previous
                                                          options:0 error:nil] : nil;
    NSDictionary* record = [parsed isKindOfClass:[NSDictionary class]] ? parsed : nil;
    if (record && [record[@"schema"] integerValue] == 1 &&
        [record[@"sha256"] isKindOfClass:[NSString class]] &&
        t2native::is_known_reference_sha256([record[@"sha256"] UTF8String])) return true;

    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setTitle:@"Identify your Turrican II disk image"];
    [panel setMessage:@"Choose your own Turrican II ADF. This prototype stores "
                       "only its fingerprint, never the disk image. "
                       "It is not a legal proof-of-purchase system."];
    [panel setCanChooseFiles:YES]; [panel setCanChooseDirectories:NO];
    [panel setAllowsMultipleSelection:NO]; [panel setAllowedFileTypes:@[@"adf"]];
    for (;;) {
      if ([panel runModal] != NSModalResponseOK) return false;
      NSURL* selected = [[panel URLs] firstObject];
      const char* path = [selected fileSystemRepresentation];
      auto detected = t2native::probe_adf(path ? path : "");
      if (!detected.structurally_supported) {
        dialog(@"ADF structure not recognized", [NSString stringWithUTF8String:detected.reason.c_str()]);
        continue;
      }
      NSString* hash = sha256_file(selected);
      if (!hash || !t2native::is_known_reference_sha256([hash UTF8String])) {
        dialog(@"Edition not identified", @"This ADF is structurally recognized but its SHA-256 "
               "does not match the first tested image. Other valid editions will be added "
               "after analysis. A file hash cannot establish legal ownership.");
        continue;
      }
      NSDictionary* entry = @{@"schema": @1,
                               @"edition": @"user-supplied-reference-uae-old-v1",
                               @"sha256": hash,
                               @"asset_cache": @"synthetic-placeholder-only"};
      NSData* data = [NSJSONSerialization dataWithJSONObject:entry options:NSJSONWritingPrettyPrinted error:&error];
      if (!data || ![data writeToURL:manifest options:NSDataWritingAtomic error:&error]) {
        dialog(@"Could not save identification manifest", [error localizedDescription]);
        return false;
      }
      return true;
    }
  }
}
