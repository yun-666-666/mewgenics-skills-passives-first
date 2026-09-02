[CmdletBinding()]
param(
    [string]$ArtifactDirectory
)

$ErrorActionPreference = 'Stop'

$workspaceRoot = $PSScriptRoot
$version = (Get-Content -Raw -LiteralPath (Join-Path $workspaceRoot 'VERSION')).Trim()
$dataDescriptionPath = Join-Path $workspaceRoot 'SkillsPassivesFirstData\description.json'
$dataVersion = (Get-Content -Raw -LiteralPath $dataDescriptionPath | ConvertFrom-Json).version
if ($dataVersion -ne $version) {
    throw "SkillsPassivesFirstData version $dataVersion does not match VERSION $version."
}
if ([string]::IsNullOrWhiteSpace($ArtifactDirectory)) {
    $ArtifactDirectory = Join-Path $workspaceRoot 'dist'
}
$ArtifactDirectory = [IO.Path]::GetFullPath($ArtifactDirectory)

$buildOutput = Join-Path $workspaceRoot 'build\package\compiled'
$stagingRoot = Join-Path $workspaceRoot "build\package\SkillsPassivesFirst-v$version-win-x64"
$modsDirectory = Join-Path $stagingRoot 'Mods'
$mewtatorDirectory = Join-Path $stagingRoot 'Mewtator\mods\SkillsPassivesFirstData'

& (Join-Path $workspaceRoot 'SkillsPassivesFirst\build.ps1') -Configuration Release -OutputDirectory $buildOutput

if (Test-Path -LiteralPath $stagingRoot) {
    Remove-Item -LiteralPath $stagingRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $modsDirectory, $mewtatorDirectory, $ArtifactDirectory | Out-Null

Copy-Item -LiteralPath (Join-Path $buildOutput 'SkillsPassivesFirst.dll') -Destination $modsDirectory
Copy-Item -LiteralPath (Join-Path $buildOutput 'SkillsPassivesFirst.ini') -Destination $modsDirectory
Copy-Item -LiteralPath $dataDescriptionPath -Destination $mewtatorDirectory
Copy-Item -LiteralPath (Join-Path $workspaceRoot 'SkillsPassivesFirstData\data') -Destination $mewtatorDirectory -Recurse
Copy-Item -LiteralPath (Join-Path $workspaceRoot 'README.md') -Destination $stagingRoot
Copy-Item -LiteralPath (Join-Path $workspaceRoot 'LICENSE') -Destination $stagingRoot
Copy-Item -LiteralPath (Join-Path $workspaceRoot 'CHANGELOG.md') -Destination $stagingRoot

$archive = Join-Path $ArtifactDirectory "SkillsPassivesFirst-v$version-win-x64.zip"
if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}
Compress-Archive -Path (Join-Path $stagingRoot '*') -DestinationPath $archive -CompressionLevel Optimal
Write-Output "Packaged SkillsPassivesFirst ${version}: $archive"
