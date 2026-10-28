# SPDX-License-Identifier: GPL-3.0-or-later
[CmdletBinding()]
param([switch]$SkipDependencies, [switch]$SkipQt, [switch]$PackageOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path "$PSScriptRoot/../..").Path
$mozcRoot = Join-Path $projectRoot 'work/windows/mozc'
$lock = Get-Content (Join-Path $projectRoot 'windows/upstream.json') -Raw | ConvertFrom-Json
function Run-Native([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed ($LASTEXITCODE): $Arguments" }
}
Push-Location $projectRoot
try {
    if (!(Test-Path (Join-Path $mozcRoot '.git'))) {
        Run-Native git @('clone','--no-checkout',$lock.repository,$mozcRoot)
        Run-Native git @('-C',$mozcRoot,'checkout','--detach',$lock.commit)
    }
    Run-Native python @('windows/scripts/prepare.py','--source',$mozcRoot)
    if (!$PackageOnly) {
    Run-Native cmake @('-S','windows','-B','work/windows/core-build','-A','x64')
    Run-Native cmake @('--build','work/windows/core-build','--config','Release','--parallel','4')
    Run-Native ctest @('--test-dir','work/windows/core-build','-C','Release','--output-on-failure')
    }
    Push-Location (Join-Path $mozcRoot 'src')
    try {
        if (!$PackageOnly) {
        if (!$SkipDependencies) { Run-Native python @('build_tools/update_deps.py') }
        Run-Native bazelisk @('build','//win32/tip:mozc_tip64','--config','release_build','--platforms=//:windows-x86_64',"--disk_cache=$projectRoot/work/windows/bazel-cache")
        if (!$SkipQt) { Run-Native python @('build_tools/build_qt.py','--release','--confirm_license','--target_arch=x64') }
        }
        Run-Native bazelisk @('build','package','--config','release_build','--platforms=//:windows-x86_64',"--disk_cache=$projectRoot/work/windows/bazel-cache")
    } finally { Pop-Location }
    $output = Join-Path $projectRoot 'outputs/windows'
    New-Item -ItemType Directory -Force $output | Out-Null
    $package = Join-Path $output "TailKana-$($lock.version)-windows-x64.msi"
    Copy-Item (Join-Path $mozcRoot 'src/bazel-bin/win32/installer/Mozc64.msi') $package -Force
    Copy-Item (Join-Path $projectRoot 'windows/README.md') (Join-Path $output 'README.txt') -Force
    Copy-Item (Join-Path $projectRoot 'LICENSE') (Join-Path $output 'LICENSE.txt') -Force
    $hash = (Get-FileHash $package -Algorithm SHA256).Hash.ToLower()
    "$hash  $([IO.Path]::GetFileName($package))" | Set-Content (Join-Path $output 'SHA256SUMS.txt') -Encoding ascii
    Write-Host "Windows installer ready: $package"
} finally { Pop-Location }
