#include "voiceinput.h"
#include <QCoreApplication>
#include <QFileInfo>
#include <QDebug>
#include <QProcessEnvironment>
#include <QStringConverter>

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
bool VoiceInput::isReady()     const { return m_isReady; }
bool VoiceInput::isSpeaking()  const { return m_isSpeaking; }

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

void VoiceInput::setSpeaking(bool value)
{
    if (m_isSpeaking == value) return;
    m_isSpeaking = value;
    emit isSpeakingChanged();
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

// UTF-16LE Base64 для -EncodedCommand
static QString toEncodedPS(const QString &script)
{
    QStringEncoder enc(QStringEncoder::Utf16LE);
    const QByteArray utf16le = enc.encode(script);
    return QString::fromLatin1(utf16le.toBase64());
}

void VoiceInput::stopSpeaking()
{
    if (m_speakProcess) {
        QProcess *old = m_speakProcess;
        m_speakProcess = nullptr;
        old->disconnect();
        if (old->state() != QProcess::NotRunning) {
            old->kill();
            connect(old, &QProcess::finished, old, &QProcess::deleteLater);
        } else {
            old->deleteLater();
        }
    }
    setSpeaking(false);
}

void VoiceInput::speak(const QString &text, int rate)
{
    if (text.isEmpty()) return;

    stopSpeaking(); // kill any ongoing speech first

    // Escape single quotes; everything else is literal in PS single-quoted strings
    const QString escaped = QString(text).replace("'", "''");
    const QString rateStr  = QString::number(qBound(-10, rate, 10));

    // Voice selection priority:
    //   1. Russian male  (Pavel — if installed via Windows language settings)
    //   2. Any male      (David / Mark / etc.)
    //   3. Russian female (Irina — usually pre-installed)
    //   4. First available
    // SSML prosody adds slight pitch variation for a less robotic feel.
    const QString script = QString(
        "Add-Type -AssemblyName System.Speech\n"
        "$s = New-Object System.Speech.Synthesis.SpeechSynthesizer\n"
        "$vs = $s.GetInstalledVoices() | Where-Object { $_.Enabled }\n"
        "$v  = $vs | Where-Object { $_.VoiceInfo.Culture.Name -match '^ru' -and $_.VoiceInfo.Gender -eq 'Male'   } | Select-Object -First 1\n"
        "if (-not $v) { $v = $vs | Where-Object { $_.VoiceInfo.Gender -eq 'Male'   } | Select-Object -First 1 }\n"
        "if (-not $v) { $v = $vs | Where-Object { $_.VoiceInfo.Culture.Name -match '^ru' } | Select-Object -First 1 }\n"
        "if (-not $v) { $v = $vs | Select-Object -First 1 }\n"
        "if ($v) { $s.SelectVoice($v.VoiceInfo.Name) }\n"
        "$s.Rate = %2\n"
        // Try SSML for slightly more natural prosody; fall back to plain Speak on error
        "$xml = '<speak version=\"1.0\" xmlns=\"http://www.w3.org/2001/10/synthesis\" xml:lang=\"ru-RU\">"
                "<prosody pitch=\"+2%\">%1</prosody></speak>'\n"
        "try { $s.SpeakSsml($xml) } catch { $s.Speak('%1') }"
    ).arg(escaped, rateStr);

    auto *proc = new QProcess(this);
    m_speakProcess = proc;
    setSpeaking(true);

    connect(proc, &QProcess::finished, this, [this, proc] {
        if (m_speakProcess == proc) {
            m_speakProcess = nullptr;
            setSpeaking(false);
        }
        proc->deleteLater();
    });

    proc->start("powershell.exe",
                QStringList{"-NonInteractive", "-NoProfile",
                            "-EncodedCommand", toEncodedPS(script)});
}
