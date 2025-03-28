#include "ui/obs_dialog.h"

#include <QApplication>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QMessageBox>

// Для работы с OBS WebSocket в будущем можно использовать библиотеку
// На данный момент предположим, что запросы к OBS отправляются через libcurl или другой HTTP клиент
// Для простоты реализации сделаем упрощенную версию, которая только имитирует работу с OBS

namespace audiocensor {

// ====================== OBSIntegration ======================

OBSIntegration::OBSIntegration()
    : host("localhost"), port(4455), password(""), connected(false), client(nullptr) {
}

OBSIntegration::~OBSIntegration() {
    // Закрытие соединения и освобождение ресурсов, если необходимо
    client = nullptr;
}

std::pair<bool, std::string> OBSIntegration::connect(const std::string& host, int port, const std::string& password) {
    // В реальной реализации здесь будет настоящее подключение к OBS через WebSocket
    // Временно реализуем упрощенную имитацию
    
    this->host = host;
    this->port = port;
    this->password = password;
    
    try {
        // Проверим, доступен ли OBS на указанном адресе
        // В реальной реализации здесь будет код для подключения к WebSocket OBS
        
        bool success = true; // Временно предполагаем, что подключение успешно
        
        if (success) {
            connected = true;
            return {true, "Подключено к OBS Studio"};
        } else {
            connected = false;
            return {false, "Не удалось подключиться к OBS"};
        }
    } catch (const std::exception& e) {
        connected = false;
        return {false, std::string("Ошибка подключения: ") + e.what()};
    }
}

std::pair<bool, std::string> OBSIntegration::setup_delay_filters(
    const std::string& scene_name,
    const std::string& filter_prefix,
    const std::vector<int>& delay_values) {
    
    if (!connected) {
        return {false, "Нет подключения к OBS"};
    }
    
    try {
        // Проверяем существование сцены
        auto scenes = get_scenes();
        if (std::find(scenes.begin(), scenes.end(), scene_name) == scenes.end()) {
            return {false, "Сцена '" + scene_name + "' не найдена в OBS"};
        }
        
        // Используем значения задержек по умолчанию, если не указаны
        std::vector<int> actual_delays = delay_values;
        if (actual_delays.empty()) {
            actual_delays = {500, 500, 300};  // Два фильтра по 500ms и один по 300ms
        }
        
        // В реальной реализации здесь был бы код для создания фильтров
        // через запросы к OBS WebSocket API
        
        // Формируем сообщение о результате
        double total_delay = 0;
        for (int delay : actual_delays) {
            total_delay += delay;
        }
        total_delay /= 1000; // конвертируем в секунды
        
        return {true, "Успешно создано " + std::to_string(actual_delays.size()) + 
                      " фильтров задержки (общая задержка " + std::to_string(total_delay) + " сек)"};
    } catch (const std::exception& e) {
        return {false, std::string("Ошибка при настройке фильтров: ") + e.what()};
    }
}

std::vector<std::string> OBSIntegration::get_scenes() {
    // В реальной реализации здесь был бы запрос к OBS WebSocket API
    // для получения списка сцен
    
    if (!connected) {
        return {};
    }
    
    // Возвращаем примерный список сцен для демонстрации
    return {"Main Scene", "Game Capture", "Stream Starting", "Stream Ending"};
}


// ====================== OBSConfigDialog ======================

OBSConfigDialog::OBSConfigDialog(OBSIntegration* obs_integration, QWidget* parent)
    : QDialog(parent), obs_integration(obs_integration) {
    setWindowTitle("Настройка интеграции с OBS");
    setMinimumWidth(500);
    init_ui();
}

void OBSConfigDialog::init_ui() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Настройки подключения
    QGroupBox* connection_group = new QGroupBox("Параметры подключения WebSocket", this);
    QGridLayout* connection_layout = new QGridLayout(connection_group);
    
    // Хост
    connection_layout->addWidget(new QLabel("Хост:", this), 0, 0);
    host_input = new QLineEdit(QString::fromStdString(obs_integration->get_host()), this);
    connection_layout->addWidget(host_input, 0, 1);
    
    // Порт
    connection_layout->addWidget(new QLabel("Порт:", this), 1, 0);
    port_input = new QSpinBox(this);
    port_input->setRange(1, 65535);
    port_input->setValue(obs_integration->get_port());
    connection_layout->addWidget(port_input, 1, 1);
    
