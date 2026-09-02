[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'

$workspaceRoot = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -Raw -LiteralPath (Join-Path $workspaceRoot 'VERSION')).Trim()
$versionParts = $version.Split('.')
$invalidVersionPart = $versionParts | Where-Object { $_ -notmatch '^\d+$' }
if ($versionParts.Count -ne 3 -or $null -ne $invalidVersionPart) {
    throw 'VERSION must contain a three-part numeric version, for example 1.0.2.'
}

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $workspaceRoot "build\$Configuration"
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    $vswhereCommand = Get-Command vswhere.exe -ErrorAction SilentlyContinue
    if ($null -eq $vswhereCommand) {
        throw 'vswhere.exe was not found. Install Visual Studio 2022 or Build Tools 2022 with the x64 C++ tools.'
    }
    $vswhere = $vswhereCommand.Source
}

$installationPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($installationPath)) {
    $visualStudioRoots = @(
        (Join-Path $env:ProgramFiles 'Microsoft Visual Studio\2022'),
        (Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\2022')
    ) | Where-Object { Test-Path -LiteralPath $_ }

    $installationPath = $visualStudioRoots |
        ForEach-Object { Get-ChildItem -LiteralPath $_ -Directory } |
        Where-Object {
            (Test-Path -LiteralPath (Join-Path $_.FullName 'Common7\Tools\VsDevCmd.bat')) -and
            (Test-Path -LiteralPath (Join-Path $_.FullName 'VC\Auxiliary\Build\vcvarsall.bat'))
        } |
        Select-Object -First 1 -ExpandProperty FullName
}
if ([string]::IsNullOrWhiteSpace($installationPath)) {
    throw 'No Visual Studio 2022 installation with the x64 C++ tools was found.'
}

$vsDevCmd = Join-Path $installationPath 'Common7\Tools\VsDevCmd.bat'
if (-not (Test-Path -LiteralPath $vsDevCmd)) {
    throw "Visual Studio developer command file was not found: $vsDevCmd"
}

$intermediateDirectory = Join-Path $workspaceRoot "build\obj\$Configuration"
New-Item -ItemType Directory -Force -Path $OutputDirectory, $intermediateDirectory | Out-Null

$source = Join-Path $PSScriptRoot 'src\SkillsPassivesFirst.cpp'
$include = Join-Path $workspaceRoot 'third_party\mewjector'
$output = Join-Path $OutputDirectory 'SkillsPassivesFirst.dll'
$resourceScript = Join-Path $intermediateDirectory 'SkillsPassivesFirst.version.rc'
$resourceObject = Join-Path $intermediateDirectory 'SkillsPassivesFirst.version.res'
$objectFile = Join-Path $intermediateDirectory 'SkillsPassivesFirst.obj'
$compilerPdb = Join-Path $intermediateDirectory 'SkillsPassivesFirst.compiler.pdb'
$linkerPdb = Join-Path $intermediateDirectory 'SkillsPassivesFirst.pdb'
$importLibrary = Join-Path $intermediateDirectory 'SkillsPassivesFirst.lib'
$numericVersion = "$($versionParts[0]),$($versionParts[1]),$($versionParts[2]),0"

$resourceContent = @"
#include <windows.h>

VS_VERSION_INFO VERSIONINFO
 FILEVERSION $numericVersion
 PRODUCTVERSION $numericVersion
 FILEFLAGSMASK 0x3fL
 FILEFLAGS 0x0L
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_DLL
 FILESUBTYPE 0x0L
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904b0"
        BEGIN
            VALUE "CompanyName", "wordy"
            VALUE "FileDescription", "Skills & Passives First"
            VALUE "FileVersion", "$version"
            VALUE "InternalName", "SkillsPassivesFirst"
            VALUE "OriginalFilename", "SkillsPassivesFirst.dll"
            VALUE "ProductName", "Skills & Passives First - 3 Rerolls"
            VALUE "ProductVersion", "$version"
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x0409, 1200
    END
END
"@
Set-Content -LiteralPath $resourceScript -Value $resourceContent -Encoding Ascii

$optimization = if ($Configuration -eq 'Release') { '/O2' } else { '/Od /Zi' }
$compile = "call `"$vsDevCmd`" -arch=x64 -host_arch=x64 && rc /nologo /fo `"$resourceObject`" `"$resourceScript`" && cl /nologo /std:c++20 /utf-8 /LD $optimization /MT /EHsc /W4 /I `"$include`" /Fo`"$objectFile`" /Fd`"$compilerPdb`" `"$source`" `"$resourceObject`" /Fe:`"$output`" /link /nologo /PDB:`"$linkerPdb`" /IMPLIB:`"$importLibrary`""

cmd /d /s /c $compile
if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE."
}

Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'SkillsPassivesFirst.ini') -Destination $OutputDirectory -Force
Write-Output "Built SkillsPassivesFirst $version ($Configuration): $output"
