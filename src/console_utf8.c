/**
 * @file console_utf8.c
 * @brief Windows / Linux 控制台 UTF-8 支持
 */
#include "console_utf8.h"
#include <stdio.h>
#include <locale.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

void console_setup_utf8(void)
{
#ifdef _WIN32
    /* 控制台代码页设为 UTF-8 (65001)；输入/输出分别设置 */
    SetConsoleCP(65001);
    SetConsoleOutputCP(65001);
#endif
    /* 让宽字符/多字节 locale 按 UTF-8 处理 */
    setlocale(LC_ALL, ".UTF-8");
    setlocale(LC_CTYPE, ".UTF-8");
}
