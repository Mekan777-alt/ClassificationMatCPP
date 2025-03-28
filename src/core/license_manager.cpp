#include "audiocensor/license_manager.h"
#include "audiocensor/security.h"
#include "audiocensor/constants.h"

#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QDateTime>

#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <openssl/md5.h>
#include <openssl/sha.h>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <random>
#include <filesystem>

namespace audiocensor {

using json = nlohmann::json;

// Функция для работы с cURL
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t realsize = size * nmemb;
    output->append(static_cast<char*>(contents), realsize);
    return realsize;
}

LicenseManager::LicenseManager(QObject* parent)
    : QObject(parent) {
    
    // Проверка целостности и окружения
    _check_environment();
    
    // Инициализация настроек
    settings = new QSettings(QString::fromStdString(LICENSE_COMPANY), 
                            QString::fromStdString(LICENSE_APP));
    
    // Загрузка сохраненных значений
    license_key = settings->value("license_key", "").toString().toStdString();
    expiry_date = settings->value("expiry_date", "").toString().toStdString();
    
    // Дешифруем URL при инициализации
    verification_url = security::deobscure_str(API_VERIFICATION_URL);
    trial_url = security::deobscure_str(API_TRIAL_URL);
    words_api_url = security::deobscure_str(API_WORDS_URL);
    telegram_contact = security::deobscure_str(TELEGRAM_CONTACT);
    
    // Добавляем небольшую задержку для защиты от автоматизированных атак
    security::add_random_delay(50, 100); // 50-100 мс
    
    // Инициализация cURL
    curl_global_init(CURL_GLOBAL_ALL);
}

LicenseManager::~LicenseManager() {
    delete settings;
    curl_global_cleanup();
}

void LicenseManager::_check_environment() {
    // Защита от отладки
    if (security::detect_debugger()) {
        // В release-версии здесь можно добавить "защитное" поведение,
        // например, случайное завершение или некорректная работа
    }
    
    // Проверка целостности
    if (!security::check_executable_integrity()) {
        // В release-версии здесь можно добавить "защитное" поведение
    }
}

void LicenseManager::reset_all_license_data() {
    license_key = "";
    expiry_date = "";
    
    // Очищаем все сохраненные значения
    settings->remove("license_key");
    settings->remove("expiry_date");
    // Удаляем устаревшее значение, если оно существует
    settings->remove("first_run_date");
    settings->sync();
}

std::pair<bool, std::string> LicenseManager::activate_trial_period() {
    try {
        // Защитная задержка
        security::add_random_delay(50, 100);
        
        // Получаем machine_id для идентификации устройства
        std::string machine_id = security::get_machine_id();
        
        // Подготавливаем данные для запроса с добавлением уникального идентификатора
        json payload;
        payload["machine_id"] = machine_id;
        payload["timestamp"] = static_cast<int>(std::time(nullptr));
        payload["request_id"] = _generate_request_id();
        
        // Преобразуем JSON в строку
        std::string payload_str = payload.dump();
        
        // Выполняем запрос к API с помощью cURL
        CURL* curl = curl_easy_init();
        if (!curl) {
            return {false, "Ошибка инициализации cURL"};
        }
        
        std::string response_data;
        
        curl_easy_setopt(curl, CURLOPT_URL, trial_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload_str.length());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        // Устанавливаем заголовки
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        CURLcode res = curl_easy_perform(curl);
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            return {false, std::string("Ошибка cURL: ") + curl_easy_strerror(res)};
        }
        
        if (http_code == 200) {
            try {
                // Парсим JSON ответ
                json data = json::parse(response_data);
                
                if (data["status"] == "valid") {
                    // Проверяем полученные данные
                    std::unordered_map<std::string, std::string> license_data;
                    license_data["license_key"] = data["license_key"];
                    license_data["expiry_date"] = data["expiry_date"];
                    
                    if (!_validate_license_data(license_data)) {
                        return {false, "Получены некорректные данные лицензии"};
                    }
                    
                    // Сохраняем полученный ключ и дату окончания
                    license_key = data["license_key"];
                    expiry_date = data["expiry_date"];
                    
                    // Сохраняем в настройки
                    settings->setValue("license_key", QString::fromStdString(license_key));
                    settings->setValue("expiry_date", QString::fromStdString(expiry_date));
                    settings->sync();
                    
                    return {true, "Тестовая лицензия успешно активирована"};
                } else {
                    // Обработка ошибок от сервера
                    std::string error_message = data.contains("message") ? 
                                               data["message"] : "Неизвестная ошибка";
                    std::string error_code = data.contains("error_code") ? 
                                            data["error_code"] : "UNKNOWN_ERROR";
                    
                    if (error_code == "TRIAL_ALREADY_USED") {
                        return {false, "Это устройство уже использовало тестовый период"};
                    } else if (error_code == "NO_KEYS_AVAILABLE") {
                        return {false, "Нет доступных тестовых ключей. Попробуйте позже."};
                    } else {
                        return {false, error_message};
                    }
                }
            } catch (const json::parse_error& e) {
                return {false, std::string("Ошибка разбора JSON: ") + e.what()};
            }
        } else {
            return {false, "Ошибка сервера: " + std::to_string(http_code)};
        }
    } catch (const std::exception& e) {
        return {false, std::string("Ошибка при получении тестового ключа: ") + e.what()};
    }
}

