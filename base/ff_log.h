#ifndef __FF_LOG_HPP__
#define __FF_LOG_HPP__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>

// Set ff_log_level by "export ff_log_level=#level#
extern unsigned int ff_log_level;

void ff_log_init();
void _ff_log(const char* prefix, const char* tag, const char* fname, const char* fmt, ...);

#define LOG_LEVEL_ERROR   0
#define LOG_LEVEL_WARN    1
#define LOG_LEVEL_INFO    2
#define LOG_LEVEL_DEBUG   3
#define LOG_LEVEL_VERBOSE 4

#ifndef MODULE_TAG
#define MODULE_TAG "ff_media"
#endif

#define ff_log(LEVEL, Prefix, fmt, ...)                                    \
    do {                                                                   \
        if (ff_log_level >= LEVEL)                                         \
            _ff_log(Prefix, MODULE_TAG, __FUNCTION__, fmt, ##__VA_ARGS__); \
    } while (0)

#define ff_log_m(LEVEL, Prefix, fmt, ...)                                            \
    do {                                                                             \
        if (ff_log_level >= LEVEL)                                                   \
            _ff_log(Prefix, typeid(*this).name(), __FUNCTION__, fmt, ##__VA_ARGS__); \
    } while (0)

#define ff_error(fmt, ...)   ff_log(LOG_LEVEL_ERROR, "ERROR", fmt, ##__VA_ARGS__)
#define ff_error_m(fmt, ...) ff_log_m(LOG_LEVEL_ERROR, "ERROR", fmt, ##__VA_ARGS__)

#define ff_warn(fmt, ...)   ff_log(LOG_LEVEL_WARN, "WARN", fmt, ##__VA_ARGS__)
#define ff_warn_m(fmt, ...) ff_log_m(LOG_LEVEL_WARN, "WARN", fmt, ##__VA_ARGS__)

#define ff_info(fmt, ...)   ff_log(LOG_LEVEL_INFO, "INFO", fmt, ##__VA_ARGS__)
#define ff_info_m(fmt, ...) ff_log_m(LOG_LEVEL_INFO, "INFO", fmt, ##__VA_ARGS__)

#define ff_debug(fmt, ...)   ff_log(LOG_LEVEL_DEBUG, "DEBUG", fmt, ##__VA_ARGS__)
#define ff_debug_m(fmt, ...) ff_log_m(LOG_LEVEL_DEBUG, "DEBUG", fmt, ##__VA_ARGS__)

#define ff_verbo(fmt, ...)   ff_log(LOG_LEVEL_VERBOSE, "VERBOSE", fmt, ##__VA_ARGS__)
#define ff_verbo_m(fmt, ...) ff_log_m(LOG_LEVEL_VERBOSE, "VERBOSE", fmt, ##__VA_ARGS__)

#define ff_print(fmt, ...) _ff_log(NULL, NULL, NULL, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif
#endif
