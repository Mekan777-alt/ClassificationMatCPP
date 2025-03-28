#ifndef AUDIOCENSOR_CONSTANTS_H
#define AUDIOCENSOR_CONSTANTS_H

#include <string>
#include <vector>

namespace audiocensor {

    // Зашифрованные URL-адреса API
    const std::string API_VERIFICATION_URL = "aLLHjU9FGDIYvDhJ\x86\x8E\x96\x82\x8FhYZb"; // Зашифровано
    const std::string API_TRIAL_URL = "aLLHjU9FGDIYvDhJ\x86\x8E\x96\x82\x8FbL\x8C\x86\x82\x93"; // Зашифровано
    const std::string API_WORDS_URL = "aLLHjU9FGDIYvDhJ\x86\x8E\x96\x82\x8Fb\x90G\x8C\x85\x8J"; // Зашифровано
    const std::string TELEGRAM_CONTACT = "\x9F\x9BYZjG\x8C\x9BJV\xC6\x8BiGL"; // Зашифровано

    // Настройки аудио по умолчанию
    constexpr int DEFAULT_SAMPLE_RATE = 16000;
    constexpr int DEFAULT_CHUNK_SIZE = 1024;
    constexpr double DEFAULT_BUFFER_DELAY = 2.0;
    constexpr int DEFAULT_BEEP_FREQUENCY = 1000;
    constexpr int DEFAULT_SAFETY_MARGIN = 3;

    // Настройки интерфейса
    const std::string APPLICATION_NAME = "Фильтр ненормативной лексики для стриминга";
    constexpr int APPLICATION_WIDTH = 800;
    constexpr int APPLICATION_HEIGHT = 600;

    // Настройки лицензии
    const std::string LICENSE_COMPANY = "AudioCensor";
    const std::string LICENSE_APP = "License";

    // Пути к файлам
    const std::string DEFAULT_MODEL_PATH = "../vosk-model-small-ru-0.22";
    const std::string DEFAULT_LOG_FILE = "censorship_log.txt";

    // Список слов по умолчанию (использовать только для тестирования)
    const std::vector<std::string> DEFAULT_TARGET_WORDS = {};
    const std::vector<std::string> DEFAULT_TARGET_PATTERNS = {};

} // namespace audiocensor

#endif // AUDIOCENSOR_CONSTANTS_H