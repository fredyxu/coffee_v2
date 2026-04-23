Firmware package exported by ./fw

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
