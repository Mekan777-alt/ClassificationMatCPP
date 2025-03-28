#ifndef AUDIOCENSOR_SECURITY_H
#define AUDIOCENSOR_SECURITY_H

#include <string>
#include <chrono>

namespace audiocensor {
namespace security {

/**
 * @brief Обфусцирует строковое значение
 * @param s Исходная строка
 * @return Обфусцированная строка
 */
std::string obscure_str(const std::string& s);

/**
 * @brief Восстанавливает обфусцированное строковое значение
 * @param s Обфусцированная строка
 * @return Исходная строка
 */
std::string deobscure_str(const std::string& s);

/**
 * @brief Обфусцирует числовое значение
 * @param n Исходное число
 * @return Обфусцированное число
 */
int obscure_num(int n);

/**
 * @brief Восстанавливает обфусцированное числовое значение
 * @param n Обфусцированное число
 * @return Исходное число
 */
int deobscure_num(int n);

/**
 * @brief Добавляет небольшую случайную задержку для защиты от тайминг-атак
 * @param min_ms Минимальная задержка в миллисекундах
 * @param max_ms Максимальная задержка в миллисекундах
 * @return Фактическая задержка в секундах
 */
double add_random_delay(unsigned min_ms = 1, unsigned max_ms = 10);

/**
 * @brief Обнаруживает наличие отладчика
 * @return true если приложение запущено под отладчиком, false в противном случае
 */
bool detect_debugger();

/**
 * @brief Пытается определить запуск в виртуальной машине
 * @return true если приложение запущено в виртуальной машине, false в противном случае
 */
bool detect_virtual_machine();

/**
 * @brief Проверяет, запущена ли программа как скомпилированный EXE
 * @return true если приложение запущено как EXE, false в противном случае
 */
bool is_running_as_executable();

/**
 * @brief Вычисляет хеш файла
 * @param file_path Путь к файлу
 * @param block_size Размер блока для чтения
 * @return Хеш в формате SHA-256
 */
std::string calculate_file_hash(const std::string& file_path, size_t block_size = 65536);

/**
 * @brief Проверяет целостность исполняемого файла
 * @return true если файл не модифицирован, false в противном случае
 */
bool check_executable_integrity();

/**
 * @brief Создает уникальный идентификатор устройства
 * @return Уникальный ID устройства
 */
std::string get_machine_id();

/**
 * @brief Генерирует соль на основе аппаратных характеристик
 * @return Строка с солью
 */
std::string get_hardware_salt();

/**
 * @brief Комплексная проверка окружения
 * @return true если окружение безопасно, false в противном случае
 */
bool validate_environment();

} // namespace security
} // namespace audiocensor

/**
 * @brief Вычисляет MD5 хеш строки
 * @param str Исходная строка
 * @return MD5 хеш в виде строки в шестнадцатеричном формате
 */
std::string md5_hash(const std::string& str);

/**
 * @brief Вычисляет SHA-256 хеш строки
 * @param str Исходная строка
 * @return SHA-256 хеш в виде строки в шестнадцатеричном формате
 */
std::string sha256_hash(const std::string& str);

#endif // AUDIOCENSOR_SECURITY_H