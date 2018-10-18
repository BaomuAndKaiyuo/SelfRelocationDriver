// This is a demo driver with self relocation ability

#include "ntifs.h"
#define __MODULE__ "SelfReloDriver"

#define VA_POOL_TAG 'FEEB'
#define TIME_INTERVAL_IN_SEC    10

#pragma warning( disable:4100 )

NTSTATUS
DriverEntry(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath);

VOID
TimerDpcRoutine(
	struct _KDPC  *Dpc,
	PVOID  DeferredContext,
	PVOID  SystemArgument1,
	PVOID  SystemArgument2);



/// <summary>
/// Global Variables
/// </summary>

//Timer DPC routine
KTIMER			g_Timer;
KDPC			g_Dpc;
WORK_QUEUE_ITEM g_WorkItem;
ULONG			g_DriverSize;
ULONG			g_FireCount = 0;

//Original driver base
PUCHAR				g_OriginalDriverBase = NULL;
PCHAR				g_OrigianlFormatString = "[%d] Timer Fired @ %04u%02u%02u-%02u%02u%02u.%04u\n";
PKDEFERRED_ROUTINE	g_OriginalTimerDpcRoutine = TimerDpcRoutine;
PKTIMER				g_OriginalTimer = &g_Timer;
PKDPC				g_OriginalDpc = &g_Dpc;
PWORK_QUEUE_ITEM	g_OriginalWorkItem = &g_WorkItem;
PULONG				g_OriginalFireCount = &g_FireCount;

//New driver base
PUCHAR				g_NewDriverBase = NULL;
PCHAR				g_NewFormatString = NULL;
PKDEFERRED_ROUTINE	g_NewTimerDpcRoutine = NULL;
PKTIMER				g_NewTimer = NULL;
PKDPC				g_NewDpc = NULL;
PWORK_QUEUE_ITEM	g_NewWorkItem = NULL;
PULONG				g_NewFireCount = NULL;

//MACRO for relocating pointer
#define RELOCATE_POINTER(p)	(g_NewDriverBase + (((PUCHAR)p) - g_OriginalDriverBase))



#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT,DriverEntry)
#pragma alloc_text(PAGE, TimerDpcRoutine)
#endif

/// <summary>
/// Driver Entry Point
/// </summary>
/// <param name="DriverObject">The pointer to DRIVER_OBJECT</param>
/// <param name="RegistryPath">The pointer to Unicode string specifying registry path</param>
/// <returns>NTSTATUS</returns>
NTSTATUS
DriverEntry(
	PDRIVER_OBJECT DriverObject,
	PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);

	BOOLEAN Status = FALSE;

	//save the driver starting address and the size 
	g_OriginalDriverBase = DriverObject->DriverStart;
	g_DriverSize = DriverObject->DriverSize;

	DbgPrint("[m] %s!%s g_OriginalDriverBase = %p, g_DriverSize = %x\n", __MODULE__, __FUNCTION__, g_OriginalDriverBase, g_DriverSize);

	//allocate nonpaged pool memory for new driver image
	g_NewDriverBase = ExAllocatePoolWithTag(NonPagedPool, g_DriverSize, VA_POOL_TAG);

	if (!g_NewDriverBase)
	{
		DbgPrint("[-] %s!%s ExAllocatePoolWithTag(%x) failed\n", __MODULE__, __FUNCTION__, g_DriverSize);
		goto Exit;
	}

	DbgPrint("[m] %s!%s g_NewDriverBase = %p, g_DriverSize = %x\n", __MODULE__, __FUNCTION__, g_NewDriverBase, g_DriverSize);

	//relocate pointer
	g_NewFormatString = (PCHAR)RELOCATE_POINTER(g_OrigianlFormatString);
	g_NewTimerDpcRoutine = (PKDEFERRED_ROUTINE)RELOCATE_POINTER(g_OriginalTimerDpcRoutine);
	g_NewTimer = (PKTIMER)RELOCATE_POINTER(g_OriginalTimer);
	g_NewDpc = (PKDPC)RELOCATE_POINTER(g_OriginalDpc);
	g_NewWorkItem = (PWORK_QUEUE_ITEM)RELOCATE_POINTER(g_OriginalWorkItem);
	g_NewFireCount = (PULONG)RELOCATE_POINTER(g_OriginalFireCount);

	//copy the loaded driver image to nonpaged pool memory
	RtlCopyMemory(g_NewDriverBase, g_OriginalDriverBase, g_DriverSize);

	//initialize timer
	KeInitializeTimer(g_NewTimer);
	
	//initialize DPC routine
	KeInitializeDpc(g_NewDpc, g_NewTimerDpcRoutine, NULL);
	
	//initialize work item to free the relocated driver
	//used when we decide to unload the driver
	ExInitializeWorkItem(g_NewWorkItem, (PWORKER_THREAD_ROUTINE)ExFreePool, g_NewDriverBase);

	//setup timer
	//KeSetTimer uses 100ns due time
	LARGE_INTEGER DueTime;
	DueTime.QuadPart = (ULONGLONG)(-1) * 10 * 1000 * 1000 * TIME_INTERVAL_IN_SEC;
	LONG Period = TIME_INTERVAL_IN_SEC * 1000;

	//start the timer
	KeSetTimerEx(g_NewTimer, DueTime, Period, g_NewDpc);

	Status = TRUE;

Exit:
	if (!Status)
	{
		if (g_NewDriverBase)
		{
			ExFreePool(g_NewDriverBase);
		}
	}

	//here we unload the original driver
	DbgPrint("[+] %s.sys unloaded\n", __MODULE__);
	return STATUS_UNSUCCESSFUL;
}//DriverEntry()


/// <summary>
/// Timer DPC routine to fire time periodically from the relocated driver 
/// </summary>
/// <param name="Dpc">The pointer to KDPC object</param>
/// <param name="DeferredContext">driver defined context information</param>
/// <param name="SystemArgument1">Specifies driver-determined context data</param>
/// <param name="SystemArgument2">Specifies driver-determined context data</param>
/// <returns>No return value</returns>

VOID
TimerDpcRoutine(
	struct _KDPC  *Dpc,
	PVOID  DeferredContext,
	PVOID  SystemArgument1,
	PVOID  SystemArgument2)
{
	LARGE_INTEGER SystemTime;
	LARGE_INTEGER LocalTime;
	TIME_FIELDS TimeFields;

	(*g_NewFireCount)++;

	//get the system time
	KeQuerySystemTime(&SystemTime);

	//convert system time to local time
	ExSystemTimeToLocalTime(&SystemTime, &LocalTime);

	//retrieve time fields from the local time
	RtlTimeToTimeFields(&LocalTime, &TimeFields);

	//print the time informaion (YYYYMMDD-HHMMSS.ssss)
	DbgPrint(g_NewFormatString,
		*g_NewFireCount,
		TimeFields.Year,
		TimeFields.Month,
		TimeFields.Day,
		TimeFields.Hour,
		TimeFields.Minute,
		TimeFields.Second,
		TimeFields.Milliseconds);

	//after the timer fires 5 times, we cancel the time and call the workitem to free the relocated driver
	if (*g_NewFireCount >= 5)
	{
		//no need to check, KeCancelTimer always returns TRUE for periodic timer
		KeCancelTimer(g_NewTimer);
		ExQueueWorkItem(g_NewWorkItem, DelayedWorkQueue);

		DbgPrint("[m] relocated driver should be freed~\n");
	}

}//TimerDpcRoutine