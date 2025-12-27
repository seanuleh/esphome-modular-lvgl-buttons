import time
import threading
import paho.mqtt.client as mqtt
import subprocess
import RPi.GPIO as GPIO
import os

# -------- CONFIG --------
MQTT_BROKER = "192.168.1.99"
MQTT_PORT = 1883
MQTT_TOPIC_VIDEO_SET = "monitor/input/set"
MQTT_TOPIC_VIDEO_STATE = "monitor/input/state"
MQTT_TOPIC_KVM_SET = "monitor/kvm_usb/set"
MQTT_TOPIC_KVM_STATE = "monitor/kvm_usb/state"
MQTT_TOPIC_USB_SWITCH_SET = "usb_switch/device/set"
MQTT_TOPIC_USB_SWITCH_STATE = "usb_switch/device/state"
STATE_REPORT_INTERVAL = 10

USB_SWITCH_PIN = 11
STATE_FILE = "/home/sean/usb_switch_state.txt"
# ------------------------

GPIO.setmode(GPIO.BOARD)
GPIO.setwarnings(False)
GPIO.setup(USB_SWITCH_PIN, GPIO.IN)

VCP_INPUT = "60"
VCP_KVM = "E7"
VCP_VOLUME = "62"  # Found via your capabilities check

INPUT_MAC = "0x19"
INPUT_PC = "0x0f"
KVM_MAC = "0xA800"
KVM_PC = "0xB800"

def get_usb_switch_state():
    if not os.path.exists(STATE_FILE): return "pc"
    with open(STATE_FILE, "r") as f:
        state = f.read().strip()
        return state if state in ["mac", "pc"] else "pc"

def save_usb_switch_state(state):
    with open(STATE_FILE, "w") as f:
        f.write(state)

def pulse_usb_switch():
    print("Pulsing Physical USB Switch...")
    GPIO.setup(USB_SWITCH_PIN, GPIO.OUT)
    GPIO.output(USB_SWITCH_PIN, GPIO.LOW)
    time.sleep(0.5)
    GPIO.setup(USB_SWITCH_PIN, GPIO.IN)

def run_ddc(cmd):
    ddcutil_cmd = cmd.replace("ddcutil", "ddcutil --skip-ddc-checks", 1)
    full_cmd = f'script -q -c "{ddcutil_cmd}" /dev/null'
    result = subprocess.run(full_cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=10)
    lines = result.stdout.strip().splitlines()
    return lines[-1].strip() if lines else ""

# --- The Audio Fix ---
def silence_monitor():
    """Forces the monitor volume to 0 to counter the firmware bug."""
    print("Dell Bug: Resetting monitor volume to 0...")
    run_ddc(f"ddcutil setvcp {VCP_VOLUME} 0")

def read_video_state(client):
    output = run_ddc(f"ddcutil getvcp {VCP_INPUT}")
    try:
        val_hex = output.split('(sl=')[1].split(')')[0].strip() if "(sl=" in output else output.split("current value = ")[1].split(" ")[0].strip()
        state = "pc" if int(val_hex, 16) == int(INPUT_PC, 16) else "mac"
        client.publish(MQTT_TOPIC_VIDEO_STATE, state, retain=True)
    except: pass

def read_kvm_state(client):
    output = run_ddc(f"ddcutil getvcp {VCP_KVM}")
    try:
        sh_hex = output.split("sh=")[1].split(",")[0].strip()
        state = "pc" if int(sh_hex, 16) == 0xB8 else "mac"
        client.publish(MQTT_TOPIC_KVM_STATE, state, retain=True)
    except: pass

def set_video(target):
    val = INPUT_MAC if target == "mac" else INPUT_PC
    run_ddc(f"ddcutil setvcp {VCP_INPUT} {val}")
    time.sleep(1.5) # Wait for firmware to re-init audio
    silence_monitor()

def set_kvm(target):
    val = KVM_MAC if target == "mac" else KVM_PC
    run_ddc(f"ddcutil setvcp {VCP_KVM} {val}")
    time.sleep(1.5) # Wait for firmware to re-init audio
    silence_monitor()

def set_usb_switch(target, client):
    current = get_usb_switch_state()
    if target != current:
        pulse_usb_switch()
        save_usb_switch_state(target)
    client.publish(MQTT_TOPIC_USB_SWITCH_STATE, target, retain=True)

def on_connect(client, userdata, flags, rc):
    client.subscribe([(MQTT_TOPIC_VIDEO_SET, 0), (MQTT_TOPIC_KVM_SET, 0), (MQTT_TOPIC_USB_SWITCH_SET, 0)])
    threading.Thread(target=state_reporter_thread, args=(client,), daemon=True).start()

def on_message(client, userdata, msg):
    payload = msg.payload.decode().lower()
    if msg.topic == MQTT_TOPIC_VIDEO_SET:
        set_video(payload)
        read_video_state(client)
    elif msg.topic == MQTT_TOPIC_KVM_SET:
        set_kvm(payload)
        read_kvm_state(client)
    elif msg.topic == MQTT_TOPIC_USB_SWITCH_SET:
        set_usb_switch(payload, client)

def state_reporter_thread(client):
    while True:
        read_kvm_state(client)
        read_video_state(client)
        client.publish(MQTT_TOPIC_USB_SWITCH_STATE, get_usb_switch_state(), retain=True)
        time.sleep(STATE_REPORT_INTERVAL)

def main():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_forever()

if __name__ == "__main__":
    try:
        main()
    finally:
        GPIO.cleanup()