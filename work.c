#include "work.h"
//----------------------- переменные из других файлов --------------------------------//

//----------------------- переменные из этого файла ----------------------------------//
uint32_t interval_ms = 500;                         
uint64_t previous_time = 0;
bool buzzerState = false;
bool chargeState = false;
task_t logicTask, buttonTask, adcTask, lcdTask, buzzerTask, shedulerBuzzerTask, chargeTask;
float batteryVoltage = 0.0f;
uint8_t stateOutTR = 0;
uint16_t adc1TR1 = 0, adc2TR2 = 0;
int32_t adcOffset1 = 0, adcOffset2 = 0;                                                        // Показания датчиков тока при нулевом токе (калибровка при старте)
uint32_t buzzerDuration = 0, buzzerPause = 0;
//------------------------------ функции ---------------------------------------------//
static void taskInit(task_t *task, void (*task_func)(void), uint32_t interval_ms) {                // Функция инициализации структуры задачи
    task->func = task_func;
    task->interval_ms = interval_ms;
    task->previous_time_us = time_us_64();
}

static void taskRun(task_t *task) {                                                                // Обработчик задачи (вызывается в главном цикле)
    uint64_t current_time = time_us_64();
    if ((current_time - task->previous_time_us) >= (task->interval_ms * 1000ULL)) {          // Проверяем, пришло ли время выполнения
        task->previous_time_us = current_time; // Обновляем таймер
        task->func();                          // Выполняем функцию
    }
}

static void buzzerTaskFunc() {
    static uint32_t duration, pause = 0;
    if (buzzerState) {
        if (duration < buzzerDuration) {
            gpio_put(PIN_BUZZER, 1);
            duration ++;
        } else if (pause < buzzerPause) {
            gpio_put(PIN_BUZZER, 0);
            pause ++;
        } else {
            duration = 0;
            pause = 0;
        }
    } else {
        gpio_put(PIN_BUZZER, 0);
    }
}

static void setBuzer (uint32_t duration, uint32_t pause) {
    if (!duration) {
      gpio_put(PIN_BUZZER, 0);
      buzzerState = false;
    } else {
      buzzerDuration = duration;
      buzzerPause = pause;
      buzzerState = true;
    }
}

static void shedulerBuzzerTaskFunc(){
    if (gpio_get(PIN_BUTTON)){
        setBuzer (1000, 0);
        return;
    }
    if (batteryVoltage < LOW_BATTERY - VOLTAGE_HYSTERESIS){
        setBuzer (10, 50);
        return;
    }
    setBuzer (0, 0);
}

static void systemShutdown(const char *message) {                                          // Общая процедура аварийного/ручного выключения
    stateOutTR = STATE_STOP;
    lcd_clear();
    lcd_print(message);
    gpio_put(PIN_BUZZER, 1);
    sleep_ms(1000);
    gpio_put(PIN_RELAY, 0);
    gpio_put(PIN_PSU_TR, 0);
    gpio_put(PIN_BAT_TR, 0);
    gpio_put(PIN_CHARGE, 0);
    gpio_put(PIN_LATCH, 0);
    while (1) {
        watchdog_update();                                                                  // Не даём watchdog перезагрузить МК, пока не отключится питание по PIN_LATCH
    }
}

static void perifInit(){                                                                           // Инициализация пинов
    gpio_init(PIN_LATCH); gpio_set_dir(PIN_LATCH, GPIO_OUT); gpio_put(PIN_LATCH, 1);
    gpio_init(PIN_BUTTON);  gpio_set_dir(PIN_BUTTON, GPIO_IN);
    gpio_init(PIN_BUZZER);  gpio_set_dir(PIN_BUZZER, GPIO_OUT);
    gpio_init(PIN_RELAY);   gpio_set_dir(PIN_RELAY, GPIO_OUT);
    gpio_init(PIN_TR_READ); gpio_set_dir(PIN_TR_READ, GPIO_OUT);
    gpio_init(PIN_PSU_TR); gpio_set_dir(PIN_PSU_TR, GPIO_OUT);
    gpio_init(PIN_BAT_TR); gpio_set_dir(PIN_BAT_TR, GPIO_OUT);
    gpio_init(PIN_CHARGE);  gpio_set_dir(PIN_CHARGE, GPIO_OUT);
    gpio_init(PIN_LED_PSU); gpio_set_dir(PIN_LED_PSU, GPIO_OUT);
    gpio_init(PIN_LED_BAT); gpio_set_dir(PIN_LED_BAT, GPIO_OUT); 
    adc_init();
    adc_gpio_init(26); adc_gpio_init(27); adc_gpio_init(28);
}

