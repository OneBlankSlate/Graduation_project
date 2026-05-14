// ThreadMonitor.h - 用户态通信头文件
#pragma once

#include <tchar.h>
#include <Windows.h>
#include <iostream>
#include <vector>
#include "IoControlHelper.h"

// 线程监控通信结构
typedef struct _COMMUNICATE_THREAD_MON {
    OPERATE_TYPE OperateType;
} COMMUNICATE_THREAD_MON, * PCOMMUNICATE_THREAD_MON;

