#include"IoControlRoutine.h"
#include"ProcessHelper.h"
#include"ProcessModule.h"
#include"MemoryHelper.h"
#include"ApcInject.h"
#include"ThreadInject.h"

LPFN_SERVICEADDRESS __ServiceArray[] = {
	NULL,
	PsEnumProcess,  //Ã¶¾Ù½ø³Ì
	PsEnumProcessModules,
	PsDumpProcessModule,
	ApcInject,
	ThreadInject,
	PsUnloadProcessModule,
	PsProtectProcess,
	PsUnprotectProcess,
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
