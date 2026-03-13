param(
  [ValidateSet("Debug", "Release")]
  [string]$Configuration = "Debug",
  [string]$Architecture = "x64",
  [string]$Generator = "Visual Studio 17 2022",
  [string]$LibmpvReleaseTag = "20260307",
  [switch]$Run,
  [switch]$ForceLibmpv,
  [switch]$SkipDeploy
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Require-Command {
  param(
    [string]$Name,
    [string]$Message
  )

  $command = Get-Command $Name -ErrorAction SilentlyContinue
  if (-not $command) {
    throw $Message
  }

  return $command.Source
}

function Get-RepoRoot {
  param([string]$StartDirectory)

  $current = [System.IO.Path]::GetFullPath($StartDirectory)
  while ($true) {
    if (Test-Path (Join-Path $current ".git")) {
      return $current
    }

    $parent = Split-Path -Parent $current
    if ($parent -eq $current) {
      throw "Could not locate the repository root from '$StartDirectory'. Run this script from inside the repository."
    }

    $current = $parent
  }
}

function Invoke-VsDevCommand {
  param(
    [string]$Command,
    [string]$WorkingDirectory
  )

  $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) {
    throw "vswhere.exe was not found. Install Visual Studio 2022 with Desktop development with C++."
  }

  $installationPath = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
  if (-not $installationPath) {
    throw "Could not locate a Visual Studio installation with MSBuild."
  }

  $vsDevCmd = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
  if (-not (Test-Path $vsDevCmd)) {
    throw "VsDevCmd.bat was not found at '$vsDevCmd'."
  }

  $fullCommand = "call `"$vsDevCmd`" -arch=$Architecture -host_arch=$Architecture && cd /d `"$WorkingDirectory`" && $Command"
  $arguments = @("/d", "/s", "/c", $fullCommand)
  & cmd.exe @arguments
  if ($LASTEXITCODE -ne 0) {
    throw "Command failed after initializing the Visual Studio build environment: $Command"
  }
}

function Ensure-Vcpkg {
  param(
    [string]$RepoRoot
  )

  if ($env:VCPKG_ROOT) {
    $candidate = [System.IO.Path]::GetFullPath($env:VCPKG_ROOT)
  } else {
    $candidate = Join-Path $RepoRoot ".tools\vcpkg"
  }

  if (-not (Test-Path (Join-Path $candidate ".git"))) {
    git clone --depth 1 https://github.com/microsoft/vcpkg $candidate
    if ($LASTEXITCODE -ne 0) {
      throw "Failed to clone vcpkg into '$candidate'."
    }
  }

  $bootstrap = Join-Path $candidate "bootstrap-vcpkg.bat"
  if (-not (Test-Path $bootstrap)) {
    throw "vcpkg bootstrap script not found at '$bootstrap'."
  }

  & $bootstrap -disableMetrics
  if ($LASTEXITCODE -ne 0) {
    throw "Failed to bootstrap vcpkg."
  }

  return $candidate
}

function Ensure-SevenZip {
  if (Get-Command 7z -ErrorAction SilentlyContinue) {
    return
  }

  throw "7z was not found in PATH. Install 7-Zip and reopen the shell before running this script."
}

$repoRoot = Get-RepoRoot -StartDirectory (Get-Location).Path
$buildRoot = Join-Path $repoRoot "build"
$nativeBuildDir = Join-Path $buildRoot "native-shell-win"
$libmpvDir = Join-Path $buildRoot "windows-libmpv"
$deployExe = Join-Path $nativeBuildDir "$Configuration\video-preview-native.exe"
$repoRootWindows = $repoRoot -replace '/', '\'

Require-Command -Name "git" -Message "git is required."
Require-Command -Name "cmake" -Message "cmake is required in PATH."
Require-Command -Name "cargo" -Message "Rust/cargo is required in PATH. Install rustup before running this script."
Ensure-SevenZip

$vcpkgRoot = Ensure-Vcpkg -RepoRoot $repoRoot
$env:VCPKG_ROOT = $vcpkgRoot
$env:VCPKG_DEFAULT_BINARY_CACHE = Join-Path $repoRoot ".cache\vcpkg\archives"
$env:VCPKG_DOWNLOADS = Join-Path $repoRoot ".cache\vcpkg\downloads"

New-Item -ItemType Directory -Path $env:VCPKG_DEFAULT_BINARY_CACHE -Force | Out-Null
New-Item -ItemType Directory -Path $env:VCPKG_DOWNLOADS -Force | Out-Null
New-Item -ItemType Directory -Path $buildRoot -Force | Out-Null

& (Join-Path $vcpkgRoot "vcpkg.exe") install --triplet x64-windows --x-manifest-root=$repoRoot
if ($LASTEXITCODE -ne 0) {
  throw "Failed to install vcpkg dependencies."
}

$forceFlag = if ($ForceLibmpv) { " -Force" } else { "" }
$prepareLibmpv = @(
  'powershell',
  '-NoProfile',
  '-ExecutionPolicy', 'Bypass',
  '-File', 'scripts\windows\prepare-libmpv.ps1',
  '-Destination', "`"$libmpvDir`"",
  '-ReleaseTag', $LibmpvReleaseTag
) -join ' '

Invoke-VsDevCommand -WorkingDirectory $repoRootWindows -Command ($prepareLibmpv + $forceFlag)

$configureCommand = @(
  'cmake',
  '-S', 'apps/native-shell',
  '-B', "`"$nativeBuildDir`"",
  '-G', "`"$Generator`"",
  '-A', $Architecture,
  "-DCMAKE_TOOLCHAIN_FILE=`"$vcpkgRoot\scripts\buildsystems\vcpkg.cmake`"",
  '-DVCPKG_TARGET_TRIPLET=x64-windows',
  "-DVCPKG_MANIFEST_DIR=`"$repoRootWindows`"",
  "-DLIBMPV_DIR=`"$libmpvDir`""
) -join ' '
Invoke-VsDevCommand -WorkingDirectory $repoRootWindows -Command $configureCommand

$buildCommand = @(
  'cmake',
  '--build', "`"$nativeBuildDir`"",
  '--config', $Configuration,
  '--parallel', '4'
) -join ' '
Invoke-VsDevCommand -WorkingDirectory $repoRootWindows -Command $buildCommand

if (-not $SkipDeploy) {
  $deployCommand = @(
    'cmake',
    '--build', "`"$nativeBuildDir`"",
    '--target', 'deploy-video-preview-native',
    '--config', $Configuration
  ) -join ' '
  Invoke-VsDevCommand -WorkingDirectory $repoRootWindows -Command $deployCommand
}

Write-Host ""
Write-Host "Native shell build complete."
Write-Host "Executable: $deployExe"
Write-Host "LIBMPV_DIR: $libmpvDir"
Write-Host "VCPKG_ROOT: $vcpkgRoot"

if ($Run) {
  if (-not (Test-Path $deployExe)) {
    throw "The built executable was not found at '$deployExe'."
  }

  Write-Host "Launching $deployExe"
  Start-Process -FilePath $deployExe -WorkingDirectory (Split-Path -Parent $deployExe)
}
