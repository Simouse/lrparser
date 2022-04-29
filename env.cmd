@echo off
python3 -c "import graphviz; import PyQt5; import clipboard;"
if not %ERRORLEVEL% equ 0 (
    echo [91m[Error] Packages are not installed completely.[0m
    echo You need python3 in PATH.
    echo You need graphviz, PyQt5, and clipboard in python site-packages.
    pause
) else (
	echo [92m[Success] Python3 and related packages are installed.[0m
	set "PATH=bin;build;%PATH%"
)