bool LicenseManager::_validate_license_data(const std::unordered_map<std::string, std::string>& data) {
    if (data.empty()) {
        return false;
    }
    
    // Проверяем наличие необходимых полей
    if (data.find("license_key") == data.end() || data.find("expiry_date") == data.end()) {
        return false;
    }
    
    // Проверяем формат ключа (должен соответствовать определенному шаблону)
    const std::string& license_key = data.at("license_key");
    if (license_key.empty() || license_key.length() < 10) { // упрощенная проверка
        return false;
    }
    
    // Проверяем формат даты
    try {
        const std::string& expiry_date_str = data.at("expiry_date");
        std::tm tm = {};
        std::istringstream ss(expiry_date_str);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) {
            return false;
        }
    } catch (...) {
        return false;
    }
    
    return true;
}

std::string LicenseManager::_generate_request_id() {
    // Генерирует уникальный идентификатор запроса
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0, 1);
    
    std::stringstream ss;
    ss << std::time(nullptr) << dis(gen);
    
    unsigned char md[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(ss.str().c_str()), ss.str().length(), md);
    
    std::stringstream result;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        result << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md[i]);
    }
    
    return result.str();
}

bool LicenseManager::check_trial_period() {
    // Защитная задержка
    security::add_random_delay(50, 100);
    
    if (license_key.empty() || expiry_date.empty()) {
        return false;
    }
    
    try {
        std::tm tm = {};
        std::istringstream ss(expiry_date);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) {
            return false;
        }
        
        std::time_t expiry_time = std::mktime(&tm);
        std::time_t now = std::time(nullptr);
        
        return now <= expiry_time;
    } catch (...) {
        return false;
    }
}

std::unordered_map<std::string, std::string> LicenseManager::get_license_status() {
    // Защитная задержка
    security::add_random_delay(50, 100);
    
    std::unordered_map<std::string, std::string> status;
    status["has_license"] = "false";
    status["license_key"] = "";
    status["expiry_date"] = "";
    status["days_remaining"] = "0";
    status["trial_active"] = "false";
    status["status_text"] = "Нет лицензии";
    
    // Проверяем лицензию
    if (!license_key.empty() && !expiry_date.empty()) {
        status["has_license"] = "true";
        status["license_key"] = license_key;
        status["expiry_date"] = expiry_date;
        
        try {
            std::tm expiry_tm = {};
            std::istringstream ss(expiry_date);
            ss >> std::get_time(&expiry_tm, "%Y-%m-%d");
            if (!ss.fail()) {
                std::time_t expiry_time = std::mktime(&expiry_tm);
                std::time_t now = std::time(nullptr);
                
                // Расчет оставшихся дней
                int days_remaining = static_cast<int>((expiry_time - now) / (60 * 60 * 24));
                status["days_remaining"] = std::to_string(std::max(0, days_remaining));
                
                // Определяем, тестовая ли это лицензия
                bool is_trial = _is_trial_key(license_key, expiry_date);
                status["trial_active"] = is_trial ? "true" : "false";
                
                if (days_remaining > 0) {
                    if (is_trial) {
                        status["status_text"] = "Тестовая лицензия активна, осталось " + 
                                                std::to_string(days_remaining) + " дней";
                    } else {
                        status["status_text"] = "Лицензия активна, осталось " + 
                                                std::to_string(days_remaining) + " дней";
                    }
                } else {
                    status["status_text"] = "Лицензия истекла";
                    status["has_license"] = "false";
                }
            } else {
                status["status_text"] = "Ошибка проверки лицензии";
            }
        } catch (const std::exception& e) {
            std::cerr << "Ошибка при расчете оставшихся дней лицензии: " << e.what() << std::endl;
            status["status_text"] = "Ошибка проверки лицензии";
        }
    }
    
    return status;
}

