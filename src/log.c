#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <syslog.h>

#define MAX_LOG_SIZE 256

int log_level = LOG_ERR;

static void emit_log(int level, char* line) {
    //TODO for level
    if (log_level <= level) {
        //uncomment to use syslog, then it will show on router's log page
        //syslog(syslog_level, "%s", line);
        fprintf(stderr, "%s", line);
    }
}

static void logv(int filter, const char *format, va_list vl) {
    char buf[MAX_LOG_SIZE];
    vsnprintf(buf, sizeof(buf), format, vl);
    buf[sizeof(buf) - 1] = '\0';

    emit_log(filter, buf);
}

void loginfo(const char *format, ...) {
    va_list ap;

    if (log_level <= LOG_INFO) {
        return;
    }

    va_start(ap, format);
    logv(LOG_INFO, format, ap);
    va_end(ap);
}

void logerr(const char *format, ...) {
    va_list ap;

    va_start(ap, format);
    logv(LOG_ERR, format, ap);
    va_end(ap);
}

