#ifndef AUDIOCENSOR_LICENSE_DIALOG_H
#define AUDIOCENSOR_LICENSE_DIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>

namespace audiocensor {

    // Предварительное объявление класса
    class LicenseManager;

    /**
     * @brief Диалог для активации лицензии
     */
    class LicenseDialog : public QDialog {
        Q_OBJECT

    public:
        /**
         * @brief Конструктор
         * @param license_manager Указатель на менеджер лицензий
         * @param parent Родительский виджет
         */
        explicit LicenseDialog(LicenseManager* license_manager, QWidget* parent = nullptr);

        private slots:
            /**
             * @brief Активирует пробный период
             */
            void activate_trial();

        /**
         * @brief Открывает Telegram с указанным контактом
         */
        void open_telegram();

        /**
         * @brief Активация введенного ключа
         */
        void activate_license();

    private:
        /**
         * @brief Инициализация пользовательского интерфейса
         */
        void init_ui();

    private:
        LicenseManager* license_manager;
        bool trial_active;
        bool has_license;

        // UI элементы
        QLineEdit* key_input;
        QPushButton* activate_button;
        QPushButton* trial_button;
        QPushButton* telegram_button;
        QPushButton* exit_button;
    };

    /**
     * @brief Диалог отображения информации о лицензии
     */
    class LicenseInfoDialog : public QDialog {
        Q_OBJECT

    public:
        /**
         * @brief Конструктор
         * @param license_manager Указатель на менеджер лицензий
         * @param parent Родительский виджет
         */
        explicit LicenseInfoDialog(LicenseManager* license_manager, QWidget* parent = nullptr);

    private:
        /**
         * @brief Инициализация пользовательского интерфейса
         */
        void init_ui();

    private:
        LicenseManager* license_manager;
    };

} // namespace audiocensor

#endif // AUDIOCENSOR_LICENSE_DIALOG_H