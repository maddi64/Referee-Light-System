import time
import threading
import subprocess
import psutil
import os
from gpiozero import Button, LED
from paho.mqtt.client import Client

# OLED display setup
from luma.core.interface.serial import i2c
from luma.oled.device import sh1106
from luma.core.render import canvas
from PIL import Image, ImageFont

# === Configuration ===
SWITCH_PIN = 23
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_DECISION_TOPIC = "owlcms/decision/A"
MQTT_DOWN_TOPIC = "owlcms/fop/down/A"
MQTT_DECISION_REQUEST_TOPIC = "owlcms/decisionRequest/A/"

# === GPIO Devices ===
switch = Button(SWITCH_PIN)
WIFI_LED_ON = LED(22)
WIFI_LED_OFF = LED(27)
MQTT_LED_ON = LED(24)
MQTT_LED_OFF = LED(25)
RESET_PIN = 17
resetLiftBtn = Button(RESET_PIN)

# === OLED Display Init ===
serial = i2c(port=1, address=0x3C)
device = sh1106(serial, width=128, height=64, rotate=0, framebuffer="full")
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 20)

# === State Variables ===
mqtt_client = None
mqtt_connected = False
ref1Decision = None
ref2Decision = None
ref3Decision = None
decisionsMade = 0
reminder_timer = None
down_signal_time = None
decision_lock = threading.Lock()
down_signal_triggered = False
# dot_counter = 0


# === Functions ===

def is_wifi_connected():
    try:
        subprocess.check_output(["ping", "-c", "1", "192.168.68.60"], timeout=2)
        return True
    except Exception:
        return False
        
def is_mosquitto_running():
    """Check if mosquitto MQTT server is running"""
    try:
        for proc in psutil.process_iter(['pid', 'name']):
            if 'mosquitto' in proc.info['name'].lower():
                return True
        return False
    except Exception:
        return False

def is_owlcms_running():
    """Check if OWLCMS jar file is running"""
    try:
        for proc in psutil.process_iter(['pid', 'cmdline']):
            if proc.info['cmdline']:
                cmdline = ' '.join(proc.info['cmdline'])
                if 'owlcms.jar' in cmdline and '/home/mwu/.local/share/owlcms/58.3.2/owlcms.jar' in cmdline:
                    return True
        return False
    except Exception:
        return False

# --- Function to draw the UI layout ---
def draw_ui():
    
    global mqtt_connected
    wifi_status = is_wifi_connected()
    mosquitto_status = is_mosquitto_running()
    owlcms_status = is_owlcms_running()
    current_mode = "standalone" if switch.is_pressed else "integrated"

    
    with canvas(device) as draw:
        if wifi_status:
            draw.text((5, 5), 'WIFI:  OK', fill=255, font=font)
        else:
            draw.text((5, 5), 'WIFI:  ERR', fill=255, font=font)

        # MQTT status
        if mosquitto_status:
            draw.text((5, 25), 'SERVER:OK', fill=255, font=font)
        else:
            draw.text((5, 25), 'SERVER:ERR', fill=255, font=font)

        # OWLCMS status
        if current_mode == "integrated" and owlcms_status:
            draw.text((5, 45), "OWLCMS:OK", fill=255, font=font)
        else:
            draw.text((5, 45), "OWLCMS:OFF", fill=255, font=font)
    

def oled_update_loop():
    while True:
        draw_ui()
        time.sleep(1)  # Update every 0.5 seconds for smoother animation


def cancel_timer():
    global reminder_timer
    if reminder_timer is not None:
        reminder_timer.cancel()
        reminder_timer = None


def resetLift():
    global decisionsMade, ref1Decision, ref2Decision, ref3Decision
    global reminder_timer, down_signal_triggered, down_signal_time
    cancel_timer()
    decisionsMade = 0
    ref1Decision = None
    ref2Decision = None
    ref3Decision = None
    down_signal_time = None
    down_signal_triggered = False
    print("Lift reset: Decisions and counters cleared.")


def process_down_signal():
    global down_signal_time
    down_signal_time = time.time()
    mqtt_client.publish(MQTT_DOWN_TOPIC, "")
    print("Down signal triggered.")


def process_decision_request(ref_number):
    mqtt_client.publish(MQTT_DECISION_REQUEST_TOPIC + ref_number, "on")
    print(f"Reminder sent to referee {ref_number}.")


