param(
    [Parameter(Mandatory = $true)]
    [string]$EngineRoot,
    [switch]$LauncherOnly
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$outputRoot = Join-Path $projectRoot 'DedicatedServer'
$uat = Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
if (!(Test-Path -LiteralPath $uat)) { throw "Engine not found: $EngineRoot" }
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null

if (!$LauncherOnly)
{
    # Archive the complete server package, including cooked content.
    & $uat BuildCookRun "-project=$projectRoot/DeepRaiders.uproject" `
        -noP4 -build -cook -stage -pak -archive -server -noclient `
        -serverplatform=Win64 -serverconfig=Development `
        "-archivedirectory=$outputRoot" '-ubtargs=-NoUBA'
    if ($LASTEXITCODE -ne 0) { throw 'Server packaging failed.' }
}

$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
& $compiler /nologo /target:exe "/out:$outputRoot/StartRoomMaster.exe" `
    (Join-Path $PSScriptRoot 'RoomMasterLauncher.cs')
if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed.' }
[IO.File]::WriteAllText((Join-Path $outputRoot 'EnginePath.txt'), $EngineRoot)
Write-Host "Ready: $outputRoot/StartRoomMaster.exe"
