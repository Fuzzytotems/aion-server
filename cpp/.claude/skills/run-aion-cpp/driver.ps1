<#
driver.ps1 - build, start, probe, stop and test the C++ Aion servers of this checkout (cpp/build/msvc, Debug configuration).

  powershell -NoProfile -ExecutionPolicy Bypass -File cpp\.claude\skills\run-aion-cpp\driver.ps1 <command> [options]

  status                          ports 2106/9014/7777/3306 and their owners, MariaDB, every aion_* process and where it runs from
  build [-Targets a,b]            configure build/msvc when needed (msvc preset, -DAION_BUILD_CHAT_SERVER=ON), then build
                                  (default aion_game_server,aion_login_server; --parallel 4 -- -p:CL_MPCount=2 -nr:false)
  start [-TimeoutSec 420]         MariaDB if down, then the login server and the game server from build/msvc, each in its own
                                  minimized console; refuses when 2106/9014/7777 are taken; waits for "Game server started"
  probe                           TCP connect to 2106 and 7777 and print the first packet each server sends (length + first bytes)
  logs [-Server gs|ls] [-Errors] [-Tail n]
  stop                            Ctrl+C to exactly the PIDs `start` recorded (game server first), then wait; touches nothing else
  unit [-Filter <ctest regex>] [-Jobs 4]
                                  the unit suite: ctest -LE "scenario|geo|m4|nightly|stress|smoke" with the test database env
  gate <name>                     ctest -R ^gs\.(scenario|smoke)\.<name>$ with the test env (m5a, m5c, startup, ...); prints the
                                  verdict, the time and where the gate's logs and census went

Exit code 0 on success; 1 on a refusal or failure (the reason is printed); unit/gate return ctest's exit code.
The servers read the Java tree's configs (login-server/config, game-server/config incl. mygs.properties), i.e. the live
schemas aion_ls/aion_gs, exactly like the owner's play kit in D:\aion-dev\play - which is why start refuses when that kit runs.
#>
[CmdletBinding()]
param(
	[Parameter(Position = 0)][string]$Command = 'help',
	[Parameter(Position = 1)][string]$Name,
	[string[]]$Targets = @('aion_game_server', 'aion_login_server'),
	[string]$Filter,
	[switch]$Errors,
	[int]$Tail = 30,
	[ValidateSet('gs', 'ls')][string]$Server = 'gs',
	[int]$TimeoutSec = 420,
	[int]$Jobs = 4,
	[switch]$Force
)
$ErrorActionPreference = 'Stop'

$CppRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$RepoRoot = Split-Path $CppRoot -Parent
$BuildDir = Join-Path $CppRoot 'build\msvc'
$BuildConfig = 'Debug'
$LsExe = Join-Path $BuildDir "login-server\$BuildConfig\aion_login_server.exe"
$GsExe = Join-Path $BuildDir "game-server\$BuildConfig\aion_game_server.exe"
$LsDir = Join-Path $RepoRoot 'login-server'
$GsDir = Join-Path $RepoRoot 'game-server'
$StateDir = Join-Path $BuildDir '.run-aion-cpp'
$StateFile = Join-Path $StateDir 'servers.json'
$Helper = Join-Path $PSScriptRoot 'send-ctrl-c.ps1'
$DevDir = 'D:\aion-dev'
$PlayKit = 'D:\aion-dev\play'
$MariaDbHome = Join-Path $DevDir 'mariadb-11.8.9-winx64'
$MariaDbIni = Join-Path $DevDir 'mariadb-data\my.ini'
$ServerPorts = @{ 2106 = 'login server, clients'; 9014 = 'login server, game servers'; 7777 = 'game server, clients' }   # a plain hashtable: an
# [ordered] one would read $ServerPorts[2106] as a position
$TestEnv = [ordered]@{
	AION_TEST_DATABASE_URL      = 'jdbc:mysql://localhost:3306/aion_cpp_test'
	AION_TEST_DATABASE_USER     = 'root'
	AION_TEST_DATABASE_PASSWORD = ''   # optional; PowerShell drops an empty variable, which the tests read as empty
	AION_TEST_LS_DATABASE_URL   = 'jdbc:mysql://127.0.0.1:3306/aion_ls_test'
	AION_TEST_GS_DATABASE_URL   = 'jdbc:mysql://127.0.0.1:3306/aion_cpp_test?characterEncoding=UTF-8'
	AION_TEST_CS_DATABASE_URL   = 'jdbc:mysql://127.0.0.1:3306/aion_cs_test'
}
$UnitExclude = 'scenario|geo|m4|nightly|stress|smoke'

