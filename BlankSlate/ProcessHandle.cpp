#include"ProcessHandle.h"
#include"ProcessHelper.h"



BOOL EnumProcessHandles(HANDLE ProcessIdentity, vector<HANDLE_INFORMATION_ENTRY>& HandleInfo)
{
	HandleInfo.clear();
	ULONG NumberOfHandle = 1000;
	PHANDLES_INFORMATION v5 = NULL;
	BOOL IsOk = FALSE;
	ULONG Size = 0;
	COMMUNICATE_PROCESS_HANDLE CommunicateProcessHandle;
	CommunicateProcessHandle.OperateType = ENUM_PROCESS_HANDLES;
	CommunicateProcessHandle.ProcessIdentity = ProcessIdentity;
	do {
		Size = sizeof(HANDLES_INFORMATION) + NumberOfHandle * sizeof(HANDLE_INFORMATION_ENTRY);
		v5 = (PHANDLES_INFORMATION)malloc(Size);
		memset(v5, 0, Size);
		if (!v5)
		{
			break;
		}
		IsOk = CommunicateDevice(&CommunicateProcessHandle, sizeof(COMMUNICATE_PROCESS_HANDLE), (PVOID)v5, Size, NULL);
		NumberOfHandle = v5->NumberOfHandle + 100;
	} while (!IsOk && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
	if (IsOk && v5->NumberOfHandle > 0)
	{
		for (ULONG i = 0; i < v5->NumberOfHandle; i++)
		{
			HANDLE_INFORMATION_ENTRY Entry;
			Entry.Handle = v5->HandleInfo[i].Handle;
			Entry.Object = v5->HandleInfo[i].Object;
			Entry.Index = v5->HandleInfo[i].Index;
			Entry.Count = v5->HandleInfo[i].Count;
			if (v5->HandleInfo[i].HandleName)
			{
				wcsncpy_s(Entry.HandleName, (wcslen(v5->HandleInfo[i].HandleName) + 1) * 2, v5->HandleInfo[i].HandleName, 0x1000);
			}
			if (v5->HandleInfo[i].HandleType)
			{
				wcsncpy_s(Entry.HandleType, (wcslen(v5->HandleInfo[i].HandleType) + 1) * 2, v5->HandleInfo[i].HandleType, 0x1000);
			}

			HandleInfo.push_back(Entry);
		}
	}
	if (v5)
	{
		free(v5);
		v5 = NULL;
	}
	return IsOk;
}