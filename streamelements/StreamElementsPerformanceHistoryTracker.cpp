#include "StreamElementsPerformanceHistoryTracker.hpp"

#ifndef WIN32
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/mach_time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

static const size_t BUF_SIZE = 60;

#ifdef WIN32
static uint64_t FromFileTime(const FILETIME& ft) {
	ULARGE_INTEGER uli = { 0 };
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	return uli.QuadPart;
}
#endif

StreamElementsPerformanceHistoryTracker::StreamElementsPerformanceHistoryTracker()
{
	os_event_init(&m_quit_event, OS_EVENT_TYPE_AUTO);
	os_event_init(&m_done_event, OS_EVENT_TYPE_AUTO);

	std::thread thread([this]() {
		do {
			// CPU
            {
#ifdef WIN32
                FILETIME idleTime;
                FILETIME kernelTime;
                FILETIME userTime;

                if (::GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
                    static bool hasSavedStartingValues = false;
                    static uint64_t savedIdleTime;
                    static uint64_t savedKernelTime;
                    static uint64_t savedUserTime;

                    if (!hasSavedStartingValues) {
                        savedIdleTime = FromFileTime(idleTime);
                        savedKernelTime = FromFileTime(kernelTime);
                        savedUserTime = FromFileTime(userTime);

                        hasSavedStartingValues = true;
                    }

                    const uint64_t SECOND_PART = 10000000L;
                    const uint64_t MS_PART = SECOND_PART / 1000L;

                    uint64_t idleInt = FromFileTime(idleTime) - savedIdleTime;
                    uint64_t kernelInt = FromFileTime(kernelTime) - savedKernelTime;
                    uint64_t userInt = FromFileTime(userTime) - savedUserTime;

                    uint64_t idleMs = idleInt / MS_PART;
                    uint64_t kernelMs = kernelInt / MS_PART;
                    uint64_t userMs = userInt / MS_PART;

                    uint64_t idleSec = idleMs / (uint64_t)1000;
                    uint64_t kernelSec = kernelMs / (uint64_t)1000;
                    uint64_t userSec = userMs / (uint64_t)1000;

                    uint64_t idleMod = idleMs % (uint64_t)1000;
                    uint64_t kernelMod = kernelMs % (uint64_t)1000;
                    uint64_t userMod = userMs % (uint64_t)1000;

                    seconds_t idleRat = idleSec + ((seconds_t)idleMod / 1000.0);
                    seconds_t kernelRat = kernelSec + ((seconds_t)kernelMod / 1000.0);
                    seconds_t userRat = userSec + ((seconds_t)userMod / 1000.0);

                    // https://msdn.microsoft.com/en-us/84f674e7-536b-4ae0-b523-6a17cb0a1c17
                    // lpKernelTime [out, optional]
                    // A pointer to a FILETIME structure that receives the amount of time that
                    // the system has spent executing in Kernel mode (including all threads in
                    // all processes, on all processors)
                    //
                    // >>> This time value also includes the amount of time the system has been idle.
                    //

                    cpu_usage_t item;

                    item.idleSeconds = idleRat;
                    item.totalSeconds =  kernelRat + userRat;
                    item.busySeconds = kernelRat + userRat - idleRat;

                    std::lock_guard<std::recursive_mutex> guard(m_mutex);
                    m_cpu_usage.push_back(item);

                    while (m_cpu_usage.size() > BUF_SIZE) {
                        m_cpu_usage.erase(m_cpu_usage.begin());
                    }
                }
#else
                mach_port_t mach_port = mach_host_self();
                host_cpu_load_info_data_t cpu_load_info;

                mach_msg_type_number_t cpu_load_info_count = HOST_CPU_LOAD_INFO_COUNT;
                if (host_statistics((host_t)mach_port, HOST_CPU_LOAD_INFO, (host_info_t)&cpu_load_info, &cpu_load_info_count) == KERN_SUCCESS) {
                    cpu_usage_t item;

                    item.idleSeconds = (seconds_t)(cpu_load_info.cpu_ticks[CPU_STATE_IDLE]) / (seconds_t)CLOCKS_PER_SEC;
                    item.totalSeconds = (seconds_t)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM] + cpu_load_info.cpu_ticks[CPU_STATE_USER] + cpu_load_info.cpu_ticks[CPU_STATE_IDLE] + cpu_load_info.cpu_ticks[CPU_STATE_NICE]) / (seconds_t)CLOCKS_PER_SEC;
                    item.busySeconds = (seconds_t)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM] + cpu_load_info.cpu_ticks[CPU_STATE_USER] + cpu_load_info.cpu_ticks[CPU_STATE_NICE]) / (seconds_t)CLOCKS_PER_SEC;

                    std::lock_guard<std::recursive_mutex> guard(m_mutex);
                    m_cpu_usage.push_back(item);

                    while (m_cpu_usage.size() > BUF_SIZE) {
                        m_cpu_usage.erase(m_cpu_usage.begin());
                    }
                }
#endif
            }

            // Memory
            {
#ifdef WIN32
                memory_usage_t mem;

                mem.dwLength = sizeof(mem);

                if (GlobalMemoryStatusEx(&mem)) {
                    std::lock_guard<std::recursive_mutex> guard(m_mutex);

                    m_memory_usage.push_back(mem);

                    while (m_memory_usage.size() > BUF_SIZE) {
                        m_memory_usage.erase(m_memory_usage.begin());
                    }
                }
#else
                memory_usage_t mem;

                mach_port_t mach_port = mach_host_self();
                vm_statistics_data_t vm_stats;

                mach_msg_type_number_t vm_info_count = HOST_VM_INFO_COUNT;
                vm_size_t page_size;
                if (host_statistics((host_t)mach_port, HOST_VM_INFO, (host_info_t)&vm_stats, &vm_info_count) == KERN_SUCCESS &&
                    host_page_size(mach_port, &page_size) == KERN_SUCCESS) {
                    int64_t free_memory = (int64_t)vm_stats.free_count * (int64_t)page_size;
                    int64_t used_memory = ((int64_t)vm_stats.active_count + (int64_t)vm_stats.inactive_count + (int64_t)vm_stats.wire_count) * (int64_t)page_size;
                    int64_t total_memory = free_memory + used_memory;

                    mem.dwMemoryLoad = used_memory * 100L / total_memory;

                    std::lock_guard<std::recursive_mutex> guard(m_mutex);

                    m_memory_usage.push_back(mem);

                    while (m_memory_usage.size() > BUF_SIZE) {
                        m_memory_usage.erase(m_memory_usage.begin());
                    }
                }
#endif
            }
		} while (0 != os_event_timedwait(m_quit_event, 60000));

		os_event_signal(m_done_event);
	});

	thread.detach();
}

StreamElementsPerformanceHistoryTracker::~StreamElementsPerformanceHistoryTracker()
{
	os_event_signal(m_quit_event);
	os_event_wait(m_done_event);

	os_event_destroy(m_done_event);
	os_event_destroy(m_quit_event);
}

std::vector<StreamElementsPerformanceHistoryTracker::memory_usage_t> StreamElementsPerformanceHistoryTracker::getMemoryUsageSnapshot()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return std::vector<memory_usage_t>(m_memory_usage);
}

std::vector<StreamElementsPerformanceHistoryTracker::cpu_usage_t> StreamElementsPerformanceHistoryTracker::getCpuUsageSnapshot()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return std::vector<cpu_usage_t>(m_cpu_usage);
}
