import time
import threading
from gpiozero import Button
from gpiozero import LED
from paho.mqtt.client import Client

# GPIO and MQTT configuration
SWITCH_PIN = 17
MQTT_BROKER = "localhost"
MQTT_PORT = 1883
MQTT_DECISION_TOPIC = "owlcms/decision/A"
MQTT_DOWN_TOPIC = "owlcms/fop/down/A"
MQTT_DECISION_REQUEST_TOPIC = "owlcms/decisionRequest/A/"

# GPIO devices
switch = Button(SWITCH_PIN)
WIFI_LED = LED(27)
MQTT_LED = LED(22)

RESET_PIN = 23
resetLiftBtn = Button(RESET_PIN)

# MQTT client instance
mqtt_client = None
mqtt_connected = False

# Referee decisions
ref1Decision = None
ref2Decision = None
ref3Decision = None

decisionsMade = 0
reminder_timer = None  # Timer for reminders

down_signal_time = None
decision_lock = threading.Lock()
down_signal_triggered = False

# Function to cancel any existing timer
def cancel_timer():
    global reminder_timer
    if reminder_timer is not None:
        reminder_timer.cancel()
        reminder_timer = None

# Function to reset decisions and counters
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

# Function to process down signal
def process_down_signal():
    global down_signal_time
    down_signal_time = time.time()
    mqtt_client.publish(MQTT_DOWN_TOPIC, "")
    print("Down signal triggered.")

# Function to send decision request
def process_decision_request(ref_number):
    mqtt_client.publish(MQTT_DECISION_REQUEST_TOPIC + ref_number, "on")
    #print(message);
    print(f"Reminder sent to referee {ref_number}.")

# Function to process referee decisions
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

            # Trigger down signal if not already sent
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
                    cancel_timer()  # Cancel any active reminder

                    # Start auto-reset after 8 seconds
                    threading.Timer(8, resetLift).start()
                else: 
                    # Set a reminder for the third referee
                    cancel_timer()  # Cancel existing timer
                    if ref1Decision is None:
                        reminder_timer = threading.Timer(3, process_decision_request, args=("1",))
                    elif ref2Decision is None:
                        reminder_timer = threading.Timer(3, process_decision_request, args=("2",))
                    elif ref3Decision is None:
                        reminder_timer = threading.Timer(3, process_decision_request, args=("3",))
                    reminder_timer.start()

    except ValueError:
        print(f"Invalid message format: {message}")

# MQTT callbacks
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe(MQTT_DECISION_TOPIC)
        client.subscribe(MQTT_DECISION_REQUEST_TOPIC)
        print("Connected to Mosquitto broker.")
        MQTT_LED.on()
    else:
        print(f"Failed to connect, return code {rc}")
        MQTT_LED.off()

def on_message(client, userdata, message):
    message_str = message.payload.decode()
    if message.topic == MQTT_DECISION_TOPIC:
        process_referee_decision(message_str)
    #if message.topic == MQTT_DECISION_REQUEST_TOPIC:
        #print("Message received on "+MQTT_DECISION_REQUEST_TOPIC+": Message is "+message_str)
        #process_decision_request(message_str)

# Function to set up MQTT
def setup_mqtt():
    global mqtt_client, mqtt_connected
    mqtt_client = Client()
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT)
    mqtt_client.loop_start()
    mqtt_connected = True
    print("MQTT setup complete.")

# Mode-handling functions
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
        mqtt_client = None
        mqtt_connected = False
        print("MQTT disconnected for integrated mode.")

# Set up reset button event
resetLiftBtn.when_pressed = resetLift

# Main loop
try:
    print("Starting program. Press Ctrl+C to exit.")
    last_mode = None  # Track the last mode to avoid redundant actions

    while True:
        mode = "standalone" if switch.is_pressed else "integrated"
        
        if mode != last_mode:  # Only act if the mode changes
            if mode == "standalone":
                handle_standalone()
            else:
                handle_integrated()
        
        last_mode = mode
        time.sleep(1)  # Adjust polling frequency if needed

except KeyboardInterrupt:
    print("Exiting program...")
finally:
    if mqtt_connected:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
