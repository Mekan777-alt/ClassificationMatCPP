#!/bin/bash
# Скрипт сборки для AudioCensor
# Поддерживает macOS, Linux и Windows (через MSYS2/MinGW)

set -e # Остановка при ошибке

# Определение текущей платформы
detect_platform() {
    if [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
        echo "linux"
    elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        echo "windows"
    else
        echo "unknown"
    fi
}

# Установка зависимостей
install_dependencies() {
    local platform=$1
    echo "🔍 Определение платформы: $platform"

    case $platform in
        macos)
            echo "📦 Установка зависимостей для macOS..."
            # Проверка наличия Homebrew
            if ! command -v brew &> /dev/null; then
                echo "❌ Homebrew не установлен. Установите его с помощью:"
                echo "/bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
                exit 1
            fi

            # Установка основных зависимостей
            brew install cmake || true
            brew install qt@6 || true
            brew install portaudio || true
            brew install openssl || true
            brew install curl || true
            brew install nlohmann-json || true

            # Пути для CMake
            QT_PATH=$(brew --prefix qt@6)
            OPENSSL_PATH=$(brew --prefix openssl)
            PORTAUDIO_PATH=$(brew --prefix portaudio)
            ;;

        linux)
            echo "📦 Установка зависимостей для Linux..."
            # Определение дистрибутива
            if command -v apt-get &> /dev/null; then
                # Debian/Ubuntu
                sudo apt-get update
                sudo apt-get install -y cmake build-essential
                sudo apt-get install -y qt6-base-dev libqt6widgets6 libqt6core6
                sudo apt-get install -y libportaudio2 libportaudio-dev
                sudo apt-get install -y libssl-dev
                sudo apt-get install -y libcurl4-openssl-dev
                sudo apt-get install -y nlohmann-json3-dev
            elif command -v dnf &> /dev/null; then
                # Fedora
                sudo dnf install -y cmake gcc-c++
                sudo dnf install -y qt6-qtbase-devel
                sudo dnf install -y portaudio-devel
                sudo dnf install -y openssl-devel
                sudo dnf install -y libcurl-devel
                sudo dnf install -y json-devel
            elif command -v pacman &> /dev/null; then
                # Arch Linux
                sudo pacman -Sy
                sudo pacman -S --noconfirm cmake base-devel
                sudo pacman -S --noconfirm qt6-base
                sudo pacman -S --noconfirm portaudio
                sudo pacman -S --noconfirm openssl
                sudo pacman -S --noconfirm curl
                sudo pacman -S --noconfirm nlohmann-json
            else
                echo "❌ Неподдерживаемый дистрибутив Linux"
                echo "Установите зависимости вручную:"
                echo "- CMake"
                echo "- Qt6"
                echo "- PortAudio"
                echo "- OpenSSL"
                echo "- libcurl"
                echo "- nlohmann/json"
                exit 1
            fi
            ;;

        windows)
            echo "📦 Установка зависимостей для Windows через MSYS2..."
            # Предполагается, что MSYS2 уже установлен
            pacman -Sy
            pacman -S --noconfirm mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc
            pacman -S --noconfirm mingw-w64-x86_64-qt6-base
            pacman -S --noconfirm mingw-w64-x86_64-portaudio
            pacman -S --noconfirm mingw-w64-x86_64-openssl
            pacman -S --noconfirm mingw-w64-x86_64-curl
            pacman -S --noconfirm mingw-w64-x86_64-nlohmann-json
            ;;

        *)
            echo "❌ Неподдерживаемая платформа: $platform"
            exit 1
            ;;
    esac

    echo "✅ Зависимости установлены"
}

# Скачивание модели Vosk
download_vosk_model() {
    echo "📥 Скачивание модели Vosk для русского языка..."

    mkdir -p resources/models
    cd resources/models

    # Проверка наличия модели
    if [ -d "vosk-model-small-ru-0.22" ]; then
        echo "✅ Модель Vosk уже скачана"
        cd ../..
        return
    fi

    # Скачивание модели
    local model_url="https://alphacephei.com/vosk/models/vosk-model-small-ru-0.22.zip"

    # Выбор утилиты для скачивания
    if command -v curl &> /dev/null; then
        curl -L -o vosk-model.zip "$model_url"
    elif command -v wget &> /dev/null; then
        wget -O vosk-model.zip "$model_url"
    else
        echo "❌ Для скачивания требуется curl или wget"
        cd ../..
        exit 1
    fi

    # Распаковка модели
    echo "📦 Распаковка модели..."
    if command -v unzip &> /dev/null; then
        unzip vosk-model.zip
        rm vosk-model.zip
    else
        echo "❌ Для распаковки требуется unzip"
        cd ../..
        exit 1
    fi

    cd ../..
    echo "✅ Модель Vosk скачана и распакована"
}

