# Marks vendored copies of module files as copies (dot-source: . "$PSScriptRoot\vendor_header.ps1"). docs/CHANGELOG.md "Vendored copies marked".
# A module's files exist in its repo module-<name>, in the engine's modules/<name> and in each game's modules/<name>; a one-line first line says
# "Vendored copy of module-<name> <version>; change it in that repo", so a person or an agent edits the right one. Only C/C++ sources and headers
# carry it (a comment there is always valid and their hashes are text hashes). Every hash (module.files, the engine lock comparison) skips the
# line, so adding it changes no recorded hash and an unmodified copy stays unmodified.
$VendorHeaderExtensions = @(".h", ".hpp", ".cpp", ".c", ".inl")
$VendorHeaderPattern = '\A(﻿)?// Vendored copy of module-[^\r\n]*(\r\n|\n)'

function Test-VendorHeaderFile([string]$path) {
    return $VendorHeaderExtensions -contains [IO.Path]::GetExtension($path).ToLowerInvariant()
}

# The file's text without a vendored header line
function Remove-VendorHeaderText([string]$text) {
    return [regex]::Replace($text, $VendorHeaderPattern, '', 1)
}

# Reads a file as BOM flag + text
function Read-VendorText([string]$path) {
    $bytes = [IO.File]::ReadAllBytes($path)
    $hasBom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
    $skip = if ($hasBom) { 3 } else { 0 }
    return @{ bom = $hasBom; text = (New-Object Text.UTF8Encoding($false)).GetString($bytes, $skip, $bytes.Length - $skip) }
}

function Write-VendorText([string]$path, [bool]$bom, [string]$text) {
    $body = (New-Object Text.UTF8Encoding($false)).GetBytes($text)
    $out = New-Object Collections.Generic.List[byte]
    if ($bom) { $out.AddRange([byte[]](0xEF, 0xBB, 0xBF)) }
    $out.AddRange($body)
    [IO.File]::WriteAllBytes($path, $out.ToArray())
}

# Writes (or rewrites) the header of a vendored file; the file's line ending and BOM are kept
function Set-VendorHeader([string]$path, [string]$module, [string]$version) {
    if (-not (Test-VendorHeaderFile $path)) { return }
    $file = Read-VendorText $path
    $newline = if ($file.text -match "\r\n") { "`r`n" } else { "`n" }
    $line = "// Vendored copy of module-$module $version; change it in that repo (tools\module.ps1 update), not here."
    $new = $line + $newline + (Remove-VendorHeaderText $file.text)
    if ($new -ne $file.text) { Write-VendorText $path $file.bom $new }
}

# A copy of the file without its vendored header in a temp file (for comparing with the repo's blob), or "" when it has none
function New-VendorStrippedCopy([string]$path) {
    if (-not (Test-VendorHeaderFile $path)) { return "" }
    $file = Read-VendorText $path
    $stripped = Remove-VendorHeaderText $file.text
    if ($stripped -eq $file.text) { return "" }
    $temp = [IO.Path]::GetTempFileName()
    Write-VendorText $temp $file.bom $stripped
    return $temp
}