# ---------------------------------------------------------------------------------------------------------------------------- helpers

function Say([string]$Text, [string]$Color) { if ($Color) { Write-Host $Text -ForegroundColor $Color } else { Write-Host $Text } }

function Test-Under([string]$Path, [string]$Dir) {
	if (-not $Path) { return $false }
	return $Path.StartsWith($Dir.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)
}

function Get-Origin([string]$Path) {
	if (-not $Path) { return 'unknown path (exited, or not readable)' }
	if (Test-Under $Path $PlayKit) { return 'OWNER PLAY KIT - never touch' }
	if (Test-Under $Path $BuildDir) { return 'build/msvc' }
	if (Test-Under $Path (Join-Path $CppRoot 'build')) { return 'another cpp/build dir' }
	if (Test-Under $Path $DevDir) { return 'D:\aion-dev' }
	return 'other'
}

function Get-PortOwner([int]$Port) {
	$conn = Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue | Select-Object -First 1
	if (-not $conn) { return $null }
	$proc = Get-Process -Id $conn.OwningProcess -ErrorAction SilentlyContinue
	$procName = '?'; $procPath = $null
	if ($proc) { $procName = $proc.ProcessName; $procPath = $proc.Path }
	return [pscustomobject]@{ Port = $Port; ProcessId = [int]$conn.OwningProcess; Name = $procName; Path = $procPath; Origin = (Get-Origin $procPath) }
}

function Test-Port([int]$Port) { return [bool](Get-NetTCPConnection -State Listen -LocalPort $Port -ErrorAction SilentlyContinue) }

function Invoke-Native([scriptblock]$Block) {
	# Windows PowerShell 5.1 turns a native command's stderr into a terminating error under ErrorActionPreference Stop
	$saved = $ErrorActionPreference; $ErrorActionPreference = 'Continue'
	try { & $Block } finally { $ErrorActionPreference = $saved }
}

function Test-MariaDb {
	$admin = Join-Path $MariaDbHome 'bin\mariadb-admin.exe'
	if (-not (Test-Path $admin)) { return "mariadb-admin not found at $admin" }
	$out = Invoke-Native { & $admin "--defaults-file=$MariaDbIni" -u root --connect-timeout=1 ping 2>&1 }
	return (($out | ForEach-Object { "$_" }) -join ' ').Trim()
}

function Get-CacheValue([string]$Key) {
	$cache = Join-Path $BuildDir 'CMakeCache.txt'
	if (-not (Test-Path $cache)) { return $null }
	$line = Select-String -LiteralPath $cache -Pattern "^$([regex]::Escape($Key)):[A-Z]+=(.*)$" | Select-Object -First 1
	if ($line) { return $line.Matches[0].Groups[1].Value }
	return $null
}

function Get-CMake([string]$Tool) {
	# the cmake/ctest the build directory was configured with; else the copy bundled with Visual Studio 2026
	$key = 'CMAKE_COMMAND'
	if ($Tool -eq 'ctest') { $key = 'CMAKE_CTEST_COMMAND' }
	$path = Get-CacheValue $key
	if (-not $path -or -not (Test-Path $path)) {
		$path = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\$Tool.exe"
	}
	if (-not (Test-Path $path)) { throw "$Tool.exe not found (looked in the CMake cache and at $path)" }
	return $path
}

