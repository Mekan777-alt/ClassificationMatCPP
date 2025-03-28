} catch (const std::exception& e) {
        emit logMessage(QString("❌ Непредвиденная ошибка настройки аудио потоков: %1").arg(e.what()));
        return false;
    }
}

std::vector<short> AudioProcessor::generate_beep(double duration) {
    // Проверяем, есть ли бип такой длительности в кэше
    if (beep_cache.find(duration) != beep_cache.end()) {
        return beep_cache[duration];
    }
    
    // Если нет, генерируем новый
    int samples = static_cast<int>(current_sample_rate * duration);
    std::vector<short> beep_data(samples);
    
    double beep_frequency = std::stod(config.at("beep_frequency"));
    double beep_volume = 0.5; // Громкость бипа (0.0 - 1.0)
    
    for (int i = 0; i < samples; i++) {
        double t = static_cast<double>(i) / current_sample_rate;
        beep_data[i] = static_cast<short>(
            sin(2 * M_PI * beep_frequency * t) * 32767 * beep_volume
        );
    }
    
    // Добавляем в кэш и возвращаем
    beep_cache[duration] = beep_data;
    return beep_data;
}

void AudioProcessor::run() {
    running = true;
    program_start_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() / 1000.0;
    
    chunks_processed = 0;
    
    // Очистка буферов
    {
        QMutexLocker locker(&buffer_lock);
        audio_buffer.clear();
        audio_buffer.resize(buffer_size_in_chunks * std::stoi(config.at("chunk_size")));
    }
    
    {
        QMutexLocker locker(&regions_lock);
        censored_regions.clear();
    }
    
    emit logMessage("🎤 Запись и обработка аудио начаты");
    double buffer_delay_sec = std::stod(config.at("buffer_delay"));
    emit logMessage(QString("📊 Буферизация: воспроизведение начнется через %1 секунд...").
                  arg(buffer_delay_sec, 0, 'f', 1));
    
    // Запускаем стримы
    Pa_StartStream(input_stream);
    Pa_StartStream(output_stream);
    
    // Подготавливаем буфер для чтения данных
    int chunk_size = std::stoi(config.at("chunk_size"));
    std::vector<short> input_chunk(chunk_size);
    std::vector<short> output_chunk(chunk_size);
    
    // Переменные для распознавания
    std::string audio_data_for_recognition;
    bool recognition_active = true;
    
    // Ждем, пока буфер наполнится
    auto recording_start = std::chrono::system_clock::now();
    double buffer_delay = std::stod(config.at("buffer_delay"));
    
    while (std::chrono::duration<double>(std::chrono::system_clock::now() - recording_start).count() < buffer_delay && running) {
        if (!paused) {
            try {
                // Чтение данных с микрофона
                Pa_ReadStream(input_stream, input_chunk.data(), chunk_size);
                
                // Добавляем в буфер
                {
                    QMutexLocker locker(&buffer_lock);
                    for (short sample : input_chunk) {
                        audio_buffer.push_back(sample);
                        if (audio_buffer.size() > buffer_size_in_chunks * chunk_size) {
                            audio_buffer.pop_front();
                        }
                    }
                }
                
                // Обновляем информацию о буфере в UI
                emit bufferUpdate(static_cast<int>(audio_buffer.size()), 
                                buffer_size_in_chunks * chunk_size);
                
                // Накапливаем данные для распознавания
                if (recognition_active && config.at("enable_censoring") == "true") {
                    // Копируем в строку для распознавания
                    const char* input_data = reinterpret_cast<const char*>(input_chunk.data());
                    audio_data_for_recognition.append(input_data, chunk_size * sizeof(short));
                    
                    // Отправляем на распознавание речи
                    if (vosk_recognizer_accept_waveform(recognizer.get(), 
                                                      input_data, 
                                                      chunk_size * sizeof(short))) {
                        const char* result_json = vosk_recognizer_result(recognizer.get());
                        process_recognition_result(result_json);
                    }
                }
                
            } catch (const std::exception& e) {
                emit logMessage(QString("❌ Ошибка при записи аудио: %1").arg(e.what()));
            }
        }
        
        // Небольшая пауза для экономии CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    emit logMessage("🔊 Воспроизведение аудио начато");
    
    // Основной цикл обработки
    while (running) {
        if (!paused) {
            try {
                // Запись с микрофона
                Pa_ReadStream(input_stream, input_chunk.data(), chunk_size);
                
                // Добавляем в буфер
                {
                    QMutexLocker locker(&buffer_lock);
                    for (short sample : input_chunk) {
                        audio_buffer.push_back(sample);
                        if (audio_buffer.size() > buffer_size_in_chunks * chunk_size) {
                            audio_buffer.pop_front();
                        }
                    }
                }
                
                // Накапливаем данные для распознавания
                if (recognition_active && config.at("enable_censoring") == "true") {
                    // Копируем в строку для распознавания
                    const char* input_data = reinterpret_cast<const char*>(input_chunk.data());
                    audio_data_for_recognition.append(input_data, chunk_size * sizeof(short));
                    
                    // Ограничиваем размер данных для распознавания
                    size_t max_recognition_bytes = current_sample_rate * sizeof(short) * 2; // ~2 секунды аудио
                    if (audio_data_for_recognition.size() > max_recognition_bytes) {
                        audio_data_for_recognition = audio_data_for_recognition.substr(
                            audio_data_for_recognition.size() - max_recognition_bytes
                        );
                    }
                    
                    // Отправляем на распознавание речи
                    if (vosk_recognizer_accept_waveform(recognizer.get(), 
                                                      input_data, 
                                                      chunk_size * sizeof(short))) {
                        const char* result_json = vosk_recognizer_result(recognizer.get());
                        process_recognition_result(result_json);
                    }
                }
                
                // Воспроизведение с задержкой
                std::fill(output_chunk.begin(), output_chunk.end(), 0); // Заполняем нулями
                
                // Извлекаем чанк из буфера
                size_t samples_available;
                {
                    QMutexLocker locker(&buffer_lock);
                    samples_available = audio_buffer.size();
                    
                    if (samples_available >= chunk_size) {
                        for (int i = 0; i < chunk_size; i++) {
                            if (!audio_buffer.empty()) {
                                output_chunk[i] = audio_buffer.front();
                                audio_buffer.pop_front();
                            }
                        }
                    } else {
                        // Если недостаточно данных, продолжаем цикл
                        continue;
                    }
                }
                
                // Проверяем, нужно ли цензурировать этот чанк
                if (config.at("enable_censoring") == "true") {
                    QMutexLocker locker(&regions_lock);
                    for (size_t i = 0; i < censored_regions.size(); i++) {
                        auto& region = censored_regions[i];
                        int start_idx = std::get<0>(region);
                        int end_idx = std::get<1>(region);
                        bool processed = std::get<2>(region);
                        
                        if (start_idx <= chunks_processed && chunks_processed <= end_idx) {
                            // Применяем цензуру - заменяем чанк на тишину или бип
                            std::fill(output_chunk.begin(), output_chunk.end(), 0);
                            emit censorApplied(chunks_processed, start_idx, end_idx);
                            
                            // Если достигли конца интервала, отмечаем его как обработанный
                            if (chunks_processed == end_idx) {
                                std::get<2>(region) = true;
                            }
                        }
                    }
                    
                    // Удаляем обработанные интервалы
                    censored_regions.erase(
                        std::remove_if(censored_regions.begin(), censored_regions.end(), 
                                      [](const auto& r) { return std::get<2>(r); }),
                        censored_regions.end()
                    );
                }
                
                // Отправляем на выход
                Pa_WriteStream(output_stream, output_chunk.data(), chunk_size);
                
                // Увеличиваем счетчик обработанных чанков
                chunks_processed++;
                
                // Обновляем информацию о буфере в UI
                emit bufferUpdate(static_cast<int>(audio_buffer.size()), 
                                buffer_size_in_chunks * chunk_size);
                
            } catch (const std::exception& e) {
                emit logMessage(QString("❌ Ошибка при обработке аудио: %1").arg(e.what()));
            }
        }
        
        // Небольшая пауза для экономии CPU
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Очистка ресурсов
    cleanup_resources();
    emit logMessage("✅ Обработка аудио завершена");
}

void AudioProcessor::process_recognition_result(const std::string& result_json) {
    try {
        // Парсим JSON
        json result = json::parse(result_json);
        
        // Проверяем наличие результатов
        if (!result.contains("result") || !result["result"].is_array()) {
            return;
        }
        
        auto words = result["result"];
        if (words.empty()) {
            return;
        }
        
        // Выводим все распознанные слова для отладки, если включено
        if (config.at("debug_mode") == "true") {
            QStringList all_words;
            for (const auto& word : words) {
                all_words.append(QString::fromStdString(word["word"].get<std::string>()).toLower());
            }
            emit logMessage(QString("🔍 Распознано: %1").arg(all_words.join(", ")));
        }
        
        // Текущее время от начала программы
        double elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count() / 1000.0 - program_start_time;
        
        // Создаем детектор слов
        WordDetector detector(std::unordered_map<std::string, std::string>());
        
        // Подготавливаем списки целевых слов и паттернов
        std::vector<std::string> target_patterns;
        std::vector<std::string> target_words;
        
        // Разбор строки с паттернами
        if (config.find("target_patterns") != config.end()) {
            try {
                // Предполагаем, что это JSON-строка с массивом
                target_patterns = json::parse(config.at("target_patterns")).get<std::vector<std::string>>();
            } catch (...) {
                // Если не удалось разобрать как JSON, пробуем как обычную строку с разделителями
                std::istringstream iss(config.at("target_patterns"));
                std::string pattern;
                while (std::getline(iss, pattern, ',')) {
                    if (!pattern.empty()) {
                        target_patterns.push_back(pattern);
                    }
                }
            }
        }
        
        // Разбор строки с целевыми словами
        if (config.find("target_words") != config.end()) {
            try {
                // Предполагаем, что это JSON-строка с массивом
                target_words = json::parse(config.at("target_words")).get<std::vector<std::string>>();
            } catch (...) {
                // Если не удалось разобрать как JSON, пробуем как обычную строку с разделителями
                std::istringstream iss(config.at("target_words"));
                std::string word;
                while (std::getline(iss, word, ',')) {
                    if (!word.empty()) {
                        target_words.push_back(word);
                    }
                }
            }
        }
        
        // Параметры для вычисления индексов чанков
        int chunk_size = std::stoi(config.at("chunk_size"));
        double buffer_delay = std::stod(config.at("buffer_delay"));
        int safety_margin = std::stoi(config.at("safety_margin"));
        double chunks_per_second = static_cast<double>(current_sample_rate) / chunk_size;
        
        for (const auto& word : words) {
            std::string word_text = word["word"].get<std::string>();
            std::transform(word_text.begin(), word_text.end(), word_text.begin(),
                         [](unsigned char c){ return std::tolower(c); });
            
            // Проверяем, является ли слово запрещенным
            bool is_prohibited;
            std::string matched_pattern;
            std::tie(is_prohibited, matched_pattern) = detector.is_prohibited_word(word_text, 
                                                                               target_patterns, 
                                                                               target_words);
            
            if (is_prohibited) {
                // Получаем время начала и конца слова
                double start_time = word["start"].get<double>();
                double end_time = word["end"].get<double>();
                
                // Рассчитываем индексы чанков для цензуры с учетом текущей частоты дискретизации
                int chunks_offset_start = static_cast<int>((start_time - (elapsed_time - buffer_delay)) *
                                                      chunks_per_second) - safety_margin;
                int chunks_offset_end = static_cast<int>((end_time - (elapsed_time - buffer_delay)) *
                                                    chunks_per_second) + safety_margin;
                
                // Абсолютные индексы чанков для цензуры
                int censored_chunk_start = chunks_processed + chunks_offset_start;
                int censored_chunk_end = chunks_processed + chunks_offset_end;
                
                // Добавляем регион для цензуры
                {
                    QMutexLocker locker(&regions_lock);
                    censored_regions.push_back(std::make_tuple(censored_chunk_start, censored_chunk_end, false));
                }
                
                // Уведомляем о найденном слове
                emit wordDetected(QString::fromStdString(word_text), start_time, end_time);
                
                // Улучшенное форматирование сообщения
                QString log_message = QString("⚠️ Обнаружено ненормативная лексика: \"%1\"\n")
                                    .arg(QString::fromStdString(word_text));
                
                log_message += QString("   Время: %1с - %2с (длительность: %3с)\n")
                            .arg(start_time, 0, 'f', 2)
                            .arg(end_time, 0, 'f', 2)
                            .arg(end_time - start_time, 0, 'f', 2);
                
                emit logMessage(log_message);
                
                // Сохраняем в файл, если включено
                if (config.at("log_to_file") == "true") {
                    try {
                        std::ofstream log_file(config.at("log_file"), std::ios::app);
                        if (log_file.is_open()) {
                            auto now = std::chrono::system_clock::now();
                            auto now_time_t = std::chrono::system_clock::to_time_t(now);
                            std::tm now_tm = *std::localtime(&now_time_t);
                            
                            char timestamp[20];
                            std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &now_tm);
                            
                            log_file << timestamp << " - Обнаружено: \"" << word_text << "\" "
                                    << (matched_pattern.empty() ? "" : "(шаблон: " + matched_pattern + ") ")
                                    << "(время: " << start_time << "с-" << end_time << "с)" << std::endl;
                            
                            log_file.close();
                        }
                    } catch (const std::exception& e) {
                        emit logMessage(QString("❌ Ошибка записи в лог-файл: %1").arg(e.what()));
                    }
                }
            }
        }
        
    } catch (const json::parse_error& e) {
        emit logMessage(QString("❌ Ошибка при разборе JSON результатов распознавания: %1").arg(e.what()));
    } catch (const std::exception& e) {
        emit logMessage(QString("❌ Ошибка при обработке результатов распознавания: %1").arg(e.what()));
    }
}

void AudioProcessor::update_config(const std::unordered_map<std::string, std::string>& new_config) {
    config = new_config;
    
    // Пересчитываем размер буфера
    buffer_size_in_chunks = static_cast<int>(std::stod(config.at("buffer_delay")) * current_sample_rate / 
                                          std::stoi(config.at("chunk_size"))) + 2;
    
    // Очищаем кэш бипов при изменении частоты или громкости
    beep_cache.clear();
}

void AudioProcessor::pause() {
    paused = true;
}

void AudioProcessor::resume() {
    paused = false;
}

void AudioProcessor::stop_processing() {
    running = false;
    
    // Ждем завершения потока
    wait(1000); // Ждем максимум 1 секунду
    
    // Освобождаем ресурсы
    cleanup_resources();
}

void AudioProcessor::cleanup_resources() {
    // Правильное освобождение ресурсов
    try {
        if (input_stream) {
            Pa_StopStream(input_stream);
            Pa_CloseStream(input_stream);
            input_stream = nullptr;
        }
    } catch (const std::exception& e) {
        emit logMessage(QString("Ошибка при закрытии потока ввода: %1").arg(e.what()));
    }
    
    try {
        if (output_stream) {
            Pa_StopStream(output_stream);
            Pa_CloseStream(output_stream);
            output_stream = nullptr;
        }
    } catch (const std::exception& e) {
        emit logMessage(QString("Ошибка при закрытии потока вывода: %1").arg(e.what()));
    }
    
    try {
        Pa_Terminate();
    } catch (const std::exception& e) {
        emit logMessage(QString("Ошибка при завершении PortAudio: %1").arg(e.what()));
    }
    
    // Очищаем буферы
    {
        QMutexLocker locker(&buffer_lock);
        audio_buffer.clear();
    }
    
    {
        QMutexLocker locker(&regions_lock);
        censored_regions.clear();
    }
    
    // Очищаем кэш бипов
    beep_cache.clear();
    
    // Освобождаем распознаватель и модель
    recognizer.reset();
    model.reset();
    
    emit logMessage("✅ Ресурсы аудио освобождены");
}#include "audiocensor/audio_processor.h"
#include "audiocensor/word_detector.h"
#include "audiocensor/constants.h"

#include <QDebug>
#include <QMutexLocker>
#include <QDateTime>

#include <portaudio.h>
#include <vosk_api.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <thread>
#include <cmath>
#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <functional>

namespace audiocensor {

using json = nlohmann::json;

// Callback-функция для получения данных с микрофона
static int inputCallback(const void* inputBuffer, void* outputBuffer,
                         unsigned long framesPerBuffer,
                         const PaStreamCallbackTimeInfo* timeInfo,
                         PaStreamCallbackFlags statusFlags,
                         void* userData) {
    // В этой реализации мы не используем callback для обработки,
    // а читаем данные напрямую из потока
    return paContinue;
}

// Callback-функция для отправки данных на выход
static int outputCallback(const void* inputBuffer, void* outputBuffer,
                          unsigned long framesPerBuffer,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData) {
    // В этой реализации мы не используем callback для обработки,
    // а пишем данные напрямую в поток
    return paContinue;
}

AudioProcessor::AudioProcessor(const std::unordered_map<std::string, std::string>& config, QObject* parent)
    : QThread(parent), config(config), running(false), paused(false),
      audio(nullptr), input_stream(nullptr), output_stream(nullptr),
      model(nullptr), recognizer(nullptr),
      current_sample_rate(DEFAULT_SAMPLE_RATE), current_channels(1),
      buffer_size_in_chunks(0), program_start_time(0), chunks_processed(0),
      input_device_index(-1), output_device_index(-1) {
    
    // Инициализация буфера
    buffer_size_in_chunks = static_cast<int>(
        std::stod(config.at("buffer_delay")) * DEFAULT_SAMPLE_RATE / DEFAULT_CHUNK_SIZE) + 2;
    audio_buffer.resize(buffer_size_in_chunks * DEFAULT_CHUNK_SIZE);
}

AudioProcessor::~AudioProcessor() {
    // Останавливаем поток и освобождаем ресурсы
    if (running) {
        stop_processing();
    }
}

bool AudioProcessor::initialize_audio() {
    try {
        // Инициализация PortAudio
        PaError err = Pa_Initialize();
        if (err != paNoError) {
            emit logMessage(QString("Ошибка инициализации PortAudio: %1").
                          arg(Pa_GetErrorText(err)));
            return false;
        }
        
        audio = nullptr; // PortAudio не требует хранения состояния, используем Pa_ функции напрямую
        emit logMessage("✅ Аудио система инициализирована");
        
        // Получаем список устройств с информацией о них
        QList<QPair<int, QString>> devices_info;
        
        int numDevices = Pa_GetDeviceCount();
        if (numDevices < 0) {
            emit logMessage(QString("Ошибка при получении списка устройств: %1").
                          arg(Pa_GetErrorText(numDevices)));
            return false;
        }
        
        for (int i = 0; i < numDevices; i++) {
            const PaDeviceInfo* deviceInfo = Pa_GetDeviceInfo(i);
            if (!deviceInfo) continue;
            
            // Добавляем только устройства, которые поддерживают ввод или вывод
            if (deviceInfo->maxInputChannels > 0 || deviceInfo->maxOutputChannels > 0) {
                QString name = QString("%1").arg(deviceInfo->name);
                if (deviceInfo->maxInputChannels > 0 && deviceInfo->maxOutputChannels > 0) {
                    name += " (In/Out)";
                } else if (deviceInfo->maxInputChannels > 0) {
                    name += " (In)";
                } else {
                    name += " (Out)";
                }
                
                devices_info.append(QPair<int, QString>(i, name));
                
                // Улучшенное форматирование лога устройства
                QString device_log = QString("📱 Устройство [%1]: %2").arg(i).arg(deviceInfo->name);
                device_log += QString("\n   Каналы: Вход %1, Выход %2").
                            arg(deviceInfo->maxInputChannels).
                            arg(deviceInfo->maxOutputChannels);
                device_log += QString("\n   Частота: %1 Гц").arg(deviceInfo->defaultSampleRate);
                emit logMessage(device_log);
            }
        }
        
        // Отправляем информацию об устройствах в UI
        emit deviceListUpdate(devices_info);
        emit logMessage(QString("📊 Обнаружено %1 аудио устройств").arg(devices_info.size()));
        return true;
        
    } catch (const std::exception& e) {
        emit logMessage(QString("❌ Ошибка инициализации аудио: %1").arg(e.what()));
        return false;
    }
}

bool AudioProcessor::setup_streams(int input_index, int output_index) {
    try {
        // Сохраняем индексы устройств
        input_device_index = input_index;
        output_device_index = output_index;
        
        // Проверяем валидность индексов устройств
        int numDevices = Pa_GetDeviceCount();
        if (input_index >= numDevices || output_index >= numDevices) {
            emit logMessage(QString("❌ Неверные индексы устройств: вход=%1, выход=%2").
                          arg(input_index).arg(output_index));
            return false;
        }
        
        // Закрытие предыдущих потоков, если они есть
        if (input_stream) {
            try {
                Pa_StopStream(input_stream);
                Pa_CloseStream(input_stream);
                input_stream = nullptr;
            } catch (const std::exception& e) {
                emit logMessage(QString("Предупреждение: не удалось закрыть входной поток: %1").arg(e.what()));
            }
        }
        
        if (output_stream) {
            try {
                Pa_StopStream(output_stream);
                Pa_CloseStream(output_stream);
                output_stream = nullptr;
            } catch (const std::exception& e) {
                emit logMessage(QString("Предупреждение: не удалось закрыть выходной поток: %1").arg(e.what()));
            }
        }
        
        // Получаем информацию об устройствах
        const PaDeviceInfo* input_device_info = Pa_GetDeviceInfo(input_index);
        const PaDeviceInfo* output_device_info = Pa_GetDeviceInfo(output_index);
        
        if (!input_device_info || !output_device_info) {
            emit logMessage("❌ Не удалось получить информацию об устройствах");
            return false;
        }
        
        // Проверяем, что устройства имеют нужные каналы
        if (input_device_info->maxInputChannels <= 0) {
            emit logMessage("❌ Выбранное устройство ввода не поддерживает ввод");
            return false;
        }
        
        if (output_device_info->maxOutputChannels <= 0) {
            emit logMessage("❌ Выбранное устройство вывода не поддерживает вывод");
            return false;
        }
        
        // Определяем рабочую частоту дискретизации
        int input_rate = static_cast<int>(input_device_info->defaultSampleRate);
        int output_rate = static_cast<int>(output_device_info->defaultSampleRate);
        
        // Стандартные частоты дискретизации от низкой к высокой
        std::vector<int> standard_rates = {8000, 16000, 22050, 32000, 44100, 48000};
        
        // Определяем оптимальную частоту дискретизации
        if (input_rate == output_rate) {
            current_sample_rate = input_rate;
            emit logMessage(QString("📊 Устройства имеют одинаковую частоту %1 Гц").arg(input_rate));
        } else {
            // Находим наибольшую общую частоту
            std::vector<int> common_rates;
            for (int rate : standard_rates) {
                if (rate <= std::min(input_rate, output_rate)) {
                    common_rates.push_back(rate);
                }
            }
            
            if (!common_rates.empty()) {
                current_sample_rate = *std::max_element(common_rates.begin(), common_rates.end());
                emit logMessage(QString("📊 Выбрана общая частота %1 Гц").arg(current_sample_rate));
            } else {
                // Если подходящих стандартных частот нет, используем 16000 Гц
                current_sample_rate = 16000;
                emit logMessage(QString("📊 Выбрана безопасная частота %1 Гц").arg(current_sample_rate));
            }
        }
        
        // Определяем количество каналов (всегда используем моно для обработки)
        current_channels = 1;
        
        // Создаем распознаватель с учетом выбранной частоты дискретизации
        try {
            model = std::shared_ptr<VoskModel>(vosk_model_new(config.at("model_path").c_str()),
                                              vosk_model_free);
            if (!model) {
                emit logMessage("❌ Ошибка создания модели Vosk");
                return false;
            }
            
            recognizer = std::shared_ptr<VoskRecognizer>(
                vosk_recognizer_new(model.get(), current_sample_rate),
                vosk_recognizer_free
            );
            if (!recognizer) {
                emit logMessage("❌ Ошибка создания распознавателя Vosk");
                return false;
            }
            
            vosk_recognizer_set_words(recognizer.get(), 1);
            
        } catch (const std::exception& e) {
            emit logMessage(QString("❌ Ошибка создания распознавателя: %1").arg(e.what()));
            return false;
        }
        
        // Обновляем размер буфера с учетом новой частоты дискретизации
        buffer_size_in_chunks = static_cast<int>(std::stod(config.at("buffer_delay")) * current_sample_rate / 
                                              std::stoi(config.at("chunk_size"))) + 2;
        
        // Очищаем и ресайзим буфер
        audio_buffer.clear();
        audio_buffer.resize(buffer_size_in_chunks * std::stoi(config.at("chunk_size")));
        
        // Отправляем информацию о выбранной конфигурации
        QVariantMap device_config;
        device_config["sample_rate"] = current_sample_rate;
        device_config["channels"] = current_channels;
        device_config["input_device"] = input_device_info->name;
        device_config["output_device"] = output_device_info->name;
        device_config["buffer_size"] = buffer_size_in_chunks;
        emit deviceInfoUpdate(device_config);
        
        // Открытие входного потока
        PaStreamParameters inputParameters;
        inputParameters.device = input_index;
        inputParameters.channelCount = current_channels;
        inputParameters.sampleFormat = paInt16;
        inputParameters.suggestedLatency = input_device_info->defaultLowInputLatency;
        inputParameters.hostApiSpecificStreamInfo = nullptr;
        
        PaError err = Pa_OpenStream(&input_stream, &inputParameters, nullptr,
                                   current_sample_rate, std::stoi(config.at("chunk_size")),
                                   paNoFlag, nullptr, nullptr);
        
        if (err != paNoError) {
            emit logMessage(QString("❌ Ошибка открытия входного потока: %1").arg(Pa_GetErrorText(err)));
            return false;
        }
        
        emit logMessage(QString("✅ Входной поток открыт (устройство %1)").arg(input_index));
        
        // Открытие выходного потока
        PaStreamParameters outputParameters;
        outputParameters.device = output_index;
        outputParameters.channelCount = current_channels;
        outputParameters.sampleFormat = paInt16;
        outputParameters.suggestedLatency = output_device_info->defaultLowOutputLatency;
        outputParameters.hostApiSpecificStreamInfo = nullptr;
        
        err = Pa_OpenStream(&output_stream, nullptr, &outputParameters,
                           current_sample_rate, std::stoi(config.at("chunk_size")),
                           paNoFlag, nullptr, nullptr);
        
        if (err != paNoError) {
            emit logMessage(QString("❌ Ошибка открытия выходного потока: %1").arg(Pa_GetErrorText(err)));
            
            // Закрываем входной поток, если выходной не удалось открыть
            if (input_stream) {
                Pa_CloseStream(input_stream);
                input_stream = nullptr;
            }
            
            return false;
        }
        
        emit logMessage(QString("✅ Выходной поток открыт (устройство %1)").arg(output_index));
        emit logMessage(QString("✅ Аудио потоки настроены (вход: %1, выход: %2)").
                      arg(input_index).arg(output_index));
        emit logMessage(QString("📊 Частота дискретизации: %1 Гц, Каналов: %2").
                      arg(current_sample_rate).arg(current_channels));
        
        return true;
        
    } catch (const std::exception& e) {
        emit logMessage(QString("❌ Непредвиденная ошибка настройки аудио потоков: %1").arg(e.what()));
        return false;
    }
}