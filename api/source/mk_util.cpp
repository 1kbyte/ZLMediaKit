﻿/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_util.h"
#include <stdarg.h>
#include <assert.h>
#include "Util/util.h"
#include "Util/logger.h"
using namespace std;
using namespace toolkit;

#ifndef _WIN32
#define _strdup strdup
#endif

API_EXPORT char* API_CALL mk_util_get_exe_path(){
    return _strdup(exePath().data());
}

API_EXPORT char* API_CALL mk_util_get_exe_dir(const char *relative_path){
    if(relative_path){
        return _strdup((exeDir() + relative_path).data());
    }
    return _strdup(exeDir().data());
}

API_EXPORT uint64_t API_CALL mk_util_get_current_millisecond(){
    return getCurrentMillisecond();
}

API_EXPORT char* API_CALL mk_util_get_current_time_string(const char *fmt){
    assert(fmt);
    return _strdup(getTimeStr(fmt).data());
}

API_EXPORT char* API_CALL mk_util_hex_dump(const void *buf, int len){
    assert(buf && len > 0);
    return _strdup(hexdump(buf,len).data());
}

API_EXPORT void API_CALL mk_log_printf(int level, const char *file, const char *function, int line, const char *fmt, ...) {
    assert(file && function && fmt);
    LogContextCapturer info(Logger::Instance(), (LogLevel) level, file, function, line);
    va_list ap;
    va_start(ap, fmt);
    char *str = nullptr;
    vasprintf(&str, fmt, ap);
    va_end(ap);
    info << str;
    free(str);
}

