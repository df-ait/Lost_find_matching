#include <stdio.h>
#include <string.h>
#include <time.h>
#include "ui_util.h"

//消除换行符
void ui_trim_newline(char *s)
{
    if (!s) return;
    size_t n = strlen(s);
    //从最后一个字符倒着找\n,\r，替换为结束符\0
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = '\0';
        n--;
    }
}

void ui_read_line(const char *prompt, char *buf, int size)
{
    if (prompt){
        printf("%s", prompt);
    }
    if (!fgets(buf, size, stdin)){
        //fget失败就返回空串
        buf[0] = '\0';
        return;
    }
    ui_trim_newline(buf);
}

int ui_parse_datetime(const char *str, time_t *out)
{
    if (!str || !out || !str[0]) return -1;
    struct tm t;
    memset(&t, 0, sizeof(t));
    int y, mo, d, h, mi;
    if (sscanf(str, "%d-%d-%d %d:%d", &y, &mo, &d, &h, &mi) != 5)
        return -1;
    t.tm_year = y - 1900;
    t.tm_mon  = mo - 1;//从0开始访问月份
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min  = mi;
    t.tm_sec  = 0;
    t.tm_isdst = -1;
    //把填好的struct tm转成 time_t 时间戳（从 1970-01-01 起的秒数）
    *out = mktime(&t);
    //强转再比较
    return (*out == (time_t)-1) ? -1 : 0;
}