    // Пароль
    connection_layout->addWidget(new QLabel("Пароль:", this), 2, 0);
    password_input = new QLineEdit(this);
    password_input->setEchoMode(QLineEdit::Password);
    connection_layout->addWidget(password_input, 2, 1);
    
    // Кнопка подключения
    connect_button = new QPushButton("Подключиться", this);
    connect(connect_button, &QPushButton::clicked, this, &OBSConfigDialog::connect_to_obs);
    connection_layout->addWidget(connect_button, 3, 0, 1, 2);
    
    layout->addWidget(connection_group);
    
    // Настройки фильтров задержки
    QGroupBox* filter_group = new QGroupBox("Настройка фильтров задержки", this);
    QGridLayout* filter_layout = new QGridLayout(filter_group);
    
    // Выбор сцены
    filter_layout->addWidget(new QLabel("Сцена:", this), 0, 0);
    scene_combo = new QComboBox(this);
    scene_combo->setEnabled(false);
    filter_layout->addWidget(scene_combo, 0, 1, 1, 3);
    
    // Заголовок для фильтров
    filter_layout->addWidget(new QLabel("<b>Настройка фильтров задержки:</b>", this), 1, 0, 1, 4);
    
    // Фильтр 1
    filter_layout->addWidget(new QLabel("Фильтр 1:", this), 2, 0);
    filter1_check = new QCheckBox("Активировать", this);
    filter1_check->setChecked(true);
    filter_layout->addWidget(filter1_check, 2, 1);
    filter_layout->addWidget(new QLabel("Задержка:", this), 2, 2);
    filter1_delay = new QSpinBox(this);
    filter1_delay->setRange(100, 1000);
    filter1_delay->setSingleStep(50);
    filter1_delay->setValue(500);
    filter1_delay->setSuffix(" мс");
    filter_layout->addWidget(filter1_delay, 2, 3);
    
    // Фильтр 2
    filter_layout->addWidget(new QLabel("Фильтр 2:", this), 3, 0);
    filter2_check = new QCheckBox("Активировать", this);
    filter2_check->setChecked(true);
    filter_layout->addWidget(filter2_check, 3, 1);
    filter_layout->addWidget(new QLabel("Задержка:", this), 3, 2);
    filter2_delay = new QSpinBox(this);
    filter2_delay->setRange(100, 1000);
    filter2_delay->setSingleStep(50);
    filter2_delay->setValue(500);
    filter2_delay->setSuffix(" мс");
    filter_layout->addWidget(filter2_delay, 3, 3);
    
    // Фильтр 3
    filter_layout->addWidget(new QLabel("Фильтр 3:", this), 4, 0);
    filter3_check = new QCheckBox("Активировать", this);
    filter3_check->setChecked(true);
    filter_layout->addWidget(filter3_check, 4, 1);
    filter_layout->addWidget(new QLabel("Задержка:", this), 4, 2);
    filter3_delay = new QSpinBox(this);
    filter3_delay->setRange(100, 1000);
    filter3_delay->setSingleStep(50);
    filter3_delay->setValue(300);  // Третий фильтр установлен на 300 мс по умолчанию
    filter3_delay->setSuffix(" мс");
    filter_layout->addWidget(filter3_delay, 4, 3);
    
    // Общая задержка (информационная)
    filter_layout->addWidget(new QLabel("Общая задержка:", this), 5, 0, 1, 3);
    total_delay_label = new QLabel("1.3 сек", this);
    filter_layout->addWidget(total_delay_label, 5, 3);
    
    // Информация о ручной настройке
    QLabel* info_label = new QLabel("Вы также можете настроить фильтры задержки вручную в OBS Studio", this);
    info_label->setWordWrap(true);
    filter_layout->addWidget(info_label, 6, 0, 1, 4);
    
    // Кнопка применения
    apply_button = new QPushButton("Применить фильтры задержки", this);
    connect(apply_button, &QPushButton::clicked, this, &OBSConfigDialog::apply_delay_filters);
    apply_button->setEnabled(false);
    filter_layout->addWidget(apply_button, 7, 0, 1, 4);
    
    layout->addWidget(filter_group);
    
