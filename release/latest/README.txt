ZClub Coffee Firmware

Version: latest
Firmware Version: v0.2.1-g59db960-dirty
Commit: 59db960
Dirty Worktree: 1
Build Time UTC: 2026-06-28T05:35:21Z
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
