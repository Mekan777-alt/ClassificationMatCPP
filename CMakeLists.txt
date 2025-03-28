cmake_minimum_required(VERSION 3.14)
project(AudioCensor VERSION 1.0.0 LANGUAGES CXX)

# Установка стандарта C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Включаем автоматическую обработку MOC, UIC и RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Директории с исходными файлами
set(SOURCE_DIR src)
set(INCLUDE_DIR include)
set(RESOURCES_DIR tools/resources)

# Поиск необходимых пакетов
find_package(Qt6 COMPONENTS Core Widgets REQUIRED)
find_package(OpenSSL QUIET)
if(NOT OpenSSL_FOUND)
    message(WARNING "OpenSSL not found, building without SSL support")
    # Определяем флаг для компиляции без OpenSSL
    add_definitions(-DNO_SSL_SUPPORT)
endif()
find_package(CURL REQUIRED)
find_package(PortAudio REQUIRED)

# Найти json библиотеку (nlohmann/json)
find_package(nlohmann_json CONFIG REQUIRED)

# Поиск Vosk
find_library(VOSK_LIBRARY vosk REQUIRED)
find_path(VOSK_INCLUDE_DIR vosk_api.h REQUIRED)

# Определение исходных файлов
set(CORE_SOURCES
        ${SOURCE_DIR}/core/security.cpp
        ${SOURCE_DIR}/core/word_detector.cpp
        ${SOURCE_DIR}/core/license_manager.cpp
        ${SOURCE_DIR}/core/audio_processor.cpp
        ${SOURCE_DIR}/core/config_manager.cpp
)

set(UI_SOURCES
        ${SOURCE_DIR}/ui/main_window.cpp
        ${SOURCE_DIR}/ui/license_dialog.cpp
        ${SOURCE_DIR}/ui/obs_dialog.cpp
)

set(APP_SOURCES
        ${SOURCE_DIR}/main.cpp
        ${CORE_SOURCES}
        ${UI_SOURCES}
        include/audiocensor/audio_processor.h
)

# Включение директорий заголовочных файлов
include_directories(${INCLUDE_DIR})
include_directories(${VOSK_INCLUDE_DIR})

# Создание исполняемого файла
add_executable(audiocensor ${APP_SOURCES})

# Связывание с библиотеками
target_link_libraries(audiocensor PRIVATE
        Qt6::Core
        Qt6::Widgets
        OpenSSL::SSL
        OpenSSL::Crypto
        CURL::libcurl
        ${PORTAUDIO_LIBRARIES}
        ${VOSK_LIBRARY}
        nlohmann_json::nlohmann_json
)

# Установка директорий include для цели
target_include_directories(audiocensor PRIVATE
        ${INCLUDE_DIR}
        ${PORTAUDIO_INCLUDE_DIRS}
)

# Копирование модели Vosk и других ресурсов при сборке
add_custom_command(TARGET audiocensor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/resources/models
        $<TARGET_FILE_DIR:audiocensor>/models
)

# Установка для Windows: копирование DLL
if(WIN32)
    # Получить путь к Qt DLL
    get_target_property(QT_CORE_DLL Qt6::Core LOCATION)
    get_target_property(QT_WIDGETS_DLL Qt6::Widgets LOCATION)
    get_filename_component(QT_BIN_DIR ${QT_CORE_DLL} DIRECTORY)

    # Копировать Qt DLL в папку с приложением
    add_custom_command(TARGET audiocensor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
            ${QT_CORE_DLL}
            ${QT_WIDGETS_DLL}
            $<TARGET_FILE_DIR:audiocensor>
    )

    # Копировать другие необходимые DLL при необходимости
endif()

# Настройка инсталляции
install(TARGETS audiocensor DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/resources/models DESTINATION bin)