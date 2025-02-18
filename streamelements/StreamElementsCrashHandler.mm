/*
 * MacOS X Crash Handler.
 *
 * The following code was used as reference when writing this module:
 * https://github.com/danzimm/mach_fun/blob/master/exceptions.c
 *
 * The best description of this code can be obtained here:
 * https://stackoverflow.com/questions/2824105/handling-mach-exceptions-in-64bit-os-x-application
 *
 * To handle mach exceptions, you have to register a mach port for the exceptions you are interested in.
 * You then wait for a message to arrive on the port in another thread. When a message arrives,
 * you call exc_server() whose implementation is provided by System.library. exec_server() takes the
 * message that arrived and calls one of three handlers that you must provide. catch_exception_raise(),
 * catch_exception_raise_state(), or catch_exception_raise_state_identity() depending on the
 * arguments you passed to task_set_exception_ports(). This is how it is done for 32 bit apps.
 *
 * For 64 bit apps, the 32 bit method still works but the data passed to you in your handler may be
 * truncated to 32 bits. To get 64 bit data passed to your handlers requires a little extra work that
 * is not very straight forward and as far as I can tell not very well documented. I stumbled onto the
 * solution by looking at the sources for GDB.
 *
 * Instead of calling exc_server() when a message arrives at the port, you have to call mach_exc_server()
 * instead. The handlers also have to have different names as well catch_mach_exception_raise(),
 * catch_mach_exception_raise_state(), and catch_mach_exception_raise_state_identity(). The parameters
 * for the handlers are the same as their 32 bit counterparts. The problem is that mach_exc_server() is
 * not provided for you the way exc_server() is. To get the implementation for mach_exc_server() requires
 * the use of the MIG (Mach Interface Generator) utility. MIG takes an interface definition file and
 * generates a set of source files that include a server function that dispatches mach messages to handlers
 * you provide. The 10.5 and 10.6 SDKs include a MIG definition file <mach_exc.defs> for the exception
 * messages and will generate the mach_exc_server() function. You then include the generated source files
 * in your project and then you are good to go.
 *
 * The nice thing is that if you are targeting 10.6+ (and maybe 10.5) you can use the same exception
 * handling for both 32 and 64 bit. Just OR the exception behavior with MACH_EXCEPTION_CODES when you set
 * your exception ports. The exception codes will come through as 64 bit values but you can truncate them
 * to 32 bits in your 32 bit build.
 *
 * I took the mach_exc.defs file and copied it to my source directory, opened a terminal and used the
 * command mig -v mach_exc.defs. This generated mach_exc.h, mach_excServer.c, and mach_excUser.c. I then
 * included those files in my project, added the correct declaration for the server function in my source
 * file and implemented my handlers. I then built my app and was good to go.
 */

#include "StreamElementsCrashHandler.hpp"
#include "StreamElementsGlobalStateManager.hpp"

//#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <pthread.h>

#include <mutex>

//#include "mach_exc.h"

#include <assert.h>
#include <stdbool.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/sysctl.h>

#import <BugSplatMac/BugSplat.h>
#import <BugSplatMac/BugSplatDelegate.h>
//@import BugSplatMac;
//#import <BugSplatMac/BugSplat.h>

static bool AmIBeingDebugged(void)
    // Returns true if the current process is being debugged (either
    // running under the debugger or has a debugger attached post facto).
{
    int                 junk;
    int                 mib[4];
    struct kinfo_proc   info;
    size_t              size;

    // Initialize the flags so that, if sysctl fails for some bizarre
    // reason, we get a predictable result.

    info.kp_proc.p_flag = 0;

    // Initialize mib, which tells sysctl the info we want, in this case
    // we're looking for information about a specific process ID.

    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PID;
    mib[3] = getpid();

    // Call sysctl.

    size = sizeof(info);
    junk = sysctl(mib, sizeof(mib) / sizeof(*mib), &info, &size, NULL, 0);
    assert(junk == 0);

    // We're being debugged if the P_TRACED flag is set.

    return ( (info.kp_proc.p_flag & P_TRACED) != 0 );
}

