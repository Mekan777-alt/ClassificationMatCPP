#include <QApplication>
#include <QMessageBox>
#include <iostream>

#include "audiocensor/license_manager.h"
#include "ui/main_window.h"

int main(int argc, char *argv[]) {
    try {
        QApplication app(argc, argv);

        // Инициализация основных компонентов
        audiocensor::MainWindow window;
        window.show();

        // Проверяем лицензию
        if (window.check_license_on_launch()) {
            return app.exec();
        } else {
            return 0; // Выход, если лицензия не активирована
        }
    } catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << std::endl;
        QMessageBox::critical(nullptr, "Критическая ошибка",
                             QString("Произошла критическая ошибка при запуске приложения: %1").arg(e.what()));
        return 1;
    }
}