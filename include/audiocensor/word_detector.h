#ifndef AUDIOCENSOR_WORD_DETECTOR_H
#define AUDIOCENSOR_WORD_DETECTOR_H

#include <string>
#include <vector>
#include <unordered_map>
#include <tuple>
#include <regex>
#include <chrono>

namespace audiocensor {

/**
 * @brief Класс для обнаружения нежелательных слов в тексте
 */
class WordDetector {
public:
    /**
     * @brief Конструктор
     * @param config Конфигурация детектора
     */
    explicit WordDetector(const std::unordered_map<std::string, std::string>& config = {});
    
    /**
     * @brief Проверяет, является ли слово запрещенным
     * @param word_text Проверяемое слово
     * @param patterns Список регулярных выражений для проверки
     * @param target_words Список запрещенных слов
     * @return Пара (bool, string) - результат проверки и причина
     */
    std::tuple<bool, std::string> is_prohibited_word(
        const std::string& word_text,
        const std::vector<std::string>& patterns = {},
        const std::vector<std::string>& target_words = {}
    );
    
    /**
     * @brief Обрабатывает результаты распознавания и возвращает регионы для цензуры
     * @param result Результаты распознавания в формате JSON
     * @param current_chunk_idx Текущий индекс чанка
     * @param elapsed_time Прошедшее время
     * @return Список регионов для цензуры в формате (начало_чанка, конец_чанка, обработано)
     */
    std::vector<std::tuple<int, int, bool>> process_recognition_result(
        const std::string& result_json,
        int current_chunk_idx = 0,
        double elapsed_time = 0.0
    );
    
    /**
     * @brief Периодически проверяет целостность системы
     * @return true если система не скомпрометирована, false в противном случае
     */
    bool check_periodic_integrity();
    
    /**
     * @brief Сбрасывает кэш проверок
     */
    void reset_cache();
    
    /**
     * @brief Возвращает количество обнаруженных запрещенных слов
     * @return Количество обнаружений
     */
    int get_detection_count() const { return detection_count; }
    
private:
    /**
     * @brief Нормализует слово для сравнения
     * @param word Исходное слово
     * @return Нормализованное слово
     */
    std::string _normalize_word(const std::string& word);
    
    /**
     * @brief Генерирует ключ для кэширования результатов проверки
     * @param word_text Проверяемое слово
     * @param patterns Список регулярных выражений
     * @param target_words Список запрещенных слов
     * @return Ключ для кэша
     */
    std::string _generate_cache_key(
        const std::string& word_text,
        const std::vector<std::string>& patterns,
        const std::vector<std::string>& target_words
    );
    
    /**
     * @brief Защита от слишком частых вызовов (брутфорс-атак)
     */
    void _throttle_check();
    
    /**
     * @brief Проверяет наличие отладчика
     * @return true если обнаружен отладчик, false в противном случае
     */
    bool _check_debugger();
    
    /**
     * @brief Проверяет, запущена ли программа как скомпилированный EXE
     * @return true если программа запущена как EXE, false в противном случае
     */
    bool _is_running_as_executable();
    
    /**
     * @brief Проверяет целостность исполняемого файла
     * @return true если файл не модифицирован, false в противном случае
     */
    bool _check_exe_integrity();
    
private:
    std::unordered_map<std::string, std::string> config;
    std::unordered_map<std::string, std::tuple<bool, std::string>> cache;
    int detection_count;
    std::chrono::system_clock::time_point _last_check_time;
    
    // Защита от быстрого перебора
    int _throttle_attempts;
    std::chrono::system_clock::time_point _throttle_start_time;
    
    // Регулярные выражения
    std::vector<std::regex> compiled_patterns;
};

} // namespace audiocensor

#endif // AUDIOCENSOR_WORD_DETECTOR_H