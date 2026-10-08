<#
.SYNOPSIS
Runs one SQL file through the DuckDB CLI under a hard timeout. If the process
never exits, captures a thread dump, kills it and fails.

.DESCRIPTION
Used by the windows-live-query job in MainDistributionPipeline.yml (issue #70).
The point of the timeout is issue #69: on Windows the CLI could stay alive
forever after a successful snowflake_query(), so a plain `duckdb -c` step would
run until the job limit. A process that is still alive after -TimeoutSeconds is
treated as hung: gdb attaches and prints every thread's backtrace to
$env:RUNNER_TEMP\<Label>-threads.txt (uploaded by the workflow), then the
process is killed and the script exits 1.

Output of the CLI is printed to the job log (where GitHub masks secrets); it is
deliberately NOT written into the uploaded artifact, because a failing
CREATE SECRET can echo the statement.

.PARAMETER DuckDb
Path to duckdb.exe.

.PARAMETER SqlFile
SQL file executed with `.read`. Written with forward slashes for the CLI.

.PARAMETER Label
Short name used in log lines and the thread-dump file name.

.PARAMETER Expect
Optional literal that must appear in stdout for the check to pass.

.PARAMETER TimeoutSeconds
Wall-clock budget for the whole CLI run, including exit. Default 180.
#>
param(
    [Parameter(Mandatory = $true)] [string] $DuckDb,
    [Parameter(Mandatory = $true)] [string] $SqlFile,
    [Parameter(Mandatory = $true)] [string] $Label,
    [string] $Expect = "",
    [int] $TimeoutSeconds = 180
)

$ErrorActionPreference = "Stop"

function Find-Gdb {
    $cmd = Get-Command gdb -ErrorAction SilentlyContinue
    if ($cmd) {
        return $cmd.Source
    }
    $candidates = @(
        "C:\ProgramData\mingw64\mingw64\bin\gdb.exe",
        "C:\msys64\mingw64\bin\gdb.exe",
        "C:\msys64\ucrt64\bin\gdb.exe",
        "C:\Strawberry\c\bin\gdb.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) {
            return $c
        }
    }
    return $null
}

# cdb reads the PDB-less MSVC binaries' export tables and resolves module!symbol
# frames that gdb (a MinGW build) prints as ??. Present when the image's Windows
# SDK includes the Debugging Tools; optional.
function Find-Cdb {
    $candidates = @(
        "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe",
        "C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) {
            return $c
        }
    }
    return $null
}

$tempDir = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [System.IO.Path]::GetTempPath() }
$stdout = Join-Path $tempDir "$Label.stdout.txt"
$stderr = Join-Path $tempDir "$Label.stderr.txt"
$dump = Join-Path $tempDir "$Label-threads.txt"
$sqlForCli = $SqlFile -replace '\\', '/'

# Start-Process joins the list with spaces and does not quote, so the .read
# argument carries its own quotes to survive the space after the dot-command.
$argList = @('-unsigned', '-noheader', '-list', '-c', "`".read $sqlForCli`"")

Write-Host "== ${Label}: $DuckDb $($argList -join ' ')"
$sw = [System.Diagnostics.Stopwatch]::StartNew()
$proc = Start-Process -FilePath $DuckDb -ArgumentList $argList -NoNewWindow -PassThru `
    -RedirectStandardOutput $stdout -RedirectStandardError $stderr
# Touching Handle makes ExitCode reliable after WaitForExit (PowerShell quirk).
$null = $proc.Handle

$exited = $proc.WaitForExit($TimeoutSeconds * 1000)
$sw.Stop()

if (-not $exited) {
    $hungPid = $proc.Id
    Write-Host "HANG: $Label still running after $TimeoutSeconds s (pid $hungPid). Capturing threads."
    try {
        # Commands go through a script file, not -ex: PowerShell split each
        # multi-word -ex argument at its spaces on the way to gdb.exe, so the
        # first run captured gdb's help text instead of backtraces.
        $gdb = Find-Gdb
        if ($gdb) {
            $gdbScript = Join-Path $tempDir "$Label.gdb"
            Set-Content -Path $gdbScript -Encoding ascii -Value @(
                "set pagination off",
                "set print thread-events on",
                "info sharedlibrary",
                "info threads",
                "thread apply all bt"
            )
            Write-Host "gdb: $gdb"
            "===== gdb =====" | Out-File -FilePath $dump -Encoding utf8
            & $gdb -p $hungPid -batch -x $gdbScript 2>&1 | Tee-Object -FilePath $dump -Append
        }
        else {
            "gdb not found on this runner" | Tee-Object -FilePath $dump -Append
        }
        $cdb = Find-Cdb
        if ($cdb) {
            Write-Host "cdb: $cdb"
            "===== cdb =====" | Out-File -FilePath $dump -Encoding utf8 -Append
            # Command file for the same reason as gdb. -pv attaches
            # non-invasively, so it works after gdb has detached. lm = loaded
            # modules, ~*k = every thread's stack, qd = detach and quit.
            $cdbScript = Join-Path $tempDir "$Label.cdb"
            Set-Content -Path $cdbScript -Encoding ascii -Value @("lm", "~*k", "qd")
            & $cdb -pv -p $hungPid -cf $cdbScript 2>&1 | Tee-Object -FilePath $dump -Append
        }
        else {
            "cdb not found on this runner" | Tee-Object -FilePath $dump -Append
        }
        Write-Host "-- stdout so far --"
        if (Test-Path $stdout) { Get-Content $stdout }
        Write-Host "-- stderr so far --"
        if (Test-Path $stderr) { Get-Content $stderr }
    }
    finally {
        # Always reap the hung process, even if gdb or the log reads throw;
        # otherwise it outlives the step and the runner.
        Stop-Process -Id $hungPid -Force -ErrorAction SilentlyContinue
    }
    Write-Host "FAIL: $Label did not exit (issue #69). Thread dump: $dump"
    exit 1
}

Write-Host "-- stdout --"
$outText = if (Test-Path $stdout) { Get-Content $stdout -Raw } else { "" }
if ($outText) { Write-Host $outText }
Write-Host "-- stderr --"
if (Test-Path $stderr) { Get-Content $stderr }

if ($proc.ExitCode -ne 0) {
    Write-Host "FAIL: $Label exited with code $($proc.ExitCode) after $([int]$sw.Elapsed.TotalSeconds) s"
    exit 1
}
if ($Expect -and ($outText -notmatch [regex]::Escape($Expect))) {
    Write-Host "FAIL: $Label exited 0 but stdout does not contain '$Expect'"
    exit 1
}
Write-Host "OK: $Label exited 0 in $([int]$sw.Elapsed.TotalSeconds) s"
exit 0
