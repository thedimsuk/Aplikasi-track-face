$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildDir = Join-Path $Root "build-windows"
$PackageDir = Join-Path $BuildDir "package"
$InstallDir = Join-Path $env:LOCALAPPDATA "Programs\Avatar Studio"
$VcpkgRoot = Join-Path $env:LOCALAPPDATA "vcpkg"

if (-not (Test-Path (Join-Path $Root "CMakeLists.txt"))) {
    throw "Jalankan skrip dari salinan proyek Avatar Studio yang lengkap."
}
if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    throw "winget tidak ditemukan. Pasang App Installer dari Microsoft Store, lalu jalankan ulang skrip."
}

function Install-WingetPackage {
    param(
        [Parameter(Mandatory = $true)][string]$Id,
        [string[]]$ExtraArgs = @()
    )

    & winget install --id $Id --exact --silent `
        --accept-source-agreements --accept-package-agreements @ExtraArgs
    if ($LASTEXITCODE -ne 0) {
        throw "winget gagal memasang $Id (kode $LASTEXITCODE)."
    }
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Install-WingetPackage -Id "Git.Git"
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    Install-WingetPackage -Id "Kitware.CMake"
}
Install-WingetPackage -Id "Microsoft.VCRedist.2015+.x64"

# Make newly installed command-line tools available in this PowerShell process.
$env:PATH += ";$env:ProgramFiles\CMake\bin"
$env:PATH += ";$env:ProgramFiles\Git\cmd"
$env:PATH += ";$env:LOCALAPPDATA\Programs\Git\cmd"

$VsWhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$VsInstall = $null
if (Test-Path $VsWhere) {
    $VsInstall = & $VsWhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
}
if (-not $VsInstall) {
    Write-Host "Memasang Visual Studio 2022 Build Tools (C++ dan Windows SDK)..."
    Install-WingetPackage -Id "Microsoft.VisualStudio.2022.BuildTools" `
        -ExtraArgs @("--override", "--wait --quiet --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended")
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git belum tersedia setelah pemasangan. Tutup dan buka kembali PowerShell, lalu jalankan ulang skrip."
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake belum tersedia setelah pemasangan. Tutup dan buka kembali PowerShell, lalu jalankan ulang skrip."
}

if (-not (Test-Path (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat"))) {
    if (Test-Path $VcpkgRoot) {
        throw "Folder vcpkg sudah ada tetapi tidak lengkap: $VcpkgRoot"
    }
    git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
    if ($LASTEXITCODE -ne 0) {
        throw "Gagal mengunduh vcpkg."
    }
}

& (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat") -disableMetrics
if ($LASTEXITCODE -ne 0) {
    throw "Gagal menyiapkan vcpkg."
}

$Toolchain = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"
cmake -S $Root -B $BuildDir `
    "-DCMAKE_TOOLCHAIN_FILE=$Toolchain" `
    -DVCPKG_TARGET_TRIPLET=x64-windows
if ($LASTEXITCODE -ne 0) {
    throw "Konfigurasi CMake gagal."
}

cmake --build $BuildDir --config Release --parallel
if ($LASTEXITCODE -ne 0) {
    throw "Build Avatar Studio gagal."
}

cpack --config (Join-Path $BuildDir "CPackConfig.cmake") `
    -G ZIP -C Release -B $PackageDir
if ($LASTEXITCODE -ne 0) {
    throw "Pembuatan paket ZIP gagal."
}

$Package = Get-ChildItem -Path $PackageDir -Filter "AvatarStudio-*.zip" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
if (-not $Package) {
    throw "Paket ZIP hasil build tidak ditemukan di $PackageDir"
}

New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
Expand-Archive -LiteralPath $Package.FullName -DestinationPath $InstallDir -Force

$Exe = Join-Path $InstallDir "bin\avatar-studio.exe"
if (-not (Test-Path $Exe)) {
    throw "Executable hasil instalasi tidak ditemukan: $Exe"
}

$StartMenu = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
$ShortcutPath = Join-Path $StartMenu "Avatar Studio.lnk"
$Shell = New-Object -ComObject WScript.Shell
$Shortcut = $Shell.CreateShortcut($ShortcutPath)
$Shortcut.TargetPath = $Exe
$Shortcut.WorkingDirectory = Split-Path $Exe
$Shortcut.Save()

Write-Host "Avatar Studio berhasil dipasang di: $InstallDir"
Write-Host "Buka Avatar Studio dari menu Start."
Write-Host "Untuk memuat model Live2D, unduh Cubism SDK resmi secara terpisah."
