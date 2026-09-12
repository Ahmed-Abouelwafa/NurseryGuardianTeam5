import tkinter as tk
import threading
import sounddevice as sd
import cv2
import time
from scipy.io.wavfile import read
import serial
import csv
import os
import requests

from predict import detect_cry

CREDENTIALS_FILE = "credentials.csv"
SAMPLE_RATE = 44100
DURATION = 5
WAIT_BETWEEN_FILES = 15
SERIAL_PORT = "COM9"
BAUD_RATE = 9600

def load_telegram_credentials():
    token, chat_id = None, None
    if os.path.exists(CREDENTIALS_FILE):
        try:
            with open(CREDENTIALS_FILE, mode="r", encoding="utf-8") as f:
                reader = csv.DictReader(f)
                row = next(reader, None)
                if row:
                    token = row.get("username", "").strip()
                    chat_id = row.get("password", "").strip()
            print("Telegram Credentials Loaded Successfully!")
        except Exception as e:
            print("Error reading credentials file:", e)
    return token, chat_id

TELEGRAM_TOKEN, TELEGRAM_CHAT_ID = load_telegram_credentials()

def send_telegram_alert(message_text):
    if not TELEGRAM_TOKEN or not TELEGRAM_CHAT_ID:
        print("Telegram missing credentials!")
        root.after(0, lambda: telegram_status.config(text="Telegram: Missing Credentials"))
        return

    def worker():
        try:
            url = f"https://api.telegram.org/bot{TELEGRAM_TOKEN}/sendMessage"
            payload = {"chat_id": TELEGRAM_CHAT_ID, "text": message_text}
            res = requests.post(url, json=payload, timeout=5)
            if res.status_code == 200:
                root.after(0, lambda: telegram_status.config(text="Telegram: Gas Alert Sent!"))
            else:
                root.after(0, lambda: telegram_status.config(text="Telegram: Failed"))
        except Exception as e:
            print("Telegram send error:", e)
            root.after(0, lambda: telegram_status.config(text="Telegram: Error"))

    threading.Thread(target=worker, daemon=True).start()

root = tk.Tk()
root.title("Smart Nursery Guardian")
root.geometry("800x720")
root.minsize(700, 600)

HUNGRY_AUDIO = "hungry.wav"
TIRED_AUDIO = "tired.wav"
DISCOMFORT_AUDIO = "discomfort.wav"

try:
    stm32 = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
    print("STM32 connected")
except Exception as e:
    stm32 = None
    print("STM32 connection error:", e)

def send_command(command):
    if stm32 and stm32.is_open:
        try:
            stm32.write((command + "\n").encode())
            print("TX:", command)
        except Exception as e:
            print("Serial send error:", e)

def play_response_video():
    video_path = "hungry.mp4"
    if not os.path.exists(video_path):
        print("Video not found!")
        return

    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print("Could not open video!")
        return

    start_time = time.time()
    while time.time() - start_time < 5:
        ret, frame = cap.read()
        if not ret:
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            continue

        cv2.imshow("Baby Response", frame)

        if cv2.waitKey(20) & 0xFF == ord("q"):
            break

    cap.release()
    cv2.destroyWindow("Baby Response")

def update_prediction(label, confidence):
    prediction_label.config(text=f"Prediction: {label.capitalize()}")
    confidence_label.config(text=f"Confidence: {confidence:.2f}%")

def update_sensors(temperature, motion, light, gas):
    temperature_value.config(text=f"{temperature} °C")
    motion_value.config(text=motion)
    light_value.config(text=light)
    gas_value.config(text=gas)

