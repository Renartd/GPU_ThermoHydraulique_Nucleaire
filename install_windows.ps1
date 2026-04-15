# ============================================================
#  install_windows.ps1
#  Installation complete + compilation + lancement
#  Windows 10/11 - PowerShell 5.1+
#
#  Lancer en tant qu'administrateur :
#    Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
#    .\install_windows.ps1
# ============================================================

$ErrorActionPreference = "Stop"
$ROOT      = Split-Path -Parent $MyInvocation.MyCommand.Path
$MSYS2_DIR = "C:\msys64"
$BASH      = "$MSYS2_DIR\usr\bin\bash.exe"

Write-Host ""
Write-Host "============================================"
Write-Host "  Simulateur Nucleaire - Installation Windows"
Write-Host "============================================"
Write-Host ""

function Download-File {
    param([string]$url, [string]$dest)
    if (!(Test-Path $dest)) {
        Write-Host "  Telechargement : $(Split-Path -Leaf $dest) ..."
        Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
        Write-Host "  OK"
    } else {
        Write-Host "  Deja present : $(Split-Path -Leaf $dest)"
    }
}

function To-MsysPath {
    param([string]$winPath)
    $drive = $winPath.Substring(0,1).ToLower()
    $rest  = $winPath.Substring(2).Replace("\", "/")
    return "/$drive$rest"
}

# ── 1. MSYS2 ────────────────────────────────────────────────
Write-Host "[1/4] MSYS2..."

if (!(Test-Path $BASH)) {
    Write-Host "  MSYS2 absent - installation..."
    $installer = "$env:TEMP\msys2-installer.exe"
    Download-File `
        "https://github.com/msys2/msys2-installer/releases/download/2024-01-13/msys2-x86_64-20240113.exe" `
        $installer
    Start-Process -FilePath $installer `
        -ArgumentList @("install", "--root", "C:\msys64", "--confirm-command") `
        -Wait -NoNewWindow
    Write-Host "  Mise a jour MSYS2..."
    & $BASH --login -c "pacman -Syu --noconfirm" 2>&1 | Out-Null
    & $BASH --login -c "pacman -Syu --noconfirm" 2>&1 | Out-Null
    Write-Host "  MSYS2 installe."
} else {
    Write-Host "  MSYS2 deja installe."
}

# ── 2. Paquets ──────────────────────────────────────────────
Write-Host ""
Write-Host "[2/4] Installation GCC + Raylib + outils..."

$paquets = "make git " +
           "mingw-w64-x86_64-gcc " +    # inclut g++
           "mingw-w64-x86_64-raylib " +
           "mingw-w64-x86_64-cmake"     # pkgconf installe automatiquement en dependance

# bash --login charge le profil MSYS2 et le PATH MinGW64
& $BASH --login -c "pacman -S --noconfirm --needed $paquets"
if ($LASTEXITCODE -ne 0) { throw "Echec installation paquets" }
Write-Host "  Paquets OK."

# ── 3. Vulkan SDK ─────────────────────────────────────────────
Write-Host ""
Write-Host "[3/4] Vulkan SDK..."

$glslang = $null
if ($env:VULKAN_SDK -and (Test-Path "$env:VULKAN_SDK\Bin\glslangValidator.exe")) {
    $glslang = "$env:VULKAN_SDK\Bin\glslangValidator.exe"
}
if (!$glslang) {
    $found = Get-ChildItem "C:\VulkanSDK" -Recurse -Filter "glslangValidator.exe" `
             -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $glslang = $found.FullName }
}

if (!$glslang) {
    Write-Host "  Vulkan SDK absent - installation..."
    $installer = "$env:TEMP\VulkanSDK-Installer.exe"
    Download-File "https://sdk.lunarg.com/sdk/download/latest/windows/vulkan-sdk.exe" $installer
    Start-Process -FilePath $installer `
        -ArgumentList @("--accept-licenses","--default-answer","--confirm-command","install") `
        -Wait -NoNewWindow
    # Recharger l'environnement
    $env:VULKAN_SDK = [System.Environment]::GetEnvironmentVariable("VULKAN_SDK","Machine")
    $env:PATH = [System.Environment]::GetEnvironmentVariable("PATH","Machine") + ";" +
                [System.Environment]::GetEnvironmentVariable("PATH","User")
    $found = Get-ChildItem "C:\VulkanSDK" -Recurse -Filter "glslangValidator.exe" `
             -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($found) { $glslang = $found.FullName }
}

if (!$glslang -or !(Test-Path $glslang)) {
    Write-Host ""
    Write-Host "ATTENTION : glslangValidator.exe introuvable."
    Write-Host "-> Installez manuellement : https://vulkan.lunarg.com/sdk/home"
    Write-Host "-> Puis relancez ce script."
    exit 1
}
Write-Host "  glslangValidator : $glslang"

# ── 4. Compilation ────────────────────────────────────────────
Write-Host ""
Write-Host "[4/4] Compilation..."

$rootMsys = To-MsysPath $ROOT
New-Item -ItemType Directory -Force -Path "$ROOT\calcul3d\data" | Out-Null

# Solveur C
Write-Host "  [4a] Solveur C..."
$solverDir = "$rootMsys/assemblage_solver/nuclear/code_c"
& $BASH --login -c @"
export PATH='/mingw64/bin:/usr/local/bin:/usr/bin:/bin':`$PATH
cd '$solverDir'
SRCS=`$(find . -name '*.c' -not -path '*/python*' -not -path '*/ancien*')
gcc -std=c99 -Wall -O2 `$SRCS -o assemblage_solver.exe -lm && echo 'Solveur OK'
"@
if ($LASTEXITCODE -ne 0) { throw "Echec solveur C" }

# Shaders
Write-Host "  [4b] Shaders SPIR-V..."
$shaderDir = "$ROOT\calcul3d\compute\shaders"
foreach ($s in @("diffusion","neutron_fvm","neutron_reduce")) {
    $src = "$shaderDir\$s.comp"
    $spv = "$shaderDir\$s.spv"
    if (Test-Path $src) {
        Write-Host "      $s.comp -> $s.spv"
        & $glslang -V --target-env vulkan1.3 $src -o $spv
        if ($LASTEXITCODE -ne 0) { throw "Echec shader $s" }
    }
}

# Trouver headers Vulkan
$vulkInc = ""
$vulkLib = ""
if ($env:VULKAN_SDK -and (Test-Path "$env:VULKAN_SDK\Include")) {
    $vulkInc = "$env:VULKAN_SDK\Include"
    $vulkLib = "$env:VULKAN_SDK\Lib"
} else {
    $h = Get-ChildItem "C:\VulkanSDK" -Recurse -Filter "vulkan.h" `
         -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($h) {
        $vulkInc = $h.DirectoryName
        $vulkLib = Join-Path (Split-Path (Split-Path $h.DirectoryName)) "Lib"
    }
}
if (!$vulkInc) { throw "Headers Vulkan introuvables." }
$vulkIncMsys = To-MsysPath $vulkInc
$vulkLibMsys = To-MsysPath $vulkLib

