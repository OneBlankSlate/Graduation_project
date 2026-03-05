#include"SystemModule.h"


void EnumDriverModule(vector<DRIVER_MODULE_ENTRY>& DriverModuleInfo)
{
	OPERATE_TYPE OperateType = ENUM_DRIVER_MODULE;
	ULONG NumberOfModules = 1000;
	PDRIVER_MODULES v5 = NULL;
	BOOL IsOk = FALSE;
	DriverModuleInfo.clear();
	do
	{
		ULONG Size = sizeof(PDRIVER_MODULES) + NumberOfModules * sizeof(DRIVER_MODULE_ENTRY);
		v5 = (PDRIVER_MODULES)malloc(Size);
		memset(v5, 0, Size);
		if (v5)
		{
			IsOk = CommunicateDevice(&OperateType, sizeof(OPERATE_TYPE), (PVOID)v5, Size, NULL);
		}
		NumberOfModules += 1000;
	} while (!IsOk && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
	if (IsOk && v5->NumberOfModules > 0)
	{
		NumberOfModules = v5->NumberOfModules;
		for (ULONG i = 0; i < NumberOfModules; i++)
		{
			DriverModuleInfo.push_back(v5->ModuleEntry[i]);
		}
		IsOk = TRUE;
	}
	if (v5)
	{
		free(v5);
		v5 = NULL;
	}
}