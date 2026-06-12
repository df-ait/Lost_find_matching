#ifndef UI_UTIL_H
#define UI_UTIL_H

#include <time.h>

//原来main里面的trim_newline，read_line，parse_datetime
//消除换行符
void ui_trim_newline(char *s);
//利用fget以及printf实现带有提示语的读取
void ui_read_line(const char *prompt, char *buf, int size);
//时间格式转换，把输入的时间字符串变成time_t格式
int ui_parse_datetime(const char *str, time_t *out);

#endif /* UI_UTIL_H */
