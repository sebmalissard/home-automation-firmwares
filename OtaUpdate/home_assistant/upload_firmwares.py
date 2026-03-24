#!/usr/bin/env python3
#
# Usage:
#   ./home_assistant/upload_firmwares.py
#
# To dowload firmware files upload to Home Assistant:
# https://192.168.1.2:8123/local/firmwares/RadiatorController_ESP8266_3.0.0.bin
import json
import os
from smb.SMBConnection import SMBConnection
import credentials
import paho.mqtt.client as mqtt

def upload_firmwares():
    try:
        conn = SMBConnection(credentials.SAMBA_USERNAME, credentials.SAMBA_PASSWORD, "firmware_ota_uploader_client", credentials.SAMBA_SERVER_NAME, domain=credentials.SAMBA_DOMAIN, use_ntlm_v2=True)
        if not conn.connect(credentials.SAMBA_SERVER_IP, 445):
            print("ERROR: Unable to connect to the SMB server.")
            exit(1)
        firmware_dir = "firmwares"
        for file in os.listdir(firmware_dir):
            local_path = os.path.join(firmware_dir, file)
            with open(local_path, "rb") as file_obj:
                conn.storeFile("config", f"www/firmwares/{file}", file_obj, show_progress=True)
            print(f"Uploaded {file} to Home Assistant.")
    except FileNotFoundError as e:
        print(f"ERROR: File not found - {str(e)}")
    except PermissionError as e:
        print(f"ERROR: Permission denied - {str(e)}")
    except Exception as e:
        print(f"ERROR: {e.__class__.__name__} - {str(e)}")
    finally:
        if 'conn' in locals():
            conn.close()

def mqtt_request_ota_check_update():
    client = mqtt.Client()
    client.username_pw_set(credentials.MQTT_USERNAME, credentials.MQTT_PASSWORD)
    try:
        client.connect(credentials.MQTT_BROKER_HOST_ETH, int(credentials.MQTT_BROKER_PORT), 60)
        payload = {
            "url": f"https://{credentials.MQTT_BROKER_HOST}/local/firmwares/firmwares-latest.json"
        }
        client.publish("home/ota/check_update", json.dumps(payload), qos=0, retain=True)
    except Exception as e:
        print(f"ERROR: {e.__class__.__name__} - {str(e)}")
    finally:
        client.disconnect()

if __name__ == "__main__":
    upload_firmwares()
    mqtt_request_ota_check_update()