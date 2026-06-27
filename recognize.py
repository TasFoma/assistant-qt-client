import sys
import json
import wave
import vosk

if len(sys.argv) < 2:
    print("")
    sys.exit(1)

wav_path = sys.argv[1]
model = vosk.Model("C:/vosk-model-ru")

wf = wave.open(wav_path, "rb")
rec = vosk.KaldiRecognizer(model, wf.getframerate())
result_text = ""

while True:
    data = wf.readframes(4000)
    if len(data) == 0:
        break
    if rec.AcceptWaveform(data):
        result = json.loads(rec.Result())
        result_text += result.get("text", "") + " "

final = json.loads(rec.FinalResult())
result_text += final.get("text", "")
print(result_text.strip())