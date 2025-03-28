#include "ui/license_dialog.h"
#include "audiocensor/license_manager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>

#include <ctime>
#include <iomanip>
#include <chrono>
#include <sstream>

namespace audiocensor {

LicenseDialog::LicenseDialog(LicenseManager* license_manager, QWidget* parent)
    : QDialog(parent), license_manager(license_manager) {
    setWindowTitle("Активация лицензии");
    setMinimumWidth(400);
    
    // Проверяем, активирован ли уже пробный период
    trial_active = license_manager->check_trial_period();
    
    // Проверяем, есть ли уже активная лицензия
    has_license = !license_manager->get_license_key().empty() && !license_manager->get_expiry_date().empty();
    
    init_ui();
}

void LicenseDialog::init_ui() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Если есть активная лицензия, показываем только информацию о ней
    if (has_license) {
        QString info_text;
        
        try {
            std::tm expiry_tm = {};
            std::istringstream ss(license_manager->get_expiry_date());
            ss >> std::get_time(&expiry_tm, "%Y-%m-%d");
            
            if (!ss.fail()) {
                QDateTime expiry = QDateTime::fromSecsSinceEpoch(std::mktime(&expiry_tm));
                QString expiry_str = expiry.toString("dd.MM.yyyy");
                
                info_text = QString("У вас активирована лицензия до %1.\nЕсли вы хотите активировать новую лицензию, введите новый ключ:").arg(expiry_str);
            } else {
                info_text = "У вас активирована лицензия. Если вы хотите активировать новую лицензию, введите новый ключ:";
            }
        } catch (...) {
            info_text = "У вас активирована лицензия. Если вы хотите активировать новую лицензию, введите новый ключ:";
        }
        
        QLabel* info_label = new QLabel(info_text, this);
        info_label->setWordWrap(true);
        layout->addWidget(info_label);
        
        // Поле для ввода ключа
        QLabel* key_label = new QLabel("Введите новый лицензионный ключ:", this);
        layout->addWidget(key_label);
        
        key_input = new QLineEdit(this);
        key_input->setPlaceholderText("XXXXX-XXXXX-XXXXX-XXXXX");
        layout->addWidget(key_input);
        
        // Кнопка активации
        QHBoxLayout* button_layout = new QHBoxLayout();
        activate_button = new QPushButton("Активировать новую лицензию", this);
        button_layout->addWidget(activate_button);
        
        exit_button = new QPushButton("Закрыть", this);
        button_layout->addWidget(exit_button);
        
        layout->addLayout(button_layout);
        
        // Подключаем сигналы
        connect(activate_button, &QPushButton::clicked, this, &LicenseDialog::activate_license);
        connect(exit_button, &QPushButton::clicked, this, &QDialog::accept);
    }
    // Если пробный период уже активирован, показываем только опцию ввода ключа
    else if (trial_active) {
        // Информационный текст
        QLabel* info_label = new QLabel("У вас активирован пробный период. Для активации полной версии введите лицензионный ключ:", this);
        info_label->setWordWrap(true);
        layout->addWidget(info_label);
        
        // Информация о покупке через Telegram
        QLabel* telegram_label = new QLabel(QString("Для приобретения лицензии напишите: %1").arg(QString::fromStdString(license_manager->get_telegram_contact())), this);
        telegram_label->setWordWrap(true);
        telegram_label->setStyleSheet("font-weight: bold;");
        layout->addWidget(telegram_label);
        
        // Кнопка открытия Telegram
        telegram_button = new QPushButton("Открыть Telegram", this);
        layout->addWidget(telegram_button);
        
        // Поле для ввода ключа
        QLabel* key_label = new QLabel("Введите полученный лицензионный ключ:", this);
        layout->addWidget(key_label);
        
        key_input = new QLineEdit(this);
        key_input->setPlaceholderText("XXXXX-XXXXX-XXXXX-XXXXX");
        layout->addWidget(key_input);
        
        // Кнопка активации
        QHBoxLayout* button_layout = new QHBoxLayout();
        activate_button = new QPushButton("Активировать лицензию", this);
        button_layout->addWidget(activate_button);
        
        exit_button = new QPushButton("Выход", this);
        button_layout->addWidget(exit_button);
        
        layout->addLayout(button_layout);
        
        // Подключаем сигналы
        connect(telegram_button, &QPushButton::clicked, this, &LicenseDialog::open_telegram);
        connect(activate_button, &QPushButton::clicked, this, &LicenseDialog::activate_license);
        connect(exit_button, &QPushButton::clicked, this, &QDialog::accept);
    }
    else {
        // Основной информационный текст
        QLabel* info_label = new QLabel("Выберите один из вариантов использования приложения:", this);
        info_label->setWordWrap(true);
        layout->addWidget(info_label);
        
        // 1. Пробный период
        QGroupBox* trial_group = new QGroupBox("Пробный период (10 дней)", this);
        QVBoxLayout* trial_layout = new QVBoxLayout(trial_group);
        
        QLabel* trial_desc = new QLabel("Активируйте бесплатную тестовую лицензию на 10 дней для ознакомления с приложением.", this);
        trial_desc->setWordWrap(true);
        trial_layout->addWidget(trial_desc);
        
        trial_button = new QPushButton("Активировать тестовую лицензию", this);
        trial_layout->addWidget(trial_button);
        
        layout->addWidget(trial_group);
        
        // 2. Полная лицензия
        QGroupBox* license_group = new QGroupBox("Полная лицензия", this);
        QVBoxLayout* license_layout = new QVBoxLayout(license_group);
        
        // Информация о покупке через Telegram
        QLabel* telegram_label = new QLabel(QString("Для приобретения лицензии напишите: %1").arg(QString::fromStdString(license_manager->get_telegram_contact())), this);
        telegram_label->setWordWrap(true);
        license_layout->addWidget(telegram_label);
        
        telegram_button = new QPushButton("Открыть Telegram", this);
        license_layout->addWidget(telegram_button);
        
        // Поле для ввода ключа
        QLabel* key_label = new QLabel("Введите полученный лицензионный ключ:", this);
        license_layout->addWidget(key_label);
        
        key_input = new QLineEdit(this);
        key_input->setPlaceholderText("XXXXX-XXXXX-XXXXX-XXXXX");
        license_layout->addWidget(key_input);
        
        activate_button = new QPushButton("Активировать лицензию", this);
        license_layout->addWidget(activate_button);
        
        layout->addWidget(license_group);
        
        // Кнопка выхода
        exit_button = new QPushButton("Выход", this);
        layout->addWidget(exit_button);
        
        // Подключаем сигналы
        connect(trial_button, &QPushButton::clicked, this, &LicenseDialog::activate_trial);
        connect(telegram_button, &QPushButton::clicked, this, &LicenseDialog::open_telegram);
        connect(activate_button, &QPushButton::clicked, this, &LicenseDialog::activate_license);
        connect(exit_button, &QPushButton::clicked, this, &QDialog::reject);
    }
}

