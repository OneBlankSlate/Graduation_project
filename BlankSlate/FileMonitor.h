#pragma once
#include<tchar.h>
#include<Windows.h>
#include<iostream>
#include<vector>
#include"IoControlHelper.h"
#include"FileMonCommon.h"

typedef struct COMMUNICATE_FILE_MON
{
	OPERATE_TYPE OperateType;
}COMMUNICATE_FILE_MON, * PCOMMUNICATE_FILE_MON;

// 文件保护通信结构
typedef struct _COMMUNICATE_FILE_PROTECT {
	OPERATE_TYPE OperateType;
	WCHAR FilePath[520];     // NT格式的文件路径
} COMMUNICATE_FILE_PROTECT, * PCOMMUNICATE_FILE_PROTECT;