def process_stm32_message(message):
    if message == "SYSTEM_READY":
        telegram_status.config(text="STM32: Ready")

    elif message == "EVENT:BABY_AWAKE":
        motion_value.config(text="BABY AWAKE")

    elif message == "ALERT:GAS_DETECTED":
        gas_value.config(text="DANGER!")
        buzzer_status.config(text="Buzzer: ON")
        send_telegram_alert("Alert! Gas Detected")

    elif message.startswith("DATA:"):
        try:
            data = message.replace("DATA:", "", 1)
            parts = data.split(",")

            if len(parts) < 4:
                print("Invalid DATA message:", message)
                return

            temperature = parts[0].split("=", 1)[1]
            gas = parts[1].split("=", 1)[1]
            motions = parts[2].split("=", 1)[1]
            light = parts[3].split("=", 1)[1]

            update_sensors(temperature, motions, light, gas)

            if "ALERT" in gas.upper():
                gas_value.config(text="DANGER!")
                buzzer_status.config(text="Buzzer: ON")
                send_telegram_alert("Alert! Gas Detected")
            else:
                buzzer_status.config(text="Buzzer: OFF")

            temp = float(temperature)

            if temp < 25:
                fan_status.config(text="Fan: OFF")
            elif temp <= 30:
                fan_status.config(text="Fan: MEDIUM")
            else:
                fan_status.config(text="Fan: FULL")

            if light.upper() == "DARK":
                led_status.config(text="LED: ON/Monitoring")
            else:
                led_status.config(text="LED: OFF")

        except Exception as e:
            print("DATA parsing error:", e)

def serial_reader():
    while stm32 and stm32.is_open:
        try:
            if stm32.in_waiting:
                message = stm32.readline().decode(errors="ignore").strip()
                if message:
                    print("RX:", message)
                    root.after(0, process_stm32_message, message)
            else:
                time.sleep(0.05)
        except Exception as e:
            print("Serial read error:", e)
            break

def play_audio_file(audio_file):
    if not os.path.exists(audio_file):
        print(f"Audio file not found: {audio_file}")
        return False

    try:
        sample_rate, audio_data = read(audio_file)
        sd.play(audio_data, sample_rate)
        print(f"Playing audio: {audio_file}")
        return True
    except Exception as e:
        print(f"Audio playback error for {audio_file}: {e}")
        return False

def analyze_audio_file(audio_file):
    if not os.path.exists(audio_file):
        print(f"File not found: {audio_file}")
        root.after(0, lambda: microphone_status.config(text=f"File missing: {audio_file}"))
        return

    try:
        print("\n" + "=" * 60)
        print("Starting final model analysis")
        print(f"Audio file: {audio_file}")
        print("=" * 60)

        root.after(0, lambda: microphone_status.config(text="Playing audio..."))
        root.after(0, lambda: cry_status.config(text="Cry Detection: Listening..."))

        if not play_audio_file(audio_file):
            root.after(0, lambda: microphone_status.config(text="Audio playback error"))
            return

        root.after(0, lambda: microphone_status.config(text="Audio File: Analyzing..."))

        result = detect_cry(audio_file)

        if not isinstance(result, tuple) or len(result) != 3:
            raise ValueError("detect_cry() must return prediction, confidence, speech_percentage")

        prediction, confidence, speech_percentage = result

        print(f"Predicted class: {prediction}")
        print(f"Confidence: {confidence}")
        print(f"Detected sound: {speech_percentage:.2f}%")

        root.after(0, update_prediction_result, prediction, confidence, speech_percentage)

        sd.wait()

    except Exception as e:
        print(f"Error analyzing {audio_file}: {e}")
        root.after(0, lambda: microphone_status.config(text="Audio analysis error"))

def run_final_audio_sequence():
    audio_sequence = [HUNGRY_AUDIO, TIRED_AUDIO, DISCOMFORT_AUDIO]

    try:
        for index, audio_file in enumerate(audio_sequence):
            analyze_audio_file(audio_file)

            if index < len(audio_sequence) - 1:
                print("Waiting 15 seconds before the next audio file...")
                root.after(0, lambda: microphone_status.config(text="Waiting 15 seconds for next audio..."))
                root.after(0, lambda: cry_status.config(text="Cry Detection: Waiting..."))
                time.sleep(WAIT_BETWEEN_FILES)

        print("\nFinal audio sequence completed.")
        root.after(0, lambda: microphone_status.config(text="Audio sequence completed"))
        root.after(0, lambda: cry_status.config(text="Cry Detection: Finished"))

    except Exception as e:
        print("Final audio sequence error:", e)
        root.after(0, lambda: microphone_status.config(text="Sequence error"))

