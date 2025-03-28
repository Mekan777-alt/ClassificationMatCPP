#include "audiocensor/word_detector.h"
#include "audiocensor/security.h"

#include <nlohmann/json.hpp>
#include <openssl/md5.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace audiocensor {

using json = nlohmann::json;

WordDetector::WordDetector(const std::unordered_map<std::string, std::string>& config)
    : config(config),
      detection_count(0),
      _last_check_time(std::chrono::system_clock::now()),
      _throttle_attempts(0),
      _throttle_start_time(std::chrono::system_clock::now()) {
}

std::vector<std::tuple<int, int, bool>> WordDetector::process_recognition_result(
    const std::string& result_json,
    int current_chunk_idx,
    double elapsed_time) {

    std::vector<std::tuple<int, int, bool>> censored_regions;

    // Проверка корректности входных данных
    if (result_json.empty()) {
        return censored_regions;
    }

    try {
        // Парсим JSON
        json result = json::parse(result_json);

        // Проверяем наличие поля "result"
        if (!result.contains("result") || !result["result"].is_array()) {
            return censored_regions;
        }

        auto words = result["result"];
        if (words.empty()) {
            return censored_regions;
        }

        // Получаем параметры из конфигурации
        int current_sample_rate = 16000; // По умолчанию
        int chunk_size = 1024;           // По умолчанию
        int safety_margin = 3;           // По умолчанию

        if (config.find("sample_rate") != config.end()) {
            current_sample_rate = std::stoi(config.at("sample_rate"));
        }
        if (config.find("chunk_size") != config.end()) {
            chunk_size = std::stoi(config.at("chunk_size"));
        }
        if (config.find("safety_margin") != config.end()) {
            safety_margin = std::stoi(config.at("safety_margin"));
        }
        double buffer_delay = 2.0; // По умолчанию
        if (config.find("buffer_delay") != config.end()) {
            buffer_delay = std::stod(config.at("buffer_delay"));
        }

        // Количество чанков в секунду
        double chunks_per_second = static_cast<double>(current_sample_rate) / chunk_size;

        // Обрабатываем каждое слово
        for (const auto& word : words) {
            // Проверяем формат данных
            if (!word.contains("word") || !word["word"].is_string()) {
                continue;
            }

            std::string word_text = word["word"];
            std::transform(word_text.begin(), word_text.end(), word_text.begin(),
                           [](unsigned char c){ return std::tolower(c); });

            // Пропускаем слишком короткие слова как шум
            if (word_text.length() < 3) {
                continue;
            }

            // Получаем внешние списки паттернов и слов
            std::vector<std::string> patterns;
            std::vector<std::string> target_words;

            if (config.find("target_patterns") != config.end()) {
                std::istringstream iss(config.at("target_patterns"));
                std::string pattern;
                while (std::getline(iss, pattern, ',')) {
                    patterns.push_back(pattern);
                }
            }

            if (config.find("target_words") != config.end()) {
                std::istringstream iss(config.at("target_words"));
                std::string tw;
                while (std::getline(iss, tw, ',')) {
                    target_words.push_back(tw);
                }
            }

            // Проверяем, является ли слово запрещенным
            bool is_prohibited;
            std::string matched_pattern;
            std::tie(is_prohibited, matched_pattern) = is_prohibited_word(word_text, patterns, target_words);

            if (is_prohibited) {
                // Получаем время начала и конца слова
                double start_time = word.contains("start") && word["start"].is_number() ? word["start"].get<double>() : 0;
                double end_time = word.contains("end") && word["end"].is_number() ? word["end"].get<double>() : 0;

                // Рассчитываем индексы чанков для цензуры с учетом текущей частоты дискретизации
                int chunks_offset_start = static_cast<int>((start_time - (elapsed_time - buffer_delay)) *
                                                      chunks_per_second) - safety_margin;
                int chunks_offset_end = static_cast<int>((end_time - (elapsed_time - buffer_delay)) *
                                                    chunks_per_second) + safety_margin;

                // Абсолютные индексы чанков для цензуры
                int censored_chunk_start = current_chunk_idx + chunks_offset_start;
                int censored_chunk_end = current_chunk_idx + chunks_offset_end;

                // Добавляем регион для цензуры
                censored_regions.push_back(std::make_tuple(censored_chunk_start, censored_chunk_end, false));
            }
        }

    } catch (const json::parse_error& e) {
        std::cerr << "Ошибка при разборе JSON: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Ошибка при обработке результатов распознавания: " << e.what() << std::endl;
    }

    return censored_regions;
}

std::string WordDetector::_normalize_word(const std::string& word) {
    if (word.empty()) {
        return "";
    }

    // Преобразуем к нижнему регистру и убираем пробелы в начале и конце
    std::string normalized = word;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c){ return std::tolower(c); });

    // Убираем пробелы в начале и конце
    normalized.erase(0, normalized.find_first_not_of(" \t\r\n"));
    normalized.erase(normalized.find_last_not_of(" \t\r\n") + 1);

    // Убираем повторяющиеся символы (например, "приииивет" -> "привет")
    std::string result;
    char prev_char = '\0';

    for (char c : normalized) {
        if (c != prev_char) {
            result += c;
            prev_char = c;
        } else {
            // Проверяем, является ли символ гласной
            bool is_vowel = (c == 'а' || c == 'е' || c == 'и' || c == 'о' || c == 'у' ||
                             c == 'ы' || c == 'э' || c == 'ю' || c == 'я');

            // Позволяем повторение гласных
            if (is_vowel) {
                result += c;
            }
        }
    }

    return result;
}

