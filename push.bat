@echo off
set /p msg="Enter your commit message: "

echo Configuring GPG path for Windows CMD...
git config --local gpg.program "C:/Program Files/Git/usr/bin/gpg.exe"

echo Staging changes...
git add .

echo Creating GPG-signed commit...
git commit -S -m "%msg%"

echo.
echo === ACTION REQUIRED ===
echo 1. Open your Bitwarden vault.
echo 2. Copy your Github SSH private key to your clipboard.
echo =======================
pause

echo Preparing temporary security key...
powershell -Command "Get-Clipboard" > "./id_git"
icacls "./id_git" /inheritance:r /grant:r "%USERNAME%:F" >nul

:: Set up the secure SSH configuration variable
set GIT_SSH_COMMAND=C:/Windows/System32/OpenSSH/ssh.exe -i ./id_git

echo Syncing with GitHub (Pulling latest remote changes)...
git pull origin main --rebase

echo Securely uploading code to GitHub...
git push origin main

echo Cleaning up temporary security keys...
del /f /q "./id_git"

:: Clear out your system clipboard
echo off | clip

echo Done! Complete process secure.
pause
