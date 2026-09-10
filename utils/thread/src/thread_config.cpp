#include "thread_config.h"

#include <pthread.h>
#include <sched.h>

#include <stdexcept>
#include <system_error>

namespace utils {

void ConfigureCurrentThread(
    const char* name,
    const unsigned cpu) {
    const int name_error = pthread_setname_np(pthread_self(), name);
    if (name_error != 0) {
        throw std::system_error(
            name_error,
            std::generic_category(),
            "pthread_setname_np failed");
    }

    if (cpu >= static_cast<unsigned>(CPU_SETSIZE)) {
        throw std::invalid_argument(
            "CPU index exceeds CPU_SETSIZE");
    }

    cpu_set_t cpu_set;
    CPU_ZERO(&cpu_set);
    CPU_SET(static_cast<int>(cpu), &cpu_set);

    const int affinity_error = pthread_setaffinity_np(
        pthread_self(),
        sizeof(cpu_set),
        &cpu_set);
    if (affinity_error != 0) {
        throw std::system_error(
            affinity_error,
            std::generic_category(),
            "pthread_setaffinity_np failed");
    }
}

} // namespace utils
