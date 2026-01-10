#!/usr/bin/env python3
#
# Usage:
#   ./home_assistant/upload_firmwares.py
#
# To dowload firmware files upload to Home Assistant:
# https://192.168.1.2:8123/local/firmwares/RadiatorController_ESP8266_3.0.0.bin

import os
from smb.SMBConnection import SMBConnection
import credentials

if __name__ == "__main__":
    try:
        conn = SMBConnection(credentials.USERNAME, credentials.PASSWORD, "firmware_ota_uploader_client", credentials.SERVER_NAME, domain=credentials.DOMAIN, use_ntlm_v2=True)
        
        if not conn.connect(credentials.SERVER_IP, 445):
            print("ERROR: Unable to connect to the SMB server.")
            exit(1)

        firmware_dir = "firmwares"
        for firmware_file in os.listdir(firmware_dir):
            if firmware_file.endswith(".bin"):
                local_path = os.path.join(firmware_dir, firmware_file)
                with open(local_path, "rb") as file_obj:
                    conn.storeFile("config", f"www/firmwares/{firmware_file}", file_obj, show_progress=True)
                print(f"Uploaded {firmware_file} to Home Assistant.")

    except FileNotFoundError as e:
        print(f"ERROR: File not found - {str(e)}")
    except PermissionError as e:
        print(f"ERROR: Permission denied - {str(e)}")
    except Exception as e:
        print(f"ERROR: {e.__class__.__name__} - {str(e)}")
    finally:
        if 'conn' in locals():
            conn.close()
    
    # Add upload json file