static bool initialized = false;

// Called by one of the crash handlers below to report a crash event to the analytics service
static void TrackCrash(const char* caller_reference)
{
    if (!initialized) return;

    blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: TrackCrash: %s", caller_reference);

    // This will also report the platform and a bunch of other props
    StreamElementsGlobalStateManager::GetInstance()
        ->GetAnalyticsEventsManager()
        ->trackSynchronousEvent("OBS Studio Crashed", json11::Json::object{
            { "platformCallerReference", caller_reference }
        });
    
    initialized = false;
}

extern "C" {
    // Implementation generated by mig from mach_exc.defs
    extern boolean_t mach_exc_server(mach_msg_header_t *, mach_msg_header_t *);

    // Exception handler, triggered by mach_exc_server
    kern_return_t catch_mach_exception_raise(mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t type, exception_data_t code, mach_msg_type_number_t code_count) {

        TrackCrash("catch_mach_exception_raise");

        return KERN_INVALID_ADDRESS;
    }

    // Exception handler, triggered by mach_exc_server
    kern_return_t catch_mach_exception_raise_state(mach_port_t exception_port, exception_type_t exception, exception_data_t code, mach_msg_type_number_t code_count, int *flavor, thread_state_t in_state, mach_msg_type_number_t in_state_count, thread_state_t out_state, mach_msg_type_number_t *out_state_count) {

        TrackCrash("catch_mach_exception_raise_state");

        return KERN_INVALID_ADDRESS;
    }

    // Exception handler, triggered by mach_exc_server
    kern_return_t catch_mach_exception_raise_state_identity(mach_port_t exception_port, mach_port_t thread, mach_port_t task, exception_type_t exception, exception_data_t code, mach_msg_type_number_t code_count, int *flavor, thread_state_t in_state, mach_msg_type_number_t in_state_count, thread_state_t out_state, mach_msg_type_number_t *out_state_count) {
        TrackCrash("catch_mach_exception_raise_state_identity");

        return KERN_INVALID_ADDRESS;
    }

    // Exception message pump thread
    static void *server_thread(void *arg) {
        mach_port_t exc_port = *(mach_port_t *)arg;
        kern_return_t kr;

        // Upon exception, the handler will set initialized = false
        //
        // When this happens, the OS will retry executing the failed instruction, that in
        // turn will trigger another exception which will terminate the program and trigger
        // Apple Crash Report as described here:
        // https://developer.apple.com/forums/thread/113742
        //
        // The Crash Report is not something we have access to when handling the exception
        // so in order to get it to developers, we'll package most recent crash reports when
        // user reports an issue.
        //
        // The Apple Crash Report is important since this is the only way to ensure we are
        // getting complete crash information, including source-line-level stack traces.
        // See the link above for an explanation WHY.
        //
        while(initialized) {
            if ((kr = mach_msg_server_once(mach_exc_server, 4096, exc_port, 0)) != KERN_SUCCESS) {
                blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: mach_msg_server_once: error %#x\n", kr);
                break;
            }
        }
        return (NULL);
    }

    // Initialize exception handler
    static void exn_init() {
        kern_return_t kr = 0;
        static mach_port_t exc_port;
        mach_port_t task = mach_task_self();
        pthread_t exception_thread;
        int err;
      
        mach_msg_type_number_t maskCount = 1;
        exception_mask_t mask;
        exception_handler_t handler;
        exception_behavior_t behavior;
        thread_state_flavor_t flavor;

        // Obtain Mach port which will receive exception messages
        if ((kr = mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &exc_port)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: mach_port_allocate: %#x\n", kr);
            return;
        }

        // Add send right to the exeption Mach port
        if ((kr = mach_port_insert_right(task, exc_port, exc_port, MACH_MSG_TYPE_MAKE_SEND)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: mach_port_allocate: %#x\n", kr);
            return;
        }

        // Get current Mach exception port settings
        if ((kr = task_get_exception_ports(task, EXC_MASK_ALL, &mask, &maskCount, &handler, &behavior, &flavor)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: task_get_exception_ports: %#x\n", kr);
            return;
        }

        // Start exception message pump thread
        if ((err = pthread_create(&exception_thread, NULL, server_thread, &exc_port)) != 0) {
            blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: pthread_create server_thread: %s\n", strerror(err));
            return;
        }
      
        pthread_detach(exception_thread);

        // Set exception port settings to receive all exceptions with exception codes
        if ((kr = task_set_exception_ports(task, EXC_MASK_ALL, exc_port, EXCEPTION_DEFAULT | MACH_EXCEPTION_CODES, flavor)) != KERN_SUCCESS) {
            blog(LOG_ERROR, "obs-streamelements-core: StreamElements: Crash Handler: task_set_exception_ports: %#x\n", kr);
            return;
        }

        // We're done
        blog(LOG_INFO, "obs-streamelements-core: StreamElements: Crash Handler: Initialized");
    }
}

@interface MyBugSplatDelegate : NSObject <BugSplatDelegate>
@end

@implementation MyBugSplatDelegate

- (instancetype) init
{
	self = [super init];
	
	return self;
}

- (void)bugSplatWillSendCrashReport:(BugSplat *)bugSplat {
    NSLog(@"bugSplatWillSendCrashReport called");
}

- (void)bugSplatWillSendCrashReportsAlways:(BugSplat *)bugSplat {
    NSLog(@"bugSplatWillSendCrashReportsAlways called");
}

- (void)bugSplatDidFinishSendingCrashReport:(BugSplat *)bugSplat {
    NSLog(@"bugSplatDidFinishSendingCrashReport called");
}

- (void)bugSplatWillCancelSendingCrashReport:(BugSplat *)bugSplat {
    NSLog(@"bugSplatWillCancelSendingCrashReport called");
}

- (void)bugSplatWillShowSubmitCrashReportAlert:(BugSplat *)bugSplat {
    NSLog(@"bugSplatWillShowSubmitCrashReportAlert called");
}

- (void)bugSplat:(BugSplat *)bugSplat didFailWithError:(NSError *)error {
    NSLog(@"bugSplat:didFailWithError: %@", [error localizedDescription]);
}

- (NSString *)applicationKeyForBugSplat:(BugSplat *)bugSplat signal:(NSString *)signal exceptionName:(NSString *)exceptionName exceptionReason:(NSString *)exceptionReason API_AVAILABLE(macosx(10.13)) {
    NSLog(@"applicationKeyForBugSplat called");

    auto appKey = GetStreamElementsPluginVersionString();
	
    id appKeyNSString = [NSString stringWithCString:appKey.c_str()
					   encoding:[NSString defaultCStringEncoding]];

    return [NSString stringWithFormat:@"SE.Live (Mac) %@", appKeyNSString];
}

@end

StreamElementsCrashHandler::StreamElementsCrashHandler()
{
    static std::mutex mutex;

    std::lock_guard<std::mutex> guard(mutex);

    if (!initialized) {
        initialized = true;
        
	if (!AmIBeingDebugged())
		exn_init();

	    [[BugSplat shared] setDelegate:[[MyBugSplatDelegate alloc] init]];
	    [[BugSplat shared] setPresentModally:YES];
	    [[BugSplat shared] setAutoSubmitCrashReport:NO];
	    [[BugSplat shared] setPersistUserDetails:YES];
	    [[BugSplat shared] setPresentModally:YES];
	    [[BugSplat shared] setBugSplatDatabase:@"OBS_Live"];
	    [[BugSplat shared] start];
    }
}

StreamElementsCrashHandler::~StreamElementsCrashHandler()
{
    // We never destroy the exception handler
    initialized = false;
}
