#include"ProcessMemory.h"
BOOL EnumProcessMemorys(HANDLE ProcessIdentity, vector<MEMORY_INFORMATION_ENTRY>& MemoryInfo)
{
	MemoryInfo.clear();
	ULONG NumberOfMemory = 1000;
	PMEMORYS_INFORMATION v5 = NULL;
	BOOL IsOk = FALSE;
	ULONG Size = 0;
	COMMUNICATE_PROCESS_MEMORY CommunicateProcessMemory;
	CommunicateProcessMemory.OperateType = ENUM_PROCESS_MEMORYS;
	CommunicateProcessMemory.ul.Query.ProcessIdentity = (ULONG_PTR)ProcessIdentity;
	do {
		Size = sizeof(MEMORYS_INFORMATION) + NumberOfMemory * sizeof(MEMORY_INFORMATION_ENTRY);
		v5 = (PMEMORYS_INFORMATION)malloc(Size);
		memset(v5, 0, Size);
		if (!v5)
		{
			break;
		}
		IsOk = CommunicateDevice(&CommunicateProcessMemory, sizeof(COMMUNICATE_PROCESS_MEMORY), (PVOID)v5, Size, NULL);
		NumberOfMemory = v5->NumberOfMemory + 100;
	} while (!IsOk && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
	if (IsOk && v5->NumberOfMemory > 0)
	{
		for (ULONG i = 0; i < v5->NumberOfMemory; i++)
		{
			MEMORY_INFORMATION_ENTRY Entry;
			Entry.BaseAddress = v5->MemoryInfo[i].BaseAddress;
			Entry.RegionSize = v5->MemoryInfo[i].RegionSize;
			Entry.Protect = v5->MemoryInfo[i].Protect;
			Entry.State = v5->MemoryInfo[i].State;
			Entry.Type = v5->MemoryInfo[i].Type;

			MemoryInfo.push_back(Entry);
		}
	}
	if (v5)
	{
		free(v5);
		v5 = NULL;
	}
	return IsOk;
}

const WCHAR* GetProtect(ULONG Protect)
{
	switch (Protect)
	{
	case 1:
	{
		return L"No Access";
	}
	case 2:
	{
		return L"Read";
	}
	case 4:
	{
		return L"ReadWrite";
	}
	case 8:
	{
		return L"WriteCopy";
	}
	case 32:
	{
		return L"ReadExecute";
	}
	case 260:
	{
		return L"ReadWriteGuard";
	}
	default:
	{
		return L"";
	}
	}
}

const WCHAR* GetState(ULONG State)
{
	switch (State)
	{
	case 0x1000:
	{
		return L"Commit";
	}
	case 0x2000:
	{
		return L"Reserve";
	}
	case 0x10000:
	{
		return L"Free";
	}
	default:
	{
		return L"";
	}
	}
}

const WCHAR* GetType(ULONG Type)
{
	switch (Type)
	{
	case 0x20000:
	{
		return L"Private";
	}
	case 0x40000:
	{
		return L"Map";
	}
	case 0x1000000:
	{
		return L"Image";
	}
	default:
	{
		return L"";
	}
	}
}
