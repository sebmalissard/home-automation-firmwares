#!/usr/bin/env python3

import os
import shutil

LIB_NAME = "ArduinoSebOtaUpdate"

LIBRARY_PROPERTIES = f"""\
name={LIB_NAME}
version=1.0.0
author=Seb
sentence=Common headers for OTA update
paragraph=
category=Communication
"""


if __name__ == "__main__":
    # Remove existing zip file if it exists
    if os.path.exists(f"{LIB_NAME}.zip"):
        os.remove(f"{LIB_NAME}.zip")

    # Create the library directory
    temp_dir = os.path.join(os.path.dirname(__file__), f"{LIB_NAME}")
    os.makedirs(temp_dir, exist_ok=True)

    # Copy header files
    shutil.copytree(os.path.join(os.path.dirname(__file__), "include"), temp_dir, dirs_exist_ok=True)

    # Create library.properties file
    properties_path = os.path.join(temp_dir, "library.properties")
    with open(properties_path, "w") as f:
        f.write(LIBRARY_PROPERTIES)

    # Create a zip archive of the library
    shutil.make_archive(f"{LIB_NAME}", 'zip', root_dir=os.path.dirname(temp_dir), base_dir=os.path.basename(temp_dir))

    # Clean up temporary directory
    shutil.rmtree(temp_dir)
