#!/usr/bin/env python3
"""
Voice listener: records from mic in real-time, outputs recognized phrase to stdout.
Requires: pip install sounddevice vosk
"""
import sys
import json
import queue
import vosk
import sounddevice as sd

SAMPLE_RATE = 16000
VOSK_MODEL_PATH = "C:/vosk-model-ru/vosk-model-ru-0.42"


def log(msg):
    sys.stderr.write(msg + "\n")
    sys.stderr.flush()


def main():
    # Show which microphone will be used
    try:
        dev = sd.query_devices(kind="input")
        log(f"Microphone: {dev['name']} (rate={int(dev['default_samplerate'])})")
    except Exception as e:
        log(f"Warning: cannot query mic: {e}")

    try:
        model = vosk.Model(VOSK_MODEL_PATH)
    except Exception as e:
        log(f"Model load error: {e}")
        sys.exit(1)

    rec = vosk.KaldiRecognizer(model, SAMPLE_RATE)
    audio_q = queue.Queue()

    def callback(indata, frames, time, status):
        if status:
            log(f"Audio status: {status}")
        audio_q.put(bytes(indata))

    try:
        with sd.RawInputStream(
            samplerate=SAMPLE_RATE,
            blocksize=4000,
            dtype="int16",
            channels=1,
            callback=callback,
        ):
            log("READY")

            while True:
                data = audio_q.get()
                if rec.AcceptWaveform(data):
                    result = json.loads(rec.Result())
                    text = result.get("text", "").strip()
                    if text:
                        print(text, flush=True)
                        sys.exit(0)

    except KeyboardInterrupt:
        final = json.loads(rec.FinalResult())
        text = final.get("text", "").strip()
        if text:
            print(text, flush=True)
        sys.exit(0)
    except Exception as e:
        log(f"Error: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()
