# Automatic Syrup Dispenser

ESP32-based dispenser with eight diaphragm pumps. It reads drink recipes from a microSD card, and you pick a drink with a rotary encoder and a 20x4 LCD.

## Project layout

```
SyrupDispenser/          Arduino sketch (open SyrupDispenser.ino)
  Config.h               pins, pump calibration, limits  <- edit this
  PumpController.*       timed, non-blocking pump control
  RecipeManager.*        recipe index + loading from the SD card
  Display.*              LCD screens
  Input.*                debounced buttons + rotary encoder
  DrinkEditor.*          programming new drinks with the knob
  WebRemote.*, WebPage.h WiFi + the barista web page
  Settings.*             WiFi details + web PIN, saved on the SD card
  SettingsMenu.*         the Settings menu on the machine
  TextEntry.*            typing passwords with the knob
sd_card/recipes.jsonl    example recipe file (copy to the SD card root)
HardwareTests/           standalone sketches to check each part
  LcdTest/               I2C scan, backlight, pixels, counter
  SdCardTest/            mount, list, write/read, recipe file
  EncoderTest/           knob position, encoder button, stop button
  PsramTest/             WROOM vs WROVER check (GPIO 16/17)
  PumpDriverTest/        run pumps from Serial Monitor + calibration
```

## ESP32 pinout

All pins are set in `SyrupDispenser/Config.h`.

The pins are grouped for an easy PCB layout. With the DevKit's antenna pointing up, all eight pumps are in a row down the **left** header, and the SD card, LCD, encoder and stop button are all on the **right** header. Every pin used here can do PWM, so pump speed control can be added later without rewiring.

```
               left header        right header
                   3V3  o          o  GND
                    EN  o          o  23   SD MOSI
                36 (VP) o          o  22   LCD SCL
                39 (VN) o          o  TX0  (GPIO 1, USB serial - keep free)
                    34  o          o  RX0  (GPIO 3, USB serial - keep free)
                    35  o          o  21   LCD SDA
       Pump 1       32  o          o  GND
       Pump 2       33  o          o  19   SD MISO
       Pump 3       25  o          o  18   SD SCK
       Pump 4       26  o          o  5    SD CS
       Pump 5       27  o          o  17   Encoder CLK  (labeled TX2)
       Pump 6       14  o          o  16   Encoder DT   (labeled RX2)
       Pump 7       12  o          o  4    Encoder SW
                   GND  o          o  0
       Pump 8       13  o          o  2
                   ...  o          o  15   Stop button
```

(38-pin ESP32-DevKitC shown. The 30-pin DevKit V1 has the same pins in the same order, minus a few GND and flash pins.)

### Pumps (MOSFET driver inputs, active HIGH)

| Pump | GPIO |
|------|------|
| 1    | 32   |
| 2    | 33   |
| 3    | 25   |
| 4    | 26   |
| 5    | 27   |
| 6    | 14   |
| 7    | 12   |
| 8    | 13   |

Pours run every pump with 1 kHz PWM at a 5/12 duty cycle, an effective 5 V from the 12 V supply. Only cleaning mode uses the full 12 V, with all eight pumps at once, so the power supply and wiring must handle that current. Change `PUMP_POUR_VOLTS` or `PUMP_SUPPLY_VOLTS` in `Config.h` to adjust it (and in `PumpDriverTest.ino`, which has its own copy), then recalibrate. Each pump needs a fast flyback diode (e.g. Schottky) across it, because PWM switches the motor off 1000 times a second.

### Pull-down resistors

Put a **10 kOhm resistor from each pump's MOSFET gate to GND**. It goes on the PCB between the ESP32 pin and the MOSFET gate trace, then to GND.

| Pump | GPIO | Pull-down | Why |
|------|------|-----------|-----|
| 1    | 32   | Yes | Keeps pump off while the ESP32 is off, booting, or flashing |
| 2    | 33   | Yes | Same |
| 3    | 25   | Yes | Same |
| 4    | 26   | Yes | Same |
| 5    | 27   | Yes | Same |
| 6    | 14   | Yes | Same, and GPIO 14 can twitch briefly at boot |
| 7    | 12   | **Required** | GPIO 12 must be LOW at reset or the ESP32 **will not boot** |
| 8    | 13   | Yes | Same as pumps 1-5 |

That's **8 resistors total**, one per pump. Without them, a gate floats whenever the ESP32 isn't actively driving it, and with pump power on, a pump can switch itself on.

