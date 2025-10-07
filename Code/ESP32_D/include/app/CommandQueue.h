#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "app/AppState.h"

/**
 * @brief Типы команд для межпроцессорного взаимодействия
 */
enum class CommandType {
    UPDATE_PARAMS,  // Обновить параметры стимуляции
    START_STIM,     // Запустить стимуляцию
    STOP_STIM,      // Остановить стимуляцию
    GET_STATUS,     // Запросить статус (для будущего использования)
    EMERGENCY_STOP  // Аварийная остановка
};

/**
 * @brief Структура команды для передачи между ядрами
 */
struct Command {
    CommandType type;
    StimParams params;
    uint32_t timestamp;  // Для отладки и профилирования
    
    Command() : type(CommandType::UPDATE_PARAMS), timestamp(0) {}
    
    Command(CommandType t) : type(t), timestamp(millis()) {}
    
    Command(CommandType t, const StimParams& p) 
        : type(t), params(p), timestamp(millis()) {}
};

/**
 * @brief Обертка над FreeRTOS очередью для type-safe работы
 */
class CommandQueue {
public:
    CommandQueue(size_t queueSize = 10);
    ~CommandQueue();
    
    /**
     * @brief Отправить команду (неблокирующая)
     * @param cmd Команда для отправки
     * @param timeoutMs Таймаут в миллисекундах (0 = не блокировать)
     * @return true если команда отправлена успешно
     */
    bool send(const Command& cmd, uint32_t timeoutMs = 0);
    
    /**
     * @brief Получить команду из очереди
     * @param cmd Буфер для команды
     * @param timeoutMs Таймаут в миллисекундах (0 = не блокировать)
     * @return true если команда получена
     */
    bool receive(Command& cmd, uint32_t timeoutMs = 0);
    
    /**
     * @brief Проверить, есть ли команды в очереди
     */
    bool hasCommands() const;
    
    /**
     * @brief Получить количество команд в очереди
     */
    size_t getCount() const;
    
    /**
     * @brief Очистить очередь
     */
    void clear();
    
    /**
     * @brief Проверить, инициализирована ли очередь
     */
    bool isValid() const { return queue_ != nullptr; }

private:
    QueueHandle_t queue_;
    size_t queueSize_;
};