@echo off
where /q python3
if %ERRORLEVEL% EQU 1 (
    echo [Error] python3 is not in PATH.
    pause
    exit 1
)
python3 -c "import graphviz; import PyQt5; import clipboard;"
if %ERRORLEVEL% EQU 1 (
    echo [Error] Packages are not installed completely.
    pause
    exit 1
)
echo [Success] Python3 and related packages are installed.
set PATH=bin;build;%PATH%