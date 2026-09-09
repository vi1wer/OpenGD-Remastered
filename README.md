<div align="center">

# OpenGD Remastered

**A remastered fork of [OpenGD](https://github.com/Open-GD/OpenGD)** with bug fixes, gameplay improvements, UI work, and quality-of-life changes.

[Repository](https://github.com/vi1wer/OpenGD-Remastered) · [Issues](https://github.com/vi1wer/OpenGD-Remastered/issues) · [Releases](https://github.com/vi1wer/OpenGD-Remastered/releases)

![platform](https://img.shields.io/badge/platform-Windows-blue)
![license](https://img.shields.io/badge/license-GPL--3.0-blue)
![version](https://img.shields.io/badge/version-1.3.0-green)

</div>

## About

OpenGD Remastered continues the open-source Geometry Dash client started by the OpenGD team. This fork focuses on making the game playable: official levels, menus closer to 2.2, garage / shop / creator UI, portals, orbs, triggers, practice mode, and many crash / collision fixes.

Powered by [axmol](https://github.com/axmolengine/axmol).

**Version 1.3.0** — see [CHANGELOG.md](CHANGELOG.md).

## What's new vs upstream OpenGD

- Playable official levels with many physics / collision fixes
- Portals, orbs, pads, dual mode, dash, and expanded trigger support
- Teleport portals (classic Y-offset behavior)
- Slope collisions and decoration / no-touch handling
- Main menu, Garage (icons + colors), Creator layout, Shop
- Pause menu, practice checkpoints (Z / X), progress UI
- Official level select page colors (`LevelSelectLayer::colorForPage` / `GameToolbox::colorForIdx`)
- Broader object support and main-level rating icons
- Debug options (hitboxes, progress overlays, and related toggles)
- **1.2:** Level Settings, editor playtest dual/solo, window resize fixes, dual ceiling bounds

See [CHANGELOG.md](CHANGELOG.md) for details.

## Status

Playable on Windows for local / official-style levels. Editor is a **preview** (Level Settings + playtest); not a full Geometry Dash 2.2 clone — online features and some advanced systems are still incomplete.

## Requirements

- Windows (primary target for 1.2)
- CMake 3.20+
- C++20 compiler (MSVC / VS 2022 recommended)
- [axmol](https://github.com/axmolengine/axmol) (`AX_ROOT`)
- Geometry Dash **2.2** `Resources` placed next to the built executable (same workflow as upstream OpenGD)

## Build (Windows)

```powershell
git clone https://github.com/axmolengine/axmol
cd axmol
./setup.ps1
# restart the terminal so AX_ROOT is available

git clone https://github.com/vi1wer/OpenGD-Remastered.git
cd OpenGD-Remastered
cmake -B build
cmake --build build --config RelWithDebInfo --target OpenGD
```

## License

GPL-3.0 — see [LICENSE](LICENSE).

Fork of [OpenGD](https://github.com/Open-GD/OpenGD); same license terms apply.

## Credits

- [OpenGD](https://github.com/Open-GD/OpenGD) and upstream contributors
- [axmol](https://github.com/axmolengine/axmol)
- [GD 1.0 decomps](https://github.com/Wyliemaster/Geometry-Dash-1.0) by Wylie
- [GD Physics decomps](https://github.com/camila314/gdp) by Camila
- [GD 2.1 decomps](https://github.com/matcool/gd-decomps) by mat
- [hps](https://github.com/jl2922/hps)
- [gdclone](https://github.com/opstic/gdclone)

## Disclaimer

Unofficial fan project. Not affiliated with RobTop Games.
