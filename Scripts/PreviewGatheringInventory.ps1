param(
    [string]$EditorPath = 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$projectFile = Join-Path $projectRoot 'PeaceSign.uproject'
if (-not (Test-Path -LiteralPath $EditorPath)) { throw 'Unreal Editor executable was not found. Supply -EditorPath.' }
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'Binaries/Win64/UnrealEditor-PeaceSign.dll'))) {
    throw 'Build PeaceSignEditor Win64 Development before opening the preview.'
}

$previewName = 'gathering-play-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + ([guid]::NewGuid().ToString('N').Substring(0, 8))
$previewRoot = Join-Path (Join-Path $projectRoot '.codex-tmp') $previewName
$previewSaves = Join-Path $previewRoot 'Saved/SaveGames'
New-Item -ItemType Directory -Path $previewSaves -Force | Out-Null
$originalSaves = Join-Path $projectRoot 'Saved/SaveGames'
if (Test-Path -LiteralPath $originalSaves) {
    Get-ChildItem -LiteralPath $originalSaves -Filter '*.sav' -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $previewSaves
    }
}
$logPath = Join-Path $previewRoot 'PreviewGame.log'
$arguments = @(
    ('"' + $projectFile + '"'), '/Game/Maps/TestMap', '-game', '-windowed', '-ForceRes',
    '-ResX=1440', '-ResY=900', '-NoSound', '-nop4', '-nosplash',
    ('-UserDir="' + $previewRoot + '"'), '-ExecCmds="PS.Gathering.PreviewItems"',
    '-PSGatheringPreviewCapture', ('-abslog="' + $logPath + '"')
)
# The requested preview is an interactive game window; leave it open for the user.
$previewProcess = Start-Process -FilePath $EditorPath -ArgumentList $arguments -WorkingDirectory $projectRoot -WindowStyle Normal -PassThru
[pscustomobject]@{ ProcessId = $previewProcess.Id; PreviewDirectory = $previewRoot; Log = $logPath }
