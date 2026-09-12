import sounddevice as sd
from scipy.io.wavfile import write

from predict import detect_cry


SAMPLE_RATE = 44100
DURATION = 5

AUDIO_FILE = "live_recording.wav"


def record_and_detect():

    print("🎤 Recording...")

    audio = sd.rec(
        int(DURATION * SAMPLE_RATE),
        samplerate=SAMPLE_RATE,
        channels=1
    )

    sd.wait()

    write(
        AUDIO_FILE,
        SAMPLE_RATE,
        audio
    )

    print("✅ Recording finished!")

    # Cry detection
    prediction, confidence, sound_percentage = detect_cry(
        AUDIO_FILE
    )

    return prediction, confidence, sound_percentage

