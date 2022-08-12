md build

gcc -std=c99 -g ^
main_win32.c ^
bsp_win32.c ^
hook.c ^
test.c ^
test_01_0.c ^
test_01_1.c ^
test_02_0.c ^
test_02_1.c ^
test_02_2.c ^
test_03.c ^
test_04.c ^
test_05_0.c ^
test_05_1.c ^
test_06.c ^
test_07.c ^
test_08.c ^
test_09.c ^
test_10.c ^
test_11.c ^
..\eventos\eventos.c ^
..\eventos\eos_kernel.c ^
..\eventos\eos_kernel_ex.c ^
..\libcpu\win32\cpu_port.c ^
-I ..\eventos ^
-I . ^
-I ..\libcpu\win32 ^
-o build\e ^
-l Winmm
