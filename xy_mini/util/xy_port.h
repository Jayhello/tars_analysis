//
// Created by Administrator on 2023/8/18.
//

#pragma once

#include "xy_platform.h"
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/dir.h>
#include <sys/types.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <unordered_map>

using namespace std;

namespace xy {

    class TC_Port {
    public:
        /**
         * @brief 在s1的长度n中搜索s2
         * @return 搜索到的指针, 找不到返回NULL
         */
        static const char *strnstr(const char *s1, const char *s2, int pos1);

        static int strcmp(const char *s1, const char *s2);

        static int strncmp(const char *s1, const char *s2, size_t n);

        static int strcasecmp(const char *s1, const char *s2);

        static int strncasecmp(const char *s1, const char *s2, size_t n);

        static void localtime_r(const time_t *clock, struct tm *result);

        static void gmtime_r(const time_t *clock, struct tm *result);

        static time_t timegm(struct tm *timeptr);

        static int gettimeofday(struct timeval &tv);

        static int chmod(const char *path, mode_t mode);

        static FILE *fopen(const char *path, const char *mode);

#if TARGET_PLATFORM_WINDOWS
        typedef struct _stat stat_t;
#else
        typedef struct stat stat_t;
#endif

        static int lstat(const char *path, stat_t *buf);

        static int mkdir(const char *path);

        static int rmdir(const char *path);

        static int closeSocket(int fd);

        static int64_t getpid();

        static std::string getEnv(const std::string &name);

        static void setEnv(const std::string &name, const std::string &value);

        /**
         * exec command
         * @param cmd
         * @return string
         */
        static std::string exec(const char *cmd);

        /**
         * exec command
         *
         * @param cmd
         * @param errstr, if error, get error message
         * @return string
         */
        static std::string exec(const char *cmd, std::string &errstr);

        static void registerCtrlC(std::function<void()> callback);

        static void registerTerm(std::function<void()> callback);

    protected:

        static void registerSig(int sig, std::function<void()> callback);

        static void registerSig(int sig);

        static std::mutex _mutex;

        static unordered_map<int, vector<std::function<void()>>> _callbacks;

#if TARGET_PLATFORM_LINUX || TARGET_PLATFORM_IOS

        static void sighandler(int sig_no);

#else
        static BOOL WINAPI HandlerRoutine(DWORD dwCtrlType);
#endif
};

} // xy
