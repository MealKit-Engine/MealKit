# MealKit engine repository build on every OS (Windows: build.bat does the same). Configurations: tools/build.ps1
#   pwsh build.ps1 [Release|Debug|Editor|Headless|...]
param([string]$Config = "Release", [string[]]$Target = @())
& (Join-Path $PSScriptRoot "tools/build.ps1") -Project $PSScriptRoot -Config $Config -Target $Target
exit $LASTEXITCODE
