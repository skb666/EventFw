md build

gcc -std=c99 -g ^
main.c ^
hook.c ^
..\..\eventos\eos_kernel.c ^
..\..\eventos\eos_kernel_ex.c ^
..\..\libcpu\win32\cpu_port.c ^
-I ..\..\eventos ^
-I ..\..\libcpu\win32 ^
-o build\e ^
-l Winmm
