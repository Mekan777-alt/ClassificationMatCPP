#ifndef AUDIOCENSOR_CONFIG_MANAGER_H
#define AUDIOCENSOR_CONFIG_MANAGER_H

#include <QSettings>
#include <string>
#include <unordered_map>
#include <vector>

namespace audiocensor {

/**
 * @brief Класс для управления конфигурацией приложения
 */
class ConfigManager {
public:
    /**
     * @brief Конструктор
     */
    ConfigManager();
    
    /**
     * @brief Деструктор
     */
    ~ConfigManager();
    
    /**
     * @brief Возвращает текущую конфигурацию
     * @return Копия текущей конфигурации
     */
    std::unordered_map<std::string, std::string> get_config() const;
    
    /**
     * @brief Возвращает текущий список целевых слов
     * @return Список целевых слов
     */
    std::vector<std::string> get_target_words() const;
    
    /**
     * @brief Возвращает текущий список целевых паттернов
     * @return Список целевых паттернов
     */
    std::vector<std::string> get_target_patterns() const;
    
    /**
     * @brief Обновляет конфигурацию
     * @param config Новая конфигурация
     */
    void update_config(const std::unordered_map<std::string, std::string>& config);
    
    /**
     * @brief Обновляет список целевых слов
     * @param words Новый список целевых слов
     */
    void update_target_words(const std::vector<std::string>& words);
    
    /**
     * @brief Обновляет список целевых паттернов
     * @param patterns Новый список целевых паттернов
     */
    void update_target_patterns(const std::vector<std::string>& patterns);
    
    /**
     * @brief Сбрасывает конфигурацию к значениям по умолчанию
     */
    void reset_config();
    
private:
    /**
     * @brief Загружает конфигурацию по умолчанию
     * @return Конфигурация по умолчанию
     */
    std::unordered_map<std::string, std::string> _load_default_config();
    
    /**
     * @brief Загружает сохраненную конфигурацию
     */
    void _load_saved_config();
    
    /**
     * @brief Сохраняет текущую конфигурацию
     */
    void _save_config();
    
private:
    QSettings* settings;
    std::unordered_map<std::string, std::string> _config;
    std::vector<std::string> _target_words;
    std::vector<std::string> _target_patterns;
};

} // namespace audiocensor

#endif // AUDIOCENSOR_CONFIG_MANAGER_H