# Сборка проекта
build_project() {
    local platform=$1
    local build_type=$2

    echo "🔨 Сборка проекта (тип: $build_type)..."

    # Создание директории для сборки
    mkdir -p build
    cd build

    case $platform in
        macos)
            # Настройка CMake для macOS
            cmake .. \
                -DCMAKE_BUILD_TYPE=$build_type \
                -DCMAKE_PREFIX_PATH="$QT_PATH" \
                -DOPENSSL_ROOT_DIR="$OPENSSL_PATH" \
                -DPORTAUDIO_INCLUDE_DIRS="$PORTAUDIO_PATH/include" \
                -DPORTAUDIO_LIBRARIES="$PORTAUDIO_PATH/lib/libportaudio.dylib"
            ;;

        linux)
            # Настройка CMake для Linux
            cmake .. -DCMAKE_BUILD_TYPE=$build_type
            ;;

        windows)
            # Настройка CMake для Windows (MSYS2/MinGW)
            cmake .. -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=$build_type
            ;;
    esac

    # Компиляция
    if [ "$platform" = "windows" ] && command -v mingw32-make &> /dev/null; then
        mingw32-make -j$(nproc)
    else
        make -j$(nproc 2>/dev/null || echo 2)
    fi

    cd ..

    echo "✅ Сборка завершена"
}

# Копирование необходимых файлов
copy_dependencies() {
    local platform=$1

    echo "📋 Копирование зависимостей..."

    case $platform in
        macos)
            # Создание пакета приложения для macOS
            echo "🍎 Создание пакета приложения для macOS..."
            mkdir -p build/AudioCensor.app/Contents/MacOS
            mkdir -p build/AudioCensor.app/Contents/Resources

            # Копирование исполняемого файла
            cp build/audiocensor build/AudioCensor.app/Contents/MacOS/

            # Копирование моделей
            cp -r resources/models build/AudioCensor.app/Contents/Resources/

            # Создание Info.plist
            cat > build/AudioCensor.app/Contents/Info.plist << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>audiocensor</string>
    <key>CFBundleIconFile</key>
    <string>audiocensor.icns</string>
    <key>CFBundleIdentifier</key>
    <string>com.audiocensor.app</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>AudioCensor</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF
            ;;

        linux)
            # Копирование ресурсов для Linux
            mkdir -p build/resources
            cp -r resources/models build/resources/
            ;;

        windows)
            # Копирование DLL и ресурсов для Windows
            mkdir -p build/resources
            cp -r resources/models build/resources/

            # Копирование необходимых DLL для Windows
            if [ -d "/mingw64/bin" ]; then
                cp /mingw64/bin/Qt6Core.dll build/
                cp /mingw64/bin/Qt6Gui.dll build/
                cp /mingw64/bin/Qt6Widgets.dll build/
                cp /mingw64/bin/libportaudio-2.dll build/
                cp /mingw64/bin/libssl-1_1-x64.dll build/
                cp /mingw64/bin/libcrypto-1_1-x64.dll build/
                cp /mingw64/bin/libcurl-4.dll build/
            fi
            ;;
    esac

    echo "✅ Зависимости скопированы"
}

# Создание пакета для распространения
create_package() {
    local platform=$1

    echo "📦 Создание пакета для распространения..."

    case $platform in
        macos)
            # Создание DMG
            if command -v hdiutil &> /dev/null; then
                hdiutil create -volname "AudioCensor" -srcfolder build/AudioCensor.app -ov -format UDZO AudioCensor.dmg
                echo "✅ Создан пакет AudioCensor.dmg"
            else
                echo "⚠️ Не удалось создать DMG (hdiutil не найден)"
            fi
            ;;

        linux)
            # Создание архива tar.gz
            tar -czvf AudioCensor-linux.tar.gz -C build audiocensor resources
            echo "✅ Создан пакет AudioCensor-linux.tar.gz"
            ;;

        windows)
            # Создание архива ZIP
            if command -v zip &> /dev/null; then
                cd build
                zip -r ../AudioCensor-windows.zip audiocensor.exe *.dll resources
                cd ..
                echo "✅ Создан пакет AudioCensor-windows.zip"
            else
                echo "⚠️ Не удалось создать ZIP (zip не найден)"
            fi
            ;;
    esac
}

# Основная функция
main() {
    # Проверка аргументов
    local build_type="Release"
    local skip_deps=0
    local skip_model=0
    local create_pkg=0

    for arg in "$@"; do
        case $arg in
            --debug)
                build_type="Debug"
                ;;
            --skip-deps)
                skip_deps=1
                ;;
            --skip-model)
                skip_model=1
                ;;
            --package)
                create_pkg=1
                ;;
            --help)
                echo "Использование: $0 [опции]"
                echo "Опции:"
                echo "  --debug        Сборка в режиме отладки"
                echo "  --skip-deps    Пропустить установку зависимостей"
                echo "  --skip-model   Пропустить загрузку модели Vosk"
                echo "  --package      Создать пакет для распространения"
                echo "  --help         Показать эту справку"
                exit 0
                ;;
        esac
    done

    # Определение платформы
    local platform=$(detect_platform)
    echo "🖥️  Платформа: $platform"

    # Установка зависимостей
    if [ $skip_deps -eq 0 ]; then
        install_dependencies "$platform"
    else
        echo "📦 Пропуск установки зависимостей"
    fi

    # Скачивание модели Vosk
    if [ $skip_model -eq 0 ]; then
        download_vosk_model
    else
        echo "📥 Пропуск загрузки модели Vosk"
    fi

    # Сборка проекта
    build_project "$platform" "$build_type"

    # Копирование зависимостей
    copy_dependencies "$platform"

    # Создание пакета
    if [ $create_pkg -eq 1 ]; then
        create_package "$platform"
    fi

    echo "🎉 Сборка AudioCensor успешно завершена!"
    echo "▶️  Запустить программу: ./build/audiocensor"
}

# Запуск основной функции с переданными аргументами
main "$@"