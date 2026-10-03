# Helper of driver.ps1 stop: sends Ctrl+C to the console of ONE process (a server started by driver.ps1 start in its own console).
# It must run in a separate powershell process: it detaches from its own console, attaches to the server's and raises CTRL_C_EVENT for
# process group 0 (every process on that console = the server and this helper, which ignores it). Exit codes: 0 sent, 2 attach failed, 3 raise failed.
param([Parameter(Mandatory)][uint32]$ProcessId)
Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class AionConsoleCtrl {
	[DllImport("kernel32.dll", SetLastError = true)] public static extern bool AttachConsole(uint pid);
	[DllImport("kernel32.dll", SetLastError = true)] public static extern bool FreeConsole();
	[DllImport("kernel32.dll", SetLastError = true)] public static extern bool SetConsoleCtrlHandler(IntPtr handler, bool add);
	[DllImport("kernel32.dll", SetLastError = true)] public static extern bool GenerateConsoleCtrlEvent(uint ctrlEvent, uint processGroupId);
}
'@
[AionConsoleCtrl]::FreeConsole() | Out-Null
if (-not [AionConsoleCtrl]::AttachConsole($ProcessId)) { exit 2 }
[AionConsoleCtrl]::SetConsoleCtrlHandler([IntPtr]::Zero, $true) | Out-Null   # this helper ignores the Ctrl+C it raises
$sent = [AionConsoleCtrl]::GenerateConsoleCtrlEvent(0, 0)
Start-Sleep -Seconds 2
[AionConsoleCtrl]::FreeConsole() | Out-Null
if (-not $sent) { exit 3 }
exit 0
