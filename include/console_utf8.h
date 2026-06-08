#ifndef CONSOLE_UTF8_H
#define CONSOLE_UTF8_H

//在 main 开头调用，设置控制台与 locale 为 UTF-8
//这样就不需要自己每次调整编码格式
void console_setup_utf8(void);

#endif /* CONSOLE_UTF8_H */
