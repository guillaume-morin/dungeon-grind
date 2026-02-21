# Dungeon Grind

A terminal-based idle RPG written in C with ncurses. 80×24 fixed layout, no mouse, no dependencies beyond libc and a curses library. Runs on macOS, Linux, and Windows.

The hero fights automatically. You decide where the power goes.

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

## How It Works

You pick a class, name your hero, and select a dungeon. Combat runs on a tick timer — the hero auto-attacks, uses skills, takes damage, and loots. Your job is to allocate talent points, choose skills, buy and equip gear, and decide when to push into harder dungeons.

There is no passive stat growth from leveling. The hero only gets stronger through:
- **Talent points** — manually allocated into 7 stats (1 point per level)
- **Equipment** — dropped by enemies or purchased from the shop
- **Skills** — chosen at level milestones (10, 20, 30, 40, 50, 60)

Death resets the dungeon to stage 1 and costs 10% gold.

## Controls

| Key | Action |
|---|---|
| `W` / `S` or `Up` / `Down` | Navigate menus |
| `A` / `D` or `Left` / `Right` | Switch tabs (items, encyclopedia) |
| `Enter` | Confirm / Select |
| `Esc` | Back |
| `1-4` | Bulk sell items by rarity (equipment screen) |
| `U` | Unequip (equipment screen) |
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
| STR | Physical damage (1.5x for warrior) | +1 max HP per point |
| AGI | Crit chance (AGI/200), dodge (AGI/300) | Attack speed for rogue |
| INT | Spell damage (1.8x for mage) | +0.2% XP bonus per point |
| WIS | Heal power (1.5x for priest) | Flat damage reduction (WIS * 0.15) |
| VIT | +5 max HP per point | Heals VIT*2 HP per kill |
| DEF | Damage reduction DEF/(DEF+100) | Block chance (warrior only, DEF/250) |
| SPD | Tick rate: 800 - SPD*8 ms (min 300ms) | — |

Talent points above 30 (soft cap) yield diminishing returns via integer halving: `effective = 30 + (overflow / 2)`. This means every other point past 30 gives no visible bonus — the character screen shows when the next bonus occurs.

## Dungeons

9 dungeons from level 1 to 90. Each contains 5 normal enemy types and 1 boss. A boss spawns after 50 kills, drops guaranteed loot with weighted rarity, then the dungeon pauses for 6 ticks before restarting.

Item drops from normal enemies are rare (~0.1-0.3% per kill). Per-dungeon rarity caps prevent early dungeons from dropping high-tier gear. Boss loot can roll one rarity tier higher than the dungeon cap.

## Skills

At levels 10, 20, 30, 40, 50, and 60, you choose 1 of 2 class-specific skills per tier. Skills auto-fire in combat, highest tier first, with cooldowns and resource costs. Inspired by WoW — bleeds, shields, executes, immunities.

Skills can be reset for 10 talent points from the skills menu.

## Equipment

7 slots: Weapon, Helmet, Chest, Legs, Boots, Ring, Amulet. Armor is class-specific (warriors wear plate, mages wear robes). Rings and amulets are universal.

5 rarity tiers: Common, Uncommon, Rare, Epic, Legendary. Legendary items only exist at level 90.

The shop rotates items appropriate for your class and level. You can swap-equip directly from the shop (old item sold automatically). Bulk sell by rarity with keys 1-4 in the equipment screen.

## Save System

3 save slots stored as binary files in `~/.dungeon-grind/` (macOS/Linux) or `%APPDATA%\.dungeon-grind\` (Windows). Auto-saves every 30 seconds and on quit. Save format uses a magic number + version check — incompatible saves are silently skipped on the slot select screen.

## Architecture

```
src/
  game.h    — All type definitions, constants, and function declarations
  main.c    — Entry point, game loop (60 lines)
  data.c    — Static game data: classes, enemies, items, skills, dungeons
  hero.c    — Hero creation, stat computation, leveling, equipment
  combat.c  — Tick-based auto-combat, skills, loot, boss encounters
  ui.c      — All ncurses rendering and input handling
  save.c    — Binary save/load with versioned format
```

**game.h** is the single header. Every `.c` file includes only `game.h`. All game data lives in `data.c` as static const arrays — no runtime allocation, no dynamic loading. The `GameState` struct holds the entire mutable world state and gets passed everywhere.

**The game loop** in `main.c` is 45 lines: poll input, tick combat if enough time elapsed, auto-save every 30s, render, sleep 16ms. The tick rate is derived from the hero's effective SPD stat. Boss delay and death timers override the normal tick rate.

**Combat** resolves one full exchange per tick: buffs tick down, skills fire (highest tier first), hero attacks with variance/crit/armor, enemy retaliates with dodge/block/shield checks. Absorb shields are checked before HP damage. Skills are data-driven — the `SkillDef` struct has 20 fields covering damage, buffs, DoTs, heals, shields, conditions, and the `apply_skill` function interprets them generically.

**Items** use `char name[MAX_NAME]` instead of `const char *` so the full `ItemDef` struct can be memcpy'd into the hero's equipment/inventory arrays and written to save files without pointer fixup.

**Stat computation** happens in `hero_effective_stats()` which combines base class stats + talent points (with soft cap) + equipment bonuses, then derives all secondary stats (damage, crit, dodge, DR, tick rate, etc.). This function is called frequently and is the single source of truth for combat math.

## License

This project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE) for the full text.
