#ifndef AUDIOCENSOR_AUDIO_PROCESSOR_H
#define AUDIOCENSOR_AUDIO_PROCESSOR_H

#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QStringList>

#include <deque>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <atomic>
#include <memory>

// Прототипы классов PortAudio и Vosk
typedef void PaStream;
struct VoskModel;
struct VoskRecognizer;

namespace audiocensor {

/**
 * @brief Поток для обработки аудио и цензуры нежелательных слов
 */
class AudioProcessor : public QThread {
    Q_OBJECT

public:
    /**
     * @brief Конструктор
     * @param config Конфигурация аудио процессора
     * @param parent Родительский объект
     */
    explicit AudioProcessor(const std::unordered_map<std::string, std::string>& config, 
                           QObject* parent = nullptr);
    
    /**
     * @brief Деструктор
     */
    ~AudioProcessor();
    
    /**
     * @brief Инициализация аудио-устройств
     * @return true если инициализация прошла успешно, false в противном случае
     */
    bool initialize_audio();
    
    /**
     * @brief Настройка входных и выходных потоков с учетом характеристик устройств
     * @param input_index Индекс входного устройства
     * @param output_index Индекс выходного устройства
     * @return true если настройка прошла успешно, false в противном случае
     */
    bool setup_streams(int input_index, int output_index);
    
    /**
     * @brief Генерирует бип заданной длительности
     * @param duration Длительность бипа в секундах
     * @return Вектор с сэмплами бипа
     */
    std::vector<short> generate_beep(double duration);
    
    /**
     * @brief Обновляет конфигурацию
     * @param config Новая конфигурация
     */
    void update_config(const std::unordered_map<std::string, std::string>& config);
    
    /**
     * @brief Приостанавливает обработку аудио
     */
    void pause();
    
    /**
     * @brief Возобновляет обработку аудио
     */
    void resume();
    
    /**
     * @brief Останавливает обработку аудио и корректно освобождает ресурсы
     */
    void stop_processing();
    
    /**
     * @brief Проверяет, запущен ли аудио процессор
     * @return true если процессор запущен, false в противном случае
     */
    bool is_running() const { return running; }
    
    /**
     * @brief Проверяет, приостановлен ли аудио процессор
     * @return true если процессор приостановлен, false в противном случае
     */
    bool is_paused() const { return paused; }

protected:
    /**
     * @brief Основной метод потока - обрабатывает аудио и применяет цензуру
     */
    void run() override;

private:
    /**
     * @brief Обрабатывает результаты распознавания и отмечает регионы для цензуры
     * @param result_json Результаты распознавания в формате JSON
     */
    void process_recognition_result(const std::string& result_json);
    
    /**
     * @brief Освобождает все аудио ресурсы
     */
    void cleanup_resources();

signals:
    /**
     * @brief Сигнал для обновления статуса
     * @param status Текущий статус
     */
    void statusUpdate(const QString& status);
    
    /**
     * @brief Сигнал для списка устройств
     * @param devices Список устройств
     */
    void deviceListUpdate(const QList<QPair<int, QString>>& devices);
    
    /**
     * @brief Сигнал для характеристик выбранного устройства
     * @param info Информация об устройстве
     */
    void deviceInfoUpdate(const QVariantMap& info);
    
    /**
     * @brief Сигнал для лога
     * @param message Сообщение для лога
     */
    void logMessage(const QString& message);
    
    /**
     * @brief Сигнал для обнаружения слова
     * @param word Обнаруженное слово
     * @param start_time Время начала
     * @param end_time Время конца
     */
    void wordDetected(const QString& word, double start_time, double end_time);
    
    /**
     * @brief Сигнал для применения цензуры
     * @param chunk Номер чанка
     * @param start Начало региона цензуры
     * @param end Конец региона цензуры
     */
    void censorApplied(int chunk, int start, int end);
    
    /**
     * @brief Сигнал для обновления состояния буфера
     * @param current Текущий размер буфера
     * @param maximum Максимальный размер буфера
     */
    void bufferUpdate(int current, int maximum);

private:
    // Конфигурация
    std::unordered_map<std::string, std::string> config;
    
    // Флаги состояния
    std::atomic<bool> running;
    std::atomic<bool> paused;
    
    // Аудио объекты
    void* audio; // Эквивалент PyAudio
    PaStream* input_stream;
    PaStream* output_stream;
    
    // Объекты распознавания речи
    std::shared_ptr<VoskModel> model;
    std::shared_ptr<VoskRecognizer> recognizer;
    
    // Динамические параметры аудио
    int current_sample_rate;
    int current_channels;
    
    // Буферы и счетчики
    std::deque<short> audio_buffer;
    int buffer_size_in_chunks;
    std::vector<std::tuple<int, int, bool>> censored_regions;
    QMutex buffer_lock;
    QMutex regions_lock;
    double program_start_time;
    int chunks_processed;
    
    // Кэш для бипов
    std::unordered_map<double, std::vector<short>> beep_cache;
    
    // Индексы устройств
    int input_device_index;
    int output_device_index;
};

} // namespace audiocensor

#endif // AUDIOCENSOR_AUDIO_PROCESSOR_H