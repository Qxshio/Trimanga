$ErrorActionPreference = 'Stop'

$packageName = 'scanlation-tool'
$toolsDir = Split-Path -Parent $MyInvocation.MyCommand.Definition
$url64 = 'https://github.com/example/scanlation-tool/releases/download/v0.1.0/scanlation-tool-windows-x64.zip'
$checksum64 = 'REPLACE_WITH_ZIP_SHA256'

Install-ChocolateyZipPackage `
  -PackageName $packageName `
  -Url64bit $url64 `
  -UnzipLocation $toolsDir `
  -Checksum64 $checksum64 `
  -ChecksumType64 'sha256'