static void funcButtonMonitor() {
    static uint64_t press_start_time = 0;
    static bool is_pressing = false;
    if (gpio_get(PIN_BUTTON)) {
        if (!is_pressing) {                                                                 // Кнопка только что была нажата
            is_pressing = true;
            press_start_time = time_us_64();                                                // Фиксируем время начала
        } else {
            uint64_t current_time = time_us_64();                                           // Кнопка все еще удерживается, проверяем сколько времени прошло
            if ((current_time - press_start_time) >= (HOLD_TIME_MS * 1000ULL)) {
                systemShutdown("   SYSTEM OFF   ");
            }
        }
    } else {
        if (is_pressing) {
            is_pressing = false;                                                            // Кнопка отпущена
        }
    }
}

static void funcADCMonitor() {
    static uint16_t adc0_raw_filtered = 0;
    static bool is_first_run = true;
    adc_select_input(0);                     // 1. Читаем АЦП 0 (GPIO 26)
    uint16_t raw0 = adc_read();
    adc_select_input(1);                     // 2. Читаем АЦП 1 (GPIO 27)
    uint16_t raw1 = adc_read();
    adc_select_input(2);                     // 3. Читаем АЦП 2 (GPIO 28)
    uint16_t raw2 = adc_read();
    if (is_first_run) {                      // При первом запуске сразу записываем сырые данные, чтобы фильтр не полз от нуля
        adc0_raw_filtered = raw0;
        adc1TR1 = raw1;
        adc2TR2 = raw2;
        is_first_run = false;
    } else {
        adc0_raw_filtered = (adc0_raw_filtered * (FILTER_FACTOR - 1) + raw0) / FILTER_FACTOR;   // Применяем фильтр (новое значение = 1/8 от текущего + 7/8 от предыдущего)
        adc1TR1 = (adc1TR1 * (FILTER_FACTOR - 1) + raw1) / FILTER_FACTOR;
        adc2TR2 = (adc2TR2 * (FILTER_FACTOR - 1) + raw2) / FILTER_FACTOR;
    }    
    float pin_voltage = (adc0_raw_filtered * VREF) / ADC_MAX;                                   // Вычисляем напряжение на пине, затем умножаем на делитель
    batteryVoltage = pin_voltage * DIVIDER_RATIO;
}

static void calibrateCurrentSensors() {                                                    // Вызывать пока нагрузка отключена (реле и зарядка выключены, ключи закрыты)
    uint32_t sum1 = 0, sum2 = 0;
    for (int i = 0; i < CALIBRATION_SAMPLES; i++) {
        adc_select_input(1);
        sum1 += adc_read();
        adc_select_input(2);
        sum2 += adc_read();
        sleep_ms(1);
    }
    adcOffset1 = sum1 / CALIBRATION_SAMPLES;
    adcOffset2 = sum2 / CALIBRATION_SAMPLES;
}

static void lcdPrintTaskFunc() {
    if (stateOutTR == STATE_STOP) return;
    if (stateOutTR == STATE_SOURCE_UNKNOWN) {
        lcd_set_cursor(0, 0);
        lcd_print("POWER: ON       ");
    } else if (stateOutTR == STATE_PSU) {
        lcd_set_cursor(0, 0);
        lcd_print("POWER: 220V     ");
    } else if (stateOutTR == STATE_BAT) {
        lcd_set_cursor(0, 0);
        lcd_print("POWER: BAT      ");
    } else if (stateOutTR == STATE_LOW_BAT) {
        lcd_set_cursor(0, 0);
        lcd_print("LOW BATTERY  ");
    }
    char buf[17];
    snprintf(buf, sizeof(buf), "V BAT:%6.2f %-3s", batteryVoltage, chargeState ? "CHG" : "");
    lcd_set_cursor(1, 0);
    lcd_print(buf);
}

static void funcChargeTask() {                                       // Проверка "нужна ли зарядка" реже, чем остальная логика -
    if (gpio_get(PIN_BAT_TR)) return;                                 // чтобы дребезг напряжения (особенно без АКБ на стенде) не дёргал реле зарядки
    if (batteryVoltage < CHARGE_V_ON - VOLTAGE_HYSTERESIS) {
        gpio_put(PIN_CHARGE, 1);
        chargeState = true;
    } else if (batteryVoltage > CHARGE_V_OFF + VOLTAGE_HYSTERESIS) {
        gpio_put(PIN_CHARGE, 0);
        chargeState = false;
    }
}

