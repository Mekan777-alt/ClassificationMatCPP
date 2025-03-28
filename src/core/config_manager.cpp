#include "audiocensor/config_manager.h"
#include "audiocensor/constants.h"
#include "audiocensor/security.h"

#include <QSettings>
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>

namespace audiocensor {

using json = nlohmann::json;

ConfigManager::ConfigManager() {
    settings = new QSettings("AudioCensor", "Config");
    _config = _load_default_config();
    _load_saved_config();
}

ConfigManager::~ConfigManager() {
    delete settings;
}

std::unordered_map<std::string, std::string> ConfigManager::_load_default_config() {
    std::unordered_map<std::string, std::string> config;
    
    config["model_path"] = DEFAULT_MODEL_PATH;
    config["sample_rate"] = std::to_string(DEFAULT_SAMPLE_RATE);
    config["chunk_size"] = std::to_string(DEFAULT_CHUNK_SIZE);
    config["buffer_delay"] = std::to_string(DEFAULT_BUFFER_DELAY);
    config["beep_frequency"] = std::to_string(DEFAULT_BEEP_FREQUENCY);
    config["enable_censoring"] = "true";
    config["log_to_file"] = "false";
    config["log_file"] = DEFAULT_LOG_FILE;
    config["debug_mode"] = "false";
    config["safety_margin"] = std::to_string(DEFAULT_SAFETY_MARGIN);
    
    // Преобразуем дефолтные списки слов и паттернов в JSON строки
    _target_words = DEFAULT_TARGET_WORDS;
    _target_patterns = DEFAULT_TARGET_PATTERNS;
    
    config["target_words"] = json(_target_words).dump();
    config["target_patterns"] = json(_target_patterns).dump();
    
    return config;
}

void ConfigManager::_load_saved_config() {
    // Загружаем сохраненные настройки
    for (const auto& [key, value] : _config) {
        // Специальная обработка для списков
        if (key == "target_words" || key == "target_patterns") {
            QString saved_value = settings->value(QString::fromStdString(key), "").toString();
            if (!saved_value.isEmpty()) {
                try {
                    // Если значение было зашифровано
                    if (saved_value.startsWith("ENC:")) {
                        std::string encrypted = saved_value.mid(4).toStdString();
                        std::string decrypted = security::deobscure_str(encrypted);
                        _config[key] = decrypted;
                        
                        // Парсим JSON-строку в вектор
                        if (key == "target_words") {
                            _target_words = json::parse(decrypted).get<std::vector<std::string>>();
                        } else { // target_patterns
                            _target_patterns = json::parse(decrypted).get<std::vector<std::string>>();
                        }
                    } else {
                        // Для обратной совместимости со старыми версиями
                        std::string value_str = saved_value.toStdString();
                        _config[key] = value_str;
                        
                        // Парсим JSON-строку в вектор
                        if (key == "target_words") {
                            _target_words = json::parse(value_str).get<std::vector<std::string>>();
                        } else { // target_patterns
                            _target_patterns = json::parse(value_str).get<std::vector<std::string>>();
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Ошибка при разборе JSON для " << key << ": " << e.what() << std::endl;
                    // Если не удалось декодировать, используем значение по умолчанию
                }
            }
        } else {
            QVariant saved_value = settings->value(QString::fromStdString(key), "");
            if (!saved_value.isNull()) {
                _config[key] = saved_value.toString().toStdString();
            }
        }
    }
}

std::unordered_map<std::string, std::string> ConfigManager::get_config() const {
    return _config;
}

std::vector<std::string> ConfigManager::get_target_words() const {
    return _target_words;
}

std::vector<std::string> ConfigManager::get_target_patterns() const {
    return _target_patterns;
}

void ConfigManager::update_config(const std::unordered_map<std::string, std::string>& config) {
    for (const auto& [key, value] : config) {
        _config[key] = value;
    }
    _save_config();
}

void ConfigManager::update_target_words(const std::vector<std::string>& words) {
    _target_words = words;
    _config["target_words"] = json(words).dump();
    _save_config();
}

void ConfigManager::update_target_patterns(const std::vector<std::string>& patterns) {
    _target_patterns = patterns;
    _config["target_patterns"] = json(patterns).dump();
    _save_config();
}

void ConfigManager::_save_config() {
    for (const auto& [key, value] : _config) {
        // Шифруем списки перед сохранением
        if (key == "target_words" || key == "target_patterns") {
            std::string encrypted = "ENC:" + security::obscure_str(value);
            settings->setValue(QString::fromStdString(key), QString::fromStdString(encrypted));
        } else {
            settings->setValue(QString::fromStdString(key), QString::fromStdString(value));
        }
    }
    settings->sync();
}

void ConfigManager::reset_config() {
    _config = _load_default_config();
    _target_words = DEFAULT_TARGET_WORDS;
    _target_patterns = DEFAULT_TARGET_PATTERNS;
    _save_config();
}

} // namespace audiocensor

