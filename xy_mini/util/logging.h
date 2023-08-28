//
// Created by root on 20-8-9.
//

#pragma once
#include <stdio.h>
#include <sstream>
#include <atomic>
#include <string>
#include <cstring>
#include "xy_noncopyable.h"

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#ifdef NDEBUG
#define hlog(level, ...)                                                                \
    do {                                                                                \
        if (level <= Logger::getLogger().getLogLevel()) {                               \
            Logger::getLogger().logv(level, __FILENAME__, __LINE__, __func__, __VA_ARGS__); \
        }                                                                               \
    } while (0)
#else
#define hlog(level, ...)                                                                \
    do {                                                                                \
        if (level <= Logger::getLogger().getLogLevel()) {                               \
            snprintf(0, 0, __VA_ARGS__);                                                \
            Logger::getLogger().logv(level, __FILENAME__, __LINE__, __func__, __VA_ARGS__); \
        }                                                                               \
    } while (0)

#endif

#define Trace(...) hlog(Logger::LTRACE, __VA_ARGS__)
#define Debug(...) hlog(Logger::LDEBUG, __VA_ARGS__)
#define Info(...) hlog(Logger::LINFO, __VA_ARGS__)
#define Warn(...) hlog(Logger::LWARN, __VA_ARGS__)
#define Error(...) hlog(Logger::LERROR, __VA_ARGS__)
#define fatal(...) hlog(Logger::LFATAL, __VA_ARGS__)

#ifndef fatalif
#define fatalif(b, ...)                        \
    do {                                       \
        if ((b)) {                             \
            hlog(Logger::LFATAL, __VA_ARGS__); \
        }                                      \
    } while (0)
#endif

#ifndef exitif
#define exitif(b, ...)                         \
    do {                                       \
        if ((b)) {                             \
            hlog(Logger::LERROR, __VA_ARGS__); \
            _exit(1);                          \
        }                                      \
    } while (0)
#endif

#define return_if(b, ...)                      \
    do {                                       \
        if ((b)) {                             \
            hlog(Logger::LERROR, __VA_ARGS__); \
            return;                            \
        }                                      \
    } while (0)

#define return_ret_if(b, ret, ...)             \
    do {                                       \
        if ((b)) {                             \
            hlog(Logger::LERROR, __VA_ARGS__); \
            return ret;                        \
        }                                      \
    } while (0)

#define RETURN_IF(b, msg, ret)                 \
    do {                                       \
        if ((b)) {                             \
            Log << msg;                        \
            return ret;                        \
        }                                      \
    } while (0)

#define setloglevel(l) Logger::getLogger().setLogLevel(l)
#define setlogfile(n) Logger::getLogger().setFileName(n)

namespace xy{

struct Logger : private noncopyable {
    enum LogLevel { LFATAL = 0, LERROR, LUERR, LWARN, LINFO, LDEBUG, LTRACE, LALL };
    Logger();
    ~Logger();
    void logv(int level, const char *file, int line, const char *func, const char *fmt...);

    void setFileName(const std::string &filename);
    void setLogLevel(const std::string &level);
    void setLogLevel(LogLevel level) { level_ = std::min(LALL, std::max(LFATAL, level)); }

    LogLevel getLogLevel() { return level_; }
    const char *getLogLevelStr() { return levelStrs_[level_]; }
    int getFd() { return fd_; }

    void adjustLogLevel(int adjust) { setLogLevel(LogLevel(level_ + adjust)); }
    void setRotateInterval(long rotateInterval) { rotateInterval_ = rotateInterval; }
    static Logger &getLogger();

private:
    void maybeRotate();
    static const char *levelStrs_[LALL + 1];
    int fd_;
    LogLevel level_;
    long lastRotate_;
    std::atomic<int64_t> realRotate_;
    long rotateInterval_;
    std::string filename_;
};

class ScopeLog{
public:
    ~ScopeLog(){
        if(not cancel_){
            Info("%s", ss_.str().c_str());
        }
    }

    template<typename T>
    std::ostream& operator << (const T& v){
        if(not ss_.str().empty()){
            ss_ << sep_;
        }
        ss_ << v << sep_;
        return ss_;
    }

    void setSep(const std::string& sep){
        sep_ = sep;
    }

    void cancel(){
        cancel_ = true;
    }

protected:
    std::string sep_ = " ";
    bool cancel_ = false;
    std::stringstream ss_;
};


} // xy

