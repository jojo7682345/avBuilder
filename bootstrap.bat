@echo off
echo Compiling bootstrap mechanism
mkdir .bootstrap
cd .bootstrap
gcc -o tmp.exe -x c "../bootstrap" -Wl,-rpath,.
echo Compiled bootstrap mechanism
echo Bootstrapping
call tmp.exe %*
set "ret=%errorlevel%"
if %ret% == 0 (
    echo Bootstrapping successful
) else (
   echo Bootstrapping failed
)
del tmp.exe
cd ..
rmdir /s /q .bootstrap
exit /b %ret%