#include"ProcessModule.h"
#include"ProcessHelper.h"
#include"ProcessModuleWindow.h"
BOOL EnumProcessModules(ULONG_PTR ProcessIdentity, vector<MODULE_INFORMATION_ENTRY>& ModuleInfo)
{
	ModuleInfo.clear();
	ULONG NumberOfModule = 1000;
	PMODULES_INFORMATION v5 = NULL;
	BOOL IsOk = FALSE;
	ULONG Size = 0;
	COMMUNICATE_PROCESS_MODULE CommunicateProcessModule;
	CommunicateProcessModule.OperateType = ENUM_PROCESS_MODULES;
	CommunicateProcessModule.ProcessIdentity = ProcessIdentity;
	do {
		Size = sizeof(MODULES_INFORMATION) + NumberOfModule * sizeof(MODULE_INFORMATION_ENTRY);
		v5 = (PMODULES_INFORMATION)malloc(Size);
		memset(v5, 0, Size);
		if (!v5)
		{
			break;
		}
		IsOk = CommunicateDevice(&CommunicateProcessModule, sizeof(COMMUNICATE_PROCESS_MODULE), (PVOID)v5, Size, NULL);
		NumberOfModule = v5->NumberOfModules + 100;
	} while (!IsOk && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
	if (IsOk && v5->NumberOfModules > 0)
	{
		for (ULONG i = 0; i < v5->NumberOfModules; i++)
		{
			MODULE_INFORMATION_ENTRY Entry;
			Entry.ModuleBase = v5->ModuleInfo[i].ModuleBase;
			Entry.SizeOfImage = v5->ModuleInfo[i].SizeOfImage;
			wcsncpy_s(Entry.ModulePath, MAX_PATH, v5->ModuleInfo[i].ModulePath, MAX_PATH);
			ModuleInfo.push_back(Entry);
		}
	}
	if (v5)
	{
		free(v5);
		v5 = NULL;
	}
	return IsOk;
}



