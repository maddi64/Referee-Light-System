from luma.core.interface.serial import i2c
from luma.oled.device import sh1106
from luma.core.render import canvas
from PIL import ImageFont
from time import sleep
import os
import subprocess

# Setup display
serial = i2c(port=1, address=0x3C)
device = sh1106(serial)
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", 20)

dot_counter = 0

def wifi_connected():
    try:
        # This checks for a default route via wlan0 (or any active connection)
        result = subprocess.run(["ip", "route"], capture_output=True, text=True)
        return "default via" in result.stdout
    except Exception:
        return False

def mosquitto_running():
    try:
        result = subprocess.run(["pgrep", "-f", "mosquitto"], capture_output=True, text=True)
        return result.returncode == 0
    except Exception:
        return False

def owlcms_running():
    try:
        result = subprocess.run(["pgrep", "-f", "owlcms.jar"], capture_output=True, text=True)
        return result.returncode == 0
    except Exception:
        return False

def system_is_ready():
    return wifi_connected() and mosquitto_running() and owlcms_running()

try:
    while not system_is_ready():
        dot_counter = (dot_counter + 1) % 4
        dots = "." * dot_counter if dot_counter else ""
        with canvas(device) as draw:
            draw.text((5, 25), f"Booting{dots}", fill=255, font=font)
        sleep(1)
except KeyboardInterrupt:
    pass
