# Deploy RelWithDebInfo build into release\OpenGD
# Usage: powershell -File deploy-release.ps1

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "build\bin\OpenGD\RelWithDebInfo"
$dst = Join-Path $root "release\OpenGD"

if (-not (Test-Path $src)) {
	Write-Error "Build output not found: $src"
}

New-Item -ItemType Directory -Force -Path $dst | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $dst "Content") | Out-Null

# Binary + runtime DLLs (skip symlink dirs axslc/Content)
Get-ChildItem $src -Force | Where-Object {
	-not $_.PSIsContainer -or ($_.Name -notin @("axslc", "Content"))
} | ForEach-Object {
	$target = Join-Path $dst $_.Name
	if ($_.PSIsContainer) {
		Copy-Item $_.FullName $target -Recurse -Force
	} else {
		Copy-Item $_.FullName $target -Force
	}
	Write-Host "Copied $($_.Name)"
}

# Project Content (Custom levels, object.json, etc.)
$contentSrc = Join-Path $root "Content"
$contentDst = Join-Path $dst "Content"
if (Test-Path $contentSrc) {
	Copy-Item (Join-Path $contentSrc "*") $contentDst -Recurse -Force
	# Strip reverse-engineering / scratch dumps from the shipped tree
	$junk = @(
		"*_raw.txt",
		"*_dump.txt",
		"sneak_peek*",
		"create_menu_ids_raw.txt",
		"blocks_2.2_raw.txt",
		"level_settings_init_dump.txt"
	)
	$customDst = Join-Path $contentDst "Custom"
	if (Test-Path $customDst) {
		foreach ($pat in $junk) {
			Get-ChildItem $customDst -Filter $pat -ErrorAction SilentlyContinue | Remove-Item -Force
		}
	}
	Write-Host "Synced Content -> release\OpenGD\Content"
}

# Convenience launcher in release folder
$runBat = @"
@echo off
cd /d "%~dp0"
start "" OpenGD.exe
"@
Set-Content -Path (Join-Path $dst "run.bat") -Value $runBat -Encoding ASCII

Write-Host "Deploy done: $dst"
