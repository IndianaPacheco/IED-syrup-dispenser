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

### LCD (20x4, I2C, address `0x27`)

| LCD backpack | GPIO |
|--------------|------|
| SDA          | 21   |
| SCL          | 22   |

### Rotary encoder and stop button

| Signal          | GPIO | Notes                                          |
|-----------------|------|------------------------------------------------|
| Encoder CLK     | 33   | Internal pull-up                               |
| Encoder DT      | 34   | Input-only: uses the KY-040's onboard pull-up  |
| Encoder SW      | 35   | Input-only: uses the KY-040's onboard pull-up  |
| Stop button     | 4    | Wired to GND, internal pull-up                 |

Buttons are active LOW (pressed = connected to GND).

The KY-040 has 10 kΩ pull-ups (marked `103`) on CLK, DT and SW, so no extra resistors are needed. Power its **+** pin from **3.3 V**, not 5 V, or those pull-ups put 5 V on the ESP32 pins.

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

1. **Name**: the screen shows the alphabet `a`–`z` and a space (drawn as `_`), with `>` on the selected letter. Press to add it. Turn past the last letter for **Delete letter**, **Finish name** and **Exit no save**. The first letter of each word is capitalized automatically. Names can be up to 39 characters; trailing spaces are dropped.

   ```
   Name:Mocha_
    a b c d e f g h i j
    k l m n o p q r s t
    u v w x y z>_ more
   ```
2. **Amounts**: for each pump 1–8, turn to set the mL, then press to go to the next pump. Each click is 0.5 mL, and spinning fast moves 2 mL per click.
3. **Finish**: pick **Save drink**, **Edit drink** (back to the name with your amounts kept), or **Exit no save**. A saved drink is added to the end of `/recipes.jsonl` with the next free `id`, and the menu jumps to it. A drink with every pump at 0 mL can't be saved.

The stop button does nothing while programming. If the card has no `recipes.jsonl` yet, the menu shows only **+ New drink**, and saving creates the file.

When you edit a drink, the editor opens with `>` on **Finish name**, so pressing goes straight to the amounts. Edit and Delete rewrite `/recipes.jsonl` through a temporary `/recipes.tmp`, which replaces the original only after the full copy is written. You can still edit `/recipes.jsonl` on a computer. Keep one drink per line, and give each drink a unique `id`.

## Barista web page

A barista can order drinks from the register, a tablet or a phone on the same WiFi. The page lists the drinks, shows what the machine is doing, and has a **STOP** button. A web order only shows on the LCD (`Web order: ... Cup in, press knob`). Nothing pours until someone at the machine presses the knob. Orders are refused while someone is using the knob or a drink is pouring.

The **Drinks** tab manages the drink list from the browser. Tap **+ New drink**, or tap a drink to edit it. You can change the name and the mL for each of the eight pumps, then **Save**. **Delete** asks you to confirm first. Saving and deleting need the PIN. They follow the same rules as the machine: a name of 1�39 plain characters, 0�100 mL per pump, and at least one pump above 0. They're only allowed while nobody is using the knob and nothing is pouring, since rewriting the recipe file mid-pour could throw off pump timing. If the list changed since the page loaded (someone edited a drink on the machine or in another browser), the page reloads the list instead of changing the wrong drink.

### Settings (WiFi and PIN)

WiFi starts **off**. Set it up on the machine: scroll to the last menu entry, **Settings**, and press. The menu is knob-only with no PIN, because customers can't reach the machine.

- **WiFi network**: scans for nearby networks. Pick one and type its password. The list ends with:
  - **Hidden network**: type the network name, then the password.
  - **Own network**: the dispenser makes its own network, `SyrupDispenser`. Join it from the register and open `http://192.168.4.1/`. Use this on school WiFi, which usually blocks devices from reaching each other. Set a password of 8+ characters, or leave it empty for an open network (the PIN still guards the page).
  - **WiFi off**.
- **Web PIN**: what the page asks for before sending orders (default `1234`).
- **Web address**: the address to open on the register, plus the network and PIN. It updates when WiFi connects.

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
