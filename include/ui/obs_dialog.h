#ifndef AUDIOCENSOR_OBS_DIALOG_H
#define AUDIOCENSOR_OBS_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QSpinBox>
#include <QCheckBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <vector>
#include <string>
#include <memory>

namespace audiocensor {

/**
 * @brief Класс для интеграции с OBS через WebSocket API
 */
class OBSIntegration {
public:
    /**
     * @brief Конструктор
     */
    OBSIntegration();
    
    /**
     * @brief Деструктор
     */
    ~OBSIntegration();
    
    /**
     * @brief Подключается к OBS WebSocket серверу
     * @param host Хост для подключения
     * @param port Порт для подключения
     * @param password Пароль для подключения
     * @return Пара (успех, сообщение)
     */
    std::pair<bool, std::string> connect(const std::string& host, int port, const std::string& password);
    
    /**
     * @brief Настраивает фильтры задержки с указанными значениями
     * @param scene_name Имя сцены
     * @param filter_prefix Префикс для названия фильтра
     * @param delay_values Значения задержек в миллисекундах
     * @return Пара (успех, сообщение)
     */
    std::pair<bool, std::string> setup_delay_filters(const std::string& scene_name,
                                                   const std::string& filter_prefix = "CensorAI Delay",
                                                   const std::vector<int>& delay_values = {});
    
    /**
     * @brief Возвращает список доступных сцен в OBS
     * @return Список имен сцен
     */
    std::vector<std::string> get_scenes();
    
    /**
     * @brief Проверяет, установлено ли соединение
     * @return true если соединение установлено, false в противном случае
     */
    bool is_connected() const { return connected; }
    
    /**
     * @brief Возвращает хост для подключения
     * @return Хост
     */
    std::string get_host() const { return host; }
    
    /**
     * @brief Возвращает порт для подключения
     * @return Порт
     */
    int get_port() const { return port; }
    
private:
    std::string host;
    int port;
    std::string password;
    bool connected;
    void* client; // Указатель на клиент WebSocket
};

/**
 * @brief Диалог настройки интеграции с OBS
 */
class OBSConfigDialog : public QDialog {
    Q_OBJECT
    
public:
    /**
     * @brief Конструктор
     * @param obs_integration Указатель на интеграцию с OBS
     * @param parent Родительский виджет
     */
    explicit OBSConfigDialog(OBSIntegration* obs_integration, QWidget* parent = nullptr);
    
private slots:
    /**
     * @brief Обновляет отображение общей задержки
     */
    void update_total_delay();
    
    /**
     * @brief Подключается к OBS WebSocket серверу
     */
    void connect_to_obs();
    
    /**
     * @brief Применяет фильтры задержки к выбранной сцене
     */
    void apply_delay_filters();
    
private:
    /**
     * @brief Инициализация пользовательского интерфейса
     */
    void init_ui();
    
private:
    OBSIntegration* obs_integration;
    
    // UI элементы
    QLineEdit* host_input;
    QSpinBox* port_input;
    QLineEdit* password_input;
    QPushButton* connect_button;
    QComboBox* scene_combo;
    QCheckBox* filter1_check;
    QCheckBox* filter2_check;
    QCheckBox* filter3_check;
    QSpinBox* filter1_delay;
    QSpinBox* filter2_delay;
    QSpinBox* filter3_delay;
    QLabel* total_delay_label;
    QPushButton* apply_button;
    QPushButton* close_button;
    QLabel* status_label;
};

} // namespace audiocensor

#endif // AUDIOCENSOR_OBS_DIALOG_H