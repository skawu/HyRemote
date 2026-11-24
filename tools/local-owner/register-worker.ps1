#requires -Version 5.1
<#
Opt-in one-time installer for the HyRemote Local Owner inbox.
Without -Register this is read-only and never installs a service or
runs a remote work order.
#>
[CmdletBinding()]
param(
    [switch] $Register,
    [Parameter(Mandatory = $true)] [string] $Workspace,
    [Parameter(Mandatory = $true)] [string] $KitConfig,
    [Parameter(Mandatory = $true)] [string] $PrefixBin,
    [string] $StateDir = (Join-Path $HOME ".hyremote-local-owner")
)
$ErrorActionPreference = "Stop"
if ([System.Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw "Only the qualified Windows host may register the worker"
}
$worker = Join-Path $PSScriptRoot "worker.py"
$python = (Get-Command "python.exe" -ErrorAction Stop).Source
$null = Get-Command "gh.exe" -ErrorAction Stop
$null = Get-Command "git.exe" -ErrorAction Stop
foreach ($required in @($worker, $Workspace, $KitConfig, $PrefixBin)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Missing required local path: $required"
    }
}
$worker = (Resolve-Path -LiteralPath $worker).Path
$Workspace = (Resolve-Path -LiteralPath $Workspace).Path
$KitConfig = (Resolve-Path -LiteralPath $KitConfig).Path
$PrefixBin = (Resolve-Path -LiteralPath $PrefixBin).Path
foreach ($field in @($worker, $Workspace, $KitConfig, $PrefixBin, $StateDir)) {
    if ($field -match '[\x00-\x1F"]') {
        throw "Unsafe local path characters; cannot register"
    }
}
& $python $worker --self-test
if ($LASTEXITCODE -ne 0) {
    throw "Local worker offline self-test failed"
}
& gh.exe auth status --hostname github.com *> $null
if ($LASTEXITCODE -ne 0) {
    throw "GitHub CLI is not authenticated for the current Windows user"
}
$argLine = ('"{0}" --once --execute --workspace "{1}" --kit-config "{2}" --prefix-bin "{3}" --state-dir "{4}"' -f $worker, $Workspace, $KitConfig, $PrefixBin, $StateDir)
Write-Host "HyRemote Local Owner worker, user $([System.Security.Principal.WindowsIdentity]::GetCurrent().Name)"
Write-Host "Task: HyRemote-LocalOwner-Inbox (every 2 minutes; current interactive user)"
Write-Host "Candidate workspace: $Workspace"
Write-Host "Kit profile: $KitConfig"
Write-Host "Installed binaries: $PrefixBin"
Write-Host "Local ledger: $StateDir"
if (-not $Register) {
    Write-Host "DRY_RUN. Re-run with -Register for one-time local opt-in. No task was created."
    exit 0
}
$taskName = "HyRemote-LocalOwner-Inbox"
$action = New-ScheduledTaskAction -Execute $python -Argument $argLine -WorkingDirectory $PSScriptRoot
$trigger = New-ScheduledTaskTrigger -Once -At (Get-Date).AddMinutes(1) -RepetitionInterval (New-TimeSpan -Minutes 2)
$principal = New-ScheduledTaskPrincipal -UserId ([System.Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType Interactive -RunLevel Limited
$settings = New-ScheduledTaskSettingsSet -MultipleInstances IgnoreNew -ExecutionTimeLimit ([TimeSpan]::Zero)
Register-ScheduledTask -TaskName $taskName -Action $action -Trigger $trigger -Principal $principal -Settings $settings -Description "Reviewed GitHub orders to restricted Windows Local Owner worker" -Force | Out-Null
Write-Host "REGISTERED: $taskName. First actual RECEIVED ACK on #95 proves delivery."
