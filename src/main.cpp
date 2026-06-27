#include <windows.h>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include "llamaclient.h"
#include "voiceinput.h"

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    // Регистрируем LlamaClient
    LlamaClient client;
    engine.rootContext()->setContextProperty("llamaClient", &client);

    // Регистрируем VoiceInput
    VoiceInput voiceInput;
    engine.rootContext()->setContextProperty("voiceInput", &voiceInput);

    engine.load(QUrl("qrc:/src/main.qml"));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}