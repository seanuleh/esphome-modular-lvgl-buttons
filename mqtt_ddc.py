import time
import threading
import paho.mqtt.client as mqtt
import subprocess

# -------- CONFIG --------
MQTT_BROKER = "192.168.1.99"
MQTT_PORT = 1883
MQTT_TOPIC_VIDEO_SET = "monitor/input"
MQTT_TOPIC_VIDEO_STATE = "monitor/input/state"
MQTT_TOPIC_KVM_SET = "monitor/kvm_usb/set"
MQTT_TOPIC_KVM_STATE = "monitor/kvm_usb/state"
STATE_REPORT_INTERVAL = 10
# ------------------------

# --- VCP VALUES ---
VCP_INPUT = "60"
VCP_KVM = "E7"

INPUT_MAC = "0x19"   # Thunderbolt input
INPUT_PC = "0x0f"    # DisplayPort input

KVM_MAC = "0xA800"   # Thunderbolt USB
KVM_PC = "0xB800"    # USB-C
# -------------------


# --- Helper to run ddcutil and suppress warnings ---
def run_ddc(cmd):
    # Using --skip-ddc-checks is still good practice.
    ddcutil_cmd = cmd.replace("ddcutil", "ddcutil --skip-ddc-checks", 1)

    # The `script` command creates a pseudo-terminal (pty), making the
    # executed command believe it's in an interactive session. This should
    # prevent ddcutil from sending diagnostic messages to syslog.
    # -q = quiet mode
    # -c = run command
    # /dev/null = discard the typescript file
    full_cmd = f'script -q -c "{ddcutil_cmd}" /dev/null'
    
    print(f"Running: {full_cmd}")
    result = subprocess.run(
        full_cmd,
        shell=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        timeout=10
    )

    if result.stderr:
        print(f"script/ddcutil stderr: {result.stderr.strip()}")

    # The output from `script` can have extra whitespace/control characters.
    # We return the last non-empty line, which should be the actual ddcutil output.
    lines = result.stdout.strip().splitlines()
    return lines[-1].strip() if lines else ""


# --- Read actual video input state from monitor ---
def read_video_state(client):
    output = run_ddc(f"ddcutil getvcp {VCP_INPUT}")
    if not output:
        return

    try:
        val_hex = ""
        if "(sl=" in output:
            # Handles format: "VCP code 0x60 (Input Source...): Invalid value (sl=0x19)"
            val_hex = output.split('(sl=')[1].split(')')[0].strip()
        elif "current value = " in output:
            # Handles format: "VCP code 0x60 (Input Source...): current value = 0x19..."
            val_hex = output.split("current value = ")[1].split(" ")[0].strip()
        else:
            raise ValueError("Unsupported ddcutil output format")

        val = int(val_hex, 16)
        mac_val = int(INPUT_MAC, 16)
        pc_val = int(INPUT_PC, 16)
    except Exception as e:
        print("Failed to parse VCP 60:", e, output)
        return

    if val == pc_val:
        state = "pc"
    elif val == mac_val:
        state = "mac"
    else:
        state = "unknown"

    print("Video state:", state)
    client.publish(MQTT_TOPIC_VIDEO_STATE, state, retain=True)


# --- Read actual KVM state from monitor ---
def read_kvm_state(client):
    output = run_ddc(f"ddcutil getvcp {VCP_KVM}")
    if not output:
        return

    try:
        sh_hex = output.split("sh=")[1].split(",")[0].strip()
        sh = int(sh_hex, 16)
    except Exception as e:
        print("Failed to parse VCP E7:", e, output)
        return

    if sh == 0xB8:
        state = "pc"
    elif sh == 0xA8:
        state = "mac"
    else:
        state = "unknown"

    print("KVM state:", state)
    client.publish(MQTT_TOPIC_KVM_STATE, state, retain=True)


# --- Set video input ---
def set_video(target):
    if target == "mac":
        run_ddc(f"ddcutil setvcp {VCP_INPUT} {INPUT_MAC}")
    elif target == "pc":
        run_ddc(f"ddcutil setvcp {VCP_INPUT} {INPUT_PC}")


# --- Set KVM routing ---
def set_kvm(target):
    if target == "mac":
        run_ddc(f"ddcutil setvcp {VCP_KVM} {KVM_MAC}")
    elif target == "pc":
        run_ddc(f"ddcutil setvcp {VCP_KVM} {KVM_PC}")


# ---- MQTT CALLBACKS ----
def on_connect(client, userdata, flags, rc):
    print(f"Connected (rc={rc})")
    client.subscribe([(MQTT_TOPIC_VIDEO_SET, 0), (MQTT_TOPIC_KVM_SET, 0)])

    # Start background reporter
    t = threading.Thread(target=state_reporter_thread, args=(client,), daemon=True)
    t.start()


def on_message(client, userdata, msg):
    payload = msg.payload.decode().lower()

    print(f"Received {msg.topic}: {payload}")

    if msg.topic == MQTT_TOPIC_VIDEO_SET:
        set_video(payload)
        read_video_state(client) # Immediately publish updated state

    elif msg.topic == MQTT_TOPIC_KVM_SET:
        set_kvm(payload)
        read_kvm_state(client)  # Immediately publish updated state


# --- Periodic state reporter ---
def state_reporter_thread(client):
    while True:
        read_kvm_state(client)
        read_video_state(client)
        time.sleep(STATE_REPORT_INTERVAL)


# ---- MAIN ----
def main():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message

    print("Connecting to MQTT...")
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_forever()


if __name__ == "__main__":
    main()