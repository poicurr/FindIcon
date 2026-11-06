@echo off
setlocal
set "projectDir=%~dp0"
set "buildDir=%projectDir%build\release"
cmake -S "%projectDir%." -B "%buildDir%" -DCMAKE_BUILD_TYPE=Release %*
if errorlevel 1 exit /b %errorlevel%
cmake --build "%buildDir%" --config Release --parallel
exit /b %errorlevel%
