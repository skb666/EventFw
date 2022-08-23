#ifndef EXSH_H
#define EXSH_H

#include <stdint.h>
#include <stdbool.h>

enum
{
    ExSh_Null = 128,
    ExSh_F1, ExSh_F2, ExSh_F3, ExSh_F4, ExSh_F5, ExSh_F6, 
    ExSh_F7, ExSh_F8, ExSh_F9, ExSh_F10, ExSh_F12,
    ExSh_Up, ExSh_Down, ExSh_Right, ExSh_Left,
    ExSh_Home, ExSh_Insert, ExSh_Delect, ExSh_End, ExSh_PageUp, ExSh_PageDown,
    ExSh_Max
};

typedef void (* exsh_func_exit_t)(void *paras);
typedef void (* exsh_func_t)(uint8_t key_id, void *paras);
typedef uint8_t (* exsh_key_parser_t)(uint8_t *key, uint8_t count);

void ex_shell_init(exsh_key_parser_t func_parser,
                   exsh_func_t func,
                   exsh_func_exit_t func_exit,
                   void *paras);
void ex_shell_key_register(uint8_t key, exsh_func_t func, void *paras);
void ex_shell_run(void);
bool ex_shell_is_running(void);

#endif