**No other pin gets a pull-down.** The encoder (GPIO 17, 16, 4) and stop button (GPIO 15) use the ESP32's internal pull-*ups*, and the SD card and LCD pins work as they are. A pull-down on GPIO 15 or GPIO 4 would read as a held button.

If you use a MOSFET driver module that already has a gate pull-down on board, you don't need to add another one for that pump.

### microSD card (SPI, VSPI bus)

| SD module | GPIO |
|-----------|------|
| CS        | 5    |
| SCK       | 18   |
| MISO      | 19   |
| MOSI      | 23   |

### LCD (20x4, I2C, address `0x27`)

| LCD backpack | GPIO |
|--------------|------|
| SDA          | 21   |
| SCL          | 22   |

### Rotary encoder and stop button

| Signal          | GPIO | Notes                                          |
|-----------------|------|------------------------------------------------|
| Encoder CLK     | 17   | Labeled **TX2** on the board. Internal pull-up |
| Encoder DT      | 16   | Labeled **RX2** on the board. Internal pull-up |
| Encoder SW      | 4    | Internal pull-up                               |
| Stop button     | 15   | Wired to GND, internal pull-up                 |

Buttons are active LOW (pressed = connected to GND).

All four pins use the ESP32's internal pull-ups, so no extra resistors are needed. Power the KY-040's **+** pin from **3.3 V**, not 5 V, or its onboard pull-ups put 5 V on the ESP32 pins.

**Finding GPIO 16 and 17:** most DevKits label them by their serial-port names, **RX2** (GPIO 16) and **TX2** (GPIO 17), on the right header just below D5. Don't mix them up with **RX0 / TX0** near the top of the same header: those are GPIO 3 and 1, the USB serial pins, and must stay free for uploading and Serial Monitor.

GPIO 16 and 17 don't exist on **ESP32-WROVER** modules (they're wired to the PSRAM chip). Run `HardwareTests/PsramTest` before making the PCB: it should report no PSRAM (a WROOM module).

## Hardware tests

Each folder in `HardwareTests/` is its own sketch. Open its `.ino`, upload it, and open Serial Monitor at 115200 baud. Instructions and the pass criteria are at the top of each file.

Test in this order: module check, LCD, SD card, encoder, then pumps (with pump power disconnected first).

### Module check (WROOM vs WROVER)

`HardwareTests/PsramTest` tells you which ESP32 module your board has. This matters because the encoder uses GPIO 16 and 17, which a **WROVER** module wires to its PSRAM chip, so those pins don't work there. Run it before building the PCB:

1. Open `HardwareTests/PsramTest/PsramTest.ino`.
2. If the **Tools** menu has a **PSRAM** option, set it to **Enabled**.
3. Upload, open Serial Monitor at 115200 baud, and press the board's **EN/RST** button if nothing shows up.

| Result | Module | What to do |
|--------|--------|------------|
| `PSRAM size: 0 bytes` | WROOM | Nothing. GPIO 16/17 are fine. |
| `PSRAM size: 4194304` (or similar) | WROVER | Move the encoder CLK/DT off GPIO 16/17 in `Config.h` and the encoder test. |

If the label on the module's metal shield is generic, this test is the reliable way to tell. A WROVER shield is also longer (about 31 mm vs 25 mm).

### Other notes

The test sketches have their own copy of the pin numbers, so if you change `Config.h`, update the tests too.

## Building

- Board: **ESP32 Dev Module** (esp32 core 3.x)
- Libraries: **ArduinoJson** 7.x, **LiquidCrystal I2C** (Frank de Brabander)

## Recipes

One JSON object per line in `/recipes.jsonl` on a FAT32 microSD card:

```json
{"id":1,"name":"Vanilla Latte","amounts_ml":[10,0,0,0,0,0,0,0]}
```

The eight numbers are mL for pumps 1â€“8.

### Choosing a drink

Each drink is shown with its options underneath. Turn the knob to browse drinks. Press, and a `>` appears on **Pour**; turning now moves the `>` between the options instead of changing drinks:

```
Vanilla Latte
Drink 1/3
>Pour      Edit
 Delete    Back
```

Press again to pick the marked option. **Pour** dispenses the drink. **Edit** opens it in the drink editor (below) with its name and amounts filled in, and saving overwrites it in place. **Delete** asks `No` / `Yes`, with `No` selected first. **Back** (or the stop button) hides the `>` and returns to browsing. The screens don't show button hints.