function Read-LogLines([string]$Path) {
	# the server keeps its log open; share read/write/delete so neither side fails
	if (-not (Test-Path -LiteralPath $Path)) { return @() }
	$fs = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]'ReadWrite, Delete')
	try { $reader = New-Object IO.StreamReader($fs, [Text.Encoding]::UTF8); $text = $reader.ReadToEnd() } finally { $fs.Dispose() }
	return $text -split "`r?`n" | Where-Object { $_ -ne '' }   # callers wrap it in @() for an array
}

function Get-LogTime([string]$Line) {
	# 2026-09-28T12:48:55,625-07:00 INFO  [main] ...
	if ($Line -match '^(\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d),(\d{3})([+-]\d\d:\d\d)') {
		return [DateTimeOffset]::ParseExact("$($Matches[1]).$($Matches[2])$($Matches[3])", "yyyy-MM-dd'T'HH:mm:ss.fffzzz",
			[Globalization.CultureInfo]::InvariantCulture)
	}
	return $null
}

function Get-LogPath([string]$Which) {
	if ($Which -eq 'ls') { return Join-Path $LsDir 'log\server_console.log' }
	return Join-Path $GsDir 'log\server_console.log'
}

function Read-State {
	if (-not (Test-Path $StateFile)) { return $null }
	return Get-Content -Raw -LiteralPath $StateFile | ConvertFrom-Json
}

function Write-State($State) {
	New-Item -ItemType Directory -Force -Path $StateDir | Out-Null
	$State | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $StateFile -Encoding UTF8
}

function Get-RecordedProcess($Entry) {
	# the recorded process, only if that PID still is the very process start launched (same exe, same start time)
	if (-not $Entry) { return $null }
	$proc = Get-Process -Id $Entry.processId -ErrorAction SilentlyContinue
	if (-not $proc) { return $null }
	if (-not $proc.Path -or -not [string]::Equals($proc.Path, $Entry.exe, [StringComparison]::OrdinalIgnoreCase)) { return $null }
	$recordedStart = $Entry.startTimeUtc
	if ($recordedStart -isnot [DateTime]) {
		$recordedStart = [DateTime]::Parse($recordedStart, [Globalization.CultureInfo]::InvariantCulture, [Globalization.DateTimeStyles]::RoundtripKind)
	}
	$delta = [Math]::Abs(($proc.StartTime.ToUniversalTime() - $recordedStart.ToUniversalTime()).TotalSeconds)
	if ($delta -gt 2) { return $null }
	return $proc
}

