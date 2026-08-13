#include "StreamElementsCrashConsentDialog.hpp"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <string>

/* ================================================================= */

//
// Strings match the Windows dialog word for word, and are hardcoded English for
// the same reason -- see StreamElementsCrashConsentDialog.cpp. Keep the two in
// step: users on the two platforms should be answering the same question.
//
static NSString *const kPromptText =
	@"OBS Studio has stopped working, and SE.Live can send a report to help us fix it. "
	@"What were you doing just before this happened?";

static NSString *const kContactText =
	@"All three are optional. They let us follow up about this crash, and are remembered for next time.";

static NSString *const kPrivacyText =
	@"The report includes your OBS Studio and SE.Live configuration and an image of the OBS window. "
	@"These may contain personal information, and are used only to diagnose this crash. Stream keys are removed.";

/* ================================================================= */

static NSTextField *AddLabel(NSView *parent, NSString *text, CGFloat y,
			     CGFloat width, CGFloat height, bool small)
{
	NSTextField *label = [[NSTextField alloc]
		initWithFrame:NSMakeRect(0, y, width, height)];

	[label setStringValue:text];
	[label setBezeled:NO];
	[label setDrawsBackground:NO];
	[label setEditable:NO];
	[label setSelectable:NO];

	if (small) {
		[label setFont:[NSFont systemFontOfSize:
					       [NSFont smallSystemFontSize]]];
		[label setTextColor:[NSColor secondaryLabelColor]];
	}

	[parent addSubview:label];

	return label;
}

static NSTextField *AddField(NSView *parent, NSString *caption,
			     const std::string &value, CGFloat y, CGFloat width)
{
	AddLabel(parent, caption, y + 3, 60, 17, false);

	NSTextField *field = [[NSTextField alloc]
		initWithFrame:NSMakeRect(64, y, width - 64, 22)];

	[field setStringValue:[NSString stringWithUTF8String:value.c_str()]];

	[parent addSubview:field];

	return field;
}

static std::string ToUtf8(NSTextField *field)
{
	NSString *value = [field stringValue];

	if (!value)
		return "";

	const char *utf8 = [value UTF8String];

	return utf8 ? std::string(utf8) : std::string();
}

/* ================================================================= */

StreamElementsCrashConsentDialog::Result
StreamElementsCrashConsentDialog::Prompt(const std::string &name,
					 const std::string &email,
					 const std::string &discord)
{
	Result result;

	//
	// AppKit is main-thread-only, and this is called from a signal handler
	// that may be running on any thread. Marshalling onto the main thread is
	// not an option either: on a crash that thread is quite possibly the one
	// that faulted, or is blocked, and dispatch_sync would deadlock.
	//
	// So: only prompt when we are already on the main thread. Off it, the
	// caller treats a declined result as "do not send", which is the safe
	// direction -- a report is lost rather than sent unasked.
	//
	if (![NSThread isMainThread])
		return result;

	@autoreleasepool {
		const CGFloat width = 420;

		NSView *accessory = [[NSView alloc]
			initWithFrame:NSMakeRect(0, 0, width, 210)];

		// Laid out from the bottom up: AppKit's origin is bottom-left,
		// so the first control added sits lowest on screen.
		AddLabel(accessory, kPrivacyText, 0, width, 48, true);
		AddLabel(accessory, kContactText, 52, width, 32, true);

		NSTextField *discordField =
			AddField(accessory, @"Discord:", discord, 92, width);
		NSTextField *emailField =
			AddField(accessory, @"Email:", email, 120, width);
		NSTextField *nameField =
			AddField(accessory, @"Name:", name, 148, width);

		NSScrollView *scroll = [[NSScrollView alloc]
			initWithFrame:NSMakeRect(0, 178, width, 28)];
		NSTextView *description = [[NSTextView alloc]
			initWithFrame:NSMakeRect(0, 0, width, 28)];

		[description setRichText:NO];
		[scroll setDocumentView:description];
		[scroll setHasVerticalScroller:YES];
		[scroll setBorderType:NSBezelBorder];
		[accessory addSubview:scroll];

		NSAlert *alert = [[NSAlert alloc] init];

		[alert setAlertStyle:NSAlertStyleCritical];
		[alert setMessageText:@"SE.Live"];
		[alert setInformativeText:kPromptText];
		[alert setAccessoryView:accessory];
		[alert addButtonWithTitle:@"Send report"];
		[alert addButtonWithTitle:@"Don't send"];

		[[alert window] setInitialFirstResponder:description];

		// Nothing else will raise this: the app is dying behind it.
		[NSApp activateIgnoringOtherApps:YES];

		const NSModalResponse response = [alert runModal];

		// Asked and answered, whichever button it was. Only a prompt
		// that never appeared leaves this false.
		result.prompted = true;

		if (response != NSAlertFirstButtonReturn)
			return result;

		result.consented = true;
		result.name = ToUtf8(nameField);
		result.email = ToUtf8(emailField);
		result.discord = ToUtf8(discordField);

		NSString *text = [[description textStorage] string];

		if (text) {
			const char *utf8 = [text UTF8String];

			if (utf8)
				result.description = utf8;
		}
	}

	return result;
}
