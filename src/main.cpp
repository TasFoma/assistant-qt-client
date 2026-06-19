#include <windows.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "llamaclient.h"

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    LlamaClient client;
    engine.rootContext()->setContextProperty("llamaClient", &client);

    engine.load(QUrl("qrc:/src/main.qml"));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}