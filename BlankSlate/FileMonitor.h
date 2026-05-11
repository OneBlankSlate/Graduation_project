#pragma once
#include<tchar.h>
#include<Windows.h>
#include<iostream>
#include<vector>
#include"IoControlHelper.h"
typedef struct COMMUNICATE_FILE_MON
{
	OPERATE_TYPE OperateType;
}COMMUNICATE_FILE_MON, * PCOMMUNICATE_FILE_MON;
