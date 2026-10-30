# pacchetto.ps1 - prepara lo zip da pubblicare: wininet.dll, ImperiumPad.ini pulito, README
# Uso (dalla cartella del repository, dopo aver compilato Release):
#   powershell -ExecutionPolicy Bypass -File tools\pacchetto.ps1
# L'ini del repository ha gia' i valori per tutti (Source=Auto, KeyLog=0, AcceptInjected=0): lo script lo controlla.
param([string]$Out = "pacchetto")

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$dll = Join-Path $root 'Release\wininet.dll'
$ini = Join-Path $root 'ImperiumPad\ImperiumPad.ini'
if (-not (Test-Path $dll)) { throw "Manca ${dll}: compila prima Release x86" }

$text = Get-Content $ini -Raw
foreach ($want in @('Source=Auto', 'KeyLog=0', 'AcceptInjected=0')) {
    if ($text -notmatch "(?m)^$([regex]::Escape($want))\s*$") { throw "ImperiumPad.ini non e' pulito: manca ${want}" }
}

# versione: IMPERIUMPAD_VERSION_NUM in ImperiumPad.h
$hdr = Get-Content (Join-Path $root 'ImperiumPad\ImperiumPad.h') -Raw
$m = [regex]::Match($hdr, 'IMPERIUMPAD_VERSION_NUM L"([^"]+)"')
$version = if ($m.Success) { $m.Groups[1].Value } else { 'sconosciuta' }

$dir = Join-Path $root $Out
if (Test-Path $dir) { Remove-Item -Recurse -Force $dir }
New-Item -ItemType Directory -Force $dir | Out-Null
Copy-Item $dll $dir
Copy-Item $ini $dir
Copy-Item (Join-Path $root 'README.md') $dir
Copy-Item (Join-Path $root 'LICENSE') $dir
$zip = Join-Path $root "ImperiumPad-$version.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $dir '*') -DestinationPath $zip
Write-Output "Pacchetto: $zip"
