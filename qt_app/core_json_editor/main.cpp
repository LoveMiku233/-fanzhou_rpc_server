#include "core_json_editor_window.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Core JSON Editor"));
    app.setApplicationVersion(QStringLiteral("1.0.0"));

    CoreJsonEditorWindow w;
    w.show();
    return app.exec();
}

