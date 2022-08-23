#include "exsh.h"

typedef struct exsh_key_info
{
    uint8_t key_id;
    uint8_t count;
    uint8_t key_origin[2];
} exsh_key_info_t;

static const exsh_key_info_t fkey_value_table[] =
{
    { ExSh_Null,         0, { 0x00, 0x00 }, },
    { ExSh_F1,           2, { 0x00, 0x3B }, },
    { ExSh_F2,           2, { 0x00, 0x3C }, },
    { ExSh_F3,           2, { 0x00, 0x3D }, },
    { ExSh_F4,           2, { 0x00, 0x3E }, },
    { ExSh_F5,           2, { 0x00, 0x3F }, },
    { ExSh_F6,           2, { 0x00, 0x40 }, },
    { ExSh_F7,           2, { 0x00, 0x41 }, },
    { ExSh_F8,           2, { 0x00, 0x42 }, },
    { ExSh_F9,           2, { 0x00, 0x43 }, },
    { ExSh_F10,          2, { 0x00, 0x44 }, },
    { ExSh_F12,          2, { 0xE0, 0x86 }, },
    { ExSh_Up,           2, { 0xE0, 0x48 }, },
    { ExSh_Down,         2, { 0xE0, 0x50 }, },
    { ExSh_Left,         2, { 0xE0, 0x4B }, },
    { ExSh_Right,        2, { 0xE0, 0x4D }, },
    { ExSh_Home,         2, { 0xE0, 0x47 }, },
    { ExSh_Insert,       2, { 0xE0, 0x52 }, },
    { ExSh_Delect,       2, { 0xE0, 0x53 }, },
    { ExSh_End,          2, { 0xE0, 0x4F }, },
    { ExSh_PageUp,       2, { 0xE0, 0x49 }, },
    { ExSh_PageDown,     2, { 0xE0, 0x51 }, },
};

uint8_t exsh_key_parser_terminal(uint8_t *key, uint8_t count)
{
    for (uint32_t i = 0;
         i < sizeof(fkey_value_table) / sizeof(exsh_key_info_t);
         i ++)
    {
        if (fkey_value_table[i].key_origin[0] == key[0] &&
            fkey_value_table[i].key_origin[1] == key[1])
        {
            return fkey_value_table[i].key_id;
        }
    }

    return ExSh_Null;
}

void exsh_func_normal_key_terminal(uint8_t key_id, void *paras)
{
    (void)key_id;
    (void)paras;
}

