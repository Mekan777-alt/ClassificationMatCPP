#include "ui/main_window.h"
#include "audiocensor/license_manager.h"
#include "audiocensor/config_manager.h"
#include "audiocensor/audio_processor.h"
#include "audiocensor/security.h"
#include "audiocensor/constants.h"
#include "ui/license_dialog.h"
#include "ui/obs_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>
#include <QScrollBar> // Добавлено для QScrollBar
#include <QCloseEvent> // Добавлено для QCloseEvent

#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include <iostream>
#include <fstream>
#include <chrono>
#include <random>

namespace audiocensor {

using json = nlohmann::json;

// Функция для callback cURL
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t realsize = size * nmemb;
    output->append(static_cast<char*>(contents), realsize);
    return realsize;
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      running(false),
      input_device_index(-1),
      output_device_index(-1),
      detections_count(0)
{
    // Инициализация менеджеров
    license_manager = std::make_unique<LicenseManager>();
    config_manager = std::make_unique<ConfigManager>();

    // Инициализация аудио процессора
    auto config = config_manager->get_config();
    audio_processor = std::make_unique<AudioProcessor>(config);

    // Инициализация интеграций
    setup_integration_menu();
    initialize_integrations();

    // Инициализация UI
    setWindowTitle(QString::fromStdString(APPLICATION_NAME));
    setMinimumSize(APPLICATION_WIDTH, APPLICATION_HEIGHT);
    init_ui();

    // Добавляем пункт меню для управления лицензией
    setup_license_menu();

    // Подключение сигналов
    connect_signals();

    // Инициализация аудио
    init_audio();

    // Таймер обновления статуса
    status_timer = new QTimer(this);
    connect(status_timer, &QTimer::timeout, this, &MainWindow::update_status);
    status_timer->start(1000); // обновление каждую секунду

    // Показываем информацию о лицензии
    show_license_info();
}

MainWindow::~MainWindow() {
    // Освобождение ресурсов
    if (status_timer) {
        status_timer->stop();
        delete status_timer;
    }
}

void MainWindow::init_ui() {
    // Создание центрального виджета
    QWidget* central_widget = new QWidget(this);
    setCentralWidget(central_widget);

    // Основная разметка
    QVBoxLayout* main_layout = new QVBoxLayout(central_widget);

    // Кнопка активации лицензии
    QHBoxLayout* license_button_layout = new QHBoxLayout();
    activate_license_button = new QPushButton("Активировать лицензию", this);
    license_button_layout->addWidget(activate_license_button);
    license_button_layout->addStretch(1); // Добавляем растяжку, чтобы кнопка была слева
    main_layout->addLayout(license_button_layout);

    // Панель управления
    QGroupBox* control_panel = new QGroupBox("Управление", this);
    QHBoxLayout* control_layout = new QHBoxLayout(control_panel);

    start_button = new QPushButton("Старт", this);
    stop_button = new QPushButton("Стоп", this);
    pause_button = new QPushButton("Пауза", this);

    stop_button->setEnabled(false);
    pause_button->setEnabled(false);

    control_layout->addWidget(start_button);
    control_layout->addWidget(pause_button);
    control_layout->addWidget(stop_button);

    main_layout->addWidget(control_panel);

    // Настройки аудио
    QGroupBox* audio_panel = new QGroupBox("Настройки аудио", this);
    QVBoxLayout* audio_layout = new QVBoxLayout(audio_panel);

    // Выбор устройств
    QHBoxLayout* device_layout = new QHBoxLayout();
    device_layout->addWidget(new QLabel("Вход:", this));
    input_device_combo = new QComboBox(this);
    device_layout->addWidget(input_device_combo, 1);

    device_layout->addWidget(new QLabel("Выход:", this));
    output_device_combo = new QComboBox(this);
    device_layout->addWidget(output_device_combo, 1);

    audio_layout->addLayout(device_layout);

    main_layout->addWidget(audio_panel);

    // Лог событий
    QGroupBox* log_panel = new QGroupBox("Лог событий", this);
    QVBoxLayout* log_layout = new QVBoxLayout(log_panel);

    log_text = new QTextEdit(this);
    log_text->setReadOnly(true);
    log_layout->addWidget(log_text);

    QHBoxLayout* log_control_layout = new QHBoxLayout();
    clear_log_button = new QPushButton("Очистить лог", this);
    log_control_layout->addWidget(clear_log_button);

    save_log_button = new QPushButton("Сохранить лог", this);
    log_control_layout->addWidget(save_log_button);

    log_layout->addLayout(log_control_layout);

    main_layout->addWidget(log_panel);

    // Статусная строка
    statusBar()->showMessage("Готов к работе");

    // Добавим в статусную строку метки для отображения буфера и обнаружений
    buffer_label = new QLabel("Буфер: 0/0", this);
    statusBar()->addPermanentWidget(buffer_label);

    detections_label = new QLabel("Обнаружено: 0", this);
    statusBar()->addPermanentWidget(detections_label);
}

void MainWindow::connect_signals() {
    // Кнопки управления
    connect(start_button, &QPushButton::clicked, this, &MainWindow::start_processing);
    connect(stop_button, &QPushButton::clicked, this, &MainWindow::stop_processing);
    connect(pause_button, &QPushButton::clicked, this, &MainWindow::toggle_pause);

    // Лог
    connect(clear_log_button, &QPushButton::clicked, this, &MainWindow::clear_log);
    connect(save_log_button, &QPushButton::clicked, this, &MainWindow::save_log);

    // Кнопка лицензии
    connect(activate_license_button, &QPushButton::clicked, this, &MainWindow::show_license_dialog);

    // Сигналы от аудио процессора
    connect(audio_processor.get(), &AudioProcessor::logMessage, this, &MainWindow::add_log_message);
    connect(audio_processor.get(), &AudioProcessor::wordDetected, this, &MainWindow::word_detected);
    connect(audio_processor.get(), &AudioProcessor::bufferUpdate, this, &MainWindow::update_buffer_status);
    connect(audio_processor.get(), &AudioProcessor::deviceListUpdate, this, &MainWindow::update_device_list);
}

void MainWindow::setup_license_menu() {
    // Создаем меню
    QMenu* license_menu = menuBar()->addMenu("Лицензия");

    // Добавляем действия
    QAction* activate_action = new QAction("Активировать лицензию", this);
    connect(activate_action, &QAction::triggered, this, &MainWindow::show_license_dialog);
    license_menu->addAction(activate_action);

    QAction* info_action = new QAction("Информация о лицензии", this);
    connect(info_action, &QAction::triggered, this, [this]() { show_license_info(true); });
    license_menu->addAction(info_action);

    QAction* buy_action = new QAction("Купить лицензию", this);
    connect(buy_action, &QAction::triggered, this, &MainWindow::buy_license);
    license_menu->addAction(buy_action);

    license_status_label = new QLabel("Лицензия: проверка...", this);
    statusBar()->addPermanentWidget(license_status_label);
}

void MainWindow::setup_integration_menu() {
    // Создаем меню
    QMenu* integration_menu = menuBar()->addMenu("Интеграция");

    // Добавляем пункт для OBS
    QAction* obs_action = new QAction("Настройка OBS WebSocket", this);
    connect(obs_action, &QAction::triggered, this, &MainWindow::show_obs_config_dialog);
    integration_menu->addAction(obs_action);
}

void MainWindow::initialize_integrations() {
    // Создаем объект для интеграции с OBS
    obs_integration = std::make_unique<OBSIntegration>();
}

void MainWindow::init_audio() {
    if (audio_processor->initialize_audio()) {
        add_log_message("✅ Аудио подсистема инициализирована");
    } else {
        add_log_message("❌ Ошибка инициализации аудио подсистемы");
    }
}

void MainWindow::update_device_list(const QList<QPair<int, QString>>& devices) {
    input_device_combo->clear();
    output_device_combo->clear();

    for (const auto& device : devices) {
        int index = device.first;
        QString name = device.second;

        if (name.contains("(In)") || name.contains("(In/Out)")) {
            input_device_combo->addItem(name, index);
        }

        if (name.contains("(Out)") || name.contains("(In/Out)")) {
            output_device_combo->addItem(name, index);
        }
    }
}

void MainWindow::add_log_message(const QString& message) {
    // Добавляем временную метку и сообщение в лог
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    log_text->append(QString("[%1] %2").arg(timestamp).arg(message));

    // Прокрутка до конца
    log_text->verticalScrollBar()->setValue(log_text->verticalScrollBar()->maximum());
}

void MainWindow::clear_log() {
    log_text->clear();
    add_log_message("🧹 Лог очищен");
}

void MainWindow::save_log() {
    QString file_path = QFileDialog::getSaveFileName(
        this, "Сохранить лог", "", "Текстовые файлы (*.txt)");

    if (file_path.isEmpty()) {
        return;
    }

    try {
        QFile file(file_path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream << log_text->toPlainText();
            file.close();

            add_log_message(QString("✅ Лог сохранен в %1").arg(file_path));
        } else {
            add_log_message(QString("❌ Ошибка открытия файла для записи: %1").arg(file.errorString()));
        }
    } catch (const std::exception& e) {
        add_log_message(QString("❌ Ошибка сохранения лога: %1").arg(e.what()));
    }
}

void MainWindow::start_processing() {
    if (running) {
        return;
    }

    // Проверяем лицензию перед запуском
    if (!license_manager->has_valid_license()) {
        QMessageBox::warning(this, "Ошибка лицензии",
                            "Лицензия недействительна или истекла. Пожалуйста, активируйте лицензию.");
        show_license_dialog();
        return;
    }

    // Загружаем список слов из API
    add_log_message("📡 Загрузка списка запрещённых слов из API...");
    bool api_success = load_words_from_api();

    // Если не удалось загрузить слова из API, невозможно продолжить
    if (!api_success) {
        QMessageBox::critical(this, "Ошибка загрузки данных",
                             "Не удалось загрузить список запрещённых слов с сервера.\n"
                             "Проверьте подключение к интернету и повторите попытку.");
        return;
    }

    // Получаем индексы выбранных устройств
    input_device_index = input_device_combo->currentData().toInt();
    output_device_index = output_device_combo->currentData().toInt();

    if (input_device_index < 0 || output_device_index < 0) {
        add_log_message("❌ Не выбраны устройства ввода/вывода");
        return;
    }

    // Настраиваем аудио потоки
    if (!audio_processor->setup_streams(input_device_index, output_device_index)) {
        return;
    }

    // Запускаем поток
    audio_processor->start();
    running = true;

    // Обновляем UI
    start_button->setEnabled(false);
    stop_button->setEnabled(true);
    pause_button->setEnabled(true);

    // Блокируем изменение устройств
    input_device_combo->setEnabled(false);
    output_device_combo->setEnabled(false);

    add_log_message("✅ Обработка аудио запущена");
}

void MainWindow::stop_processing() {
    if (!running) {
        return;
    }

    // Останавливаем поток
    audio_processor->stop_processing();
    running = false;

    // Очищаем списки слов и паттернов
    auto config = config_manager->get_config();
    config["target_words"] = "[]";
    config["target_patterns"] = "[]";
    config_manager->update_config(config);
    add_log_message("🧹 Списки слов и паттернов очищены");

    // Создаем новый аудио процессор
    audio_processor = std::make_unique<AudioProcessor>(config);

    // Переподключаем сигналы
    connect(audio_processor.get(), &AudioProcessor::logMessage, this, &MainWindow::add_log_message);
    connect(audio_processor.get(), &AudioProcessor::wordDetected, this, &MainWindow::word_detected);
    connect(audio_processor.get(), &AudioProcessor::bufferUpdate, this, &MainWindow::update_buffer_status);
    connect(audio_processor.get(), &AudioProcessor::deviceListUpdate, this, &MainWindow::update_device_list);

    // Повторная инициализация аудио устройств
    audio_processor->initialize_audio();

    // Обновляем UI
    start_button->setEnabled(true);
    stop_button->setEnabled(false);
    pause_button->setEnabled(false);
    pause_button->setText("Пауза");

    // Разблокируем изменение устройств
    input_device_combo->setEnabled(true);
    output_device_combo->setEnabled(true);

    add_log_message("🛑 Обработка аудио остановлена");
}

void MainWindow::toggle_pause() {
    if (!running) {
        return;
    }

    if (audio_processor->is_paused()) {
        audio_processor->resume();
        pause_button->setText("Пауза");
        add_log_message("▶️ Обработка аудио возобновлена");
    } else {
        audio_processor->pause();
        pause_button->setText("Продолжить");
        add_log_message("⏸️ Обработка аудио приостановлена");
    }
}

bool MainWindow::load_words_from_api() {
    try {
        // URL API для получения списка слов
        std::string api_url = license_manager->get_words_api_url();

        // Получаем machine_id
        std::string machine_id = security::get_machine_id();

        // Добавляем случайную задержку для защиты от автоматизированных атак
        security::add_random_delay(50, 100); // 50-100 мс

        // Генерируем уникальный идентификатор запроса
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(0, 1.0);
        std::string request_id = security::md5_hash(std::to_string(std::time(nullptr)) + std::to_string(dis(gen)));

        // Подготавливаем данные для запроса
        std::string params = "machine_id=" + machine_id +
                           "&license_key=" + license_manager->get_license_key() +
                           "&request_id=" + request_id +
                           "&timestamp=" + std::to_string(std::time(nullptr));

        // Формируем полный URL
        std::string full_url = api_url + "?" + params;

        // Выполняем GET запрос
        CURL* curl = curl_easy_init();
        if (!curl) {
            add_log_message("❌ Ошибка инициализации cURL");
            return false;
        }

        std::string response_data;

        curl_easy_setopt(curl, CURLOPT_URL, full_url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

        CURLcode res = curl_easy_perform(curl);

        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            add_log_message(QString("❌ Ошибка cURL: %1").arg(curl_easy_strerror(res)));
            return false;
        }

        if (http_code == 200) {
            // Парсим JSON ответ
            json data = json::parse(response_data);

            // Проверяем подпись ответа для защиты от подделки
            if (!license_manager->verify_api_response(response_data, machine_id)) {
                add_log_message("⚠️ Предупреждение: получены подозрительные данные от API");
                return false;
            }

            // Обновляем списки слов и регулярных выражений
            int words_count = 0;
            int patterns_count = 0;
            auto config = config_manager->get_config();

            if (data.contains("words") && data["words"].is_array()) {
                std::vector<std::string> target_words = data["words"].get<std::vector<std::string>>();
                config["target_words"] = data["words"].dump();
                words_count = target_words.size();
                add_log_message(QString("✅ Загружено %1 запрещённых слов из API").arg(words_count));
            } else {
                config["target_words"] = "[]";
                add_log_message("⚠️ API не вернул список слов, словарь пуст");
            }

            // Обновляем регулярные выражения
            if (data.contains("patterns") && data["patterns"].is_array()) {
                std::vector<std::string> target_patterns = data["patterns"].get<std::vector<std::string>>();
                config["target_patterns"] = data["patterns"].dump();
                patterns_count = target_patterns.size();
                add_log_message(QString("✅ Загружено %1 регулярных выражений из API").arg(patterns_count));
            } else {
                config["target_patterns"] = "[]";
                add_log_message("⚠️ API не вернул шаблоны регулярных выражений");
            }

            // Обновляем конфигурацию в аудио потоке
            audio_processor->update_config(config);
            add_log_message("✅ Конфигурация цензуры обновлена");

            // Сохраняем обновленную конфигурацию
            config_manager->update_config(config);

            // Проверяем, есть ли хоть какие-то слова или шаблоны
            if (words_count == 0 && patterns_count == 0) {
                add_log_message("⚠️ Внимание: не получено ни слов, ни шаблонов для цензуры!");
                return false;
            }

            return true;
        } else {
            add_log_message(QString("❌ Ошибка при загрузке списка слов: HTTP %1").arg(http_code));
            if (http_code == 401) {
                add_log_message("❌ Проверьте лицензионный ключ и доступ к API");
            }
            return false;
        }

    } catch (const std::exception& e) {
        add_log_message(QString("❌ Ошибка при загрузке списка слов: %1").arg(e.what()));
        return false;
    }
}

void MainWindow::update_status() {
    if (running) {
        QString status = "Активно";
        if (audio_processor->is_paused()) {
            status = "Пауза";
        }
        statusBar()->showMessage(QString("Статус: %1").arg(status));
    } else {
        statusBar()->showMessage("Статус: Остановлено");
    }
}

void MainWindow::word_detected(const QString& word, double start_time, double end_time) {
    detections_count++;
    detections_label->setText(QString("Обнаружено: %1").arg(detections_count));
}

void MainWindow::update_buffer_status(int current, int maximum) {
    int percentage = (maximum > 0) ? (current * 100 / maximum) : 0;
    buffer_label->setText(QString("Буфер: %1/%2 (%3%)").arg(current).arg(maximum).arg(percentage));
}

void MainWindow::show_license_dialog() {
    LicenseDialog dialog(license_manager.get(), this);
    if (dialog.exec() == QDialog::Accepted) {
        show_license_info();
    }
}

void MainWindow::show_license_info(bool show_dialog) {
    // Получаем полную информацию о статусе лицензии
    auto license_status = license_manager->get_license_status();

    // Словарь месяцев для отображения даты
    std::unordered_map<int, std::string> months = {
        {1, "января"}, {2, "февраля"}, {3, "марта"}, {4, "апреля"},
        {5, "мая"}, {6, "июня"}, {7, "июля"}, {8, "августа"},
        {9, "сентября"}, {10, "октября"}, {11, "ноября"}, {12, "декабря"}
    };

    QString status_text = "";

    if (license_status["has_license"] == "true") {
        // Показываем информацию о полной лицензии
        try {
            std::tm expiry_tm = {};
            std::istringstream ss(license_status["expiry_date"]);
            ss >> std::get_time(&expiry_tm, "%Y-%m-%d");

            if (!ss.fail()) {
                int days_remaining = std::stoi(license_status["days_remaining"]);
                int month = expiry_tm.tm_mon + 1; // tm_mon is 0-based

                QString expiry_str = QString("%1 %2 %3 года")
                                   .arg(expiry_tm.tm_mday)
                                   .arg(QString::fromStdString(months[month]))
                                   .arg(expiry_tm.tm_year + 1900);

                status_text = QString("Лицензия активна до: %1 (осталось %2 дней)")
                            .arg(expiry_str)
                            .arg(days_remaining);

                license_status_label->setText(status_text);
                statusBar()->showMessage("Программа готова к работе");
            } else {
                status_text = "Лицензия активна";
                license_status_label->setText(status_text);
                statusBar()->showMessage("Программа готова к работе");
            }
        } catch (const std::exception& e) {
            status_text = "Лицензия активна";
            license_status_label->setText(status_text);
            statusBar()->showMessage("Программа готова к работе");
        }
    } else if (license_status["trial_active"] == "true") {
        // Показываем информацию о пробном периоде
        try {
            std::tm expiry_tm = {};
            std::istringstream ss(license_status["expiry_date"]);
            ss >> std::get_time(&expiry_tm, "%Y-%m-%d");

            if (!ss.fail()) {
                int days_remaining = std::stoi(license_status["days_remaining"]);
                int month = expiry_tm.tm_mon + 1; // tm_mon is 0-based

                QString expiry_str = QString("%1 %2 %3 года")
                                   .arg(expiry_tm.tm_mday)
                                   .arg(QString::fromStdString(months[month]))
                                   .arg(expiry_tm.tm_year + 1900);

                status_text = QString("Пробный период до: %1 (осталось %2 дней)")
                            .arg(expiry_str)
                            .arg(days_remaining);

                license_status_label->setText(status_text);
                statusBar()->showMessage("Программа готова к работе");
            } else {
                status_text = "Пробный период активен";
                license_status_label->setText(status_text);
                statusBar()->showMessage("Программа готова к работе");
            }
        } catch (const std::exception& e) {
            status_text = "Пробный период активен";
            license_status_label->setText(status_text);
            statusBar()->showMessage("Программа готова к работе");
        }
    } else {
        // Ни лицензии, ни пробного периода
        status_text = "Требуется активация лицензии";
        license_status_label->setText(status_text);
        statusBar()->showMessage("Требуется активация лицензии или пробного периода");
    }

    // Если запрошено отображение диалога, показываем его
    if (show_dialog) {
        LicenseInfoDialog dialog(license_manager.get(), this);
        dialog.exec();
    }
}

void MainWindow::buy_license() {
    // Открываем ссылку на Telegram
    QDesktopServices::openUrl(
        QUrl(QString("https://t.me/%1").arg(
            QString::fromStdString(license_manager->get_telegram_contact()).replace("@", "")
        ))
    );
}

void MainWindow::show_obs_config_dialog() {
    OBSConfigDialog dialog(obs_integration.get(), this);
    dialog.exec();
}

bool MainWindow::check_license_on_launch() {
    auto license_status = license_manager->get_license_status();

    // Если нет ни лицензии, ни пробного периода, показываем диалог активации
    if (license_status["has_license"] != "true" && license_status["trial_active"] != "true") {
        LicenseDialog dialog(license_manager.get(), this);
        if (dialog.exec() != QDialog::Accepted) {
            // Если пользователь отказался от активации, закрываем приложение
            QTimer::singleShot(0, this, &MainWindow::close);
            return false;
        }

        // Обновляем информацию о лицензии после активации
        show_license_info();
    }

    // Если лицензия скоро истекает, показываем предупреждение
    else if (license_status["has_license"] == "true") {
        int days_remaining = std::stoi(license_status["days_remaining"]);
        if (days_remaining > 0 && days_remaining <= 5) {
            QMessageBox::warning(this, "Скоро истечет лицензия",
                               QString("Ваша лицензия истекает через %1 дней.\n"
                                      "Пожалуйста, обновите вашу лицензию.").arg(days_remaining));
        }
    }

    // Если пробный период скоро истекает, показываем предупреждение
    else if (license_status["trial_active"] == "true") {
        int days_remaining = std::stoi(license_status["days_remaining"]);
        if (days_remaining > 0 && days_remaining <= 1) {
            QMessageBox::warning(this, "Скоро истечет пробный период",
                               QString("Ваш пробный период истекает через %1 дней.\n"
                                      "Пожалуйста, приобретите лицензию для продолжения использования.").arg(days_remaining));
        }
    }

    return true;
}
void MainWindow::closeEvent(QCloseEvent* event) {
    if (running) {
        // Останавливаем обработку при закрытии
        audio_processor->stop_processing();
    }

    // Сохраняем конфигурацию перед закрытием
    auto config = config_manager->get_config();
    config_manager->update_config(config);

    QMainWindow::closeEvent(event);
}

} // namespace audiocensor