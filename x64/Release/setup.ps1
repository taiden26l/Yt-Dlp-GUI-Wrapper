# yt-dlp + ffmpeg setup script (Windows)
# Run as Administrator

Write-Host "===================================" -ForegroundColor Cyan
Write-Host " yt-dlp + ffmpeg Setup Script"
Write-Host "===================================" -ForegroundColor Cyan

# Check admin rights
$isAdmin = ([Security.Principal.WindowsPrincipal] `
    [Security.Principal.WindowsIdentity]::GetCurrent()
).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

if (-not $isAdmin) {
    Write-Host "Please run this script as Administrator!" -ForegroundColor Red
    exit 1
}

# ----------------------------
# Install Chocolatey
# ----------------------------
if (-not (Get-Command choco -ErrorAction SilentlyContinue)) {
    Write-Host "`nInstalling Chocolatey..." -ForegroundColor Yellow

    Set-ExecutionPolicy Bypass -Scope Process -Force

    [System.Net.ServicePointManager]::SecurityProtocol =
        [System.Net.ServicePointManager]::SecurityProtocol -bor 3072

    iex ((New-Object System.Net.WebClient).DownloadString(
        'https://community.chocolatey.org/install.ps1'
    ))
}
else {
    Write-Host "Chocolatey already installed." -ForegroundColor Green
}

# Refresh environment
refreshenv | Out-Null

# ----------------------------
# Install ffmpeg via Chocolatey
# ----------------------------
Write-Host "`nInstalling ffmpeg..." -ForegroundColor Yellow
choco install ffmpeg -y

# ----------------------------
# Install yt-dlp via Chocolatey
# ----------------------------
Write-Host "`nInstalling yt-dlp..." -ForegroundColor Yellow
choco install yt-dlp -y

# ----------------------------
# Verify installs
# ----------------------------
Write-Host "`nVerifying installs..." -ForegroundColor Cyan

$yt = Get-Command yt-dlp -ErrorAction SilentlyContinue
$ff = Get-Command ffmpeg -ErrorAction SilentlyContinue

if ($yt) {
    Write-Host "yt-dlp found ✔" -ForegroundColor Green
} else {
    Write-Host "yt-dlp NOT found ❌" -ForegroundColor Red
}

if ($ff) {
    Write-Host "ffmpeg found ✔" -ForegroundColor Green
} else {
    Write-Host "ffmpeg NOT found ❌" -ForegroundColor Red
}

Write-Host "`nSetup complete!" -ForegroundColor Cyan
pause