# Simulateur C++
Write-Host "  [4c] Simulateur C++..."
$calcul3d = "$rootMsys/calcul3d"
& $BASH --login -c @"
export PATH='/mingw64/bin:/usr/local/bin:/usr/bin:/bin':`$PATH
cd '$calcul3d'
g++ main.cpp core/AssemblageLoader.cpp \
    -I. -I'$vulkIncMsys' -L'$vulkLibMsys' \
    -o viewer_raylib.exe \
    -lraylib -lvulkan-1 -lopengl32 -lgdi32 -lwinmm \
    -std=c++17 -O2 \
    -D_USE_MATH_DEFINES -DNOMINMAX && echo 'Simulateur OK'
"@
if ($LASTEXITCODE -ne 0) { throw "Echec simulateur C++" }

# ── Lancement ─────────────────────────────────────────────────
Write-Host ""
Write-Host "=== Build termine ==="
Write-Host ""
Write-Host "Lancement solveur d'assemblage..."
Write-Host "(Repondez aux questions, le simulateur 3D demarrera ensuite)"
Write-Host ""

$dataFile = "$ROOT\calcul3d\data\Assemblage.txt"
$env:ASSEMBLAGE_OUT = $dataFile

Set-Location "$ROOT\assemblage_solver\nuclear\code_c"
& ".\assemblage_solver.exe"

if (!(Test-Path $dataFile)) { throw "Assemblage.txt non genere." }

Write-Host ""
Write-Host "Lancement simulateur 3D..."
Set-Location "$ROOT\calcul3d"
& ".\viewer_raylib.exe"