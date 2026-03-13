param(
  [string]$Destination = "",
  [string]$ReleaseTag = "",
  [string]$Architecture = "x86_64",
  [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$githubHeaders = @{
  "Accept" = "application/vnd.github+json"
  "User-Agent" = "video-preview-generator"
}

function Require-Command {
  param([string]$Name, [string]$Message)

  $command = Get-Command $Name -ErrorAction SilentlyContinue
  if (-not $command) {
    throw $Message
  }

  return $command.Source
}

function Get-ReleaseMetadata {
  param([string]$Tag)

  if ($Tag) {
    return Invoke-RestMethod -Headers $githubHeaders -Uri "https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/tags/$Tag"
  }

  return Invoke-RestMethod -Headers $githubHeaders -Uri "https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/latest"
}

if (-not $Destination) {
  $Destination = Join-Path $PSScriptRoot "..\..\build\windows-libmpv"
}

$Destination = [System.IO.Path]::GetFullPath($Destination)
$binDir = Join-Path $Destination "bin"
$includeDir = Join-Path $Destination "include"
$libDir = Join-Path $Destination "lib"
$runtimeDllPath = Join-Path $binDir "libmpv-2.dll"
$importLibPath = Join-Path $libDir "libmpv.lib"

if ((-not $Force) -and (Test-Path $runtimeDllPath) -and (Test-Path $importLibPath) -and (Test-Path (Join-Path $includeDir "mpv\client.h"))) {
  Write-Host "libmpv is already prepared at $Destination"
  Write-Host "LIBMPV_DIR=$Destination"
  exit 0
}

$sevenZip = Require-Command "7z" "7z is required to extract the mpv development archive."
$dumpbin = Require-Command "dumpbin.exe" "dumpbin.exe is required. Run this script from a Visual Studio Developer PowerShell."
$libExe = Require-Command "lib.exe" "lib.exe is required. Run this script from a Visual Studio Developer PowerShell."

$release = Get-ReleaseMetadata -Tag $ReleaseTag
$asset = $release.assets | Where-Object { $_.name -like "mpv-dev-$Architecture-*.7z" } | Select-Object -First 1
if (-not $asset) {
  throw "Could not find an mpv-dev asset for architecture '$Architecture' in release '$($release.tag_name)'."
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("vpg-libmpv-" + [System.Guid]::NewGuid().ToString("N"))
$null = New-Item -ItemType Directory -Path $tempRoot -Force
$archivePath = Join-Path $tempRoot $asset.name
$extractDir = Join-Path $tempRoot "extracted"
$defPath = Join-Path $tempRoot "libmpv.def"

try {
  Invoke-WebRequest -Headers $githubHeaders -Uri $asset.browser_download_url -OutFile $archivePath
  & $sevenZip x $archivePath "-o$extractDir" -y | Out-Null

  $headersPath = Join-Path $extractDir "include"
  $dllSourcePath = Join-Path $extractDir "libmpv-2.dll"
  if (-not (Test-Path $headersPath) -or -not (Test-Path $dllSourcePath)) {
    throw "The downloaded mpv-dev archive did not contain the expected headers and libmpv-2.dll runtime."
  }

  $null = New-Item -ItemType Directory -Path $binDir -Force
  $null = New-Item -ItemType Directory -Path $includeDir -Force
  $null = New-Item -ItemType Directory -Path $libDir -Force

  Copy-Item -Path (Join-Path $headersPath "mpv") -Destination $includeDir -Recurse -Force
  Copy-Item -Path $dllSourcePath -Destination $runtimeDllPath -Force

  $exportNames = New-Object System.Collections.Generic.List[string]
  foreach ($line in (& $dumpbin /nologo /exports $runtimeDllPath)) {
    $tokens = ($line -split "\s+") | Where-Object { $_ }
    if ($tokens.Length -ge 4 -and $tokens[0] -match "^\d+$" -and $tokens[1] -match "^[0-9A-F]+$" -and $tokens[2] -match "^[0-9A-F]+$") {
      $exportNames.Add($tokens[3])
    }
  }

  if ($exportNames.Count -eq 0) {
    throw "Failed to extract exports from libmpv-2.dll."
  }

  @(
    "LIBRARY libmpv-2.dll",
    "EXPORTS"
  ) + ($exportNames | Sort-Object -Unique | ForEach-Object { "  $_" }) | Set-Content -Path $defPath -Encoding ascii

  & $libExe /nologo "/def:$defPath" "/out:$importLibPath" /machine:x64 | Out-Null

  if (-not (Test-Path $importLibPath)) {
    throw "lib.exe did not produce $importLibPath."
  }
}
finally {
  Remove-Item -Path $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host "Prepared libmpv at $Destination"
Write-Host "LIBMPV_DIR=$Destination"
