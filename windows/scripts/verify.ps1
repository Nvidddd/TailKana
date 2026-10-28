# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param([switch]$InstallTest)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path "$PSScriptRoot/../..").Path
$lock = Get-Content (Join-Path $projectRoot 'windows/upstream.json') -Raw | ConvertFrom-Json
$package = Join-Path $projectRoot "outputs/windows/TailKana-$($lock.version)-windows-x64.msi"
if (!(Test-Path $package)) { throw "Missing installer: $package" }
$test = Join-Path $projectRoot 'work/windows/core-build/Release/tailkana-windows-registration-test.exe'
& $test --msi $package "{$($lock.identity.upgradeCode)}" $lock.installerVersion
if ($LASTEXITCODE -ne 0) { throw "Native MSI metadata verification failed: $LASTEXITCODE" }
$log = Join-Path $projectRoot 'work/windows/install.log'
if ($InstallTest) {
    $installed = $false
    try {
        $process = Start-Process msiexec.exe -ArgumentList @('/i',"`"$package`"",'/qn','/norestart','/l*v',"`"$log`"") -Wait -PassThru
        if ($process.ExitCode -notin @(0,3010)) { throw "Installer exit code $($process.ExitCode); see $log" }
        $installed = $true
        & $test "{$($lock.identity.textService)}" "{$($lock.identity.languageProfile)}"
        if ($LASTEXITCODE -ne 0) { throw "TSF registration/load test failed: $LASTEXITCODE" }
        $backend = Join-Path $projectRoot 'work/windows/mozc/src/bazel-bin/win32/tip/tailkana_backend_test.exe'
        $stdout = Join-Path $projectRoot 'work/windows/backend-test.log'
        $stderr = Join-Path $projectRoot 'work/windows/backend-test.stderr.log'
        $process = Start-Process $backend -NoNewWindow -PassThru -RedirectStandardOutput $stdout -RedirectStandardError $stderr
        if (!$process.WaitForExit(120000)) { $process.Kill(); throw 'Real converter check exceeded two minutes' }
        $process.WaitForExit(); $process.Refresh()
        Get-Content $stdout
        if ($process.ExitCode -ne 0) { Get-Content $stderr; throw "Real Windows converter test failed: $($process.ExitCode)" }
    } finally {
        if ($installed) {
            $process = Start-Process msiexec.exe -ArgumentList @('/x',"`"$package`"",'/qn','/norestart') -Wait -PassThru
            if ($process.ExitCode -notin @(0,3010)) { throw "Test uninstall failed: $($process.ExitCode)" }
        }
    }
}
Write-Host 'PASS: installer product, version, stable upgrade identity and requested registration checks'
