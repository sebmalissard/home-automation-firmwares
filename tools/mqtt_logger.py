#!/bin/python3

import argparse
import colorama
import datetime
import dotenv
import os
import paho.mqtt.client as mqtt

dotenv.load_dotenv("credentials.env")
colorama.init(autoreset=True)

mqtt_host     = os.getenv("MQTT_BROKER_HOST")
mqtt_port     = int(os.getenv("MQTT_BROKER_PORT"))
mqtt_username = os.getenv("MQTT_USERNAME")
mqtt_password = os.getenv("MQTT_PASSWORD")

LEVEL_COLORS = {
    "[ERROR]":   colorama.Fore.RED,
    "[WARNING]": colorama.Fore.YELLOW,
    "[INFO]":    colorama.Fore.GREEN,
    "[DEBUG]":   colorama.Fore.BLUE,
}

def on_connect(client, userdata, flags, reason_code):
    if reason_code != 0:
        print(f"Connection failed: {mqtt.connack_string(reason_code)}")
        return
    log_device = userdata.get("log_device")
    log_level = userdata.get("log_level")
    if log_device:
        client.subscribe(f"{log_device}/#")
        if log_level:
            client.publish(f"{log_device}/log/level", log_level, retain=True)
    else:
        client.subscribe("#")

def on_message(client, userdata, msg):
    payload = msg.payload.decode('utf-8', errors='replace')
    timestamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
    log_device = userdata.get("log_device")
    color = colorama.Fore.WHITE
    if msg.topic == f"{log_device}/log":
        for level, clr in LEVEL_COLORS.items():
            if payload.startswith(level):
                color = clr
                break
    print(color + f"[{timestamp}] {msg.topic}: {payload}")

def mqtt_connect(client):
    client.on_connect = on_connect
    client.on_message = on_message
    client.username_pw_set(mqtt_username, mqtt_password)
    try:
        client.connect(mqtt_host, mqtt_port, 60)
    except Exception as e:
        print(f"Error connecting to MQTT broker: {e}")
        exit(1)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="MQTT Logger")
    parser.add_argument("--log-device", help="Device to log (e.g., 'home/bedroom/led')")
    parser.add_argument("--log-level",  help="Log level (e.g., 'INFO', 'DEBUG')")
    args = parser.parse_args()

    userdata = {
        "log_device": args.log_device,
        "log_level": args.log_level
    }

    client = mqtt.Client(userdata=userdata)
    mqtt_connect(client)
    client.loop_forever()
