# Automatic Syrup Dispenser

ESP32-based dispenser with eight diaphragm pumps. It reads drink recipes from a microSD card, and you pick a drink with a rotary encoder and a 16x2 LCD.

## Project layout

```
SyrupDispenser/          Arduino sketch (open SyrupDispenser.ino)
  Config.h               pins, pump calibration, limits  <- edit this
  PumpController.*       timed, non-blocking pump control
  RecipeManager.*        recipe index + loading from the SD card
  Display.*              LCD screens
  Input.*                debounced buttons + rotary encoder
sd_card/recipes.jsonl    example recipe file (copy to the SD card root)
HardwareTests/           standalone sketches to check each part
  LcdTest/               I2C scan, backlight, pixels, counter
  SdCardTest/            mount, list, write/read, recipe file
  EncoderTest/           knob position, encoder button, stop button
  PumpDriverTest/        run pumps from Serial Monitor + calibration
```

## Hardware tests

Each folder in `HardwareTests/` is its own sketch. Open its `.ino`, upload it, and open Serial Monitor at 115200 baud. Instructions and the pass criteria are at the top of each file.

Test in this order: LCD, SD card, encoder, then pumps (with pump power disconnected first). The test sketches have their own copy of the pin numbers, so if you change `Config.h`, update the tests too.

## Building

- Board: **ESP32 Dev Module** (esp32 core 3.x)
- Libraries: **ArduinoJson** 7.x, **LiquidCrystal I2C** (Frank de Brabander)

## Recipes

One JSON object per line in `/recipes.jsonl` on a FAT32 microSD card:

```json
{"id":1,"name":"Vanilla Latte","amounts_ml":[10,0,0,0,0,0,0,0]}
```

The eight numbers are mL for pumps 1–8.

## Safety

- Test with pump power disconnected first.
- Calibrate `PUMP_ML_PER_SECOND` in `Config.h` before dispensing real drinks.
- The stop button is a software stop, not a safety-rated emergency stop.
