#ifndef LCD1602_H
#define LCD1602_H

//----------------------- подключим файлы ------------------------------------//
#include "pico/stdlib.h"
//----------------------- дефайним значения ----------------------------------//
#define RS_PIN 15
#define RW_PIN 14
#define E_PIN 13
//----------------------- объявим структуры ----------------------------------//

//----------------------- объявим функции ------------------------------------//
void lcd_init(void);
void lcd_clear(void);
void lcd_print(const char *str);
void lcd_set_cursor(int line, int pos);

#endif