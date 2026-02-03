@echo off
echo Launching tou_decomp.exe...
start /wait tou_decomp.exe
echo Exit Code: %ERRORLEVEL% > exitcode.log
echo Done. Exit code saved to exitcode.log
