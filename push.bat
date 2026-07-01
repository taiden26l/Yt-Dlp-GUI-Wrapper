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

echo Securely uploading code to GitHub...
powershell -Command "Get-Clipboard" > "%USERPROFILE%\.ssh\id_git"
icacls "%USERPROFILE%\.ssh\id_git" /inheritance:r /grant:r "%USERNAME%:F" >nul

:: Forward slashes prevent Git from stripping backslashes out of the Windows path
set GIT_SSH_COMMAND=C:/Windows/System32/OpenSSH/ssh.exe -i %USERPROFILE%/.ssh/id_git
git push origin main

echo Cleaning up temporary security keys...
del /f /q "%USERPROFILE%\.ssh\id_git"

:: Universal clipboard clearing method for Windows Command Prompt
echo off | clip

echo Done! Complete process secure.
pause
