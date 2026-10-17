#include "lcd1602.h"
#include "hardware/gpio.h"
#include "pico/time.h"
//----------------------- переменные из других файлов --------------------------------//

//----------------------- переменные из этого файла ----------------------------------//
const uint DATA_PINS[8] = {12, 11, 10, 9, 8, 7, 6, 5};     // Массив пинов данных от D0 (индекс 0) до D7 (индекс 7)

//------------------------------ функции ---------------------------------------------//
static void lcd_pulse_enable() {                           // Стробирование (импульс для чтения данных дисплеем)
    gpio_put(E_PIN, 1);
    sleep_us(2); // Короткая пауза, чтобы дисплей "увидел" сигнал
    gpio_put(E_PIN, 0);
    sleep_us(50); // Время на обработку команды
}

// Запись одного байта на 8 пинов данных
static void lcd_write_byte(uint8_t val) {
    for (int i = 0; i < 8; i++) {
        // Вытаскиваем i-й бит из val и подаем его на соответствующий пин
        gpio_put(DATA_PINS[i], (val >> i) & 0x01);
    }
}

// Отправка системной команды
static void lcd_send_command(uint8_t cmd) {
    gpio_put(RS_PIN, 0); // 0 = Режим команд
    gpio_put(RW_PIN, 0); // 0 = Режим записи
    lcd_write_byte(cmd);
    lcd_pulse_enable();
}

// Отправка одного символа для вывода на экран
static void lcd_send_char(char c) {
    gpio_put(RS_PIN, 1); // 1 = Режим данных (текст)
    gpio_put(RW_PIN, 0); // 0 = Режим записи
    lcd_write_byte(c);
    lcd_pulse_enable();
}

void lcd_init() {
    // 1. Инициализация всех GPIO на выход
    gpio_init(RS_PIN); gpio_set_dir(RS_PIN, GPIO_OUT);
    gpio_init(RW_PIN); gpio_set_dir(RW_PIN, GPIO_OUT);
    gpio_init(E_PIN);  gpio_set_dir(E_PIN, GPIO_OUT);
    
    for (int i = 0; i < 8; i++) {
        gpio_init(DATA_PINS[i]);
        gpio_set_dir(DATA_PINS[i], GPIO_OUT);
    }

    sleep_ms(50); // Ждем стабилизации питания дисплея

    // 2. Стандартная последовательность настройки контроллера HD44780
    lcd_send_command(0x38); // 8-битный режим, 2 строки, стандартный шрифт
    lcd_send_command(0x0C); // Включаем дисплей, выключаем мигающий курсор
    lcd_clear();
    lcd_send_command(0x06); // Курсор автоматически сдвигается вправо после буквы
}

void lcd_clear() {
    lcd_send_command(0x01);
    sleep_ms(2); // Очистка экрана требует больше времени (около 1.5 мс)
}

void lcd_print(const char *str) {
    while (*str) {
        lcd_send_char(*str++);
    }
}

void lcd_set_cursor(int line, int pos) {
    // 0x80 - адрес начала первой строки, 0xC0 - второй
    uint8_t addr = (line == 0) ? (0x80 + pos) : (0xC0 + pos);
    lcd_send_command(addr);
}

//------------------------------ примечания --------------------------------------------//