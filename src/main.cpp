#include <QApplication>
#include <QStyleFactory>
#include <QStyleHints>
#include <QPalette>
#include "widgets/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("YOLO Annotator");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("YoloAnnotator");

    // 使用Fusion样式，跨平台一致
    app.setStyle(QStyleFactory::create("Fusion"));

    // 自适应系统主题
    auto applyTheme = [&app]() {
        bool isDark = app.styleHints()->colorScheme() == Qt::ColorScheme::Dark;

        if (isDark) {
            QPalette darkPalette;
            darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
            darkPalette.setColor(QPalette::WindowText, Qt::white);
            darkPalette.setColor(QPalette::Base, QColor(42, 42, 42));
            darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
            darkPalette.setColor(QPalette::ToolTipBase, QColor(53, 53, 53));
            darkPalette.setColor(QPalette::ToolTipText, Qt::white);
            darkPalette.setColor(QPalette::Text, Qt::white);
            darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
            darkPalette.setColor(QPalette::ButtonText, Qt::white);
            darkPalette.setColor(QPalette::BrightText, Qt::red);
            darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
            darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
            darkPalette.setColor(QPalette::HighlightedText, Qt::black);
            app.setPalette(darkPalette);
        } else {
            app.setPalette(app.style()->standardPalette());
        }
    };

    applyTheme();
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged, [&]() {
        applyTheme();
    });

    MainWindow window;
    window.show();

    return app.exec();
}
