# Generate and install Arduino library

    ./gen_arduino_library.py
    # Install from IDE ArduinoSebOtaUpdate.zip, or:
    unzip ArduinoSebOtaUpdate.zip -d /mnt/c/Users/Seb/Documents/Arduino/libraries/

# Build Arduino firmware

    # Build firmware from Arduino IDE
    # Then do "Sketch" > "Export Compiled Binary"

# Create OTA image

    cmake .
    make
    ./gen_ota_firmware.py -p ../RadiatorController

# Upload OTA image on Home Assistant

    ./home_assistant/upload_firmwares.py
