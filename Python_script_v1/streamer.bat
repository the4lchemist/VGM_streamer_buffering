@echo off
set path=%~dp0
set arg1=%1
"C:\WPython64\python-3.7.4.amd64\python.exe" "%path%streamer.py" %arg1%
pause