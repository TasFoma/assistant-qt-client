#!/usr/bin/env python3
"""
Voice daemon — long-lived process, loads model once.

stdin  protocol (from Qt):
  START  → begin one recording session
  STOP   → end current session early
  EXIT   → shut down

stdout (to Qt):
  <recognized text>  — one phrase per line

stderr (informational, to Qt):
  READY  — model loaded, daemon is ready for START commands
  DONE   — recording session ended (after START/STOP cycle)
  LOG …  — Vosk internal logs (filtered in C++)
  Error: … — fatal errors
"""
import sys
sys.stdout.reconfigure(encoding='utf-8')
sys.stderr.reconfigure(encoding='utf-8')

import json
import queue
import threading
import vosk
import sounddevice as sd

SAMPLE_RATE = 16000
VOSK_MODEL_PATH = "C:/vosk-model-ru/vosk-model-ru-0.42"


def log(msg: str):
    sys.stderr.write(msg + "\n")
    sys.stderr.flush()


def main():
    # Show which microphone will be used.
    try:
        dev = sd.query_devices(kind="input")
        log(f"Microphone: {dev['name']} ({int(dev['default_samplerate'])} Hz)")
    except Exception as e:
        log(f"Warning: cannot query mic: {e}")

    # Load model once.
    try:
        model = vosk.Model(VOSK_MODEL_PATH)
    except Exception as e:
        log(f"Error: model load failed: {e}")
        sys.exit(1)

    log("READY")

    # Read stdin commands in a background thread so the main thread never blocks.
    commands: queue.Queue = queue.Queue()

    def stdin_reader():
        try:
            for line in sys.stdin:
                commands.put(line.strip())
        except Exception:
            pass
        commands.put("EXIT")

    threading.Thread(target=stdin_reader, daemon=True).start()

    # Main command loop.
    while True:
        cmd = commands.get()
        if cmd == "EXIT" or cmd == "":
            break
        if cmd != "START":
            continue

        _record_once(model, commands)


def _record_once(model, commands: queue.Queue):
    """Run one recording session until a phrase is recognized or STOP arrives."""
    rec = vosk.KaldiRecognizer(model, SAMPLE_RATE)
    audio_q: queue.Queue = queue.Queue()
    done = threading.Event()
    recognized = [""]

    def audio_callback(indata, frames, time, status):
        if not done.is_set():
            audio_q.put(bytes(indata))

    try:
        stream = sd.RawInputStream(
            samplerate=SAMPLE_RATE,
            blocksize=4000,
            dtype="int16",
            channels=1,
            callback=audio_callback,
        )
        stream.start()
    except Exception as e:
        log(f"Error: cannot open microphone: {e}")
        log("DONE")
        return

    try:
        while not done.is_set():
            # Check for STOP/EXIT command (non-blocking).
            try:
                cmd = commands.get_nowait()
                if cmd in ("STOP", "EXIT", "START"):
                    # Put START back so the outer loop sees it after DONE.
                    if cmd == "START":
                        commands.put("START")
                    done.set()
                    break
            except queue.Empty:
                pass

            # Process audio.
            try:
                data = audio_q.get(timeout=0.05)
            except queue.Empty:
                continue

            if rec.AcceptWaveform(data):
                text = json.loads(rec.Result()).get("text", "").strip()
                if text:
                    recognized[0] = text
                    done.set()
    finally:
        stream.stop()
        stream.close()

    # Grab any leftover partial result.
    if not recognized[0]:
        text = json.loads(rec.FinalResult()).get("text", "").strip()
        recognized[0] = text

    if recognized[0]:
        print(recognized[0], flush=True)

    log("DONE")


if __name__ == "__main__":
    main()
