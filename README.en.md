# NumMasterGear

Sega **Master System** and **Game Gear** emulator for the **NumWorks** calculator (N0110, N0115, N0120).

It runs as a `.nwa` application: you send the app and your ROM to [my.numworks.com/apps](https://my.numworks.com/apps), and the game starts from the calculator's menu.

*Français : [README.md](README.md)*

## Features

- `.sms` (Master System) and `.gg` (Game Gear) files, detected automatically from the ROM header.
- Full-screen display (default) or the console's original pixel size.
- Speeds x1, x2, x3, x4 and x1.5.
- Full state saving, which also works for games without a battery backup.
- Save and quit, save without quitting, erase the current game's save.
- Game reset.

## Controls

| Key | Action |
|---|---|
| Arrows | D-pad |
| OK | Button 1 |
| Back | Button 2 |
| Backspace (clear) | Pause (Master System); Start, so pause (Game Gear) |
| π | Reset the game |
| + / - | Fill the screen / original size |
| 1 / 2 / 3 / 4 / . | Speed x1 / x2 / x3 / x4 / x1.5 |
| √ | Save without quitting |
| 0 | Save and quit |
| Toolbox (hold ~1.5 s) | Erase the current game's save |
| Home | Quit without saving |

## Download

Latest version: [NumMasterGear.nwa](https://github.com/LeJoueurStar/NumMasterGear/releases/latest/download/NumMasterGear.nwa)

Then install it on [my.numworks.com/apps](https://my.numworks.com/apps) with your ROM (see below).

## Build

You need `arm-none-eabi-gcc`, `make`, `node` and `npm` (for example Debian under Termux with proot-distro).

```
make clean && make build
```

The output is `output/NumMasterGear.nwa`.

## Installation

1. Open [my.numworks.com/apps](https://my.numworks.com/apps) and send `NumMasterGear.nwa`.
2. Also send your ROM (`.sms` or `.gg`) as the app's data file.
3. Install the app on the calculator.

If you had an older version installed, reset the calculator before installing this one.

## Known limitations

- Korean cartridges (special mappers) are not fully supported for restoring saves.
- Saves use Epsilon's file records. They are deleted when another app is installed, unless you keep a copy elsewhere.
- The calculator has no speaker: sound is not emulated.

## Legal

This project does not contain any game ROM. Use your own cartridges or free games.

## License

This project is distributed under the **GPL-3.0** license (see [LICENSE](LICENSE)), except for `src/sms/z80.c`, which remains under its author's license (Juergen Buchmueller, then R. Danbrook): free, non-commercial use, with credit. The other reused files keep their original licenses (see [CREDITS.md](CREDITS.md)).
