# Old name of "tools\module.ps1 engine" (engine 0.9.3): forwards so existing command lines keep working. Removed in a later version.
param([Parameter(Position = 0, ValueFromRemainingArguments = $true)][string[]]$Arguments = @())
[Console]::Error.WriteLine("tools\modules.ps1 is now: tools\module.ps1 engine $($Arguments -join ' ')")
& (Join-Path $PSScriptRoot "module_engine.ps1") @Arguments
exit $LASTEXITCODE