void LicenseDialog::activate_trial() {
    bool success;
    std::string message;
    std::tie(success, message) = license_manager->activate_trial_period();
    
    if (success) {
        QMessageBox::information(this, "Успешно", "Тестовая лицензия активирована! Срок действия: 10 дней.");
        accept();
        
        // Обновляем информацию о лицензии
        if (parentWidget() && qobject_cast<QMainWindow*>(parentWidget())) {
            // Тут можно вызвать метод show_license_info() у родительского окна,
            // если он доступен через соответствующий сигнал или слот
        }
    } else {
        QMessageBox::critical(this, "Ошибка", 
                             QString("Не удалось активировать тестовую лицензию: %1\n"
                                   "Проверьте подключение к интернету или свяжитесь с поддержкой.")
                             .arg(QString::fromStdString(message)));
    }
}

void LicenseDialog::open_telegram() {
    QString telegram_url = QString("https://t.me/%1").arg(
        QString::fromStdString(license_manager->get_telegram_contact()).replace("@", "")
    );
    
    QDesktopServices::openUrl(QUrl(telegram_url));
}

void LicenseDialog::activate_license() {
    QString key = key_input->text().trimmed();
    if (key.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Пожалуйста, введите лицензионный ключ");
        return;
    }
    
    // Проверяем ключ онлайн
    if (license_manager->verify_license_online(key.toStdString())) {
        QMessageBox::information(this, "Успешно", "Лицензия успешно активирована!");
        accept();
    } else {
        QMessageBox::critical(this, "Ошибка",
                             "Недействительный ключ лицензии или срок её действия истёк.\n"
                             "Пожалуйста, проверьте ключ или приобретите новую лицензию.");
    }
}

LicenseInfoDialog::LicenseInfoDialog(LicenseManager* license_manager, QWidget* parent)
    : QDialog(parent), license_manager(license_manager) {
    setWindowTitle("Информация о лицензии");
    setMinimumWidth(400);
    init_ui();
}

void LicenseInfoDialog::init_ui() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Получаем информацию о лицензии
    auto license_status = license_manager->get_license_status();
    
    // Словарь месяцев для отображения даты
    std::unordered_map<int, std::string> months = {
        {1, "января"}, {2, "февраля"}, {3, "марта"}, {4, "апреля"},
        {5, "мая"}, {6, "июня"}, {7, "июля"}, {8, "августа"},
        {9, "сентября"}, {10, "октября"}, {11, "ноября"}, {12, "декабря"}
    };
    
    // Создаем метку с информацией
    QString info_text = "";
    if (license_status["has_license"] == "true") {
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
                
                if (license_status["trial_active"] == "true") {
                    info_text = QString("Активирована тестовая лицензия до: %1\n").arg(expiry_str);
                    info_text += QString("Осталось %1 дней").arg(days_remaining);
                } else {
                    info_text = QString("Активирована полная лицензия до: %1\n").arg(expiry_str);
                    info_text += QString("Осталось %1 дней").arg(days_remaining);
                }
            } else {
                info_text = QString("Активна лицензия (ошибка отображения даты)");
            }
        } catch (const std::exception& e) {
            info_text = QString("Активна лицензия (ошибка отображения даты: %1)").arg(e.what());
        }
    } else {
        info_text = "Нет активной лицензии";
    }
    
    // Добавляем заголовок
    QLabel* title_label = new QLabel("<h2>Статус лицензии</h2>", this);
    layout->addWidget(title_label);
    
    // Добавляем информацию
    QLabel* info_label = new QLabel(info_text, this);
    info_label->setStyleSheet("font-size: 12pt; margin: 10px;");
    info_label->setWordWrap(true);
    layout->addWidget(info_label);
    
    // Если есть лицензионный ключ, добавляем его
    if (!license_status["license_key"].empty()) {
        QGroupBox* key_group = new QGroupBox("Лицензионный ключ", this);
        QVBoxLayout* key_layout = new QVBoxLayout(key_group);
        QLabel* key_label = new QLabel(QString::fromStdString(license_status["license_key"]), this);
        key_label->setStyleSheet("font-family: monospace; padding: 5px;");
        key_layout->addWidget(key_label);
        layout->addWidget(key_group);
    }
    
    // Кнопка закрытия
    QHBoxLayout* button_layout = new QHBoxLayout();
    QPushButton* close_button = new QPushButton("Закрыть", this);
    connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
    button_layout->addStretch();
    button_layout->addWidget(close_button);
    button_layout->addStretch();
    layout->addLayout(button_layout);
}

} // namespace audiocensor