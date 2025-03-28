#ifndef AUDIOCENSOR_MAIN_WINDOW_H
#define AUDIOCENSOR_MAIN_WINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QTextEdit>
#include <QTimer>
#include <QMenu>
#include <QAction>
#include <QString>
#include <QMenuBar>

#include <memory>

namespace audiocensor {

// Предварительные объявления классов
class LicenseManager;
class ConfigManager;
class AudioProcessor;
class WordDetector;
class OBSIntegration;

/**
 * @brief Главное окно приложения
 */
class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    /**
     * @brief Конструктор
     * @param parent Родительский виджет
     */
    explicit MainWindow(QWidget* parent = nullptr);
    
    /**
     * @brief Деструктор
     */
    ~MainWindow();
    
    /**
     * @brief Проверяет лицензию при запуске приложения
     * @return true если лицензия валидна или активирована, false в противном случае
     */
    bool check_license_on_launch();
    
protected:
    /**
     * @brief Обработка закрытия приложения
     * @param event Событие закрытия
     */
    void closeEvent(QEvent* event);
    
private slots:
    /**
     * @brief Показывает диалог активации лицензии
     */
    void show_license_dialog();
    
    /**
     * @brief Показывает информацию о лицензии
     * @param show_dialog Если true, показывает диалог, иначе только обновляет метки
     */
    void show_license_info(bool show_dialog = false);
    
    /**
     * @brief Открывает ссылку для покупки лицензии
     */
    void buy_license();
    
    /**
     * @brief Запускает обработку аудио
     */
    void start_processing();
    
    /**
     * @brief Останавливает обработку аудио
     */
    void stop_processing();
    
    /**
     * @brief Приостанавливает/возобновляет обработку аудио
     */
    void toggle_pause();
    
    /**
     * @brief Очищает лог событий
     */
    void clear_log();
    
    /**
     * @brief Сохраняет лог событий в файл
     */
    void save_log();
    
    /**
     * @brief Загружает список запрещённых слов из API
     * @return true если загрузка прошла успешно, false в противном случае
     */
    bool load_words_from_api();
    
    /**
     * @brief Обновляет статусную строку
     */
    void update_status();
    
    /**
     * @brief Обрабатывает сигнал об обнаружении запрещенного слова
     * @param word Обнаруженное слово
     * @param start_time Время начала
     * @param end_time Время конца
     */
    void word_detected(const QString& word, double start_time, double end_time);
    
    /**
     * @brief Обновляет информацию о состоянии буфера
     * @param current Текущий размер буфера
     * @param maximum Максимальный размер буфера
     */
    void update_buffer_status(int current, int maximum);
    
    /**
     * @brief Показывает диалог настройки интеграции с OBS
     */
    void show_obs_config_dialog();
    
    /**
     * @brief Добавляет сообщение в лог
     * @param message Сообщение для добавления
     */
    void add_log_message(const QString& message);
    
    /**
     * @brief Обновляет список устройств
     * @param devices Список устройств
     */
    void update_device_list(const QList<QPair<int, QString>>& devices);
    
private:
    /**
     * @brief Инициализация пользовательского интерфейса
     */
    void init_ui();
    
    /**
     * @brief Подключение сигналов к обработчикам
     */
    void connect_signals();
    
    /**
     * @brief Инициализация аудио подсистемы
     */
    void init_audio();
    
    /**
     * @brief Настройка меню для управления лицензией
     */
    void setup_license_menu();
    
    /**
     * @brief Настройка меню для интеграции с внешними сервисами
     */
    void setup_integration_menu();
    
    /**
     * @brief Инициализация интеграций с внешними сервисами
     */
    void initialize_integrations();
    
private:
    // Менеджеры
    std::unique_ptr<LicenseManager> license_manager;
    std::unique_ptr<ConfigManager> config_manager;
    std::unique_ptr<AudioProcessor> audio_processor;
    std::unique_ptr<OBSIntegration> obs_integration;
    
    // Состояние приложения
    bool running;
    int input_device_index;
    int output_device_index;
    int detections_count;
    
    // UI элементы
    QPushButton* activate_license_button;
    QPushButton* start_button;
    QPushButton* stop_button;
    QPushButton* pause_button;
    QComboBox* input_device_combo;
    QComboBox* output_device_combo;
    QTextEdit* log_text;
    QPushButton* clear_log_button;
    QPushButton* save_log_button;
    
    // Метки статуса
    QLabel* license_status_label;
    QLabel* buffer_label;
    QLabel* detections_label;
    
    // Таймер обновления статуса
    QTimer* status_timer;
};

} // namespace audiocensor

#endif // AUDIOCENSOR_MAIN_WINDOW_H