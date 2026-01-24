#include <QApplication>
#include "widgets/MainWindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    app.setApplicationName("YOLO Annotator");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("YoloAnnotator");

    MainWindow window;
    window.show();

    return app.exec();
}
