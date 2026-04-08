#pragma once
#include<tchar.h>
#include<Windows.h>
#include<iostream>
#include<vector>
#include"IoControlHelper.h"
typedef struct COMMUNICATE_PROCESS_MON
{
	OPERATE_TYPE OperateType;
}COMMUNICATE_PROCESS_MON, * PCOMMUNICATE_PROCESS_MON;
