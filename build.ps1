$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$buildDir = Join-Path $root 'build'
$null = New-Item -ItemType Directory -Force $buildDir

$clang = Get-Command x86_64-w64-mingw32-clang++ -ErrorAction SilentlyContinue
if ($clang) {
    $clang = $clang.Source
} else {
    $clang = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\MartinStorsjo.LLVM-MinGW.UCRT_*\llvm-mingw-*\bin\x86_64-w64-mingw32-clang++.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1 -ExpandProperty FullName
}

if (-not $clang) {
    throw 'x86_64-w64-mingw32-clang++ was not found. Install LLVM MinGW with: winget install MartinStorsjo.LLVM-MinGW.UCRT'
}

$webView2Root = Join-Path $root 'third_party\webview2\1.0.4314.35\build\native'
$includeDir = Join-Path $webView2Root 'include'
$loaderLib = Join-Path $webView2Root 'x64\WebView2Loader.dll.lib'
$loaderDll = Join-Path $webView2Root 'x64\WebView2Loader.dll'

& $clang `
    -std=c++17 `
    -O2 `
    -static `
    -municode `
    -mwindows `
    -DUNICODE `
    -D_UNICODE `
    "-I$includeDir" `
    "$root\src\main.cpp" `
    "$loaderLib" `
    -lole32 -loleaut32 -luuid -lversion -lshlwapi -lgdi32 `
    -o "$buildDir\CustomBrowser.exe"

if ($LASTEXITCODE -ne 0) {
    throw "Build failed with exit code $LASTEXITCODE"
}

Copy-Item $loaderDll "$buildDir\WebView2Loader.dll" -Force
Copy-Item "$root\browser_icon.ico" "$buildDir\browser_icon.ico" -Force -ErrorAction SilentlyContinue

# Temporarily disable signing to avoid Smart App Control blocking
# $cert = Get-ChildItem Cert:\CurrentUser\My | Where-Object { $_.Subject -eq 'CN=Custom Browser Dev' } | Select-Object -First 1
# if (-not $cert) {
#     $cert = New-SelfSignedCertificate -Type CodeSigningCert -Subject 'CN=Custom Browser Dev' -CertStoreLocation 'Cert:\CurrentUser\My' -NotAfter (Get-Date).AddYears(5) -KeyExportPolicy Exportable -KeyLength 2048 -HashAlgorithm SHA256
# }
# 
# $cerPath = Join-Path $buildDir 'CustomBrowserDev.cer'
# Export-Certificate -Cert $cert -FilePath $cerPath -Type CERT | Out-Null
# $trusted = Get-ChildItem Cert:\CurrentUser\TrustedPublisher | Where-Object { $_.Thumbprint -eq $cert.Thumbprint }
# if (-not $trusted) {
#     Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\CurrentUser\TrustedPublisher | Out-Null
# }
# $rootCert = Get-ChildItem Cert:\CurrentUser\Root | Where-Object { $_.Thumbprint -eq $cert.Thumbprint }
# if (-not $rootCert) {
#     Import-Certificate -FilePath $cerPath -CertStoreLocation Cert:\CurrentUser\Root | Out-Null
# }
# 
# Set-AuthenticodeSignature -FilePath "$buildDir\CustomBrowser.exe" -Certificate $cert -HashAlgorithm SHA256 | Out-Null
Write-Host "Built $buildDir\CustomBrowser.exe (signing disabled to avoid SAC blocking)"
