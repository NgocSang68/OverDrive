@echo off
set EPOCROOT=\S60\devices\S60_3rd_FP2_SDK_v1.1\
set GCC_EXEC_PREFIX=
set COMPILER_PATH=
set PATH=C:\PROGRA~2\CSLARM~1\bin;C:\Perl\bin;C:\S60\devices\S60_3rd_FP2_SDK_v1.1\epoc32\tools;C:\S60\devices\S60_3rd_FP2_SDK_v1.1\epoc32\gcc\bin;C:\PROGRA~2\COMMON~1\Symbian\tools;C:\Windows\system32;C:\Windows

cd /d "C:\Users\THANG\Documents\Default Project\OverDrive\group"
call bldmake bldfiles
if errorlevel 1 goto :fail
call abld makefile gcce
if errorlevel 1 goto :fail

set GCC_EXEC_PREFIX=C:\PROGRA~2\CSLARM~1\lib\gcc\
set COMPILER_PATH=C:\PROGRA~2\CSLARM~1\libexec\gcc\arm-none-symbianelf\3.4.3;C:\PROGRA~2\CSLARM~1\arm-none-symbianelf\bin;C:\PROGRA~2\CSLARM~1\bin
set PATH=C:\PROGRA~2\CSLARM~1\bin;C:\PROGRA~2\CSLARM~1\libexec\gcc\arm-none-symbianelf\3.4.3;C:\PROGRA~2\CSLARM~1\arm-none-symbianelf\bin;%PATH%

call abld resource gcce urel
if errorlevel 1 goto :fail
call abld target gcce urel
if errorlevel 1 goto :fail
call abld final gcce urel
if errorlevel 1 goto :fail

echo === BUILD OK ===
goto :eof

:fail
echo === BUILD FAILED ===
exit /b 1
