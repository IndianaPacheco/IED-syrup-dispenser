# Automatic Syrup Dispenser

ESP32-based dispenser with eight peristaltic pumps. It reads drink recipes from a microSD card, and you pick a drink with a rotary encoder and a 16x2 LCD.

## Project layout

```
SyrupDispenser/          Arduino sketch (open SyrupDispenser.ino)
  Config.h               pins, pump calibration, limits  <- edit this
  PumpController.*       timed, non-blocking pump control
  RecipeManager.*        recipe index + loading from the SD card
  Display.*              LCD screens
  Input.*                debounced buttons + rotary encoder
sd_card/recipes.jsonl    example recipe file (copy to the SD card root)
```

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
