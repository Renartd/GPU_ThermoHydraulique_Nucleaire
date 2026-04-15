# ============================================================
#  run_windows.ps1
#  Lance le solveur puis le simulateur (apres installation)
#  Equivalent de orchestrateur.sh pour Windows
#
#  Usage : .\run_windows.ps1
# ============================================================

$ROOT       = Split-Path -Parent $MyInvocation.MyCommand.Path
$SOLVER_EXE = "$ROOT\assemblage_solver\nuclear\code_c\assemblage_solver.exe"
$SIM_EXE    = "$ROOT\calcul3d\viewer_raylib.exe"
$DATA_FILE  = "$ROOT\calcul3d\data\Assemblage.txt"

Write-Host ""
Write-Host "============================================"
Write-Host "  Simulateur Nucleaire-Thermique"
Write-Host "============================================"
Write-Host ""

# Verifier que les binaires existent
if (!(Test-Path $SOLVER_EXE)) {
    Write-Host "ERREUR : solveur non compile."
    Write-Host "-> Lancez d'abord : .\install_windows.ps1"
    exit 1
}
if (!(Test-Path $SIM_EXE)) {
    Write-Host "ERREUR : simulateur non compile."
    Write-Host "-> Lancez d'abord : .\install_windows.ps1"
    exit 1
}

# Creer le dossier data si absent
New-Item -ItemType Directory -Force -Path "$ROOT\calcul3d\data" | Out-Null

# ── Etape 1 : Solveur d'assemblage ───────────────────────────
Write-Host "[1/2] Solveur d'assemblage..."
Write-Host "      (repondez aux questions pour configurer le coeur)"
Write-Host ""

$env:ASSEMBLAGE_OUT = $DATA_FILE
Set-Location "$ROOT\assemblage_solver\nuclear\code_c"
& $SOLVER_EXE

if (!(Test-Path $DATA_FILE)) {
    Write-Host ""
    Write-Host "ERREUR : Assemblage.txt non genere."
    exit 1
}

Write-Host ""
Write-Host "  -> Assemblage.txt pret"
Write-Host ""

# ── Etape 2 : Simulateur 3D ───────────────────────────────────
Write-Host "[2/2] Lancement du simulateur..."
Set-Location "$ROOT\calcul3d"
& $SIM_EXE