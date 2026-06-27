#include "voiceinput.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDebug>
#include <QProcessEnvironment>

static const char PYTHON_EXE[] = "C:/Users/nasty/AppData/Local/Python/bin/python.exe";

VoiceInput::VoiceInput(QObject *parent) : QObject(parent)
{
    startDaemon();
}

VoiceInput::~VoiceInput()
{
    if (m_daemon && m_daemon->state() != QProcess::NotRunning) {
        sendCommand("EXIT");
        m_daemon->waitForFinished(2000);
        m_daemon->kill();
    }
    if (m_speakProcess && m_speakProcess->state() != QProcess::NotRunning) {
        m_speakProcess->kill();
        m_speakProcess->waitForFinished(2000);
    }
}

bool VoiceInput::isRecording() const { return m_isRecording; }
bool VoiceInput::isReady() const     { return m_isReady; }

void VoiceInput::setRecording(bool value)
{
    if (m_isRecording == value) return;
    m_isRecording = value;
    emit isRecordingChanged();
}

void VoiceInput::setReady(bool value)
{
    if (m_isReady == value) return;
    m_isReady = value;
    emit isReadyChanged();
}

QString VoiceInput::findScript(const QString &name)
{
    QString candidate = QCoreApplication::applicationDirPath() + "/" + name;
    if (QFileInfo::exists(candidate))
        return candidate;
    return name;
}

bool VoiceInput::isVoskLog(const QString &line)
{
    return line.startsWith("LOG (");
}

void VoiceInput::sendCommand(const QString &cmd)
{
    if (!m_daemon || m_daemon->state() != QProcess::Running) return;
    m_daemon->write((cmd + "\n").toUtf8());
}

void VoiceInput::startDaemon()
{
    m_daemon = new QProcess(this);

    // Recognized text arrives on stdout.
    connect(m_daemon, &QProcess::readyReadStandardOutput, this, [this]() {
        QString text = QString::fromUtf8(m_daemon->readAllStandardOutput()).trimmed();
        if (!text.isEmpty()) {
            emit textRecognized(text);
            // Recording ends automatically after recognition; stopRecording cleans up UI.
            if (m_isRecording) {
                if (m_watchdog) { m_watchdog->stop(); }
                setRecording(false);
            }
        }
    });

    // Stderr: parse protocol messages and filter Vosk noise.
    connect(m_daemon, &QProcess::readyReadStandardError, this, [this]() {
        QString raw = QString::fromUtf8(m_daemon->readAllStandardError());
        for (QString line : raw.split('\n')) {
            line = line.trimmed().remove('\r');
            if (line.isEmpty() || isVoskLog(line)) continue;

            if (line == "READY") {
                setReady(true);
                if (m_pendingStart) {
                    m_pendingStart = false;
                    // User already pressed mic — start recording now.
                    sendCommand("START");
                    startWatchdog();
                }
                continue;
            }

            if (line == "DONE") {
                // Session ended (recognition or STOP).
                if (m_watchdog) m_watchdog->stop();
                setRecording(false);
                continue;
            }

            if (line.startsWith("Error:") || line.startsWith("Microphone:")) {
                if (line.startsWith("Error:")) {
                    qWarning() << "[VoiceInput]" << line;
                    emit errorOccurred(line);
                }
                continue;
            }

            // Unexpected line — buffer it in case the process crashes.
            m_stderrBuffer += line + "\n";
        }
    });

    // Handle daemon crash.
    connect(m_daemon, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus) {
        if (m_watchdog) { m_watchdog->stop(); }
        setRecording(false);
        setReady(false);

        if (exitCode != 0 && !m_stderrBuffer.isEmpty()) {
            qWarning() << "[VoiceInput] crash:" << m_stderrBuffer.trimmed();
            emit errorOccurred(m_stderrBuffer.trimmed());
        }
        m_stderrBuffer.clear();
        m_daemon->deleteLater();
        m_daemon = nullptr;
    });

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONUTF8", "1");
    m_daemon->setProcessEnvironment(env);

    m_daemon->start(PYTHON_EXE, QStringList() << findScript("voice_daemon.py"));

    if (!m_daemon->waitForStarted(3000)) {
        emit errorOccurred("Не удалось запустить Python. Проверьте путь к python.exe.");
        m_daemon->deleteLater();
        m_daemon = nullptr;
    }
}

void VoiceInput::startWatchdog()
{
    if (!m_watchdog) {
        m_watchdog = new QTimer(this);
        m_watchdog->setSingleShot(true);
        connect(m_watchdog, &QTimer::timeout, this, [this]() {
            emit errorOccurred("Таймаут: речь не распознана за 30 сек");
            stopRecording();
        });
    }
    m_watchdog->start(30000);
}

void VoiceInput::startRecording()
{
    if (m_isRecording) return;

    setRecording(true);
    m_stderrBuffer.clear();

    if (!m_isReady) {
        // Daemon still loading the model — queue the start.
        m_pendingStart = true;
        return;
    }

    sendCommand("START");
    startWatchdog();
}

void VoiceInput::stopRecording()
{
    if (!m_isRecording) return;
    setRecording(false);
    m_pendingStart = false;

    if (m_watchdog) m_watchdog->stop();
    sendCommand("STOP");
}

void VoiceInput::speak(const QString &text)
{
    if (text.isEmpty()) return;

    if (m_speakProcess && m_speakProcess->state() != QProcess::NotRunning)
        m_speakProcess->kill();

    QString escaped = text;
    escaped.replace("'", "''");
    escaped.replace("\"", "`\"");

    QString command = QString(
        "Add-Type -AssemblyName System.Speech; "
        "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer; "
        "$rv = $s.GetInstalledVoices() | Where-Object { $_.VoiceInfo.Culture.Name -match '^ru' } | Select-Object -First 1; "
        "if ($rv) { $s.SelectVoice($rv.VoiceInfo.Name) }; "
        "$s.Rate = 0; "
        "$s.Speak('%1')"
    ).arg(escaped);

    auto *proc = new QProcess(this);
    m_speakProcess = proc;
    connect(proc, &QProcess::finished, proc, &QProcess::deleteLater);
    proc->start("powershell.exe", QStringList() << "-NoProfile" << "-NonInteractive" << "-Command" << command);
}