def process_referee_decision(message):
    global ref1Decision, ref2Decision, ref3Decision, decisionsMade, reminder_timer
    global down_signal_time, down_signal_triggered

    current_time = time.time()

    if down_signal_triggered:
        elapsed = current_time - down_signal_time
        print(f"Elapsed since down signal: {elapsed:.2f}s")
        if elapsed <= 3:
            print("In decision change window. Accepting changes.")
        elif elapsed <= 8:
            print("Cooldown period. Ignoring changes.")
            return
        else:
            print("8s passed. Auto-resetting.")
            resetLift()
            return

    try:
        ref_number, decision = message.split()
        if ref_number not in {"1", "2", "3"} or decision not in {"good", "bad"}:
            print(f"Invalid referee input: {message}")
            return

        with decision_lock:
            previous_decision = None
            if ref_number == "1":
                previous_decision = ref1Decision
                ref1Decision = decision
            elif ref_number == "2":
                previous_decision = ref2Decision
                ref2Decision = decision
            elif ref_number == "3":
                previous_decision = ref3Decision
                ref3Decision = decision

            if previous_decision is None:
                decisionsMade += 1

            print(f"Ref {ref_number} set to {decision}. Total: {decisionsMade}")

            if not down_signal_triggered:
                votes = [ref1Decision, ref2Decision, ref3Decision]
                good_count = votes.count("good")
                bad_count = votes.count("bad")
                total_decided = len([v for v in votes if v is not None])

                if good_count >= 2 or bad_count >= 2 or total_decided == 3:
                    print("Triggering down signal!")
                    process_down_signal()
                    down_signal_time = time.time()
                    down_signal_triggered = True
                    mqtt_client.publish(MQTT_DECISION_REQUEST_TOPIC + ref_number, "off")
                    cancel_timer()
                    threading.Timer(8, resetLift).start()
                else:
                    cancel_timer()
                    if total_decided == 2 and (good_count == 1 and bad_count == 1):
                        if ref1Decision is None:
                            reminder_timer = threading.Timer(3, process_decision_request, args=("1",))
                        elif ref2Decision is None:
                            reminder_timer = threading.Timer(3, process_decision_request, args=("2",))
                        elif ref3Decision is None:
                            reminder_timer = threading.Timer(3, process_decision_request, args=("3",))
                        reminder_timer.start()
    except ValueError:
        print(f"Invalid message format: {message}")


# === MQTT Callbacks ===

def on_connect(client, userdata, flags, rc):
    global mqtt_connected
    if rc == 0:
        mqtt_connected = True
        client.subscribe(MQTT_DECISION_TOPIC)
        print("Connected to MQTT broker.")
        MQTT_LED_ON.on()
        MQTT_LED_OFF.off()
    else:
        mqtt_connected = False
        MQTT_LED_ON.off()
        MQTT_LED_OFF.on()
        print(f"Failed to connect to MQTT, code {rc}")


def on_message(client, userdata, message):
    message_str = message.payload.decode()
    if message.topic == MQTT_DECISION_TOPIC:
        process_referee_decision(message_str)


def setup_mqtt():
    global mqtt_client
    mqtt_client = Client()
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT)
    mqtt_client.loop_start()


def handle_standalone():
    global mqtt_connected
    print("Standalone mode: Processing data locally.")
    if not mqtt_connected:
        setup_mqtt()


def handle_integrated():
    global mqtt_client, mqtt_connected
    print("Integrated mode: Synchronizing with external systems.")
    if mqtt_connected:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        mqtt_connected = False
        print("MQTT disconnected.")


# === Main Program ===

resetLiftBtn.when_pressed = resetLift

try:
    print("Starting system...")
    last_mode = None

    # Start OLED thread
    oled_thread = threading.Thread(target=oled_update_loop, daemon=True)
    oled_thread.start()

    while True:
        mode = "standalone" if switch.is_pressed else "integrated"
        if mode != last_mode:
            if mode == "standalone":
                handle_standalone()
            else:
                handle_integrated()
            last_mode = mode
        time.sleep(1)

except KeyboardInterrupt:
    print("Shutting down...")

finally:
    if mqtt_connected:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
    if os.path.exists("/tmp/system_ready"):
        os.remove("/tmp/system_ready")
