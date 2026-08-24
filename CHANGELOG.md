# Changelog

## 1.1.0 — 2026-08-24

### Gameplay
- Fixed black / drop orb (ID 1330): correct type, downward force, particles, hold-to-activate
- Pad / orb particles closer to original (bump / ring effects, no giant flash)
- Hitboxes: collidable outline lines work; pixel-art decorations stay non-colliding
- Hardened level load / play against intermittent crashes (HSV parse bounds, null-safe groups/colors, player particles, ground / BG fallbacks, particle release order)

### Levels / content
- Official level select: **Dash** hidden (list ends at Fingerdash)
- Demon face always shows **Hard Demon**; profile / internal rating still counts as **Easy Demon**
- Custom song download panel (boomlings song info → local MP3)

### Menus and UI
- Main level card (`LevelPage`): stars top-right, orbs bottom-left, secret coins bottom-right
- Level names: line break only for Theory Of Everything / ToE 2 (no shrinking)
- Online list / level info: user coins under difficulty rating
- In-level secret coin HUD moved to bottom-right

### Project
- After a successful build, artifacts are deployed to `release/OpenGD` (`deploy-release.ps1` / updated `b.bat`)

### Known limitations
- Still not full Geometry Dash 2.2 parity
- Online / account features incomplete
- Some triggers and editor tools still WIP

---

## 1.0.0 — 2026-08-23

First public **OpenGD Remastered** release (fork of OpenGD).

### Gameplay
- Large PlayLayer / PlayerObject remaster for playable official levels
- Portals, orbs, pads, dual mode, dash support improvements
- Teleport portals using classic Y-offset (object property 54)
- Slope collision fixes
- Practice mode checkpoints (Z place / X remove) and related UI
- Hitbox / progress debug options
- New Best and progress UI polish

### Menus and UI
- Main menu stability and background game layer fixes
- Garage: icon grid, color picker (official `colorForIdx` palette), paint mode
- Creator layer layout closer to 2.2 (locks for unfinished entries)
- Shop layer and catalog
- Pause layer
- Dialog / coming-soon helpers
- Level select background colors per official page palette
- Trailer button and related menu wiring

### Content / objects
- Expanded `object.json` / main level metadata support
- Main level difficulty / rating icons
- Hard streak / animated icon helpers

### Project
- Builds on axmol with the existing CMake layout
- Custom data kept under `Content/Custom`
- Upstream OpenGD GPL-3.0 license retained

### Known limitations
- Not full Geometry Dash 2.2 parity
- Online / account features incomplete
- Some triggers and editor tools still WIP
- Windows is the primary tested platform for 1.0
