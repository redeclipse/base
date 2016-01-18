#import <Foundation/Foundation.h>
#import <Cocoa/Cocoa.h>
#include <CoreFoundation/CoreFoundation.h>
#include <ApplicationServices/ApplicationServices.h>
#import <CoreFoundation/CFBundle.h>

// -- copied from tools.h -- including the full file introduces too many problems
#define MAXSTRLEN 512
typedef char string[MAXSTRLEN];
inline char *copystring(char *d, const char *s, size_t len = MAXSTRLEN)
{
    size_t slen = strlen(s)+1;
    if (slen > len) slen = len;
    memcpy(d, s, slen);
    d[slen-1] = 0;
    return d;
}
// --

const char *mac_personaldir() {
    static string dir;
    NSString *path = nil;
    FSRef folder;
    if(FSFindFolder(kUserDomain, kApplicationSupportFolderType, NO, &folder) == noErr) 
    {
        CFURLRef url = CFURLCreateFromFSRef(kCFAllocatorDefault, &folder);
        path = [(NSURL *)url path];
        CFRelease(url);
    }
    return path ? copystring(dir, [path fileSystemRepresentation]) : NULL;
}

const char *mac_sauerbratendir() {
    static string dir;
    NSString *path = [[NSWorkspace sharedWorkspace] fullPathForApplication:@"sauerbraten"];
    if(path) path = [path stringByAppendingPathComponent:@"Contents/gamedata"];
    return path ? copystring(dir, [path fileSystemRepresentation]) : NULL;
}

bool mac_capslock()
{
    NSUInteger flags = [NSEvent modifierFlags]&NSDeviceIndependentModifierFlagsMask;
    return (flags&NSAlphaShiftKeyMask)!=0;
}

bool mac_numlock()
{
    NSUInteger flags = [NSEvent modifierFlags]&NSDeviceIndependentModifierFlagsMask;
    return (flags&NSNumericPadKeyMask)!=0;
}
