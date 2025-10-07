#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>

/**
 * @brief Типы параметров стимуляции для UI
 */
enum class ParamType { 
    AMPLITUDE,      // Амплитуда 0-100%
    PULSE_WIDTH,    // Длительность импульса 50-400 мкс
    RATE,           // Частота 10-100 Гц
    BURST_HZ,       // Частота пачек 0.5-5.0 Гц
    BURST_DUTY      // Скважность пачек 10-50%
};

/**
 * @brief POD структура параметров стимуляции
 * Plain Old Data - можно безопасно копировать между потоками
 */
struct StimParams {
    uint8_t  amplitude;         // %
    uint16_t pulseWidthUs;      // мкс
    uint8_t  rateHz;            // Гц
    float    burstHz;           // Гц
    uint8_t  burstDutyPercent;  // %
    
    // Конструктор с безопасными значениями по умолчанию
    StimParams() 
        : amplitude(10)
        , pulseWidthUs(200)
        , rateHz(20)
        , burstHz(1.0f)
        , burstDutyPercent(20)
    {}
    
    // Конструктор с параметрами
    StimParams(uint8_t amp, uint16_t pw, uint8_t rate, float bhz, uint8_t bduty)
        : amplitude(amp)
        , pulseWidthUs(pw)
        , rateHz(rate)
        , burstHz(bhz)
        , burstDutyPercent(bduty)
    {}
};

/**
 * @brief Thread-safe менеджер состояния приложения
 * 
 * Обеспечивает безопасный доступ к параметрам из разных ядер ESP32.
 * Использует мьютексы для защиты критических секций.
 */
class AppState {
public:
    AppState();
    ~AppState();
    
    // Запрет копирования (из-за мьютекса)
    AppState(const AppState&) = delete;
    AppState& operator=(const AppState&) = delete;
    
    // === Thread-safe методы для работы с параметрами ===
    
    /**
     * @brief Получить копию текущих параметров (thread-safe)
     * @return Копия структуры параметров
     */
    StimParams getStimParams() const;
    
    /**
     * @brief Установить новые параметры (thread-safe)
     * @param params Новые параметры
     */
    void setStimParams(const StimParams& params);
    
    /**
     * @brief Изменить текущий выбранный параметр на delta
     * @param delta Изменение значения (может быть отрицательным)
     * @return true если параметр был изменен
     */
    bool adjustCurrentParam(int8_t delta);
    
    /**
     * @brief Переключиться на следующий параметр
     */
    void selectNextParam();
    
    /**
     * @brief Переключиться на предыдущий параметр
     */
    void selectPrevParam();
    
    /**
     * @brief Установить конкретный параметр для редактирования
     * @param param Тип параметра
     */
    void setCurrentParam(ParamType param);
    
    /**
     * @brief Получить текущий выбранный параметр
     * @return Тип текущего параметра
     */
    ParamType getCurrentParam() const;
    
    // === Atomic флаги состояния (быстрый доступ без мьютекса) ===
    // ВАЖНО: эти методы должны быть inline и определены в заголовке
    
    bool isStimRunning() const { return stimRunning_.load(); }
    void setStimRunning(bool running) { stimRunning_.store(running); }
    
    // === Отладка и диагностика ===
    
    /**
     * @brief Вывести текущее состояние в Serial
     */
    void printCurrentState() const;
    
    /**
     * @brief Получить имя параметра
     */
    const char* getParamName(ParamType type) const;
    
    /**
     * @brief Получить текущее значение выбранного параметра как строку
     */
    String getCurrentParamValueStr() const;
    
    /**
     * @brief Получить диапазон значений для параметра
     */
    void getParamRange(ParamType type, int& min, int& max, int& step) const;

private:
    StimParams stimParams_;
    ParamType currentParam_;
    mutable SemaphoreHandle_t mutex_;  // mutable для const методов
    std::atomic<bool> stimRunning_{false};
    
    // Вспомогательные методы
    void applyConstraints(StimParams& params) const;
};