def update_prediction_result(prediction, confidence, speech_percentage):
    microphone_status.config(text="Audio File: Ready")

    if prediction == "NO_SOUND":
        cry_status.config(text="Cry Detection: Waiting...")
        prediction_label.config(text="Prediction: ---")
        confidence_label.config(text="Confidence: ---")
        video_status.config(text="Video: Waiting for prediction...")
        servo_status.config(text="Servo: Normal")
        buzzer_status.config(text="Buzzer: OFF")
        send_command("CRY_ENDED")
        send_command("BUZZER_OFF")
        return

    cry_status.config(text="Cry Detection: Detected")

    if isinstance(confidence, dict):
        predicted_confidence = confidence.get(prediction, 0)
    else:
        predicted_confidence = float(confidence)

    update_prediction(prediction, predicted_confidence)

    send_command("CRY_DETECTED")
    servo_status.config(text="Servo: Rocking")

    if prediction.lower() == "hungry":
        video_status.config(text="Message: Baby is Hungry! Playing calming video...")
        threading.Thread(target=play_response_video, daemon=True).start()
        send_command("BUZZER_OFF")
        buzzer_status.config(text="Buzzer: OFF")

    elif prediction.lower() == "discomfort":
        video_status.config(text="Discomfort: Suggest checking diaper, clothing, position.")
        send_command("BUZZER_OFF")
        buzzer_status.config(text="Buzzer: OFF")

    elif prediction.lower() == "tired":
        video_status.config(text="WARNING: Urgent! Tired pattern detected.")
        send_command("BUZZER_ON")
        buzzer_status.config(text="Buzzer: ON")

    else:
        video_status.config(text=f"Cry Detected: {prediction.capitalize()}")
        send_command("BUZZER_OFF")
        buzzer_status.config(text="Buzzer: OFF")

def start_prediction():
    listen_button.config(state="disabled")

    def worker():
        run_final_audio_sequence()
        root.after(0, lambda: listen_button.config(state="normal"))

    threading.Thread(target=worker, daemon=True).start()

def reset_system():
    microphone_status.config(text="Audio File: Ready")
    cry_status.config(text="Cry Detection: Waiting...")
    prediction_label.config(text="Prediction: ---")
    confidence_label.config(text="Confidence: ---")
    video_status.config(text="Video: Waiting for prediction...")
    telegram_status.config(text="Telegram: Ready")
    fan_status.config(text="Fan: OFF")
    buzzer_status.config(text="Buzzer: OFF")
    led_status.config(text="LED: OFF")
    servo_status.config(text="Servo: Normal")

    send_command("CRY_ENDED")
    send_command("BUZZER_OFF")

header_frame = tk.Frame(root)
header_frame.pack(fill="x", pady=6)

title_label = tk.Label(header_frame, text="SMART NURSERY GUARDIAN", font=("Arial", 20, "bold"))
title_label.pack()

subtitle_label = tk.Label(header_frame, text="Baby Monitoring & Cry Classification System", font=("Arial", 12))
subtitle_label.pack(pady=2)

sensors_frame = tk.Frame(root, bd=2, relief="groove")
sensors_frame.pack(fill="x", padx=30, pady=2)
sensors_frame.grid_columnconfigure(0, weight=1)
sensors_frame.grid_columnconfigure(1, weight=1)

sensors_title = tk.Label(sensors_frame, text="Sensor Status", font=("Arial", 15, "bold"))
sensors_title.grid(row=0, column=0, columnspan=2, pady=2)

tk.Label(sensors_frame, text="Temperature", font=("Arial", 13, "bold")).grid(row=1, column=0, padx=25, pady=2)
temperature_value = tk.Label(sensors_frame, text="-- °C", font=("Arial", 13))
temperature_value.grid(row=1, column=1, padx=25, pady=2)