function Assert-NoCTestInBuildDir {
	# two ctest processes in one build directory corrupt its GoogleTest discovery files (m5b3-plan.md, the unit run's process note)
	foreach ($other in @(Get-CimInstance Win32_Process -Filter "Name='ctest.exe'" -ErrorAction SilentlyContinue)) {
		$cl = ([string]$other.CommandLine).Replace('/', '\')
		if ($cl.IndexOf($BuildDir, [StringComparison]::OrdinalIgnoreCase) -ge 0) {
			throw "a ctest already runs in build/msvc (pid $($other.ProcessId): $cl); wait for it to finish."
		}
		if (($cl -match 'build\\msvc' -or $cl -notmatch '--test-dir') -and -not $Force) {
			throw "a ctest runs that may be in build/msvc (pid $($other.ProcessId): $cl). Wait for it, or rerun with -Force if it certainly runs elsewhere."
		}
	}
}

function Set-TestEnv { foreach ($key in $TestEnv.Keys) { [Environment]::SetEnvironmentVariable($key, $TestEnv[$key], 'Process') } }

function Get-HexPreview([byte[]]$Bytes, [int]$Count) {
	$n = [Math]::Min($Count, $Bytes.Length)
	if ($n -le 0) { return '' }
	return ($Bytes[0..($n - 1)] | ForEach-Object { $_.ToString('X2') }) -join ' '
}

function Read-Exactly([IO.Stream]$Stream, [int]$Count) {
	$buffer = New-Object byte[] $Count
	$offset = 0
	while ($offset -lt $Count) {
		$read = $Stream.Read($buffer, $offset, $Count - $offset)
		if ($read -le 0) { throw "the server closed the connection after $offset of $Count bytes" }
		$offset += $read
	}
	return , $buffer
}

# --------------------------------------------------------------------------------------------------------------------------- commands

function Invoke-Status {
	Say "== ports"
	foreach ($port in @(2106, 9014, 7777, 3306)) {
		$what = 'MariaDB'
		if ($ServerPorts.ContainsKey($port)) { $what = $ServerPorts[$port] }
		$owner = Get-PortOwner $port
		if ($owner) { Say ("  {0,-5} {1,-27} LISTEN  pid {2} {3} [{4}] {5}" -f $port, $what, $owner.ProcessId, $owner.Name, $owner.Origin, $owner.Path) }
		else { Say ("  {0,-5} {1,-27} free" -f $port, $what) }
	}
	Say "== MariaDB: $(Test-MariaDb)"
	$state = Read-State
	$recorded = @{}
	if ($state) {
		foreach ($role in 'ls', 'gs') {
			$entry = $state.$role
			if ($entry) {
				if (Get-RecordedProcess $entry) { $recorded[[int]$entry.processId] = $role; $alive = 'running' } else { $alive = 'gone' }
				Say "== recorded by start ($StateFile): $role pid $($entry.processId), started $($entry.startTimeUtc) UTC - $alive"
			}
		}
	}
	Say "== aion processes"
	$procs = @(Get-Process -Name 'aion_*' -ErrorAction SilentlyContinue)
	if ($procs.Count -eq 0) { Say '  none' }
	foreach ($proc in $procs) {
		$tag = Get-Origin $proc.Path
		if ($recorded.ContainsKey($proc.Id)) { $tag = "started by this driver ($($recorded[$proc.Id]))" }
		elseif ($tag -eq 'build/msvc') { $tag = 'build/msvc, not started by this driver (a gate or a test?)' }
		Say ("  pid {0,-6} {1,-30} since {2:HH:mm:ss}  [{3}] {4}" -f $proc.Id, $proc.ProcessName, $proc.StartTime, $tag, $proc.Path)
	}
}

function Invoke-Build {
	$list = @($Targets | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
	if ($list.Count -eq 0) { throw 'no build targets' }
	$locking = @(Get-Process -ErrorAction SilentlyContinue | Where-Object { Test-Under $_.Path $BuildDir })
	if ($locking.Count -gt 0) {
		$names = ($locking | ForEach-Object { "$($_.ProcessName) (pid $($_.Id))" }) -join ', '
		throw "processes run from build/msvc and lock its binaries: $names. Stop them first (driver.ps1 stop for the servers it started; wait for a gate to finish)."
	}
	$builders = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
			$_.Name -in 'MSBuild.exe', 'cmake.exe' -and [string]$_.CommandLine -match 'build[\\/]msvc([\\/"\s]|$)' })
	if ($builders.Count -gt 0) { throw "another build of build/msvc is running (pid $($builders[0].ProcessId): $($builders[0].CommandLine))" }

	$cmake = Get-CMake 'cmake'
	$chat = Get-CacheValue 'AION_BUILD_CHAT_SERVER'
	$watch = [Diagnostics.Stopwatch]::StartNew()
	if ($chat -ne 'ON') {
		if ($null -eq $chat) { Say "build/msvc is not configured yet: cmake --preset msvc -DAION_BUILD_CHAT_SERVER=ON (the first configure builds the vcpkg packages)" }
		else { Say "build/msvc has AION_BUILD_CHAT_SERVER=$chat; reconfiguring with -DAION_BUILD_CHAT_SERVER=ON" }
		Push-Location $CppRoot
		try { & $cmake --preset msvc -DAION_BUILD_CHAT_SERVER=ON } finally { Pop-Location }
		if ($LASTEXITCODE -ne 0) { throw "configure failed (exit $LASTEXITCODE)" }
	}
	Say "building $($list -join ', ') ($BuildConfig, build/msvc)"
	# quoted: Windows PowerShell splits a bare -p:CL_MPCount=2 into "-p:" "CL_MPCount=2" and MSBuild fails with MSB1005
	& $cmake --build $BuildDir --config $BuildConfig --target $list --parallel 4 -- '-p:CL_MPCount=2' '-nr:false'
	$code = $LASTEXITCODE
	Say ("build {0} in {1:N0} s" -f $(if ($code -eq 0) { 'OK' } else { "FAILED (exit $code)" }), $watch.Elapsed.TotalSeconds)
	if ($code -ne 0) { exit $code }
}

