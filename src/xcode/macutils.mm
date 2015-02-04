#include <QtCore>
#include <QtGui>
#include <QtMac>
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
    size_t slen = min(strlen(s)+1, len);
    memcpy(d, s, slen);
    d[slen-1] = 0;
    return d;
}
// --

char *mac_pasteconsole(size_t *cblen)
{
    NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
    NSString *type = [pasteboard availableTypeFromArray:[NSArray arrayWithObject:NSStringPboardType]];
    if(type != nil)
    {
        NSString *contents = [pasteboard stringForType:type];
        if(contents != nil)
        {
            size_t len = [contents lengthOfBytesUsingEncoding:NSUTF8StringEncoding] + 1; // 10.4+
            if(len > 1)
            {
                char *buf = (char *)malloc(len);
                if(buf)
                {
                    if([contents getCString:buf maxLength:len encoding:NSUTF8StringEncoding]) // 10.4+
                    {
                        *cblen = len;
                        return buf;
                    }
                    free(buf);
                }
            }
        }
    }
    return NULL;
}

/*
 * 0x0A0400 = 10.4
 * 0x0A0500 = 10.5
 * 0x0A0600 = 10.6
 */
int mac_osversion()
{
    SInt32 majorVersion = 0, minorVersion = 0, bugVersion = 0;
    Gestalt(gestaltSystemVersionMajor, &majorVersion);
    Gestalt(gestaltSystemVersionMinor, &minorVersion);
    Gestalt(gestaltSystemVersionBugFix, &bugVersion);
    return (majorVersion<<16) | (minorVersion<<8) | bugVersion;
}

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

const char *mac_resourcedir()
{
    static string dir;
    CFBundleRef mainBundle = CFBundleGetMainBundle();
    if(!mainBundle) return NULL;
    CFURLRef mainBundleURL = CFBundleCopyBundleURL(mainBundle);
    if(!mainBundleURL) return NULL;
    CFStringRef cfStringRef = CFURLCopyFileSystemPath(mainBundleURL, kCFURLPOSIXPathStyle);
    if(!cfStringRef) return NULL;
    CFStringGetCString(cfStringRef, dir, MAXSTRLEN, kCFStringEncodingASCII);
    CFRelease(mainBundleURL);
    CFRelease(cfStringRef);
    return dir;
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