tk.Label(sensors_frame, text="Motion", font=("Arial", 13, "bold")).grid(row=2, column=0, padx=25, pady=2)
motion_value = tk.Label(sensors_frame, text="Waiting...", font=("Arial", 13))
motion_value.grid(row=2, column=1, padx=25, pady=2)

tk.Label(sensors_frame, text="Light", font=("Arial", 13, "bold")).grid(row=3, column=0, padx=25, pady=2)
light_value = tk.Label(sensors_frame, text="Waiting...", font=("Arial", 13))
light_value.grid(row=3, column=1, padx=25, pady=2)

tk.Label(sensors_frame, text="Gas", font=("Arial", 13, "bold")).grid(row=4, column=0, padx=25, pady=2)
gas_value = tk.Label(sensors_frame, text="Waiting...", font=("Arial", 13))
gas_value.grid(row=4, column=1, padx=25, pady=2)

cry_frame = tk.Frame(root, bd=2, relief="groove")
cry_frame.pack(fill="x", padx=30, pady=2)

tk.Label(cry_frame, text="Cry Analysis", font=("Arial", 15, "bold")).pack(pady=4)

microphone_status = tk.Label(cry_frame, text="Audio File: Ready", font=("Arial", 13))
microphone_status.pack(pady=1)

cry_status = tk.Label(cry_frame, text="Cry Detection: Waiting...", font=("Arial", 13))
cry_status.pack(pady=1)

prediction_label = tk.Label(cry_frame, text="Prediction: ---", font=("Arial", 17, "bold"))
prediction_label.pack(pady=2)

confidence_label = tk.Label(cry_frame, text="Confidence: ---", font=("Arial", 13))
confidence_label.pack(pady=1)

video_frame = tk.Frame(root, bd=2, relief="groove")
video_frame.pack(fill="x", padx=30, pady=2)

tk.Label(video_frame, text="Baby Response", font=("Arial", 15, "bold")).pack(pady=4)

video_status = tk.Label(video_frame, text="Video: Waiting for prediction...", font=("Arial", 13))
video_status.pack(pady=4)

device_frame = tk.Frame(root, bd=2, relief="groove")
device_frame.pack(fill="x", padx=30, pady=2)
device_frame.grid_columnconfigure(0, weight=1)
device_frame.grid_columnconfigure(1, weight=1)

tk.Label(device_frame, text="Device Status", font=("Arial", 15, "bold")).grid(row=0, column=0, columnspan=2, pady=2)

fan_status = tk.Label(device_frame, text="Fan: OFF", font=("Arial", 12))
fan_status.grid(row=1, column=0, padx=50, pady=2)

buzzer_status = tk.Label(device_frame, text="Buzzer: OFF", font=("Arial", 12))
buzzer_status.grid(row=1, column=1, padx=50, pady=2)

led_status = tk.Label(device_frame, text="LED: OFF", font=("Arial", 12))
led_status.grid(row=2, column=0, padx=50, pady=2)

servo_status = tk.Label(device_frame, text="Servo: Normal", font=("Arial", 12))
servo_status.grid(row=2, column=1, padx=50, pady=2)

telegram_frame = tk.Frame(root, bd=2, relief="groove")
telegram_frame.pack(fill="x", padx=30, pady=2)

tk.Label(telegram_frame, text="Telegram Alerts", font=("Arial", 15, "bold")).pack(pady=4)

telegram_status = tk.Label(telegram_frame, text="Telegram: Ready", font=("Arial", 13))
telegram_status.pack(pady=2)

listen_button = tk.Button(root, text="Start Listening", command=start_prediction, font=("Arial", 12))
listen_button.pack(pady=3)

reset_button = tk.Button(root, text="Reset System", command=reset_system, font=("Arial", 12))
reset_button.pack(pady=4)

if stm32:
    threading.Thread(target=serial_reader, daemon=True).start()

root.mainloop()