function Wait-For([scriptblock]$Condition, [int]$Seconds, [string]$What, [Diagnostics.Process]$Proc, [string]$LogPath) {
	$deadline = (Get-Date).AddSeconds($Seconds)
	while (-not (& $Condition)) {
		if ($Proc -and $Proc.HasExited) {
			if ($LogPath) { @(Read-LogLines $LogPath) | Select-Object -Last 15 | ForEach-Object { Say "  | $_" } }
			throw "${What}: the process exited with code $($Proc.ExitCode) before it was ready (log: $LogPath)"
		}
		if ((Get-Date) -gt $deadline) { throw "${What}: not ready after $Seconds s (log: $LogPath)" }
		Start-Sleep -Milliseconds 500
	}
}

function Invoke-Start {
	$state = Read-State
	if ($state) {
		$alive = @('ls', 'gs' | Where-Object { Get-RecordedProcess $state.$_ })
		if ($alive.Count -gt 0) { throw "servers started by this driver are still running ($($alive -join ', ')); run driver.ps1 stop first" }
		Remove-Item -LiteralPath $StateFile
	}
	foreach ($port in 2106, 9014, 7777) {
		$owner = Get-PortOwner $port
		if ($owner) { throw "port $port ($($ServerPorts[$port])) is taken by pid $($owner.ProcessId) $($owner.Name) [$($owner.Origin)] $($owner.Path). Refusing to start." }
	}
	foreach ($exe in $LsExe, $GsExe) {
		if (-not (Test-Path $exe)) { throw "$exe does not exist; run driver.ps1 build first" }
		Say ("using {0} (built {1:yyyy-MM-dd HH:mm})" -f $exe, (Get-Item $exe).LastWriteTime)
	}
	if (-not (Test-Port 3306)) {
		Say 'MariaDB is down: starting it with D:\aion-dev\start-mariadb.bat'
		Start-Process -FilePath 'cmd.exe' -ArgumentList '/c', "`"$DevDir\start-mariadb.bat`"" -WorkingDirectory $DevDir -WindowStyle Minimized -Wait
		Wait-For { Test-Port 3306 } 60 'MariaDB' $null (Join-Path $DevDir 'mariadb-data\mariadb.err')
	}
	Say "MariaDB: $(Test-MariaDb)"

	$state = [ordered]@{}
	$started = [DateTimeOffset]::Now
	$ls = Start-Process -FilePath $LsExe -WorkingDirectory $LsDir -WindowStyle Minimized -PassThru
	$state.ls = [ordered]@{ processId = $ls.Id; exe = $LsExe; workingDirectory = $LsDir; startTimeUtc = $ls.StartTime.ToUniversalTime().ToString('o') }
	Write-State $state
	Say "login server: pid $($ls.Id), working directory $LsDir"
	Wait-For { (Test-Port 2106) -and (Test-Port 9014) } 60 'login server (2106/9014)' $ls (Get-LogPath 'ls')
	Say ("login server listens on 2106 and 9014 after {0:N1} s" -f ([DateTimeOffset]::Now - $started).TotalSeconds)

	$started = [DateTimeOffset]::Now
	$gs = Start-Process -FilePath $GsExe -WorkingDirectory $GsDir -WindowStyle Minimized -PassThru
	$state.gs = [ordered]@{ processId = $gs.Id; exe = $GsExe; workingDirectory = $GsDir; startTimeUtc = $gs.StartTime.ToUniversalTime().ToString('o') }
	Write-State $state
	Say "game server: pid $($gs.Id), working directory $GsDir; waiting up to $TimeoutSec s for 'Game server started' (about 2.5 min with geodata)"
	$gsLog = Get-LogPath 'gs'
	$since = $started.AddSeconds(-2)
	$script:readyLine = $null
	Wait-For {
		foreach ($line in @(Read-LogLines $gsLog)) {
			if ($line -match 'Game server started') {
				$time = Get-LogTime $line
				if ($time -and $time -ge $since) { $script:readyLine = $line; return $true }
			}
		}
		return $false
	} $TimeoutSec 'game server' $gs $gsLog
	Wait-For { Test-Port 7777 } 30 'game server (7777)' $gs $gsLog
	$lines = @(Read-LogLines $gsLog)
	$errorCount = @($lines | Where-Object { $_ -match '^\S+ ERROR ' }).Count
	$warnCount = @($lines | Where-Object { $_ -match '^\S+ WARN ' }).Count
	Say "READY: $script:readyLine" 'Green'
	Say "game server log $gsLog : $errorCount ERROR, $warnCount WARN lines; ports 2106/9014/7777 listen"
}

function Invoke-Probe {
	$ok = $true
	foreach ($target in @(@{ Port = 2106; What = 'login server' }, @{ Port = 7777; What = 'game server' })) {
		$port = $target.Port
		$owner = Get-PortOwner $port
		if (-not $owner) { Say "  $port ($($target.What)): nothing listens" 'Yellow'; $ok = $false; continue }
		if ($owner.Origin -like 'OWNER PLAY KIT*' -and -not $Force) {
			Say "  $port ($($target.What)): owned by the owner's play kit (pid $($owner.ProcessId)); not probing it" 'Yellow'; $ok = $false; continue
		}
		$client = New-Object Net.Sockets.TcpClient
		try {
			$watch = [Diagnostics.Stopwatch]::StartNew()
			$pending = $client.BeginConnect('127.0.0.1', $port, $null, $null)
			if (-not $pending.AsyncWaitHandle.WaitOne(3000)) { throw 'connect timed out after 3 s' }
			$client.EndConnect($pending)
			$stream = $client.GetStream()
			$stream.ReadTimeout = 5000
			$head = Read-Exactly $stream 2
			$length = [BitConverter]::ToUInt16($head, 0)   # little-endian, counts these two bytes too
			if ($length -lt 3) { throw "implausible length field $length" }
			$body = Read-Exactly $stream ($length - 2)
			$packet = New-Object byte[] $length
			[Array]::Copy($head, 0, $packet, 0, 2); [Array]::Copy($body, 0, $packet, 2, $body.Length)
			Say ("  {0} ({1}, pid {2} [{3}]): first packet {4} bytes after {5} ms; first 16 bytes: {6}" -f $port, $target.What,
				$owner.ProcessId, $owner.Origin, $length, $watch.ElapsedMilliseconds, (Get-HexPreview $packet 16)) 'Green'
		} catch {
			Say "  $port ($($target.What)): $($_.Exception.Message)" 'Red'; $ok = $false
		} finally { $client.Close() }
	}
	if (-not $ok) { exit 1 }
}

function Invoke-Logs {
	$path = Get-LogPath $Server
	$lines = @(Read-LogLines $path)
	if ($lines.Count -eq 0) { throw "no log at $path" }
	$first = Get-LogTime $lines[0]; $last = Get-LogTime $lines[-1]
	$errorLines = @($lines | Where-Object { $_ -match '^\S+ ERROR ' })
	$warnCount = @($lines | Where-Object { $_ -match '^\S+ WARN ' }).Count
	Say "== $path ($($lines.Count) lines, $first .. $last; $($errorLines.Count) ERROR, $warnCount WARN)"
	if ($Errors) {
		if ($errorLines.Count -eq 0) { Say '(no ERROR lines)' }
		$errorLines | Select-Object -Last $Tail | ForEach-Object { Say $_ }
	}
	else { $lines | Select-Object -Last $Tail | ForEach-Object { Say $_ } }
}

function Invoke-Stop {
	$state = Read-State
	if (-not $state) { Say "nothing recorded in $StateFile; this driver stops only servers it started, so it touches nothing."; return }
	$failed = $false
	foreach ($role in 'gs', 'ls') {   # the game server first: it saves its players and leaves the login server in order
		$entry = $state.$role
		if (-not $entry) { continue }
		$proc = Get-RecordedProcess $entry
		if (-not $proc) { Say "$role pid $($entry.processId): not running any more (or the PID now belongs to another process); left alone"; continue }
		$null = $proc.Handle   # keep a handle so the exit code stays readable
		Say "$role pid $($proc.Id): sending Ctrl+C"
		$watch = [Diagnostics.Stopwatch]::StartNew()
		$sender = Start-Process -FilePath 'powershell.exe' -ArgumentList '-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$Helper`"", $proc.Id `
			-WindowStyle Hidden -Wait -PassThru
		if ($sender.ExitCode -ne 0) { Say "  the Ctrl+C helper failed (exit $($sender.ExitCode)); the process is still running" 'Red'; $failed = $true; continue }
		if ($proc.WaitForExit(180000)) { Say ("  $role stopped after {0:N1} s, exit code {1}" -f $watch.Elapsed.TotalSeconds, $proc.ExitCode) }
		else { Say "  $role did not stop within 180 s; it still runs (pid $($proc.Id)). Not killing it: look at its window and log." 'Red'; $failed = $true }
	}
	if ($failed) { exit 1 }
	Remove-Item -LiteralPath $StateFile
	$tailLine = Read-LogLines (Get-LogPath 'gs') | Select-Object -Last 1
	Say "last game server log line: $tailLine"
}

function Invoke-CTest([string[]]$Arguments) {
	Assert-NoCTestInBuildDir
	Set-TestEnv
	$ctest = Get-CMake 'ctest'
	$watch = [Diagnostics.Stopwatch]::StartNew()
	$lines = New-Object Collections.Generic.List[string]
	# --no-tests=error: "No tests were found!!!" goes to stderr, so a regex that matches nothing would otherwise exit 0
	& $ctest --test-dir $BuildDir -C $BuildConfig --no-tests=error @Arguments | ForEach-Object { Write-Host $_; $lines.Add([string]$_) }
	$code = $LASTEXITCODE
	$ran = [bool]($lines | Where-Object { $_ -match '^\d+% tests passed' })
	return [pscustomobject]@{ Code = $code; Seconds = $watch.Elapsed.TotalSeconds; Lines = $lines.ToArray(); Ran = $ran }
}

function Invoke-Unit {
	$arguments = @('-j', "$Jobs", '--output-on-failure', '-LE', $UnitExclude)
	if ($Filter) { $arguments += @('-R', $Filter) }
	$run = Invoke-CTest $arguments
	if (-not $run.Ran) { Say "no unit test matches '$Filter' (outside the labels $UnitExclude)" 'Red'; exit 1 }
	Say ("unit: {0} in {1:N0} s (ctest exit {2})" -f $(if ($run.Code -eq 0) { 'PASSED' } else { 'FAILED' }), $run.Seconds, $run.Code)
	exit $run.Code
}

function Invoke-Gate {
	if (-not $Name -or $Name -notmatch '^[A-Za-z0-9_]+$') { throw 'usage: driver.ps1 gate <name>, e.g. m5a, m5c, startup, startup_progress' }
	if ($Name -match 'stress') { throw 'stress runs are opt-in for the owner (they load the whole machine); not started' }
	$regex = "^gs\.(scenario|smoke)\.$Name`$"
	$run = Invoke-CTest @('-R', $regex, '--output-on-failure')
	if (-not $run.Ran) {
		$known = Select-String -LiteralPath (Join-Path $BuildDir 'game-server\CTestTestfile.cmake') -Pattern 'add_test\(\[=\[gs\.(scenario|smoke)\.([a-z0-9_]+)\]=\]' |
			ForEach-Object { $_.Matches[0].Groups[2].Value } | Where-Object { $_ -notmatch 'stress' } | Sort-Object -Unique
		throw "no gate '$Name'. Known: $($known -join ', ')"
	}
	$verdict = 'FAILED'
	if ($run.Code -eq 0) { $verdict = 'PASSED' }
	if ($run.Lines -match '\*\*\*Skipped|Not Run') { $verdict = 'SKIPPED (not a pass)' }
	$testLine = $run.Lines | Where-Object { $_ -match 'Test\s+#\d+: gs\.' } | Select-Object -Last 1
	Say ("gate {0}: {1} in {2:N0} s wall clock" -f $Name, $verdict, $run.Seconds) $(if ($verdict -eq 'PASSED') { 'Green' } else { 'Red' })
	if ($testLine) { Say "  $($testLine.Trim())" }
	$outDir = Join-Path $BuildDir "game-server\scenario\$BuildConfig\$Name"
	if ($Name -match '^startup') { $outDir = Join-Path $BuildDir "game-server\gs.smoke.$Name\$BuildConfig" }
	if (Test-Path $outDir) {
		Say "  output: $outDir"
		foreach ($item in @(Get-ChildItem -LiteralPath $outDir)) { Say "    $($item.Name)" }
		$census = Join-Path $outDir 'check\census.txt'
		if (Test-Path $census) {
			$entries = @(Get-Content -LiteralPath $census | Where-Object { $_ -and -not $_.StartsWith('#') }).Count
			Say "  census: $census - $entries leak entries$(if ($entries -eq 0) { ' (clean)' })"
		}
		$summary = Get-ChildItem -LiteralPath (Join-Path $outDir 'check') -Filter '*_summary.txt' -ErrorAction SilentlyContinue | Select-Object -First 1
		if ($summary) {
			$keys = Get-Content -LiteralPath $summary.FullName | Where-Object { $_ -match '^(exitCode|unportedHits|censusLeaks|liveLeaks|lockdepReports|watchdogDumps) ' }
			Say "  summary ($($summary.Name)): $($keys -join '; ')"
		}
	} else { Say "  (no output directory at $outDir)" }
	exit $run.Code
}

# ------------------------------------------------------------------------------------------------------------------------------- main

try {
	switch ($Command) {
		'status' { Invoke-Status }
		'build' { Invoke-Build }
		'start' { Invoke-Start }
		'probe' { Invoke-Probe }
		'logs' { Invoke-Logs }
		'stop' { Invoke-Stop }
		'unit' { Invoke-Unit }
		'gate' { Invoke-Gate }
		default { Get-Content -LiteralPath $PSCommandPath -TotalCount 21 | Select-Object -Skip 1 | ForEach-Object { Say $_ }; if ($Command -ne 'help') { exit 1 } }
	}
} catch {
	Say "driver: $($_.Exception.Message)" 'Red'
	exit 1
}
