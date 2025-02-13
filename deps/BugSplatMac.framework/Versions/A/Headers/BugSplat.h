//
//  BugSplat.h
//
//  Copyright © 2024 BugSplat, LLC. All rights reserved.
//

#import <Foundation/Foundation.h>

//! Project version number for BugSplat.
FOUNDATION_EXPORT double BugSplatVersionNumber;

//! Project version string for BugSplat.
FOUNDATION_EXPORT const unsigned char BugSplatVersionString[];

// In this header, you should import all the public headers of your framework using statements like #import <BugSplat/PublicHeader.h>

#if TARGET_OS_OSX
  #import <BugSplatMac/BugSplatDelegate.h>
  #import <BugSplatMac/BugSplatAttachment.h>
#else
  #import <BugSplat/BugSplatDelegate.h>
  #import <BugSplat/BugSplatAttachment.h>
#endif

NS_ASSUME_NONNULL_BEGIN

/// App's Info.plist String entry which is a customer specific BugSplat database name where crash reports will be uploaded.
/// e.g: "fred" (which will reference the https://fred.bugsplat.com/ database)
#define kBugSplatDatabase @"BugSplatDatabase"


@protocol BugSplatDelegate;

@interface BugSplat : NSObject

/*!
 *  BugSplat singleton initializer/accessor
 *
 *  @return shared instance of BugSplat
 */
+ (instancetype)shared;

/*!
 *  Configures and starts crash reporting service
 */
- (void)start;

/**
 * Set the delegate
 *
 * Defines the class that implements the optional protocol `BugSplatDelegate`.
 *
 * @see BugSplatDelegate
 */
@property (weak, nonatomic, nullable) id<BugSplatDelegate> delegate;

/** Set the userID that should used in the SDK components

 Right now this is used by the Crash Manager to attach to a crash report.

 The value can be set at any time and will be stored in the keychain on the current
 device only! To delete the value from the keychain set the value to `nil`.

 This property is optional.

 @warning When returning a non nil value, crash reports are not anonymous any more
 and the crash alerts will not show the word "anonymous"!

 @warning This property needs to be set before calling `start` to be considered
 for being added to crash reports as meta data.

 @see userName
 @see userEmail
 @see `[BITHockeyManagerDelegate userIDForHockeyManager:componentManager:]`
 */
@property (nonatomic, copy, nullable) NSString *userID;

/** Set the user name that should used in the SDK components

 Right now this is used by the Crash Manager to attach to a crash report.

 The value can be set at any time and will be stored in the keychain on the current
 device only! To delete the value from the keychain set the value to `nil`.

 This property is optional.

 @warning When returning a non nil value, crash reports are not anonymous any more
 and the crash alerts will not show the word "anonymous"!

 @warning This property needs to be set before calling `start` to be considered
 for being added to crash reports as meta data.

 @see userID
 @see userEmail
 @see `[BITHockeyManagerDelegate userNameForHockeyManager:componentManager:]`
 */
@property (nonatomic, copy, nullable) NSString *userName;

/** Set the users email address that should used in the SDK components

 Right now this is used by the Crash Manager to attach to a crash report.

 The value can be set at any time and will be stored in the keychain on the current
 device only! To delete the value from the keychain set the value to `nil`.

 This property is optional.

 @warning When returning a non nil value, crash reports are not anonymous any more
 and the crash alerts will not show the word "anonymous"!

 @warning This property needs to be set before calling `start` to be considered
 for being added to crash reports as meta data.

 @see userID
 @see userName
 @see [BITHockeyManagerDelegate userEmailForHockeyManager:componentManager:]
 */
@property (nonatomic, copy, nullable) NSString *userEmail;

/*!
 *  Submit crash reports without asking the user
 *
 *  _YES_: The crash report will be submitted without asking the user
 *  _NO_: The user will be asked if the crash report can be submitted (default)
 *
 *  Default: iOS: _YES_, macOS: _NO_
 */
@property (nonatomic, assign) BOOL autoSubmitCrashReport;

// macOS specific API
#if TARGET_OS_OSX
/*!
 *  Provide custom banner image for crash reporter.
 *  Can set directly in code or provide an image named bugsplat-logo in main bundle. Can be in asset catalog.
 */
@property (nonatomic, strong, nullable) NSImage *bannerImage;

/**
 *  Defines if the crash report UI should ask for name and email
 *
 *  Default: _YES_
 */
@property (nonatomic, assign) BOOL askUserDetails;

/**
 *  Defines if user's name and email entered in the crash report UI should be saved to the keychain.
 *
 *  Default: _NO_
 */
@property (nonatomic, assign) BOOL persistUserDetails;

/**
 *  Defines if crash reports should be considered "expired" after a certain amount of time (in seconds).
 *  If expired crash dialogue is not displayed but reports are still uploaded.
 *
 *  Default: -1 // No expiration
 */
@property (nonatomic, assign) NSTimeInterval expirationTimeInterval;

/**
 * Option to present crash reporter dialogue modally
 *
 * *Default*:  NO
 */
@property (nonatomic, assign) BOOL presentModally;

#endif

@end

NS_ASSUME_NONNULL_END
