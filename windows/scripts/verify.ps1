# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param([switch]$InstallTest)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path "$PSScriptRoot/../..").Path
$lock = Get-Content (Join-Path $projectRoot 'windows/upstream.json') -Raw | ConvertFrom-Json
$package = Join-Path $projectRoot "outputs/windows/TailKana-$($lock.version)-windows-x64.msi"
if (!(Test-Path $package)) { throw "Missing installer: $package" }
$installer = New-Object -ComObject WindowsInstaller.Installer
$db = $installer.OpenDatabase($package, 0)
function Get-Property([string]$Name) {
    $view = $db.OpenView("SELECT ``Value`` FROM ``Property`` WHERE ``Property``='$Name'")
    $view.Execute(); $record = $view.Fetch()
    if (!$record) { throw "MSI property missing: $Name" }
    return $record.StringData(1)
}
if ((Get-Property 'ProductName') -ne 'TailKana') { throw 'Wrong product name' }
if ((Get-Property 'UpgradeCode').Trim('{}') -ne $lock.identity.upgradeCode) { throw 'Wrong upgrade identity' }
if ((Get-Property 'ProductVersion') -ne $lock.installerVersion) { throw 'Wrong installer version' }
$log = Join-Path $projectRoot 'work/windows/install.log'
if ($InstallTest) {
    $installed = $false
    try {
        $process = Start-Process msiexec.exe -ArgumentList @('/i',"`"$package`"",'/qn','/norestart','/l*v',"`"$log`"") -Wait -PassThru
        if ($process.ExitCode -notin @(0,3010)) { throw "Installer exit code $($process.ExitCode); see $log" }
        $installed = $true
        $test = Join-Path $projectRoot 'work/windows/core-build/Release/tailkana-windows-registration-test.exe'
        & $test "{$($lock.identity.textService)}" "{$($lock.identity.languageProfile)}"
        if ($LASTEXITCODE -ne 0) { throw "TSF registration/load test failed: $LASTEXITCODE" }
        $backend = Join-Path $projectRoot 'work/windows/mozc/src/bazel-bin/win32/tip/tailkana_backend_test.exe'
        & $backend
        if ($LASTEXITCODE -ne 0) { throw "Real Windows converter test failed: $LASTEXITCODE" }
    } finally {
        if ($installed) {
            $process = Start-Process msiexec.exe -ArgumentList @('/x',"`"$package`"",'/qn','/norestart') -Wait -PassThru
            if ($process.ExitCode -notin @(0,3010)) { throw "Test uninstall failed: $($process.ExitCode)" }
        }
    }
}
Write-Host 'PASS: installer product, version, stable upgrade identity and requested registration checks'
