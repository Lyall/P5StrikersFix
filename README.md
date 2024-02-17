# Persona 5 Strikers Fix
[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/W7W01UAI9)</br>
[![Github All Releases](https://img.shields.io/github/downloads/Lyall/P5StrikersFix/total.svg)](https://github.com/Lyall/P5StrikersFix/releases)

## Features
- Custom resolution support.
- Ultrawide/narrow aspect ratio support.
- Correct FOV at any aspect ratio.

## Installation
- Grab the latest release of P3RFix from [here.](https://github.com/Lyall/P3RFix/releases)
- Extract the contents of the release zip in to the the game folder. e.g. "**steamapps\common\P5S**" for Steam).

### Steam Deck/Linux Additional Instructions
🚩**You do not need to do this if you are using Windows!**
- Open up the game properties in Steam and add `WINEDLLOVERRIDES="dinput8=n,b" %command%` to the launch options.

## Configuration
- See **P5StrikersFix.ini** to adjust settings for the fix.

## Known Issues
Please report any issues you see.
This list will contain bugs which may or may not be fixed.

- Mouse input in menus is misaligned.
- Floating markers (i.e. enemy health hearts/damage numbers, dialogue interaction markers) are displayed at the wrong aspect ratio.
- Overlays/frames/fades are aligned to a 16:9 frame.

## Screenshots

| ![ezgif-4-8983b25596](https://github.com/Lyall/P5StrikersFix/assets/695941/76edf561-06c0-455c-9bd4-8df7cbc0d07d) |
|:--:|
| Gameplay |

## Credits
[Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) for ASI loading. <br />
[inipp](https://github.com/mcmtroffaes/inipp) for ini reading. <br />
[spdlog](https://github.com/gabime/spdlog) for logging. <br />
[safetyhook](https://github.com/cursey/safetyhook) for hooking.
