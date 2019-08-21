/*
 * Copyright (C) 2012-2013 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _LOGD_LOG_TIMES_H__
#define _LOGD_LOG_TIMES_H__

#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include <list>
#include <memory>

#include <log/log.h>
#include <sysutils/SocketClient.h>

typedef unsigned int log_mask_t;

class LogReader;
class LogBufferElement;

class LogTimeEntry {
    static pthread_mutex_t timesLock;
    bool mRelease = false;
    bool leadingDropped;
    pthread_cond_t threadTriggeredCondition;
    pthread_t mThread;
    LogReader& mReader;
    static void* threadStart(void* me);
    const log_mask_t mLogMask;
    const pid_t mPid;
    unsigned int skipAhead[LOG_ID_MAX];
    pid_t mLastTid[LOG_ID_MAX];
    unsigned long mCount;
    unsigned long mTail;
    unsigned long mIndex;

  public:
    LogTimeEntry(LogReader& reader, SocketClient* client, bool nonBlock, unsigned long tail,
                 log_mask_t logMask, pid_t pid, log_time start_time, uint64_t sequence,
                 uint64_t timeout);

    SocketClient* mClient;
    log_time mStartTime;
    uint64_t mStart;
    struct timespec mTimeout;  // CLOCK_MONOTONIC based timeout used for log wrapping.
    const bool mNonBlock;

    // Protect List manipulations
    static void wrlock(void) {
        pthread_mutex_lock(&timesLock);
    }
    static void rdlock(void) {
        pthread_mutex_lock(&timesLock);
    }
    static void unlock(void) {
        pthread_mutex_unlock(&timesLock);
    }

    bool startReader_Locked();

    void triggerReader_Locked(void) {
        pthread_cond_signal(&threadTriggeredCondition);
    }

    void triggerSkip_Locked(log_id_t id, unsigned int skip) {
        skipAhead[id] = skip;
    }
    void cleanSkip_Locked(void);

    void release_Locked(void) {
        // gracefully shut down the socket.
        shutdown(mClient->getSocket(), SHUT_RDWR);
        mRelease = true;
        pthread_cond_signal(&threadTriggeredCondition);
    }

    bool isWatching(log_id_t id) const {
        return mLogMask & (1 << id);
    }
    bool isWatchingMultiple(log_mask_t logMask) const {
        return mLogMask & logMask;
    }
    // flushTo filter callbacks
    static int FilterFirstPass(const LogBufferElement* element, void* me);
    static int FilterSecondPass(const LogBufferElement* element, void* me);
};

typedef std::list<std::unique_ptr<LogTimeEntry>> LastLogTimes;

#endif  // _LOGD_LOG_TIMES_H__
