// ModuleMonitor.h - 用户态通信头文件
#pragma once

#include <tchar.h>
#include <Windows.h>
#include <iostream>
#include <vector>
#include "IoControlHelper.h"

// 映像加载监控通信结构
typedef struct _COMMUNICATE_MODULE_MON {
    OPERATE_TYPE OperateType;
} COMMUNICATE_MODULE_MON, * PCOMMUNICATE_MODULE_MON;
