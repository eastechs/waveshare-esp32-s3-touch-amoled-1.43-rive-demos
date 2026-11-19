#!/usr/bin/env python3
"""Install the board-specific files from Waveshare's pinned non-C demo archive."""
import argparse
import hashlib
from pathlib import Path
import re
import shutil
import tempfile
import urllib.request
import zipfile

URL = "https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.43/ESP32-S3-AMOLED-1.43-Demo-V3.zip"
SHA256 = "0d407137e944059426dd44b34e9c153f058bf6d00d540faf912f7805a33cd67e"
PREFIX = "ESP32-S3-AMOLED-1.43-Demo/ESP-IDF/"
VENDOR = Path(__file__).resolve().parents[1] / "vendor"
COMPONENTS = {
    "read_lcd_id_bsp": "07_LVGL_Test",
    "i2c_bsp": "03_I2C_QMI8658",
}


def install(archive, target):
    digest = hashlib.sha256()
    with archive.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    if digest.hexdigest() != SHA256:
        raise SystemExit("Waveshare V3 archive checksum mismatch")

    with zipfile.ZipFile(archive) as source:
        for name, example in COMPONENTS.items():
            directory = target / name
            directory.mkdir()
            for file in ("CMakeLists.txt", name + ".c", name + ".h"):
                data = source.read(f"{PREFIX}{example}/components/{name}/{file}")
                (directory / file).write_bytes(data)
        header = "03_I2C_QMI8658/components/qmi8658c/qmi8658c.h"
        (target / "qmi8658c.h").write_bytes(source.read(PREFIX + header))

        # Keep the vendor's two command tables in sync with its LCD ID probe.
        main = source.read(PREFIX + "07_LVGL_Test/main/example_qspi_with_ram.c").decode()
        tables = []
        for name in ("sh8601", "co5300"):
            pattern = rf"static const sh8601_lcd_init_cmd_t {name}_lcd_init_cmds\[\]\s*=.*?\n\}};"
            match = re.search(pattern, main, re.DOTALL)
            if not match:
                raise SystemExit(f"Panel commands missing for {name}")
            tables.append(match.group())
        (target / "panel_init.h").write_text("\n".join(tables) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, nargs="?", help="local copy of the V3 ZIP")
    args = parser.parse_args()
    destination = VENDOR / "waveshare"
    VENDOR.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(dir=VENDOR) as temp:
        staging = Path(temp) / "components"
        staging.mkdir()
        archive = args.archive
        if archive is None:
            archive = Path(temp) / "demo.zip"
            with urllib.request.urlopen(URL, timeout=60) as response, archive.open("wb") as output:
                shutil.copyfileobj(response, output)
        install(archive, staging)
        if destination.exists():
            for file in staging.rglob("*"):
                if file.is_file():
                    installed = destination / file.relative_to(staging)
                    if not installed.is_file() or file.read_bytes() != installed.read_bytes():
                        raise SystemExit(f"Conflicting board files in {destination}. Move the directory aside and retry.")
        else:
            staging.rename(destination)
    print("Waveshare board files ready.")


if __name__ == "__main__":
    main()