### Programming a drink on the machine

Scroll past the last drink to **+ New drink** and press the knob. Programming uses only the knob: turn to move the `>` marker, press to pick.

1. **Name**: the screen shows the alphabet `a`â€“`z` and a space (drawn as `_`), with `>` on the selected letter. Press to add it. Turn past the last letter for **Delete letter**, **Finish name** and **Exit no save**. The first letter of each word is capitalized automatically. Names can be up to 39 characters; trailing spaces are dropped.

   ```
   Name:Mocha_
    a b c d e f g h i j
    k l m n o p q r s t
    u v w x y z>_ more
   ```
2. **Amounts**: for each pump 1â€“8, turn to set the mL, then press to go to the next pump. Each click is 0.5 mL, and spinning fast moves 2 mL per click.
3. **Finish**: pick **Save drink**, **Edit drink** (back to the name with your amounts kept), or **Exit no save**. A saved drink is added to the end of `/recipes.jsonl` with the next free `id`, and the menu jumps to it. A drink with every pump at 0 mL can't be saved.

The stop button does nothing while programming. If the card has no `recipes.jsonl` yet, the menu shows only **+ New drink**, and saving creates the file.

When you edit a drink, the editor opens with `>` on **Finish name**, so pressing goes straight to the amounts. Edit and Delete rewrite `/recipes.jsonl` through a temporary `/recipes.tmp`, which replaces the original only after the full copy is written. You can still edit `/recipes.jsonl` on a computer. Keep one drink per line, and give each drink a unique `id`.

## Barista web page

A barista can order drinks from the register, a tablet or a phone on the same WiFi. The page lists the drinks, shows what the machine is doing, and has a **STOP** button. A web order only shows on the LCD (`Web order: ... Cup in, press knob`). Nothing pours until someone at the machine presses the knob. Orders are refused while someone is using the knob or a drink is pouring.

The **Drinks** tab manages the drink list from the browser. Tap **+ New drink**, or tap a drink to edit it. You can change the name and the mL for each of the eight pumps, then **Save**. **Delete** asks you to confirm first. Saving and deleting need the PIN. They follow the same rules as the machine: a name of 1–39 plain characters, 0–100 mL per pump, and at least one pump above 0. They're only allowed while nobody is using the knob and nothing is pouring, since rewriting the recipe file mid-pour could throw off pump timing. If the list changed since the page loaded (someone edited a drink on the machine or in another browser), the page reloads the list instead of changing the wrong drink.

### Settings (WiFi and PIN)

WiFi starts **off**. Set it up on the machine: scroll to the last menu entry, **Settings**, and press. The menu is knob-only with no PIN, because customers can't reach the machine.

- **WiFi network**: scans for nearby networks. Pick one and type its password. The list ends with:
  - **Hidden network**: type the network name, then the password.
  - **Own network**: the dispenser makes its own network, `SyrupDispenser`. Join it from the register and open `http://192.168.4.1/`. Use this on school WiFi, which usually blocks devices from reaching each other. Set a password of 8+ characters, or leave it empty for an open network (the PIN still guards the page).
  - **WiFi off**.
- **Web PIN**: what the page asks for before sending orders (default `1234`).
- **Web address**: the address to open on the register, plus the network and PIN. It updates when WiFi connects.
- **Clean pumps**: asks `No` / `Yes`, then runs all eight pumps at the full 12 V. Put the lines in water and a big container under the outlet first. It stops after 1 minute (`CLEAN_RUN_MS` in `Config.h`), or when you press the knob, the stop button, or Stop on the web page.

Typing a password works like typing a drink name, but with four pages of characters: `abc`, `ABC`, `123` (digits, space and common symbols) and `#$%`. After the last character on a page is the page switch (bottom right, shows the next page), then **Delete letter**, **Done** and **Cancel**. A space shows as a small bracket.

```
Pass:Cafe2_
 a b c d e f g h i j
 k l m n o p q r s t
 u v w x y z    >ABC
```

The settings are saved to `/settings.json` on the SD card, **in plain text**. They are never in the code, so they can't end up on GitHub. Don't copy that file into this repo; `sd_card/settings.json` is in `.gitignore` in case you do. Anyone holding the SD card can read the WiFi password.

## Safety

- Test with pump power disconnected first.
- Calibrate `PUMP_ML_PER_SECOND` in `Config.h` before dispensing real drinks.
- The stop button is a software stop, not a safety-rated emergency stop.
