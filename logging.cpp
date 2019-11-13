

#include "logging.h"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/prctl.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <iomanip>
#include <iostream>

#if defined(ANDROID)
#include "android/log.h"
#include <utils/SystemClock.h>
#endif

#ifdef ANDROID
#define GETTID()    gettid()
#else
#define GETTID()    syscall(SYS_gettid)
#endif

/*
static unsigned int GetTickCount(void)
{
    struct sysinfo  si;
    sysinfo(&si);
    return si.uptime;
}
*/

namespace logging {

static std::ostream* g_logging_stream = &std::cout;
static int gLevel = kLogLevel_Verbose;

LogMessage::LogMessage(const char * tag, const char * file, const char * func, int line, int level, bool fatal, bool condition)
    : file_(file)
    , func_(func)
    , tag_(tag)
    , line_(line)
    , fatal_(fatal)
    , condition_(condition)
    , level_(level)
{
}

LogMessage::LogMessage(const char * tag, int level)
    : tag_(tag)
    , line_(0)
    , fatal_(false)
    , condition_(true)
    , level_(level)
{
}

LogMessage::~LogMessage()
{
    if ((level_ > gLevel) && !fatal_) {
        return;
    }

    if (!condition_) {
        return;
    }
    std::ostringstream  timeoss;
    std::ostringstream   oss;
    oss << "[" << getpid() << ":" << GETTID() << "] ";

    const char * t = getThreadLoggingTag();
    if (t != NULL) {
        oss << "(" << t << ") ";
    }
    std::string threadname = getThreadName();
    if (threadname.length() > 0) {
        oss << "(" << threadname << ") ";
    }

    struct tm local_time;
    struct timeval tv;
    memset(&local_time, 0, sizeof(local_time));
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &local_time);

#if defined(ANDROID)
    // struct sysinfo  si;
    // sysinfo(&si);
    // oss << '[' << si.uptime << "] ";

    oss << '[' << android::uptimeMillis() << "] ";
#endif
    timeoss << '[';
    timeoss << std::setfill('0')
            << std::setw(2) << local_time.tm_hour
            << ':'
            << std::setw(2) << local_time.tm_min
            << ':'
            << std::setw(2) << local_time.tm_sec
            << ':'
            << std::setw(6) << tv.tv_usec
            << "] ";

    if (!file_.empty()) {
        size_t  pos = file_.find_last_of("/\\");
        if (pos != std::string::npos)
            file_ = file_.substr(pos + 1);
        oss << '[' << file_ << ':' << func_ << ':' << line_ << "] ";
    }

    std::string st = stream_.str();
    size_t pos = st.find_last_not_of("\r\n ");
    if (pos != std::string::npos)
        st = st.substr(0, pos + 1);
    oss << ' ' << st;
    oss << TERMC_NONE;
    oss << std::endl;

#if defined(ANDROID)
    __android_log_write(ANDROID_LOG_ERROR, tag_.c_str(), oss.str().c_str());
#endif
    // printf("%s",  oss.str().c_str());
    // fflush(stdout);
    (*g_logging_stream) << timeoss.str() << oss.str();
    (*g_logging_stream).flush();

    if (fatal_) {
        abort();
    }
}

ScopeTracer::ScopeTracer(const char * func, const char * name, int line)
    : name_(name)
    , func_(func)
    , line_(line)
{
    LogMessage("Tracer", kLogLevel_Important).stream() << TERMC_RED << "Into scope: " << name << " (" << func << ':' << line << ")" << TERMC_NONE;
    start_ = ntick();
}

ScopeTracer::~ScopeTracer()
{
    long long now = ntick();
    long long tu = now - start_;
    LogMessage("Tracer", kLogLevel_Important).stream() << TERMC_GREEN << "Leave scope: " << name_ << " (" << func_ << ':' << line_ << ")" << TERMC_BLUE << " Time costs: " << tu << " ns." << TERMC_NONE;
}

TimerScopeTracer::TimerScopeTracer(const char * func, const char * name, int line, long long dt)
    : name_(name)
    , func_(func)
    , line_(line)
    , difftime_(dt)
{
    start_ = ntick();
}

TimerScopeTracer::~TimerScopeTracer()
{
    long long now = ntick();
    long long tu = now - start_;

    if (tu >= difftime_) {
        LogMessage("TimerTracer").stream() << TERMC_PINK << "Leave scope: " << name_ << " (" << func_ << ':' << line_ << ")" << TERMC_BLUE << " Time costs: " << tu << " ns." << TERMC_NONE;
    }
}


static pthread_key_t    g_key = (pthread_key_t)(-1);
void setThreadLoggingTag(const char * tag)
{
    if (g_key == (pthread_key_t)(-1)) {
        pthread_key_create(&g_key, NULL);
    }
    pthread_setspecific(g_key, tag);
}

const char * getThreadLoggingTag()
{
    if (g_key == (pthread_key_t)(-1)) {
        return NULL;
    }
    return (const char *)pthread_getspecific(g_key);
}

void setLoggingStream(std::ostream& o)
{
    g_logging_stream = &o;
}

long mtick()
{
    struct timespec ts; 
    ts.tv_sec = ts.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long n = ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    return n;
}

long long ntick()
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)(t.tv_sec)*1000000000LL + t.tv_nsec;
}

void setThreadName(const std::string& name)
{
    prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);
}

std::string getThreadName()
{
    char tname[100];
    prctl(PR_GET_NAME, tname);
    return tname;
}

int currentThreadId()
{
    return (int)GETTID();
}

void setLogLevel(int level)
{
    gLevel = level;
}
} // namespace

void logging_printf(const char * filename, const char * func, int line, int level, const char * fmt, ...)
{
    char temp[4096];
    va_list args;
    va_start(args, fmt);
    vsnprintf(temp, sizeof(temp), fmt, args);
    va_end(args);
    if (filename == NULL || func == NULL) {
        ::logging::LogMessage("CF", level).stream() << temp;
    } else {
        ::logging::LogMessage("CF", filename, func, line, level).stream() << temp;
    }
}


