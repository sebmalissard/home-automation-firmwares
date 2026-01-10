#!/usr/bin/env python3

import argparse
import glob
import os
import re
import shutil
from pathlib import Path

def get_device(project_dir):
    folder = Path(project_dir)
    for ino in folder.glob("*.ino"):
        return ino.stem
    return None

def get_version(ino_file: Path):
    if ino_file.exists():
        pattern = re.compile(r'#define\s+VERSION\s+"([^"]+)"')
        with ino_file.open("r", encoding="utf-8") as f:
                for line in f:
                    match = pattern.search(line)
                    if match:
                        return match.group(1)
    return None

def get_chip(project_dir):
    folder = Path(project_dir) / "build"
    subfolders = [f for f in folder.iterdir() if f.is_dir()]
    if len(subfolders) != 1:
        print(f"ERROR: Expected one subfolder in build directory, found {len(subfolders)}")
        return None
    chip = subfolders[0].name.split('.', 1)[0]
    return chip.upper()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('-p', '--project', required=True, help='Specify the directory project target (e.g., "RadiatorController")')
    args = parser.parse_args()

    project_dir = args.project

    device_name = get_device(project_dir)
    if not device_name:
        print(f"ERROR: Could not find .ino file in project directory '{project_dir}'")
        exit(1)

    version = get_version(Path(project_dir) / f"{device_name}.ino")
    if not version:
        print(f"ERROR: Could not find firmware version in '{project_dir}/{device_name}.ino'")
        exit(1)
    
    chip = get_chip(project_dir)
    if not chip:
        print(f"ERROR: Could not determine chip from build directory in '{project_dir}/build'")
        exit(1)

    print(f"Device: {device_name}, Version: {version}, Chip: {chip}")

    os.makedirs("firmwares", exist_ok=True)
    fw_files = glob.glob(os.path.join(project_dir, "build", "*", "*.bin"))
    if len(fw_files) != 1:
        print("ERROR: Incorrect number of firmware files found.")
        exit(1)
    shutil.copy(fw_files[0], "firmwares/firmware.bin")

    cmd = f"./create_ota_image --chip {chip} --device {device_name} --version {version} --fw firmwares/firmware.bin --private-key keys/private_key.pem --out firmwares/ota_firmware.bin"
    if os.system(cmd) != 0:
        print("ERROR: OTA image generation failed.")
        exit(1)
    
    print("OTA firmware image generated successfully: firmwares/ota_firmware.bin")

    os.rename("firmwares/ota_firmware.bin", f"firmwares/{device_name}_{chip}_{version}.bin")
    os.remove("firmwares/firmware.bin")

    print(f"Final OTA firmware image: firmwares/{device_name}_{chip}_{version}.bin")
