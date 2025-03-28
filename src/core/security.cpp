#include "audiocensor/security.h"

#include <random>
#include <thread>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <openssl/sha.h>
#include <openssl/md5.h>

#ifdef _WIN32
#include <windows.h>
#include <tchar.h>
#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <process.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
#endif

namespace audiocensor {
namespace security {

// Константы для обфускации
constexpr unsigned char _XOR_KEY = 0x37;

std::string obscure_str(const std::string& s) {
    if (s.empty()) {
        return "";
    }
    
    std::string result;
    result.reserve(s.size());
    
    for (char c : s) {
        result.push_back(static_cast<char>(c ^ _XOR_KEY));
    }
    
    return result;
}

std::string deobscure_str(const std::string& s) {
    // XOR операция симметрична, поэтому используем ту же функцию
    return obscure_str(s);
}

int obscure_num(int n) {
    return (n ^ 0x1234) - 0x4321;
}

int deobscure_num(int n) {
    return (n + 0x4321) ^ 0x1234;
}

double add_random_delay(unsigned min_ms, unsigned max_ms) {
    // Инициализация генератора случайных чисел
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<double> dis(min_ms / 1000.0, max_ms / 1000.0);
    
    double delay = dis(gen);
    std::this_thread::sleep_for(std::chrono::duration<double>(delay));
    return delay;
}

bool detect_debugger() {
#ifdef _WIN32
    return IsDebuggerPresent() != 0;
#else
    // Для Linux можно проверить наличие процесса отладчика, но это сложнее
    // В упрощенном варианте возвращаем false
    return false;
#endif
}

bool detect_virtual_machine() {
    try {
#ifdef _WIN32
        // На Windows проверяем наличие характерных процессов и устройств
        // Это упрощенная версия, в реальном коде нужно больше проверок
        return false; // Упрощено для примера
#else
        // На Linux можно проверить /proc/cpuinfo и другие файлы
        return false; // Упрощено для примера
#endif
    } catch (...) {
        // При ошибке считаем, что это не VM
        return false;
    }
}

bool is_running_as_executable() {
#ifdef _WIN32
    TCHAR szFileName[MAX_PATH];
    GetModuleFileName(NULL, szFileName, MAX_PATH);
    std::filesystem::path path(szFileName);
    std::string ext = path.extension().string();
    return ext == ".exe";
#else
    // Упрощенная проверка для Linux
    return false; // Упрощено для примера
#endif
}

// Добавляем эту функцию
std::string md5_hash(const std::string& str) {
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5(reinterpret_cast<const unsigned char*>(str.c_str()), str.length(), digest);
    
    std::stringstream ss;
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    
    return ss.str();
}

std::string sha256_hash(const std::string& str) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(str.c_str()), str.length(), digest);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    
    return ss.str();
}

std::string calculate_file_hash(const std::string& file_path, size_t block_size) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        return "";
    }
    
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    
    std::vector<char> buffer(block_size);
    while (file) {
        file.read(buffer.data(), block_size);
        std::streamsize count = file.gcount();
        if (count > 0) {
            SHA256_Update(&sha256, buffer.data(), count);
        }
    }
    
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256_Final(digest, &sha256);
    
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(digest[i]);
    }
    
    return ss.str();
}

bool check_executable_integrity() {
    try {
        if (is_running_as_executable()) {
#ifdef _WIN32
            TCHAR szFileName[MAX_PATH];
            GetModuleFileName(NULL, szFileName, MAX_PATH);
            // Здесь в реальном коде нужно сравнивать с предварительно сохраненным хешем
            // В демо-версии просто возвращаем true
            return true;
#else
            // Для Linux упрощено
            return true;
#endif
        }
    } catch (...) {
        // При ошибке в безопасном режиме считаем, что все ОК
    }
    return true;
}

std::string get_hardware_salt() {
    try {
        std::vector<std::string> salt_components;
        
#ifdef _WIN32
        // Размер экрана
        int screen_width = GetSystemMetrics(SM_CXSCREEN);
        int screen_height = GetSystemMetrics(SM_CYSCREEN);
        salt_components.push_back(std::to_string(screen_width) + "x" + std::to_string(screen_height));
        
        // Добавляем название компьютера
        char computer_name[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(computer_name) / sizeof(computer_name[0]);
        if (GetComputerNameA(computer_name, &size)) {
            salt_components.push_back(std::string(computer_name));
        }
#else
        // Для Linux можно использовать uname и т.д.
        struct utsname buffer;
        if (uname(&buffer) == 0) {
            salt_components.push_back(buffer.nodename);
        }
#endif

        // Добавляем количество процессоров
        unsigned int cpu_count = std::thread::hardware_concurrency();
        salt_components.push_back(std::to_string(cpu_count));

        // Склеиваем все компоненты и хешируем
        std::string combined;
        for (const auto& comp : salt_components) {
            combined += comp;
        }
        
        return md5_hash(combined).substr(0, 16);
    } catch (...) {
        // Безопасное значение по умолчанию
        return "d5f7c8e3b2a1";
    }
}

std::string get_machine_id() {
    std::vector<std::string> machine_data;

#ifdef _WIN32
    // Имя компьютера
    char computer_name[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(computer_name) / sizeof(computer_name[0]);
    if (GetComputerNameA(computer_name, &size)) {
        machine_data.push_back(std::string(computer_name));
    }

    // В реальном коде здесь нужно больше идентификаторов устройства
    // Например, MAC-адрес, серийный номер CPU, серийный номер диска и т.д.
    // Это упрощенная версия
#else
    // Для Linux можно использовать различные идентификаторы
    struct utsname buffer;
    if (uname(&buffer) == 0) {
        machine_data.push_back(buffer.nodename);
    }
#endif

    // Если не удалось получить информацию, используем путь к домашней директории
    if (machine_data.empty()) {
        machine_data.push_back(md5_hash(std::filesystem::temp_directory_path().string()));
    }

    // Хешируем для получения стабильного ID
    std::string combined;
    for (const auto& item : machine_data) {
        if (!combined.empty()) {
            combined += ":";
        }
        combined += item;
    }
    
    std::string raw_id = sha256_hash(combined);
    
    // Добавляем соль для защиты от подбора
    std::string salted_id = raw_id + get_hardware_salt();
    return sha256_hash(salted_id);
}

bool validate_environment() {
    // Добавляем случайную задержку для защиты от анализа
    add_random_delay(10, 50);

    if (detect_debugger()) {
        // В production-коде здесь можно добавить "защитное" поведение,
        // например, случайное завершение или некорректную работу
        return false;
    }

    if (detect_virtual_machine()) {
        // Обнаружена виртуальная машина, но это не всегда проблема
        // Можно вести себя по-разному в зависимости от требований
        // В данном случае просто игнорируем
    }

    if (is_running_as_executable() && !check_executable_integrity()) {
        // Файл был модифицирован
        return false;
    }

    return true;
}

} // namespace security
} // namespace audiocensor