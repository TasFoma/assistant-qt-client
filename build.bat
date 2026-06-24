@echo off
echo Cleaning build directory...
if exist build rmdir /s /q build

echo Configuring CMake...
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH="C:/Qt/6.10.2/msvc2022_64"

echo Building...
cmake --build . --config Debug -j 8

echo Deploying Qt dependencies...
cd Debug
C:\Qt\6.10.2\msvc2022_64\bin\windeployqt6.exe --debug --qmldir ..\..\src qt_client.exe

echo.
echo Build successful! Run: build\Debug\qt_client.exe
pause