bool LicenseManager::_is_trial_key(const std::string& key, const std::string& expiry_date_str) {
    // Защитная задержка
    security::add_random_delay(50, 100);
    
    // Пример определения по сроку действия
    if (key.empty() || expiry_date_str.empty()) {
        return false;
    }
    
    try {
        std::tm expiry_tm = {};
        std::istringstream ss(expiry_date_str);
        ss >> std::get_time(&expiry_tm, "%Y-%m-%d");
        if (ss.fail()) {
            return false;
        }
        
        std::time_t expiry_time = std::mktime(&expiry_tm);
        
        // Предполагаем, что лицензия была выдана за 10 дней до истечения
        std::time_t issued_time = expiry_time - (10 * 24 * 60 * 60); // 10 дней
        
        // Текущее время
        std::time_t now = std::time(nullptr);
        
        // Если срок действия около 10 дней от даты выдачи, считаем тестовой
        int license_days = static_cast<int>((expiry_time - issued_time) / (24 * 60 * 60));
        
        if (std::abs(license_days - 10) <= 1) {
            return true;
        }
        // Если срок около 30 дней, считаем полной
        else if (std::abs(license_days - 30) <= 1) {
            return false;
        }
    } catch (...) {
        // При ошибке считаем полной лицензией
    }
    
    // По умолчанию считаем полной лицензией
    return false;
}

bool LicenseManager::save_license(const std::string& key, const std::string& expiry_date_str) {
    // Защитная задержка
    security::add_random_delay(50, 100);
    
    // Проверяем данные перед сохранением
    if (key.empty() || expiry_date_str.empty()) {
        return false;
    }
    
    try {
        // Проверяем формат даты
        std::tm tm = {};
        std::istringstream ss(expiry_date_str);
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (ss.fail()) {
            return false;
        }
    } catch (...) {
        return false;
    }
    
    this->license_key = key;
    this->expiry_date = expiry_date_str;
    settings->setValue("license_key", QString::fromStdString(key));
    settings->setValue("expiry_date", QString::fromStdString(expiry_date_str));
    settings->sync();
    return true;
}

bool LicenseManager::has_valid_license() {
    // Защитная задержка
    security::add_random_delay(50, 100);
    
    // Проверяем окружение
    _check_environment();
    
    // Проверяем наличие лицензионного ключа
    if (!license_key.empty() && !expiry_date.empty()) {
        try {
            std::tm expiry_tm = {};
            std::istringstream ss(expiry_date);
            ss >> std::get_time(&expiry_tm, "%Y-%m-%d");
            if (ss.fail()) {
                return verify_license_online(license_key);
            }
            
            std::time_t expiry_time = std::mktime(&expiry_tm);
            std::time_t now = std::time(nullptr);
            
            if (now <= expiry_time) {
                return true; // Лицензия действительна
            } else {
                // Попытка обновить онлайн
                return verify_license_online(license_key);
            }
        } catch (const std::exception& e) {
            std::cerr << "Ошибка проверки лицензии: " << e.what() << std::endl;
            return verify_license_online(license_key);
        }
    }
    
    // Нет действующей лицензии
    return false;
}