static void funcLogicTask(){
    static bool lowBat = false;
    static uint32_t off = 0;
    static uint32_t idleTicks = 0;

    if (batteryVoltage < LOW_BATTERY - VOLTAGE_HYSTERESIS) {          // Логика состояния батареи
        stateOutTR = STATE_LOW_BAT;
        lowBat = true;
    } else if (lowBat && batteryVoltage > LOW_BATTERY + VOLTAGE_HYSTERESIS) {
        stateOutTR = STATE_SOURCE_UNKNOWN;                            // Не гадаем, BAT сейчас или PSU - отдаём решение логике по adcDiff ниже, в этом же тике
        lowBat = false;
    }

    if (batteryVoltage < OFF_BATTERY) {
        off++;
    } else {
        off = 0;
    }

    if (off == 100){
        systemShutdown("   SYSTEM OFF   ");
    }
    
   int32_t adcDiff = ((int32_t)adc1TR1 - adcOffset1) - ((int32_t)adc2TR2 - adcOffset2);      // Отклонения от откалиброванного нуля: >0 - ток идёт через АКБ, <0 - через блок питания
   bool canChangeState = (stateOutTR != STATE_LOW_BAT && stateOutTR != STATE_STOP);

   if (adcDiff > SOURCE_SWITCH_THRESHOLD){
        gpio_put(PIN_BAT_TR, 1);
        gpio_put(PIN_PSU_TR, 0);
        gpio_put(PIN_LED_BAT, 1);
        gpio_put(PIN_LED_PSU, 0);
        gpio_put(PIN_CHARGE, 0);
        chargeState = false;
        if (canChangeState) stateOutTR = STATE_BAT;
        idleTicks = 0;                                                                       // Принтер реально потребляет ток - простоя нет
   } else if (adcDiff < -SOURCE_SWITCH_THRESHOLD){
        gpio_put(PIN_PSU_TR, 1);
        gpio_put(PIN_BAT_TR, 0);
        gpio_put(PIN_LED_PSU, 1);
        gpio_put(PIN_LED_BAT, 0);
        if (canChangeState) stateOutTR = STATE_PSU;
        idleTicks = 0;
   } else if (adcDiff > -SOURCE_NEUTRAL_THRESHOLD && adcDiff < SOURCE_NEUTRAL_THRESHOLD){     // Ток через оба датчика почти одинаковый - активный источник не определён,
        gpio_put(PIN_PSU_TR, 0);                                                             // закрываем оба ключа (см. гистерезис между SOURCE_NEUTRAL_THRESHOLD и SOURCE_SWITCH_THRESHOLD)
        gpio_put(PIN_BAT_TR, 0);
        gpio_put(PIN_LED_PSU, 0);
        gpio_put(PIN_LED_BAT, 0);
        if (canChangeState) stateOutTR = STATE_SOURCE_UNKNOWN;
        if (idleTicks < IDLE_SHUTDOWN_TICKS) {
            idleTicks++;
        } else {
            systemShutdown(" IDLE SHUTDOWN  ");                                              // Принтер не печатает уже IDLE_SHUTDOWN_MIN минут - выключаемся
        }
   }
}

void doWork() {
    lcd_init();
    perifInit();
    previous_time = time_us_64();
    lcd_print("UPS V1.0");
    gpio_put(PIN_BUZZER, 1);
    sleep_ms(400);
    gpio_put(PIN_BUZZER, 0);
    sleep_ms(800);
    gpio_put(PIN_CHARGE, 0);                                                                 // На время калибровки явно запрещаем зарядку и держим ключи закрытыми,
    chargeState = false;                                                                     // чтобы через датчики не тёк зарядный ток
    gpio_put(PIN_PSU_TR, 0);
    gpio_put(PIN_BAT_TR, 0);
    calibrateCurrentSensors();                                                               // Реле, зарядка и ключи выключены - токов нет, фиксируем ноль датчиков
    gpio_put(PIN_RELAY, 1);
    lcd_clear();
    taskInit(&buttonTask, funcButtonMonitor, 50);
    taskInit(&adcTask, funcADCMonitor, 100);
    taskInit(&lcdTask, lcdPrintTaskFunc, 100);
    taskInit(&logicTask, funcLogicTask, 100);
    taskInit(&buzzerTask, buzzerTaskFunc, 100);
    taskInit(&shedulerBuzzerTask, shedulerBuzzerTaskFunc, 100);
    taskInit(&chargeTask, funcChargeTask, CHARGE_CHECK_INTERVAL_MS);
    stateOutTR = STATE_SOURCE_UNKNOWN;                                                       // Источник питания ещё не определён — оба ключа держим закрытыми,
    gpio_put(PIN_PSU_TR, 0);                                                                 // чтобы не подать напряжение с АКБ в отсутствующий блок питания (и наоборот)
    gpio_put(PIN_BAT_TR, 0);
    gpio_put(PIN_LED_PSU, 0);
    gpio_put(PIN_LED_BAT, 0);
    gpio_put(PIN_CHARGE, 1);
    chargeState = true;                                                                        // Совпадает с начальным gpio_put(PIN_CHARGE, 1) выше - funcChargeTask поправит это по факту через CHARGE_CHECK_INTERVAL_MS
    gpio_put(PIN_TR_READ, 1);
    watchdog_enable(WATCHDOG_TIMEOUT_MS, true);                                             // Аппаратный watchdog: если главный цикл зависнет, МК перезагрузится
}

void Work() {
    watchdog_update();
    taskRun(&buttonTask);
    taskRun(&adcTask);
    taskRun(&lcdTask);
    taskRun(&logicTask);
    taskRun(&buzzerTask);
    taskRun(&shedulerBuzzerTask);
    taskRun(&chargeTask);
}

