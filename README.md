# Dungeon Grind

A terminal-based idle RPG written in C with ncurses. 80×24 fixed layout, no mouse, no dependencies beyond libc and a curses library. Runs on macOS, Linux, and Windows.

The hero fights automatically. You decide where the power goes.

## Quick Play

Never used a terminal or GitHub before? Follow these steps exactly.

### 1. Install Git

**macOS:** Open Terminal (search "Terminal" in Spotlight). Type:
```bash
xcode-select --install
```
Click "Install" when prompted. This gives you `git`, `gcc`, and `make`.

**Linux (Ubuntu/Debian):** Open a terminal and type:
```bash
sudo apt update && sudo apt install git build-essential libncurses-dev
```

**Windows:** Install [WSL](https://learn.microsoft.com/en-us/windows/wsl/install) first, then follow the Linux steps inside the WSL terminal.

### 2. Download and Play

```bash
git clone https://github.com/guillaume-morin/dungeon-grind.git
cd dungeon-grind
./play.sh
```

That's it. The script pulls the latest code, builds, and launches. Make sure your terminal window is at least 80 columns × 24 rows (the default size on most systems).

Your save files live in `~/.dungeon-grind/` and are never affected by updates.

### 3. Update

From now on, just run `./play.sh` from inside the `dungeon-grind` folder.

---

## Installation

### macOS

ncurses ships with macOS. Just build:

```bash
make
./dungeon-grind
```

### Linux (Debian / Ubuntu)

```bash
sudo apt update
sudo apt install build-essential libncurses-dev
make
./dungeon-grind
```

### Linux (Fedora / RHEL)

```bash
sudo dnf install gcc make ncurses-devel
make
./dungeon-grind
```

### Linux (Arch)

```bash
sudo pacman -S gcc make ncurses
make
./dungeon-grind
```

### Windows (MSYS2 / MinGW)

1. Install [MSYS2](https://www.msys2.org/) and open the **MSYS2 MinGW64** terminal.
2. Install dependencies:

```bash
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-pdcurses make
make
./dungeon-grind.exe
```

### Windows (WSL)

Install [WSL](https://learn.microsoft.com/en-us/windows/wsl/install), then follow the Linux instructions above inside the WSL terminal.

### General Notes

```bash
make clean    # remove build artifacts
make run      # build and launch
```

The Makefile auto-detects Windows (via the `OS` environment variable) and links against PDCurses instead of ncurses. On all platforms it compiles with `gcc -Wall -Wextra -O2 -std=c11`.

Save files are stored in `~/.dungeon-grind/` on macOS/Linux and `%APPDATA%\.dungeon-grind\` on Windows.

---

## How It Works

You pick a class, name your hero, and select a dungeon. Combat runs on a tick timer — the hero auto-attacks, uses skills, takes damage, and loots. Your job is to allocate talent points, choose skills, buy and equip gear, and decide when to push into harder dungeons.

There is no passive stat growth from leveling. The hero only gets stronger through:
- **Talent points** — % scaling boosts allocated into class-specific talent trees (1 point per level)
- **Equipment** — dropped by enemies or purchased from the shop
- **Skills** — chosen at level milestones (10, 20, 30, 40, 50, 60)

Death costs 10% gold and resets dungeon progress after a short revive delay.

## Controls

| Key | Action |
|---|---|
| `W` / `S` or `Up` / `Down` | Navigate menus |
| `A` / `D` or `Left` / `Right` | Switch tabs (items, encyclopedia), adjust values |
| `Enter` | Confirm / Select |
| `Esc` | Back |
| `1-4` | Bulk sell items by rarity (equipment screen) |
| `U` | Unequip (equipment screen) |
| `B` | Bulk sell menu (equipment screen) — includes auto-sell settings |
| `R` | Reset skills (skills screen, costs 10 talent points) |

## Classes

| Class | Primary | Resource | Identity |
|---|---|---|---|
| Warrior | STR | Rage | High HP, block chance, armor-heavy |
| Rogue | AGI | Energy | Fast ticks, high crit multiplier (1.8x) |
| Mage | INT | Mana | Burst spell damage, ignore-armor options |
| Priest | WIS | Mana | Sustain through healing, flat damage reduction |

Every stat benefits every class. STR always adds HP and a damage fraction. INT always boosts XP gain. WIS always provides flat DR. The primary stat just scales harder.

## Stats

| Stat | Primary Effect | Secondary Effect |
|---|---|---|
| STR | Physical damage (1.5x for warrior) | +2 max HP per point |
| AGI | Crit chance (AGI/300), dodge (AGI/300) | Attack speed for rogue |
| INT | Spell damage (1.5x for mage) | +0.2% XP bonus per point |
| WIS | Heal power (1.5x for priest) | Flat damage reduction (WIS * 0.15) |
| VIT | +5 max HP per point | Heals VIT*2 HP per kill |
| DEF | Damage reduction DEF/(DEF+100) | Block chance (warrior only, DEF/250) |
| SPD | Tick rate: 150 + 650/(1+SPD*0.02) ms | — |

Talent points above 30 (soft cap) yield diminishing returns via integer halving: `effective = 30 + (overflow / 2)`. This means every other point past 30 gives no visible bonus — the character screen shows when the next bonus occurs.

## Talents

Each class has 3 talent trees with 5 tiers of nodes. Talent points are spent to unlock % scaling boosts — not flat stats. Each node provides two bonuses (e.g., +2% STR and +1% VIT), so tree choice determines your build's identity. Talents are the primary way to specialize your hero.

## Dungeons

9 dungeons from level 1 to 82. Each contains 5 normal enemy types and 1 boss. A boss spawns after 50 kills, drops guaranteed loot with weighted rarity, then the dungeon pauses for 6 ticks before restarting.

Elites spawn occasionally with 1.5× normal stats and guaranteed drops from the normal loot table.

Normal mob drops are rare with randomized item level within the dungeon's range. Per-dungeon rarity caps prevent early dungeons from dropping high-tier gear. High-tier dungeon mobs (D4-D8) have a small chance (~2%) to drop an Epic item.

Boss loot uses tiered weight tables that shift toward Epic at higher dungeons:
- **Early (D0-D3):** Standard weights — mostly Common/Uncommon
- **High-tier (D4-D7):** Epic boosted to 25%, no Legendary
- **Final (D8):** Epic 35%, Legendary 10%

Legendary items only drop from the final dungeon boss.

### Auto-Sell

From the Bulk Sell menu (B key in equipment), you can enable auto-sell to automatically vendor drops below a rarity threshold. Toggle it on/off and set the threshold separately. Auto-sold items give half their vendor price.

## Skills

At levels 10, 20, 30, 40, 50, and 60, you choose 1 of 2 class-specific skills per tier. Skills auto-fire in combat, highest tier first, with cooldowns and resource costs. Inspired by WoW — bleeds, shields, executes, immunities.

Skills can be reset for 10 talent points from the skills menu.

## Equipment

7 slots: Weapon, Helmet, Chest, Legs, Boots, Ring, Amulet. Armor is class-specific (warriors wear plate, mages wear robes). Rings and amulets are universal.

5 rarity tiers: Common, Uncommon, Rare, Epic, Legendary. Legendary items are level 100 and only drop from the final dungeon boss.

The shop rotates items up to Epic rarity appropriate for your class and level. You can swap-equip directly from the shop (old item sold automatically). Bulk sell by rarity with keys 1-4 in the equipment screen.

## Save System

3 save slots stored as binary files in `~/.dungeon-grind/` (macOS/Linux) or `%APPDATA%\.dungeon-grind\` (Windows). Auto-saves every 30 seconds and on quit. Save format uses a magic number + version check — incompatible saves are silently skipped on the slot select screen.

## Architecture

```
src/
  game.h          — All type definitions, constants, and function declarations
  main.c          — Entry point, game loop
  data.c          — Static game data: classes, base stat tables
  data_skills.c   — Skill definitions per class and tier
  data_enemies.c  — Enemy templates, dungeon definitions
  data_items.c    — Static item table, procedural item generation, shop
  data_talents.c  — Talent tree definitions (3 trees × 4 classes)
  hero.c          — Hero creation, stat computation, leveling, equipment
  combat.c        — Tick-based auto-combat, skills, loot, boss encounters
  ui.c            — All ncurses rendering and input handling
  save.c          — Binary save/load with versioned format
```

**game.h** is the single header. Every `.c` file includes only `game.h`. All game data lives in static const arrays — no runtime allocation, no dynamic loading. The `GameState` struct holds the entire mutable world state and gets passed everywhere.

**The game loop** in `main.c` is ~45 lines: poll input, tick combat if enough time elapsed, auto-save every 30s, render, sleep 16ms. The tick rate is derived from the hero's effective SPD stat. Boss delay and death timers override the normal tick rate.

**Combat** resolves one full exchange per tick: buffs tick down, skills fire (highest tier first), hero attacks with variance/crit/armor, enemy retaliates with dodge/block/shield checks. Absorb shields are checked before HP damage. Skills are data-driven — the `SkillDef` struct has 20 fields covering damage, buffs, DoTs, heals, shields, conditions, and the `apply_skill` function interprets them generically.

**Items** use `char name[MAX_NAME]` instead of `const char *` so the full `ItemDef` struct can be memcpy'd into the hero's equipment/inventory arrays and written to save files without pointer fixup.

**Stat computation** happens in `hero_effective_stats()` which combines base class stats + talent points (with soft cap and % scaling) + equipment bonuses, then derives all secondary stats (damage, crit, dodge, DR, tick rate, etc.). This function is called frequently and is the single source of truth for combat math.

## License

This project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for the full text.
