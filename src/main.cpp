// qt_client — десктопный клиент ассистента «Мой спутник».
// Все данные и логика живут на companion_server (машина с llama-server);
// здесь только интерфейс, голос и уведомления.
#include <windows.h>
#include <mmsystem.h>
#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#include <QTextStream>
#include <QClipboard>

#include "musicplayer.h"
#include "remotebackend.h"
#include "voiceinput.h"

// Все qDebug/qInfo/qWarning/qCritical пишутся в app.log рядом с exe
static void logToAppLog(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    static QFile file("app.log");
    if (!file.isOpen() && !file.open(QIODevice::Append | QIODevice::Text))
        return;

    const char *level = type == QtDebugMsg    ? "DEBUG"
                      : type == QtInfoMsg     ? "INFO "
                      : type == QtWarningMsg  ? "WARN "
                      : type == QtCriticalMsg ? "ERROR"
                                              : "FATAL";
    QTextStream out(&file);
    out << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
        << " [" << level << "] " << msg << "\n";
    out.flush();

    fprintf(stderr, "[%s] %s\n", level, qPrintable(msg));
}

// Иконка приложения для трея: 🧠 на прозрачном фоне
static QIcon makeTrayIcon()
{
    QPixmap pm(64, 64);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setFont(QFont("Segoe UI Emoji", 40));
    p.drawText(pm.rect(), Qt::AlignCenter, QStringLiteral("🧠"));
    p.end();
    return QIcon(pm);
}

int main(int argc, char *argv[])
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    qInstallMessageHandler(logToAppLog);
    qputenv("QT_QUICK_CONTROLS_STYLE", "Fusion");

    QApplication app(argc, argv);
    app.setApplicationName("MyCompanionClient");
    app.setOrganizationName("MyCompanion");

    RemoteBackend backend;
    VoiceInput    voiceInput;
    MusicPlayer   musicPlayer;

    // Трей: уведомления Windows о напоминаниях + токен
    QSystemTrayIcon tray(makeTrayIcon());
    tray.setToolTip(QStringLiteral("🧠 Мой спутник"));

    QMenu trayMenu;
    QAction *tokenAction =
        trayMenu.addAction(QStringLiteral("📋 Скопировать токен синхронизации"));
    QObject::connect(tokenAction, &QAction::triggered, &tray, [&backend, &tray]() {
        QApplication::clipboard()->setText(backend.token());
        tray.showMessage(QStringLiteral("🔑 Токен скопирован"),
                         QStringLiteral("Вставь его в настройки приложения на телефоне"),
                         QSystemTrayIcon::Information, 8000);
    });
    QAction *quitAction = trayMenu.addAction(QStringLiteral("Выход"));
    QObject::connect(quitAction, &QAction::triggered, &app, &QApplication::quit);
    tray.setContextMenu(&trayMenu);
    tray.show();

    QObject::connect(&backend, &RemoteBackend::reminderDue, &tray,
                     [&tray](const QString &title) {
                         tray.showMessage(QStringLiteral("⏰ Напоминание"), title,
                                          QSystemTrayIcon::Information, 15000);
                         PlaySoundW(L"SystemExclamation", nullptr,
                                    SND_ALIAS | SND_ASYNC);
                     });

    QQmlApplicationEngine engine;
    QQmlContext *ctx = engine.rootContext();
    // Один RemoteBackend отвечает на все прежние имена — QML почти не меняется
    ctx->setContextProperty("backend",           &backend);
    ctx->setContextProperty("plannerManager",    &backend);
    ctx->setContextProperty("chatManager",       &backend);
    ctx->setContextProperty("navigationManager", &backend);
    ctx->setContextProperty("voiceInput",        &voiceInput);
    ctx->setContextProperty("musicPlayer",       &musicPlayer);

    engine.load(QUrl("qrc:/src/qml/Main.qml"));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
