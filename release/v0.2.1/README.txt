ZClub Coffee Firmware

Version: v0.2.1
Firmware Version: v0.2.1-ge1dd044-dirty
Commit: e1dd044
Dirty Worktree: 1
Build Time UTC: 2026-07-02T02:13:15Z
Target: ESP32-S3

Files:
- coffee.bin
- bootloader.bin
- partition-table.bin
- flash_args (if generated)
- flasher_args.json (if generated)

Flash command example:
  idf.py -p <PORT> flash

Or from this folder with esptool (if flash_args exists):
  python -m esptool --chip esp32s3 -b 460800 --before default_reset --after hard_reset write_flash @flash_args
