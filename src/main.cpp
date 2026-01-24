#include <QApplication>
#include <QStyleFactory>
#include <QStyleHints>
#include <QPalette>
#include "widgets/MainWindow.h"

// Qt 6.5+ 才有 colorScheme API
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#define HAS_COLOR_SCHEME 1
#endif

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("YOLO Annotator");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("YoloAnnotator");

    // 使用Fusion样式，跨平台一致
    app.setStyle(QStyleFactory::create("Fusion"));

    // 自适应系统主题
    auto applyTheme = [&app]() {
#ifdef HAS_COLOR_SCHEME
        bool isDark = app.styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
        // Qt 6.5 以下：检测系统调色板亮度
        QPalette sysPalette = app.palette();
        bool isDark = sysPalette.color(QPalette::Window).lightness() < 128;
#endif

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
#ifdef HAS_COLOR_SCHEME
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged, [&]() {
        applyTheme();
    });
#endif

    MainWindow window;
    window.show();

    return app.exec();
}
