#ifndef WORK_H
#define WORK_H
//----------------------- подключим файлы ------------------------------------//
#include "pico/stdlib.h"
#include "lcd1602.h"
#include "hardware/adc.h"
#include "hardware/watchdog.h"
#include <stdio.h>
//----------------------- дефайним значения ----------------------------------//
#define HOLD_TIME_MS 1000              // Время удержания (1.0 секунды)
#define WATCHDOG_TIMEOUT_MS 1500       // Таймаут аппаратного watchdog

#define LOGIC_TASK_INTERVAL_MS 100     // Период вызова funcLogicTask (используется для расчёта IDLE_SHUTDOWN_TICKS)
#define IDLE_SHUTDOWN_MIN 10           // Через сколько минут простоя (оба ключа закрыты - принтер не печатает) отключать питание
#define IDLE_SHUTDOWN_TICKS ((uint32_t)((IDLE_SHUTDOWN_MIN * 60000UL) / LOGIC_TASK_INTERVAL_MS))

#define CHARGE_CHECK_INTERVAL_MS 30000 // Как часто проверяем нужно ли включать/выключать зарядку (реже - меньше дёргания при шуме/отсутствии АКБ)

#define VOLTAGE_HYSTERESIS 0.2f        // Гистерезис для сравнений напряжения батареи

#define VREF 3.3f
#define ADC_MAX 4095.0f
#define R1 100000.0f                   // 100 кОм
#define R2 11000.0f                    // 11 кОм
#define DIVIDER_RATIO ((R1 + R2) / R2) // Коэффициент ~10.09
#define FILTER_FACTOR 2

#define CALIBRATION_SAMPLES 100        // Сколько замеров усредняем при калибровке нуля токовых датчиков (по 1 мс, ~100 мс)
#define CALIBRATION_ZERO_MIN 1700      // Допустимый диапазон нуля датчика тока (отсчёты АЦП). Нормальный ноль ACS712 - около половины шкалы (~2048),
#define CALIBRATION_ZERO_MAX 2400      // выход за диапазон = датчик не подключён или неисправен, работа с такими данными небезопасна

#define SOURCE_SWITCH_THRESHOLD  50     // Разница АЦП токовых датчиков, при которой открываем транзистор явно определившегося источника
#define SOURCE_NEUTRAL_THRESHOLD 20     // Разница АЦП, ниже которой считаем что источник не определён, и закрываем оба транзистора
#define CHARGE_V_ON  27.0f             // Напряжение включения зарядки
#define CHARGE_V_OFF 28.0f             // Напряжение выключения зарядки
#define LOW_BATTERY  25.0f             // Напряжение ниже которого считается низким
#define OFF_BATTERY  24.5f             // Напряжение отключения батареи

#define STATE_STOP    0
#define STATE_BAT     1
#define STATE_PSU     2
#define STATE_LOW_BAT 3
#define STATE_SOURCE_UNKNOWN 4          // Оба ключа закрыты, источник питания сейчас не определён (старт, простой, либо промежуточное состояние после LOW_BAT) —
                                         // логика по adcDiff в funcLogicTask сама переклассифицирует его в STATE_PSU/STATE_BAT в течение того же тика

#define PIN_BUZZER   0
#define PIN_BAT_TR   1
#define PIN_LED_PSU  2
#define PIN_LED_BAT  3
#define PIN_RELAY    4
#define PIN_TR_READ  17
#define PIN_CHARGE   21
#define PIN_PSU_TR   22
#define PIN_LATCH    23
#define PIN_BUTTON   29
//----------------------- объявим структуры ----------------------------------//
typedef struct {                // Описание структуры задачи
    void (*func)(void);         // Указатель на функцию задачи
    uint32_t interval_ms;       // Интервал выполнения в миллисекундах
    uint64_t previous_time_us;  // Время последнего выполнения в микросекундах
} task_t;
//----------------------- объявим функции ------------------------------------//
void doWork();
void Work();

#endif
