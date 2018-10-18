# SelfRelocationDriver
THIS IS A DEMO SAMPLE driver that relocates itself to a nonpaged pool and keeps running. 

Below is the sequenece.
1. In the DriverEntry, the driver allocates nonpaged pool and moved itself to the nonpaged pool memory
2. Unload the driver by returning STATUS_UNSUCCESSFUL in DriverEntry
3. Schedule Timer DPC to output information on the debugger every 10 secs
4. Free the nonpaged pool memory after timer DPC runs 5 times

The debugger output should look like this:
[m] SelfReloDriver!DriverEntry g_OriginalDriverBase = FFFFF80281DD0000, g_DriverSize = 8000

[m] SelfReloDriver!DriverEntry g_NewDriverBase = FFFF9F0B69D13000, g_DriverSize = 8000

[+] SelfReloDriver.sys unloaded

[1] Timer Fired @ 20181016-160748.0768

[2] Timer Fired @ 20181016-160758.0780

[3] Timer Fired @ 20181016-160808.0792

[4] Timer Fired @ 20181016-160818.0807

[5] Timer Fired @ 20181016-160828.0817

[m] relocated driver should be freed~

