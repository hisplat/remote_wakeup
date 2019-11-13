
#pragma once

#define TERMC_RED     "\033[1;31m"
#define TERMC_GREEN   "\033[1;32m"
#define TERMC_YELLOW  "\033[1;33m"
#define TERMC_BLUE    "\033[1;34m"
#define TERMC_PINK    "\033[1;35m"
#define TERMC_NONE    "\033[m"

#define TERMC_DRED     "\033[0;31m"
#define TERMC_DGREEN   "\033[0;32m"
#define TERMC_DYELLOW  "\033[0;33m"
#define TERMC_DBLUE    "\033[0;34m"
#define TERMC_DPINK    "\033[0;35m"

#define TERMC_BRED     "\033[1;31;40m"
#define TERMC_BGREEN   "\033[1;32;40m"
#define TERMC_BYELLOW  "\033[1;33;40m"
#define TERMC_BBLUE    "\033[1;34;40m"
#define TERMC_BPINK    "\033[1;35;40m"

#if defined(__GNUC__)
#   define __FUNC__     ((const char *) (__PRETTY_FUNCTION__))
#elif defined (__STDC_VERSION__) && __STDC_VERSION__ >= 19901L
#   define __FUNC__     ((const char *) (__func__))
#else
#   define __FUNC__     ((const char *) (__FUNCTION__))
#endif

#ifdef __cplusplus

#include <sstream>
#include <string>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include "dump.h"

namespace logging {

const int kLogLevel_Quiet = 0;
const int kLogLevel_Important = 1;
const int kLogLevel_Verbose = 2;

class LogMessageVoidify {
public:
    LogMessageVoidify() {}
    void operator&(std::ostream&) {}
};

class LogMessage {
public:
    LogMessage(const char * tag, const char * file, const char * func, int line, int level = kLogLevel_Verbose, bool fatal = false, bool condition = true);
    LogMessage(const char * tag, int level = kLogLevel_Verbose);
    ~LogMessage();

    std::ostream& stream() { return stream_; }
    std::string message() { return stream_.str(); }

private:
    LogMessage() {}
    std::ostringstream  stream_;
    std::string file_;
    std::string func_;
    std::string tag_;
    int         line_;
    bool        fatal_;
    bool        condition_;
    int         level_;
};

class ScopeTracer {
public:
    ScopeTracer(const char * func, const char * name, int line);
    ~ScopeTracer();
private:
    const char * name_;
    const char * func_;
    int          line_;
    long long    start_;
};

class TimerScopeTracer {
public:
    TimerScopeTracer(const char * func, const char * name, int line, long long dt);
    ~TimerScopeTracer();
private:
    const char * name_;
    const char * func_;
    int          line_;
    long long    start_;
    long long    difftime_;
};

long mtick();
long long ntick();

void setThreadLoggingTag(const char * tag);
const char * getThreadLoggingTag();

void setLoggingStream(std::ostream& o);

void setThreadName(const std::string& name);
std::string getThreadName();
int currentThreadId();

void setLogLevel(int level);

} // namespace logging



#define VERBOSE()   ::logging::LogMessageVoidify() & ::logging::LogMessage("HiRPC", __FILE__, __FUNC__, __LINE__, logging::kLogLevel_Verbose).stream()
#define IMPORTANT()   ::logging::LogMessageVoidify() & ::logging::LogMessage("HiRPC", __FILE__, __FUNC__, __LINE__, logging::kLogLevel_Important).stream()
#define QUIET()   ::logging::LogMessageVoidify() & ::logging::LogMessage("HiRPC", __FILE__, __FUNC__, __LINE__, logging::kLogLevel_Quiet).stream()

#define ORIGLOG(tag, file, func, line) ::logging::LogMessageVoidify() & ::logging::LogMessage(#tag, file, func, line, logging::kLogLevel_Verbose).stream()
#define DLOG_IF(condition)   ::logging::LogMessageVoidify() & ::logging::LogMessage("HiRPC", __FILE__, __FUNC__, __LINE__, logging::kLogLevel_Verbose, false, (condition)).stream()

#define FATAL(tag)  ::logging::LogMessageVoidify() & ::logging::LogMessage(#tag, __FILE__, __FUNC__, __LINE__, logging::kLogLevel_Quiet, true).stream() << TERMC_RED << "FATAL: "
#define TODO()  ::logging::LogMessageVoidify() & ::logging::LogMessage("HiRPC", __FILE__, __FUNC__, __LINE__, logging::kLogLevel_Verbose, true).stream() << TERMC_RED << "[TODO] "

#define PERROR() IMPORTANT() << "[" << errno << "] " << strerror(errno) << ". "
#define PERROR_IF(condition) ::logging::LogMessageVoidify() & ::logging::LogMessage("HiRPC", __FILE__, __FUNC__, __LINE__, logging::kLogLevel_Important, false, (condition)).stream() << "[" << errno << "] " << strerror(errno) << ". "

#define RUN_HERE()  VERBOSE() << TERMC_BYELLOW << "Run here! "
#define ATTENTION()  VERBOSE() << TERMC_RED << "[ATTENTION] "
#define ALERT()  IMPORTANT() << TERMC_RED << "Alert! " << TERMC_NONE

#define ScopeTrace(x)   ::logging::ScopeTracer x ## __LINE__(__FUNC__, #x, __LINE__)
#define LCHECK(condition)   if (!(condition)) FATAL("Assertion") << "Check failed. (" << #condition << ") "
#define TimerScopeTrace(x, t)   ::logging::TimerScopeTracer x ## __LINE__(__FUNC__, #x, __LINE__, t)

#define NFLOG() IMPORTANT()
#define DEBUG() VERBOSE()
#define SLOG() QUIET()

#endif

#ifdef __cplusplus
extern "C" {
#endif
void logging_printf(const char * filename, const char * func, int line, int level, const char * fmt, ...);
#ifdef __cplusplus
}
#endif

#define nf_quiet(fmt, ...) logging_printf(__FILE__, __FUNC__, __LINE__, 0, TERMC_NONE fmt, ##__VA_ARGS__)
#define nf_log(fmt, ...) logging_printf(__FILE__, __FUNC__, __LINE__, 1, TERMC_NONE fmt, ##__VA_ARGS__)
#define nf_debug(fmt, ...) logging_printf(__FILE__, __FUNC__, __LINE__, 2, TERMC_GREEN fmt, ##__VA_ARGS__)
#define nf_verbose(fmt, ...) logging_printf(__FILE__, __FUNC__, __LINE__, 2, TERMC_RED fmt, ##__VA_ARGS__)


