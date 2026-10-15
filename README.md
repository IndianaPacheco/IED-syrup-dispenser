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
  DrinkEditor.*          programming new drinks with the knob
sd_card/recipes.jsonl    example recipe file (copy to the SD card root)
HardwareTests/           standalone sketches to check each part
  LcdTest/               I2C scan, backlight, pixels, counter
  SdCardTest/            mount, list, write/read, recipe file
  EncoderTest/           knob position, encoder button, stop button
  PumpDriverTest/        run pumps from Serial Monitor + calibration
```

## ESP32 pinout

All pins are set in `SyrupDispenser/Config.h`.

### Pumps (MOSFET driver inputs, active HIGH)

| Pump | GPIO |
|------|------|
| 1    | 13   |
| 2    | 14   |
| 3    | 16   |
| 4    | 17   |
| 5    | 25   |
| 6    | 26   |
| 7    | 27   |
| 8    | 32   |

### microSD card (SPI, VSPI bus)

| SD module | GPIO |
|-----------|------|
| CS        | 5    |
| SCK       | 18   |
| MISO      | 19   |
| MOSI      | 23   |

### LCD (16x2, I2C, address `0x27`)

| LCD backpack | GPIO |
|--------------|------|
| SDA          | 21   |
| SCL          | 22   |

### Rotary encoder and stop button

| Signal          | GPIO | Notes                                          |
|-----------------|------|------------------------------------------------|
| Encoder CLK     | 33   | Internal pull-up                               |
| Encoder DT      | 34   | Input-only: needs external 10 kΩ pull-up to 3.3 V |
| Encoder SW      | 35   | Input-only: needs external 10 kΩ pull-up to 3.3 V |
| Stop button     | 4    | Wired to GND, internal pull-up                 |

Buttons are active LOW (pressed = connected to GND).

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

### Programming a drink on the machine

Scroll past the last drink to **+ New drink** and press the knob.

1. **Name**: turn to pick a character, then press to add it. Pick `[Delete]` to erase the last character, or `[Done]` when you're finished (up to 15 characters).
2. **Amounts**: for each pump 1–8, turn to set the mL, then press to go to the next pump. Each click is 0.5 mL, and spinning fast moves 2 mL per click.
3. **Save**: press to save. The drink is added to the end of `/recipes.jsonl` with the next free `id`, and the menu jumps to it.

The stop button goes back one step. On the name step it cancels and returns to the menu. If the card has no `recipes.jsonl` yet, the menu shows only **+ New drink**, and saving creates the file.

Drinks can't be edited or deleted on the machine yet. To change one, take out the SD card and edit `/recipes.jsonl` on a computer. Keep one drink per line, and give each drink a unique `id`.

## Safety

- Test with pump power disconnected first.
- Calibrate `PUMP_ML_PER_SECOND` in `Config.h` before dispensing real drinks.
- The stop button is a software stop, not a safety-rated emergency stop.
