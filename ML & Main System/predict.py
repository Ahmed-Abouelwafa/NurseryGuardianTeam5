import librosa
import numpy as np
import joblib
import pandas as pd
import webrtcvad


# ==========================================
# Load trained model
# ==========================================

model = joblib.load("cry_model_weighted_v3.pkl")


# ==========================================
# VAD Settings
# ==========================================

VAD_SAMPLE_RATE = 16000
FRAME_DURATION = 30  # milliseconds

FRAME_SIZE = int(
    VAD_SAMPLE_RATE * FRAME_DURATION / 1000
)

vad = webrtcvad.Vad()
vad.set_mode(2)


# ==========================================
# Check if there is sound
# ==========================================

def has_sound(audio_file):

    audio, sample_rate = librosa.load(
        audio_file,
        sr=VAD_SAMPLE_RATE,
        mono=True
    )

    # Convert audio to int16
    audio_int16 = (
        audio * 32767
    ).astype(np.int16)

    audio_bytes = audio_int16.tobytes()

    frame_bytes = FRAME_SIZE * 2

    speech_frames = 0
    total_frames = 0

    # Analyze audio frame by frame
    for start in range(
        0,
        len(audio_bytes) - frame_bytes,
        frame_bytes
    ):

        frame = audio_bytes[
            start:start + frame_bytes
        ]

        is_speech = vad.is_speech(
            frame,
            VAD_SAMPLE_RATE
        )

        total_frames += 1

        if is_speech:
            speech_frames += 1

    # Avoid division by zero
    if total_frames == 0:
        return False, 0

    speech_percentage = (
        speech_frames / total_frames
    ) * 100

    # Current threshold
    sound_detected = speech_percentage > 10

    return sound_detected, speech_percentage


# ==========================================
# Cry Detection
# ==========================================
def detect_cry(audio_file):
    # Step 1: VAD
    sound_detected, speech_percentage = has_sound(audio_file)

    if not sound_detected:
        return (
            "NO_SOUND",
            {},
            speech_percentage
        )

    # Step 2: Load audio for ML
    audio, sample_rate = librosa.load(
        audio_file,
        sr=44100,
        mono=True
    )

    # Step 3: MFCC
    mfcc = librosa.feature.mfcc(
        y=audio,
        sr=sample_rate,
        n_mfcc=13
    )

    mfcc_mean = np.mean(mfcc, axis=1)
    mfcc_std = np.std(mfcc, axis=1)

    # Step 4: RMS
    rms = np.mean(
        librosa.feature.rms(y=audio)
    )

    # Step 5: Zero Crossing Rate
    zcr = np.mean(
        librosa.feature.zero_crossing_rate(audio)
    )

    # Step 6: Duration
    duration = len(audio) / sample_rate

    # Step 7: Combine features
    features = np.concatenate([
        mfcc_mean,
        mfcc_std,
        [rms],
        [zcr],
        [duration]
    ])

    features = features.reshape(1, -1)

    # Add the feature names used during training
    feature_names = model.feature_names_in_

    features = pd.DataFrame(
        features,
        columns=feature_names
    )

    # Step 8: ML Prediction
    prediction = model.predict(features)[0]

    # Step 9: Prediction Probabilities
    probabilities = model.predict_proba(features)[0]

    classes = model.classes_

    confidence = {}

    for class_name, probability in zip(
        classes,
        probabilities
    ):
        confidence[class_name] = probability * 100

    # Step 10: Return results
    return (
        prediction,
        confidence,
        speech_percentage
    )