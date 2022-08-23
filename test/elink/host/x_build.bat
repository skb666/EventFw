md build

gcc -std=c99 -g ^
main.c ^
..\..\..\util\osal\cmsis_os.c ^
..\..\..\util\exsh\exsh.c ^
..\..\..\util\exsh\exsh_dev_terminal.c ^
..\..\..\service\elink\elink_host.c ^
-I ..\..\..\service\elink ^
-I ..\..\..\util\osal ^
-I ..\..\..\util\exsh ^
-o build\elink
