#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <atomic>

/**
 * @brief POD структура параметров стимуляции
 * Plain Old Data - можно безопасно копировать между потоками
 * УПРОЩЕННАЯ ВЕРСИЯ: только амплитуда
 */
struct StimParams {
    uint8_t stimDuty;  // Амплитуда 0-100%
    
    // Конструктор с безопасным значением по умолчанию
    StimParams() 
        : stimDuty(10)
    {}
    
    // Конструктор с параметром
    explicit StimParams(uint8_t duty)
        : stimDuty(duty)
    {}
};

/**
 * @brief Состояние одного энкодера
 * Хранит текущую позицию энкодера
 */
struct EncoderState {
    int32_t position;  // Текущая позиция энкодера
    uint8_t value;     // Текущее значение (0-100)
    
    EncoderState() 
        : position(0)
        , value(10)
    {}
};

/**
 * @brief Thread-safe менеджер состояния приложения
 * 
 * Обеспечивает безопасный доступ к параметрам из разных ядер ESP32.
 * Использует мьютексы для защиты критических секций.
 * 
 * УПРОЩЕННАЯ ВЕРСИЯ:
 * - Только amplitude в StimParams
 * - Два независимых энкодера (A и B)
 * - Каждый энкодер управляет своим значением amplitude
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
     * @brief Получить значение amplitude (thread-safe)
     * @return Текущее значение amplitude (0-100)
     */
    uint8_t getAmplitude() const;
    
    /**
     * @brief Установить значение amplitude (thread-safe)
     * @param amp Новое значение amplitude (0-100)
     */
    void setAmplitude(uint8_t amp);
    
    // === Методы для работы с Энкодером A ===
    
    /**
     * @brief Изменить значение энкодера A на delta
     * @param delta Изменение значения (может быть отрицательным)
     * @return true если значение было изменено
     */
    bool adjustEncoderA(int8_t delta);
    
    /**
     * @brief Получить состояние энкодера A
     * @return Копия состояния энкодера A
     */
    EncoderState getEncoderAState() const;
    
    /**
     * @brief Получить значение энкодера A
     * @return Текущее значение (0-100)
     */
    uint8_t getEncoderAValue() const;
    
    // === Методы для работы с Энкодером B ===
    
    /**
     * @brief Изменить значение энкодера B на delta
     * @param delta Изменение значения (может быть отрицательным)
     * @return true если значение было изменено
     */
    bool adjustEncoderB(int8_t delta);
    
    /**
     * @brief Получить состояние энкодера B
     * @return Копия состояния энкодера B
     */
    EncoderState getEncoderBState() const;
    
    /**
     * @brief Получить значение энкодера B
     * @return Текущее значение (0-100)
     */
    uint8_t getEncoderBValue() const;
    
    // === Atomic флаги состояния (быстрый доступ без мьютекса) ===
    
    bool isStimRunning() const { return stimRunning_.load(); }
    void setStimRunning(bool running) { stimRunning_.store(running); }
    
    // === Отладка и диагностика ===
    
    /**
     * @brief Вывести текущее состояние в Serial
     */
    void printCurrentState() const;

private:
    StimParams stimParams_;
    EncoderState encoderAState_;  // Состояние энкодера A
    EncoderState encoderBState_;  // Состояние энкодера B
    mutable SemaphoreHandle_t mutex_;  // mutable для const методов
    std::atomic<bool> stimRunning_{false};
    
    // Вспомогательные методы
    void applyConstraints(StimParams& params) const;
};