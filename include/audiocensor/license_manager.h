#ifndef AUDIOCENSOR_LICENSE_MANAGER_H
#define AUDIOCENSOR_LICENSE_MANAGER_H

#include <string>
#include <unordered_map>
#include <memory>

#include <QObject>
#include <QSettings>

namespace audiocensor {

/**
 * @brief Менеджер лицензий для приложения аудиоцензора с улучшенной защитой
 */
class LicenseManager : public QObject {
    Q_OBJECT
    
public:
    /**
     * @brief Конструктор
     * @param parent Родительский объект
     */
    explicit LicenseManager(QObject* parent = nullptr);
    
    /**
     * @brief Деструктор
     */
    ~LicenseManager();
    
    /**
     * @brief Полностью очищает все данные о лицензии
     */
    void reset_all_license_data();
    
    /**
     * @brief Активирует пробный период, запрашивая ключ с сервера
     * @return Пара (успех, сообщение)
     */
    std::pair<bool, std::string> activate_trial_period();
    
    /**
     * @brief Сохраняет лицензионный ключ и дату окончания
     * @param key Лицензионный ключ
     * @param expiry_date Дата окончания лицензии в формате YYYY-MM-DD
     * @return true если данные сохранены успешно, false в противном случае
     */
    bool save_license(const std::string& key, const std::string& expiry_date);
    
    /**
     * @brief Проверяет, является ли текущая лицензия тестовой
     * @return true если лицензия тестовая, false в противном случае
     */
    bool check_trial_period();
    
    /**
     * @brief Возвращает полную информацию о статусе лицензии
     * @return Структура с информацией о лицензии
     */
    std::unordered_map<std::string, std::string> get_license_status();
    
    /**
     * @brief Проверяет наличие действующей лицензии
     * @return true если лицензия действительна, false в противном случае
     */
    bool has_valid_license();
    
    /**
     * @brief Проверяет лицензионный ключ онлайн через API
     * @param key Лицензионный ключ для проверки
     * @return true если ключ действителен, false в противном случае
     */
    bool verify_license_online(const std::string& key);
    
    /**
     * @brief Очищает сохраненную лицензию
     */
    void clear_license();
    
    /**
     * @brief Возвращает URL API для получения слов
     * @return URL API
     */
    std::string get_words_api_url();
    
    /**
     * @brief Проверяет подлинность ответа API
     * @param data Данные ответа
     * @param machine_id Идентификатор устройства
     * @return true если ответ подлинный, false в противном случае
     */
    bool verify_api_response(const std::string& data_json, const std::string& machine_id);
    
    /**
     * @brief Возвращает текущий лицензионный ключ
     * @return Лицензионный ключ
     */
    std::string get_license_key() const { return license_key; }
    
    /**
     * @brief Возвращает дату окончания лицензии
     * @return Дата окончания в формате YYYY-MM-DD
     */
    std::string get_expiry_date() const { return expiry_date; }
    
    /**
     * @brief Возвращает контакт Telegram
     * @return Контакт Telegram
     */
    std::string get_telegram_contact() const { return telegram_contact; }
    
private:
    /**
     * @brief Проверяет окружение запуска приложения
     */
    void _check_environment();
    
    /**
     * @brief Проверяет корректность данных лицензии
     * @param data Данные лицензии
     * @return true если данные корректны, false в противном случае
     */
    bool _validate_license_data(const std::unordered_map<std::string, std::string>& data);
    
    /**
     * @brief Генерирует уникальный идентификатор запроса
     * @return Идентификатор запроса
     */
    std::string _generate_request_id();
    
    /**
     * @brief Определяет, является ли ключ тестовым
     * @param key Лицензионный ключ
     * @param expiry_date Дата окончания лицензии
     * @return true если ключ тестовый, false в противном случае
     */
    bool _is_trial_key(const std::string& key, const std::string& expiry_date);
    
    /**
     * @brief Генерирует подпись приложения для дополнительной проверки
     * @return Подпись приложения
     */
    std::string _generate_app_signature();
    
private:
    QSettings* settings;
    std::string license_key;
    std::string expiry_date;
    
    // Расшифрованные URL
    std::string verification_url;
    std::string trial_url;
    std::string words_api_url;
    std::string telegram_contact;
};

} // namespace audiocensor

#endif // AUDIOCENSOR_LICENSE_MANAGER_H