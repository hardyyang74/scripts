adb push %~dp0xs5013 /usr/bin/
adb shell chmod +x /usr/bin/xs5013

adb push %~dp0sendevent /usr/bin/
adb shell chmod +x /usr/bin/sendevent

adb shell sync

pause
