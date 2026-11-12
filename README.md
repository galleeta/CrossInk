# CrossInk + PIN Lock (unofficial fork)

Fork of [CrossInk](https://github.com/uxjulia/CrossInk) that adds a simple 4-digit PIN screen
when the device wakes from sleep. Casual privacy only, not encryption:
anyone with access to the SD card can bypass it.

> Looking for the original, unmodified CrossInk? It lives at
> [uxjulia/CrossInk](https://github.com/uxjulia/CrossInk). This repository is only
> a fork with the PIN lock added on top.

- **Branch with the PIN:** `pin-lock`
- **Based on CrossInk version:** X.Y.Z  _(update this on every rebase)_
- **Target device:** Xteink X4 Pro / `x4-pro`

## About this fork

Please treat it as a small personal project, tested only on my Xteink X4 Pro, and use it at your own risk. 

If something breaks in the future, or you want a change, you are very welcome to
try it yourself: the changes are small and explained in "What this fork changes"
below. I am no expert but I made this with the help of Claude free plan in a couple hours.

## Install
Download the `.bin` from Releases and flash it as you would the original.

## Usage

1. Flash this firmware.
2. On the SD card create `/.crosspoint/pin.json` with a 4 digit pin
   - Example: `{"pin": "1234"}`
   - It must be exactly 4 digits
   - The `.crosspoint` folder is hidden: enable "show hidden files" in your file manager
3. On the next boot or wake the device asks for the PIN on a touch keypad.

To change the PIN, edit the file. To disable it, delete the file.
Silent restarts (e.g. after changing settings) do not ask for the PIN.

**Not real security:** the PIN is stored in plain text on the SD card.
It only keeps casual users out of your device (for privacy or to avoid someone editing your settings...)

## Getting the firmware

### Option 1: download a release (no build needed)

Download the `firmware.bin` from the [Releases](../../releases) page and flash it (see "Flashing").

### Option 2: build it yourself

1. Install **Git**, **VS Code** and the **PlatformIO IDE** extension (the first
   start takes a few minutes while it installs its tools). On Debian you may
   also need `sudo apt install python3-venv python3-pip`. 
2. Clone this fork **with submodules** (the SDK is a submodule; without it the
   build fails with a `freeink-sdk ... not a directory` error):
   ```bash
   git clone --recursive -b pin-lock https://github.com/galleeta/CrossInk.git
   cd CrossInk
   ```
3. Open the folder in VS Code.
4. In the PlatformIO panel: **Project Tasks → x4-pro → General → Build**
   (use `x4-pro`, not `x4-pro-debug` or `x4-pro-simulator`, for normal use).
5. The result is `.pio/build/x4-pro/firmware.bin`.

## Flashing

You have two options:

- **Web flasher (recommended):** use a web flasher that accepts a custom file, for example https://crosspointreader.com/install
   - Connect the device with a USB-C **data** cable
   - Select your device (Xteink X4 Pro)
   - Go to "Advanced options" and select "Install a firmware .bin file from your computer."
   - Select the `firmware.bin` file you downloaded or built and flash it

- **PlatformIO:** connect the device with a USB-C **data** cable and run
  **x4-pro → General → Upload**. 

On Linux you need access to the serial port: `sudo usermod -aG dialout $USER`, then log out and back in.
Also, if you're having trouble flashing it, try using a non Flatpak browser.

Do not use the built-in updater with this firmware, it would install the official crossink build, which
has no PIN.

## What this fork changes

Almost everything lives in new files, so merging upstream updates is easy:

| File | Change |
| --- | --- |
| `src/PinStore.h` / `.cpp` (new) | Reads `/.crosspoint/pin.json`. |
| `src/activities/util/PinLockActivity.h` / `.cpp` (new) | The PIN screen (touch keypad; button fallback on devices without touch). It blocks global shortcuts and gestures while shown. |
| `src/main.cpp` (about 17 lines) | The boot routing chain at the end of `setup()` is wrapped in a lambda, `launchInitialActivity`. If a PIN is configured (and the boot is not a silent restart, network resume, recovery or crash), `PinLockActivity` is shown first and calls that lambda once the PIN is right. Otherwise the lambda runs immediately, as before. |

## Updating when CrossInk releases a new version

I will try to keep the firmware.bin available in the Releases section reasonably up to date with the official CrossInk version and its latest features.

However, updates may not always be immediate, and I may eventually stop maintaining this fork. If that happens, or if you simply don't want to wait for an updated release, you can build your own firmware.bin from the source code by following these steps:

1. **Prepare the tools** (once): Git, VS Code and PlatformIO, as described in
   "Option 2: build it yourself" above.
2. **Clone this fork** with submodules (skip this step if you already have it
   cloned):
   ```bash
   git clone --recursive -b pin-lock https://github.com/galleeta/CrossInk.git
   cd CrossInk
   ```
3. **Add the original CrossInk repository** as a second remote (once):
   ```bash
   git remote add upstream https://github.com/uxjulia/CrossInk.git
   ```
4. **Get the new CrossInk code and put the PIN lock on top of it.** Replace
   `<default-branch>` with the branch the original project uses (run
   `git remote show upstream` and look for the line `HEAD branch`):
   ```bash
   git checkout pin-lock
   git fetch upstream
   git rebase upstream/<default-branch>
   ```
   If git reports conflicts, open the listed files in VS Code, keep both sides
   where they overlap, then run `git add <file>` and `git rebase --continue`.
   The most likely file is `src/main.cpp`; the "What this fork changes" table
   above tells you what has to be there. To give up and go back to how things
   were, run `git rebase --abort`.
5. **Update the submodules**, in case the SDK changed:
   ```bash
   git submodule update --init --recursive
   ```
6. **Build and flash** as described in "Getting the firmware" and "Flashing".
   Check that the device boots and the PIN screen appears.

## Maintainer notes (how I update the release)

Steps for the repository owner, from the root of the project:

```bash
git checkout pin-lock
git status                                 
git branch backup-before-update            

git fetch upstream
git rebase upstream/<default-branch>        # see 'HEAD branch' in: git remote show upstream
git submodule update --init --recursive
```

1. Build **x4-pro**, flash it and check that the device boots, the PIN screen
   appears and a correct PIN lets me in.
2. Update "Based on CrossInk version" at the top of this README, commit it.
3. Push the rebased branch: `git push --force-with-lease` (the rebase rewrote
   history, so a plain push is rejected).
4. Publish a new release: **Releases → Draft a new release**, new tag
   (e.g. `v1.5.1-pin1`), attach `.pio/build/x4-pro/firmware.bin`, and say in
   the description which CrossInk version it is based on.
5. Delete the safety branch once everything works:
   `git branch -D backup-before-update`.

If something goes wrong and I want to start over:
`git reset --hard backup-before-update`.

## Credits 

All of the firmware is the work of the CrossInk project and its upstream
projects; this fork only adds the PIN lock. 

---

# Original CrossInk README

<details>
<summary>Click to show the README of the original CrossInk project (unmodified)</summary>

> **This is a personal fork of [CrossPoint Reader](https://github.com/crosspoint-reader/crosspoint-reader)** with a focus on improved fonts and minimal reading stats.

### Supported Devices

- Xteink X3
- Xteink X4
- Xteink X4 Pro
- Xteink X4 Classic
- Seeed Studio Sticky

## What's different in this fork

My goal with this fork was to maintain the core Crosspoint firmware while integrating my preferred typography and some lightweight reading statistics. I’ve focused on keeping the underlying system stable while layering in a few "nice-to-have" features and UI refinements along the way.

<table>
  <tr>
    <td align="center">
      <img src="./docs/images/bitter-small-15-margin.jpg" alt="Font: Bitter, Size: 12 pt, Margin: 15" /><br/>
      <em>Font: Bitter, Size: 12 pt, Margin: 15</em>
    </td>
    <td align="center">
      <img src="./docs/images/reading-stats.jpg" alt="Reading Stats with custom front button mapping shown" /><br/>
      <em>Reading Stats with custom front button mapping shown</em>
    </td>
  </tr>
</table>

### Highlights

- New reader fonts: Lexend Deca and Bitter.
- Music notation and selected supplemental Unicode glyph support to be able to render Project Hail Mary accurately.
- Added a custom `Minimal` theme and sleep screen option for the minimalists out there.
- Added a custom `Dashboard` theme and sleep screen option for reading stats enthusiasts.
- Reader font sizes: 10 pt, 12 pt, 14 pt, and 16 pt.
- Added ~~strikethrough~~ support.
- Made <u>underlines</u> thicker for better visibility.
- Added support for `<hr>` section breaks.
- Added support for "redaction" style rendering.
- Added improved support for tables with simple markup.
- Added ability to add bookmarks.
- Added ability to remap front buttons that only applies in the reader.
- Added Focus Reading and Guide Dots as optional reader modes.
- Added Force Paragraph Indents for books that render as one giant wall of text.
- Added ability to pin a sleep image as a favorite. The favorited image will always be displayed when your sleep settings are set to `Custom` or `Cover + Custom` (when no cover is available).
- Added more in-reader control remapping options for side buttons, short power button clicks, and long-press menu actions, and more.
- Added ability to mark a book as finished from the in-book menu. A pop-up will also display once 99% of the book is reached. This status allows tracking of total books read.
- Added ability to move finished books to "Read" folder.
- In-book menu to quickly adjust reader options without having to exit the book.
- Reading stats: total books read, total reading time, number of sessions, pages turned, average session time, pages turned per minute. You can also set your reading stats as your sleep screen.
- All-time reading stats [syncing](./docs/reading-stats-sync.md) between two CrossInk devices.
- Reading [progress sync](./docs/nearby-position-sync.md) between two CrossInk devices.
- Added customizable Auto Page Turn Interval (anything between 5-120 seconds).
- Added ability to view Recent Books as a 3x3 grid view.
- To view a more detailed list for each version, visit the [releases](https://github.com/uxjulia/CrossInk/releases) page to read release notes.

---

### Reader Fonts

The default fonts have been replaced with Lexend Deca and Bitter. These fonts have been chosen specifically to improve reading fluency and e-ink performance. These 'sturdier' typefaces feature uniform stroke weights and open geometries, allowing the X4/X3 to render crisp, high-contrast text with font-aliasing on while significantly reducing ghosting and artifacts.

- [Lexend Deca](https://fonts.google.com/specimen/Lexend+Deca) - A research-backed sans-serif typeface designed to improve reading fluency. Lexend was engineered based on the theory that reading issues are often a design problem (visual crowding) rather than a cognitive one.
- [Bitter](https://fonts.google.com/specimen/Bitter) - A "contemporary" slab serif typeface for text, it is specially designed for comfortably reading on digital screens. The consistent stroke weight of Bitter helps it render particularly well on e-ink devices. The medium weight has been chosen specifically for improved rendering on the X4/X3.

The UI now uses [Inter](https://fonts.google.com/specimen/Inter) as the display font which has improved readability at smaller sizes.

### Music and Supplemental Glyphs

- Built-in reader fonts include music notation, selected Cyrillic glyphs, and the Project Hail Mary CJK fallback ranges. Additional SD-card fonts retain emoji fallback support.

---

### Font Sizes

CrossInk includes 10 pt, 12 pt, 14 pt, and 16 pt built-in reader font sizes.

See [SD Card Fonts](./docs/sd-card-fonts.md) for installing additional font families and size ranges.

---

### Reader features

Reader Options, Focus Reading, Guide Dots, Force Paragraph Indents, reading stats, and finished-book behavior are documented in [Reader Features](./docs/reader-features.md).

### Custom button actions

CrossInk adds configurable button shortcuts.

See [Controls](./docs/controls.md) for the full action list and defaults.

---

## Tips for the best reading experience

CrossInk runs on an ESP32-C3 with limited RAM, so very large folders or complex EPUBs can be slower than they would be on a phone, tablet, or desktop app.

- Keep folders under about 200 files. For the smoothest browsing, aim for 50-100 files per folder.
- Having 1000+ books on the SD card is fine if they are split into smaller folders, such as by author, series, genre, or read/unread status.
- Avoid putting every book in the SD card root. The file browser has to scan and sort the current folder before it can show it.
- Text-first EPUBs are the best fit. Large image-heavy EPUBs, scanned books, comics, and omnibus files with thousands of sections may load slowly or fail under memory pressure.
- As a rough target, EPUBs under 20 MB tend to work the best. Files over 50 MB may still work, but they are more likely to be slow or memory-sensitive, especially if they contain many large images.
- If an EPUB is unusually slow, try [optimizing](./docs/webserver.md#epub-optimization) it with the built-in web optimizer (via File Transfer) before copying it to the SD card: remove unused high-resolution images, split very large omnibus files, and avoid embedding multiple full font families when possible.
- Use a reliable SD card and leave some free space. CrossInk stores settings, reading progress, cache files, stats, and generated book data on the card.

---

## Installation

The fastest way to install Crossink is by using Inky, Crossink's web companion app: https://inky.crossink.dev/#flash-tools

Download a `firmware-*.bin` from the [releases page](https://github.com/uxjulia/CrossInk/releases), then flash it with the web installer or command line.

See [Installation](./docs/installation.md) for step-by-step flashing and revert instructions.

---

## Guides & Documentation

Visit [https://www.crossink.dev](https://www.crossink.dev) for more user guides and additional documentation.

---

## Development quick start

CrossInk uses PlatformIO for building and flashing firmware. See [Getting Started](./docs/development/getting-started.md) for prerequisites, clone setup, and validation commands.

### Nix/NixOS

Nix/NixOS users can enter the development shell with either `nix develop` (flakes) or `nix-shell`:

```bash
nix develop -f nix
# or
nix-shell nix
```

To flash a connected ESP32-C3 device, enable PlatformIO's udev rules in your NixOS configuration:

```nix
services.udev.packages = with pkgs; [ platformio-core.udev ];
```

After rebuilding the system configuration, reconnect the device or reload udev rules.

### Build / flash / monitor

Connect your device to your computer via a USB cable. Before the first build, initialize the repository's submodules (including `freeink-sdk`):

```sh
git submodule update --init --recursive
```

Then flash the firmware using the correct environment for the device. The `default` environment is for the X3/X4 devices. ESP32-S3 devices have their own named environments.

```sh
pio run -e default --target upload
```

If PlatformIO reports `PackageException: Can not create a symbolic link for freeink-sdk/libs/hardware/BatteryMonitor, not a directory`, the `freeink-sdk` submodule is not initialized. Run the submodule command above and retry.

See [Testing and Debugging](./docs/development/testing-debugging.md) for serial logging, simulator checks, static analysis, and bug-report guidance.

---

## Notice on Contributions

This repository does not accept pull requests. Feature requests may be opened in [discussions](https://github.com/uxjulia/CrossInk/discussions), but major features requiring ongoing support should be directed upstream to [CrossPoint](https://github.com/crosspoint-reader/crosspoint-reader).

---

If you'd like to show some love and support ongoing development, please consider supporting me on Ko-fi.

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/Q5Q01M6S7)

</details>