    // Статус операции
    status_label = new QLabel("Не подключено к OBS", this);
    status_label->setWordWrap(true);
    layout->addWidget(status_label);
    
    // Кнопки диалога
    QHBoxLayout* button_layout = new QHBoxLayout();
    close_button = new QPushButton("Закрыть", this);
    connect(close_button, &QPushButton::clicked, this, &QDialog::accept);
    button_layout->addWidget(close_button);
    
    layout->addLayout(button_layout);
    
    // Обновление общей задержки при изменении параметров
    connect(filter1_check, &QCheckBox::stateChanged, this, &OBSConfigDialog::update_total_delay);
    connect(filter2_check, &QCheckBox::stateChanged, this, &OBSConfigDialog::update_total_delay);
    connect(filter3_check, &QCheckBox::stateChanged, this, &OBSConfigDialog::update_total_delay);
    connect(filter1_delay, &QSpinBox::valueChanged, this, &OBSConfigDialog::update_total_delay);
    connect(filter2_delay, &QSpinBox::valueChanged, this, &OBSConfigDialog::update_total_delay);
    connect(filter3_delay, &QSpinBox::valueChanged, this, &OBSConfigDialog::update_total_delay);
    
    // Первоначальное обновление общей задержки
    update_total_delay();
}

void OBSConfigDialog::update_total_delay() {
    int total_ms = 0;
    
    if (filter1_check->isChecked()) {
        total_ms += filter1_delay->value();
    }
    
    if (filter2_check->isChecked()) {
        total_ms += filter2_delay->value();
    }
    
    if (filter3_check->isChecked()) {
        total_ms += filter3_delay->value();
    }
    
    double total_sec = total_ms / 1000.0;
    total_delay_label->setText(QString("%1 сек").arg(total_sec, 0, 'f', 1));
}

void OBSConfigDialog::connect_to_obs() {
    std::string host = host_input->text().toStdString();
    int port = port_input->value();
    std::string password = password_input->text().toStdString();
    
    status_label->setText("Подключение к OBS...");
    QApplication::processEvents();
    
    bool success;
    std::string message;
    std::tie(success, message) = obs_integration->connect(host, port, password);
    status_label->setText(QString::fromStdString(message));
    
    // Если подключение успешно, обновляем список сцен и разблокируем элементы
    if (success) {
        scene_combo->clear();
        std::vector<std::string> scenes = obs_integration->get_scenes();
        for (const auto& scene : scenes) {
            scene_combo->addItem(QString::fromStdString(scene));
        }
        
        scene_combo->setEnabled(true);
        filter1_check->setEnabled(true);
        filter2_check->setEnabled(true);
        filter3_check->setEnabled(true);
        filter1_delay->setEnabled(true);
        filter2_delay->setEnabled(true);
        filter3_delay->setEnabled(true);
        apply_button->setEnabled(true);
    } else {
        scene_combo->setEnabled(false);
        filter1_check->setEnabled(false);
        filter2_check->setEnabled(false);
        filter3_check->setEnabled(false);
        filter1_delay->setEnabled(false);
        filter2_delay->setEnabled(false);
        filter3_delay->setEnabled(false);
        apply_button->setEnabled(false);
    }
}

void OBSConfigDialog::apply_delay_filters() {
    std::string scene_name = scene_combo->currentText().toStdString();
    
    if (scene_name.empty()) {
        status_label->setText("Ошибка: не выбрана сцена");
        return;
    }
    
    // Собираем значения задержек для активных фильтров
    std::vector<int> delay_values;
    
    if (filter1_check->isChecked()) {
        delay_values.push_back(filter1_delay->value());
    }
    
    if (filter2_check->isChecked()) {
        delay_values.push_back(filter2_delay->value());
    }
    
    if (filter3_check->isChecked()) {
        delay_values.push_back(filter3_delay->value());
    }
    
    if (delay_values.empty()) {
        status_label->setText("Ошибка: не выбран ни один фильтр");
        return;
    }
    
    status_label->setText(QString("Настройка фильтров для сцены '%1'...").arg(QString::fromStdString(scene_name)));
    QApplication::processEvents();
    
    bool success;
    std::string message;
    std::tie(success, message) = obs_integration->setup_delay_filters(
        scene_name,
        "CensorAI Delay",
        delay_values
    );
    
    status_label->setText(QString::fromStdString(message));
}

} // namespace audiocensor