std::string WordDetector::_generate_cache_key(
    const std::string& word_text,
    const std::vector<std::string>& patterns,
    const std::vector<std::string>& target_words) {

    // Хешируем слово для уникальности
    unsigned char md[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(word_text.c_str()), word_text.length(), md);

    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md[i]);
    }
    std::string word_hash = ss.str();

    // Генерируем простую сигнатуру набора паттернов и целевых слов
    std::string patterns_signature = std::to_string(patterns.size());
    std::string words_signature = std::to_string(target_words.size());

    return word_hash + "_" + patterns_signature + "_" + words_signature;
}

void WordDetector::_throttle_check() {
    auto current_time = std::chrono::system_clock::now();

    // Сбрасываем счетчик, если прошло достаточно времени
    if (std::chrono::duration_cast<std::chrono::seconds>(current_time - _throttle_start_time).count() > 10) {
        _throttle_attempts = 0;
        _throttle_start_time = current_time;
    }

    // Увеличиваем счетчик попыток
    _throttle_attempts++;

    // Если слишком много попыток за короткий промежуток времени,
    // добавляем прогрессивную задержку
    if (_throttle_attempts > 100) {
        // Более 100 проверок за 10 секунд
        double delay_factor = std::min(0.5, (_throttle_attempts - 100) / 1000.0);  // Максимум 0.5 сек
        std::this_thread::sleep_for(std::chrono::duration<double>(delay_factor));
    }
}

bool WordDetector::check_periodic_integrity() {
    auto current_time = std::chrono::system_clock::now();

    // Проверяем не чаще раза в 30 секунд
    if (std::chrono::duration_cast<std::chrono::seconds>(current_time - _last_check_time).count() < 30) {
        return true;
    }

    _last_check_time = current_time;

    // Проверяем целостность основных компонентов
    try {
        // Проверка на отладчик
        if (_check_debugger()) {
            // В production-коде можно реализовать
            // механизм самозащиты приложения
            return false;
        }

        // Проверка исполняемого файла, если запущены как EXE
        if (_is_running_as_executable() && !_check_exe_integrity()) {
            return false;
        }

        // Дополнительные проверки можно добавить здесь

        return true;
    } catch (...) {
        // Любая ошибка при проверке целостности считается подозрительной
        return false;
    }
}

bool WordDetector::_check_debugger() {
    return security::detect_debugger();
}

bool WordDetector::_is_running_as_executable() {
    return security::is_running_as_executable();
}

bool WordDetector::_check_exe_integrity() {
    return security::check_executable_integrity();
}

void WordDetector::reset_cache() {
    cache.clear();
}

std::tuple<bool, std::string> WordDetector::is_prohibited_word(
    const std::string& word_text,
    const std::vector<std::string>& patterns,
    const std::vector<std::string>& target_words) {

    // Защита от частых вызовов для предотвращения брутфорса
    _throttle_check();

    // Нормализация входных данных
    std::string normalized_word = _normalize_word(word_text);

    // Если слово короткое, скорее всего это шум
    if (normalized_word.length() < 3) {
        return std::make_tuple(false, "");
    }

    // Используем кэш для ускорения повторных проверок
    std::string cache_key = _generate_cache_key(normalized_word, patterns, target_words);
    if (cache.find(cache_key) != cache.end()) {
        return cache[cache_key];
    }

    // Добавляем небольшую случайную задержку для защиты от тайминг-атак
    security::add_random_delay(5, 20);

    // Используем внешние паттерны и слова, если предоставлены
    std::vector<std::string> patterns_to_use = patterns;
    std::vector<std::string> target_words_to_use = target_words;

    if (patterns_to_use.empty() && config.find("target_patterns") != config.end()) {
        // Разбираем строку с паттернами из конфига
        std::istringstream iss(config.at("target_patterns"));
        std::string pattern;
        while (std::getline(iss, pattern, ',')) {
            patterns_to_use.push_back(pattern);
        }
    }

    if (target_words_to_use.empty() && config.find("target_words") != config.end()) {
        // Разбираем строку с целевыми словами из конфига
        std::istringstream iss(config.at("target_words"));
        std::string word;
        while (std::getline(iss, word, ',')) {
            target_words_to_use.push_back(word);
        }
    }

    // Проверяем по регулярным выражениям
    for (const auto& pattern_str : patterns_to_use) {
        try {
            std::regex pattern(pattern_str);
            if (std::regex_search(normalized_word, pattern)) {
                auto result = std::make_tuple(true, pattern_str);
                cache[cache_key] = result;
                detection_count++;
                return result;
            }
        } catch (const std::regex_error& e) {
            // В случае ошибки в регулярном выражении, пропускаем его
            std::cerr << "Ошибка в регулярном выражении " << pattern_str << ": " << e.what() << std::endl;
            continue;
        }
    }

    // Проверяем по точному совпадению с учетом возможных вариаций слова
    for (const auto& target : target_words_to_use) {
        std::string target_normalized = _normalize_word(target);

        // Точное совпадение
        if (normalized_word == target_normalized) {
            auto result = std::make_tuple(true, "точное совпадение");
            cache[cache_key] = result;
            detection_count++;
            return result;
        }

        // Проверка на вхождение в качестве подстроки для сложных слов
        if (normalized_word.length() > 5 && target_normalized.find(target_normalized) != std::string::npos) {
            auto result = std::make_tuple(true, "частичное совпадение");
            cache[cache_key] = result;
            detection_count++;
            return result;
        }
    }

    // Сохраняем отрицательный результат в кэше
    auto result = std::make_tuple(false, "");
    cache[cache_key] = result;
    return result;