bool LicenseManager::verify_license_online(const std::string& key) {
    try {
        // Защитная задержка
        security::add_random_delay(50, 100);
        
        // Создаем уникальный хэш для устройства
        std::string machine_id = security::get_machine_id();
        
        // Отправляем запрос на сервер с дополнительными проверками
        json payload;
        payload["license_key"] = key;
        payload["machine_id"] = machine_id;
        payload["timestamp"] = static_cast<int>(std::time(nullptr));
        payload["request_id"] = _generate_request_id();
        payload["app_signature"] = _generate_app_signature();
        
        std::string payload_str = payload.dump();
        
        // Выполняем запрос к API с помощью cURL
        CURL* curl = curl_easy_init();
        if (!curl) {
            return false;
        }
        
        std::string response_data;
        
        curl_easy_setopt(curl, CURLOPT_URL, verification_url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload_str.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, payload_str.length());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
        
        // Устанавливаем заголовки
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        CURLcode res = curl_easy_perform(curl);
        
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            // При ошибке соединения используем локальные данные
            return !license_key.empty() && !expiry_date.empty();
        }
        
        if (http_code == 200) {
            try {
                json data = json::parse(response_data);
                
                if (data["status"] == "valid") {
                    // Проверяем полученные данные
                    std::unordered_map<std::string, std::string> license_data;
                    license_data["license_key"] = data["license_key"];
                    license_data["expiry_date"] = data["expiry_date"];
                    
                    if (!_validate_license_data(license_data)) {
                        return false;
                    }
                    
                    // Обновляем локальные данные
                    save_license(key, data["expiry_date"]);
                    return true;
                } else {
                    // Очищаем недействительную лицензию
                    if (data["status"] == "expired" || data["status"] == "invalid") {
                        clear_license();
                    }
                    return false;
                }
            } catch (const json::parse_error& e) {
                // При ошибке разбора JSON используем локальные данные
                return !license_key.empty() && !expiry_date.empty();
            }
        } else {
            // При ошибке сервера используем локальные данные, если они есть
            return !license_key.empty() && !expiry_date.empty();
        }
    } catch (const std::exception& e) {
        std::cerr << "Ошибка при проверке лицензии онлайн: " << e.what() << std::endl;
        // При ошибке соединения используем локальные данные
        return !license_key.empty() && !expiry_date.empty();
    }
}

std::string LicenseManager::_generate_app_signature() {
    try {
        if (security::is_running_as_executable()) {
            std::string executable_path;
            
            #ifdef _WIN32
            char buffer[MAX_PATH];
            GetModuleFileNameA(NULL, buffer, MAX_PATH);
            executable_path = buffer;
            #else
            char buffer[PATH_MAX];
            ssize_t count = readlink("/proc/self/exe", buffer, PATH_MAX);
            if (count != -1) {
                executable_path = std::string(buffer, count);
            }
            #endif
            
            // Берем первые 1024 байта для ускорения
            std::ifstream file(executable_path, std::ios::binary);
            if (file) {
                char buffer[1024];
                file.read(buffer, 1024);
                std::streamsize readBytes = file.gcount();
                
                unsigned char md[MD5_DIGEST_LENGTH];
                MD5(reinterpret_cast<const unsigned char*>(buffer), readBytes, md);
                
                std::stringstream ss;
                for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
                    ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(md[i]);
                }
                
                return ss.str();
            }
            
            // Если не удалось прочитать файл, используем запасной вариант
            return security::md5_hash("dev_mode_" + std::to_string(getpid()));
        }
        
        // В режиме разработки генерируем подпись на основе PID
        return security::md5_hash("dev_mode_" + std::to_string(getpid()));
        
    } catch (...) {
        // При любой ошибке генерируем подпись на основе времени
        return security::md5_hash("fallback_" + std::to_string(std::time(nullptr)));
    }
}

void LicenseManager::clear_license() {
    license_key = "";
    expiry_date = "";
    settings->remove("license_key");
    settings->remove("expiry_date");
    settings->sync();
}

std::string LicenseManager::get_words_api_url() {
    return words_api_url;
}

bool LicenseManager::verify_api_response(const std::string& data_json, const std::string& machine_id) {
    try {
        json data = json::parse(data_json);
        
        // Проверяем наличие подписи в ответе
        if (!data.contains("signature")) {
            return true; // Для обратной совместимости
        }
        
        // Формируем строку для проверки (без поля signature)
        json verification_data = data;
        verification_data.erase("signature");
        
        // Создаем строку для проверки в том же формате, что и на сервере
        std::string check_str = verification_data.dump(4, ' ', true, json::error_handler_t::ignore) + machine_id;
        
        // Вычисляем ожидаемую подпись
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(check_str.c_str()), check_str.length(), hash);
        
        std::stringstream ss;
        for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        std::string expected_signature = ss.str();
        
        // Сравниваем подписи
        return data["signature"] == expected_signature;
        
    } catch (...) {
        // При любой ошибке считаем ответ подозрительным
        return false;
    }
}