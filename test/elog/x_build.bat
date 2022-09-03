md build

gcc -std=c99 -g ^
main.c ^
hook.c ^
elog_test.c ^
..\..\eventos\eos.c ^
..\..\eventos\eos_kernel.c ^
..\..\portable\win32\cpu_port.c ^
..\..\service\elog\elog.c ^
-I ..\..\service\elog ^
-I ..\..\eventos ^
-I ..\..\portable\win32 ^
-o build\elog ^
-l Winmm
