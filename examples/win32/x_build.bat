md build

gcc -std=c99 -g ^
main.c ^
hook.c ^
..\..\eventos\eos.c ^
..\..\eventos\eos_kernel.c ^
..\..\libcpu\win32\cpu_port.c ^
-I ..\..\eventos ^
-I ..\..\libcpu\win32 ^
-o build\e ^
-l Winmm
