$ErrorActionPreference = 'Stop'

$workspaceRoot = Split-Path -Parent $PSScriptRoot
$vsDevCmd = 'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $vsDevCmd)) {
    throw "Visual Studio x64 tools were not found: $vsDevCmd"
}

$outputDirectory = Join-Path $PSScriptRoot 'dist'
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null

$source = Join-Path $PSScriptRoot 'src\SkillsPassivesFirst.cpp'
$include = Join-Path $workspaceRoot 'third_party\mewjector'
$output = Join-Path $outputDirectory 'SkillsPassivesFirst.dll'
$compile = "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 && cl /nologo /std:c++20 /LD /O2 /MT /EHsc /W4 /I `"$include`" `"$source`" /Fe:`"$output`" /link /nologo"

cmd /d /s /c $compile
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'SkillsPassivesFirst.ini') -Destination $outputDirectory -Force
