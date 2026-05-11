#include"IoControlHelper.h"
#include"ProcessHelper.h"
#include"MemoryHelper.h"
#include"ProcessModule.h"
#include"ProcessHandle.h"
#include"ProcessMemory.h"
#include"SystemModule.h"
#include"CallbackHelper.h"
#include"ProcMonitor.h"
#include"FileMonitor.h"
LPFN_SERVICEADDRESS __ServiceArray[] = {
	NULL,
	PsEnumProcess,  //枚举进程
	PsEnumProcessModules,
	PsEnumProcessHandles,
	PsEnumProcessMem,
	PsReadProcessMem,
	PsWriteProcessMem,
	PsModifyProcessMem,
	PsTerminateProcess,
	PsHideProcess,
	PsProtectProcess,
	PsUnprotectProcess,
	EnumDriverModule,
	MonitorProcess,
	StopMonitorProcess,
	GetProcEvents,
	PsCloseHandle,
	StartFileMonitor,      // 对应 START_FILE_MON
	StopFileMonitor,       // 对应 STOP_FILE_MON
	GetFileEvents,        // 对应 GET_EVENTS_FILE_MON
	NULL
};
NTSTATUS CommunicateNeitherControl(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue)
{
	NTSTATUS Status = STATUS_UNSUCCESSFUL;
	OPERATE_TYPE OperateType = UNKNOWN;
	LPFN_SERVICEADDRESS ServiceAddress = NULL;
	__try
	{
		if (InputBufferLength < sizeof(ULONG))
		{
			return STATUS_INVALID_PARAMETER;
		}
		ProbeForRead(InputBuffer, InputBufferLength, 1);
		if (OutputBufferLength > 0)
		{
			ProbeForWrite(OutputBuffer, OutputBufferLength, 1);
		}

	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return STATUS_UNSUCCESSFUL;
	}
	OperateType = *(OPERATE_TYPE*)InputBuffer;
	ServiceAddress = GetServiceAddress(OperateType);
	if (!ServiceAddress)
	{
		return Status;
	}
	Status = ServiceAddress(InputBuffer, InputBufferLength, OutputBuffer, OutputBufferLength, ReturnValue);
	return Status;
}

LPFN_SERVICEADDRESS GetServiceAddress(OPERATE_TYPE OperateType)
{
	if (OperateType > UNKNOWN && OperateType < SERVICE_MAX)
	{
		return __ServiceArray[OperateType];
	}
	return NULL;
}
