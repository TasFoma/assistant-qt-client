// companion_server — безоконный «мозг» ассистента для серверной машины
// (там же, где llama-server). Хранит базу, выполняет команды, следит за
// напоминаниями, проксирует чат в нейросеть и синхронизирует клиентов
// (телефон и, в будущем, десктоп) по TCP-протоколу на порту 8765.
#include <QCoreApplication>
#include <QDateTime>
#include <QFile>
#include <QTextStream>

#include "chatmanager.h"
#include "commandparser.h"
#include "databasemanager.h"
#include "mentormanager.h"
#include "navigationmanager.h"
#include "plannermanager.h"
#include "syncserver.h"

static void logToAppLog(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    static QFile file("server.log");
    if (!file.isOpen() && !file.open(QIODevice::Append | QIODevice::Text))
        return;

    const char *level = type == QtDebugMsg    ? "DEBUG"
                      : type == QtInfoMsg     ? "INFO "
                      : type == QtWarningMsg  ? "WARN "
                      : type == QtCriticalMsg ? "ERROR"
                                              : "FATAL";
    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
                         + " [" + level + "] " + msg;
    QTextStream out(&file);
    out << line << "\n";
    out.flush();
    fprintf(stderr, "%s\n", qPrintable(line));
}

int main(int argc, char *argv[])
{
    qInstallMessageHandler(logToAppLog);

    QCoreApplication app(argc, argv);
    app.setApplicationName("MyCompanion");
    app.setOrganizationName("MyCompanion");

    // База лежит рядом с exe: сервер переносим одной папкой,
    // никаких скрытых копий в профиле пользователя
    if (!DatabaseManager::instance().init(QCoreApplication::applicationDirPath()))
        return -1;

    // Показать токен для настройки клиентов: companion_server.exe --show-token
    if (app.arguments().contains("--show-token")) {
        qInfo() << "sync_token:"
                << DatabaseManager::instance().setting("sync_token");
        return 0;
    }

    ChatManager       chatManager;
    PlannerManager    plannerManager;
    MentorManager     mentorManager;
    NavigationManager navigationManager;
    SyncServer        syncServer(&plannerManager, &chatManager, &navigationManager);

    // На серверной машине llama-server обычно рядом — localhost
    auto &dbm = DatabaseManager::instance();
    if (dbm.setting("llama_url").isEmpty())
        dbm.setSetting("llama_url", "http://localhost:8080");

    if (!syncServer.start())
        return -1;

    // Гео-напоминания без координат получают их через Nominatim
    navigationManager.geocodePendingReminders();
    QObject::connect(&plannerManager, &PlannerManager::remindersChanged,
                     &navigationManager, &NavigationManager::geocodePendingReminders);

    qInfo() << "companion_server запущен. Порт:" << syncServer.port()
            << "Токен:" << syncServer.token();

    return app.exec();
}
