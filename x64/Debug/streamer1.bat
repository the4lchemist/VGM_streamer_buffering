echo on
cd /d %~dp0
set arg1=%1
echo %arg0%
copy %arg1% .
set filename="%~n1%~x1"
set newfile=temp.vgm.gz
set newfile_last=temp.vgm

move /y %filename% %newfile%
gzip -df %newfile%
VGM_streamer %newfile_last% 1
del %newfile_last%
rem pause