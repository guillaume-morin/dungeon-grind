/*
 * game.h — Central header for Dungeon Grind.
 *
 * Every struct, enum, constant and function prototype lives here.
 * The game is split into five modules:
 *   data.c  — static read-only game data (classes, items, enemies, skills)
 *   hero.c  — hero creation, stat computation, talent allocation, equip logic
 *   combat.c — auto-combat tick loop, skill engine, loot, boss encounters
 *   save.c  — binary save/load with versioned format
 *   ui.c    — all ncurses rendering and input handling
 *   main.c  — game loop (~60fps polling) and timer-based combat ticks
 */
#ifndef GAME_H
#define GAME_H

/* PDCurses (Windows) and ncurses (macOS/Linux) share the curses API.
 * PDCurses provides <curses.h>; ncurses provides both <ncurses.h> and
 * <curses.h>. We use <curses.h> as the portable common denominator. */
#include <stdint.h>

#if defined(_WIN32)
#include <curses.h>
#else
#include <ncurses.h>
#endif

/* ── dimensions ────────────────────────────────────────────────────── */

#define SCREEN_W       80
#define SCREEN_H       24
#define LEFT_W         28
#define RIGHT_W        (SCREEN_W - LEFT_W)
#define HEADER_H       1
#define PANEL_H        (SCREEN_H - HEADER_H)
#define ENEMY_DISPLAY_H 8
#define LOG_H          (PANEL_H - ENEMY_DISPLAY_H)

#define ART_LINES      5
#define ART_COLS       18

/* ── limits ────────────────────────────────────────────────────────── */

#define MAX_NAME        32
#define MAX_DESC        80
#define MAX_LOG         200
#define LOG_LINE_W      (RIGHT_W - 3)
#define MAX_INVENTORY   50
#define MAX_BUFFS       10
#define MAX_SKILL_TIERS 6
#define MAX_ENEMIES_PER_DG 5

/* ── counts ────────────────────────────────────────────────────────── */

#define NUM_STATS       7
#define NUM_SLOTS       7
#define NUM_CLASSES     4
#define NUM_DUNGEONS    9
#define NUM_SAVE_SLOTS  3
#define SOFT_CAP        30
#define BOSS_THRESHOLD  50
#define BOSS_DELAY      6
#define MAX_LEVEL       99

#define NUM_ACHIEVEMENTS 20
#define NUM_TITLES       10
#define NUM_AFFIXES      6

/* ── enums ─────────────────────────────────────────────────────────── */

enum Stat    { STR, AGI, INT_, WIS, VIT, DEF, SPD };
enum Slot    { SLOT_WEAPON, SLOT_HELMET, SLOT_CHEST, SLOT_LEGS,
               SLOT_BOOTS, SLOT_RING, SLOT_AMULET };
enum HeroClass { CLASS_WARRIOR, CLASS_ROGUE, CLASS_MAGE, CLASS_PRIEST };
enum Rarity  { RARITY_COMMON, RARITY_UNCOMMON, RARITY_RARE,
               RARITY_EPIC, RARITY_LEGENDARY, NUM_RARITIES };

enum Screen {
    SCR_SAVE_SELECT, SCR_NEW_SLOT,
    SCR_CLASS_SELECT, SCR_NAME_INPUT, SCR_MAIN,
    SCR_DUNGEON, SCR_EQUIPMENT, SCR_SHOP,
    SCR_CHARACTER, SCR_SKILLS, SCR_CONFIRM_QUIT,
    SCR_ENCYCLOPEDIA, SCR_ENCY_CLASSES, SCR_ENCY_STATS,
    SCR_ENCY_ITEMS, SCR_ENCY_ENEMIES, SCR_ENCY_BOSSES,
    SCR_ENCY_SKILLS, SCR_ENCY_DUNGEONS, SCR_ENCY_COMBAT,
    SCR_ACHIEVEMENTS, SCR_TITLES, SCR_BULK_SELL
};

/* ── color pairs (CP_*) ───────────────────────────────────────────── */

#define CP_DEFAULT   1
#define CP_RED       2
#define CP_GREEN     3
#define CP_YELLOW    4
#define CP_BLUE      5
#define CP_MAGENTA   6
#define CP_CYAN      7
#define CP_WHITE     8
#define CP_HEADER    9
#define CP_BORDER    10
#define CP_SELECTED  11
#define CP_ORANGE    12

/* ── data definitions (read-only templates) ────────────────────────── */

typedef struct {
    const char *name;
    const char *symbol;
    const char *description;
    const char *resourceName;
    int   primaryStat;
    int   baseStats[NUM_STATS];
    int   growthPerLevel[NUM_STATS];
    int   baseHp;
    int   hpPerLevel;
    int   baseResource;
    int   maxResource;
    int   resourceRegen;
} ClassDef;

/*
 * ItemDef uses char[] (not const char*) so the entire struct can be
 * binary-copied into Hero.equipment[] and saved/loaded with fwrite/fread.
 * classMask: 0 means equippable by any class; otherwise a bitmask
 * of (1 << classId) values — see CM_WAR/CM_ROG/etc. in data.c.
 */
typedef struct {
    char name[MAX_NAME];
    int  slot;
    int  rarity;
    int  levelReq;
    int  price;
    int  stats[NUM_STATS];
    int  classMask;
} ItemDef;

typedef struct {
    const char *name;
    int  dungeonIdx;
    int  hp, attack, defense, speed;
    int  xpReward, goldReward;
    float dropChance;
    const char *art[ART_LINES];
} EnemyTemplate;

typedef struct {
    const char *name;
    int  levelReq;
    const char *description;
    int  enemyIdx[MAX_ENEMIES_PER_DG];
    int  numEnemies;
    int  bossIdx;
    int  colorPair;
} DungeonDef;

/*
 * SkillDef is a data-driven ability template — combat.c's apply_skill()
 * interprets these fields generically rather than hard-coding each skill.
 * Conditional triggers: hpBelow/enemyHpBelow (% thresholds, 0 = always).
 * Buff fields (buffTicks > 0) create a runtime Buff struct.
 */
typedef struct {
    const char *name;
    const char *description;
    int  cooldown;
    int  resourceCost;
    float dmgMul;
    int   numHits;
    int   ignoreArmor;
    int   stunTicks;
    int   buffTicks;
    float buffDmgMul;
    float buffDodge;
    float buffCritBonus;
    int   buffImmune;
    int   buffHealPct;
    int   buffDmgPct;
    int   buffShieldPct;
    int   healPct;
    int   manaPct;
    int   hpBelow;
    int   enemyHpBelow;
} SkillDef;

typedef struct {
    const char *name;
    const char *description;
    int  bonusType;     /* 0=maxHP flat, 1=dmg%, 2=xp% */
    int  bonusValue;
} AchievementDef;

typedef struct {
    const char *name;
    int  achievementIdx; /* index into achievements array, -1 if special */
} TitleDef;

/* Achievement bitfield macros (128 bits across 4 uint32s). */
#define ACH_HAS(h, n) ((h)->achievements[(n)/32] & (1u << ((n)%32)))
#define ACH_SET(h, n) ((h)->achievements[(n)/32] |= (1u << ((n)%32)))

/* ── runtime objects ──────────────────────────────────────────────── */

typedef struct {
    int  stats[NUM_STATS];
    int  maxHp, damage, healPower, tickRate;
    float critChance, critMult, dodgeChance;
    float blockChance, blockReduction, dmgReduction;
    float xpMultiplier;
    int   flatDmgReduce;
} EStats;

typedef struct {
    char name[MAX_NAME];
    int  templateIdx;
    int  hp, maxHp;
    int  attack, defense, speed;
    int  xpReward, goldReward;
    float dropChance;
    int  stunned;
    int  slowed;
} Enemy;

typedef struct {
    char name[MAX_NAME];
    int  ticksLeft;
    float damageMul;
    float dodgeBonus;
    float critDmgBonus;
    int  healPerTick;
    int  immune;
    int  manaShield;
    int  doubleDmgNext;
    int  dodgeNext;
    int  shieldHp;
    int  dmgPerTick;
} Buff;

typedef struct {
    char name[MAX_NAME];
    int  classId;
    int  level, xp, gold;
    int  hp, maxHp;
    int  resource, maxResource;

    int  talents[NUM_STATS];
    int  talentPoints;

    int  hasEquip[NUM_SLOTS];
    ItemDef equipment[NUM_SLOTS];
    ItemDef inventory[MAX_INVENTORY];
    int  invCount;

    Buff buffs[MAX_BUFFS];
    int  numBuffs;

    int  skillChoices[MAX_SKILL_TIERS];   /* -1 = not yet chosen for this tier */
    int  skillCooldowns[MAX_SKILL_TIERS];

    int  totalKills, deaths;
    int  totalGoldEarned, totalXpEarned;
    int  highestLevel;

    uint32_t achievements[4];
    int  activeTitle;
    int  hardMode[NUM_DUNGEONS];
    int  bossKills[NUM_DUNGEONS];
    int  eliteKills;
    int  hardModeClears;
    int64_t lastSaveTime;
} Hero;

typedef struct {
    char text[LOG_LINE_W + 1];
    int  color;
} LogLine;

typedef struct {
    int  exists;
    char name[MAX_NAME];
    int  level;
    int  classId;
} SaveSlotInfo;

typedef struct {
    Hero    hero;
    Enemy   enemy;
    int     hasEnemy;
    int     inDungeon;
    int     paused;
    int     currentDungeon;

    LogLine log[MAX_LOG];
    int     logCount;

    int     screen;
    int     menuIdx;
    int     shopSlot;
    int     encycSlot;
    int     saveSlot;
    int     equipSort;    /* 0=rarity desc, 1=slot, 2=level desc (bag only) */
    int     equipFilter;  /* 0=all, 1..NUM_SLOTS = slot 0..6 (bag only) */
    SaveSlotInfo slotInfo[NUM_SAVE_SLOTS];

    int     running;
    int     tickCount;
    int     deathTimer;
    int     heroCreated;
    int     needsRender;

    int     dungeonKills;
    int     bossActive;
    int     bossTimer;

    int     isElite;
    int     combatDmgDealt;
    int     combatDmgTaken;
    int     combatHealed;
    int     combatTicks;
    int     hardModeActive;
    int     activeAffixes[2];

    int     offlineXp;
    int     offlineGold;
    int     offlineMin;

    char    nameBuf[MAX_NAME];
    int     nameLen;

    WINDOW *wHeader;
    WINDOW *wLeft;
    WINDOW *wEnemy;
    WINDOW *wLog;
} GameState;

/* ── hero.c ───────────────────────────────────────────────────────── */

/* Initialize a hero with class defaults and zeroed gear/talents. */
Hero    hero_create(int classId, const char *name);
/* Compute all derived stats from base + talents (soft-capped) + equipment. */
EStats  hero_effective_stats(const Hero *h);
/* XP threshold to reach the next level: 50*level + 50. */
int     hero_xp_needed(int level);
/* Current XP as a 0.0–1.0 fraction of the level-up threshold. */
float   hero_xp_pct(const Hero *h);
/* Grant XP; auto-levels (with talent point + full heal). Returns 1 if leveled. */
int     hero_add_xp(GameState *gs, int amount);
/* Spend one talent point on a stat. Returns 0 if out of points. */
int     hero_alloc_talent(Hero *h, int stat);
/* Equip item (auto-moves old item to inventory). Caller removes source copy. */
int     hero_equip(Hero *h, const ItemDef *item);
/* Move equipped item to inventory, clear slot, recompute HP. */
void    hero_unequip(Hero *h, int slot);
/* Add HP, clamped to maxHp. */
void    hero_heal(Hero *h, int amount);
/* Add resource, clamped to maxResource. */
void    hero_restore_resource(Hero *h, int amount);

/* ── combat.c ─────────────────────────────────────────────────────── */

/* Run one combat tick: cooldowns, buffs, skills, attack, enemy retaliation, death. */
void    combat_tick(GameState *gs);
/* Spawn the next enemy (or boss if kill count reached BOSS_THRESHOLD). */
void    combat_spawn(GameState *gs);
/* Tick all active buffs: apply HoT/DoT, decrement timers, remove expired. */
void    combat_tick_buffs(GameState *gs);
/* Fire the highest-tier ready skill that passes all resource/threshold checks. */
void    combat_try_skills(GameState *gs);

/* ── ui.c ─────────────────────────────────────────────────────────── */

/* Create ncurses windows and initialize color pairs. */
void    ui_init(GameState *gs);
/* Destroy ncurses windows and call endwin(). */
void    ui_destroy(GameState *gs);
/* Redraw all panels (left menu, enemy/detail, combat log) via wnoutrefresh + doupdate. */
void    ui_render(GameState *gs);
/* Process one keypress; dispatches to current screen's input handler. */
void    ui_handle_key(GameState *gs, int ch);
/* Append a colored line to the combat log (ring buffer, MAX_LOG entries). */
void    ui_log(GameState *gs, const char *text, int color);

/* ── save.c ───────────────────────────────────────────────────────── */

/* Write hero + dungeon state to binary save file. Returns 1 on success. */
int     save_game(GameState *gs);
/* Load save file into gs; resets combat state and respawns if in dungeon. */
int     load_game(GameState *gs);
/* Scan all save files and populate gs->slotInfo[] for the save-select screen. */
void    save_refresh_slots(GameState *gs);
/* Delete a save file by slot index. */
void    save_delete_slot(int slot);

/* ── data.c ───────────────────────────────────────────────────────── */

/* Bounds-checked accessors for static game data arrays. Return NULL on bad id. */
const ClassDef     *data_class(int id);
const DungeonDef   *data_dungeon(int id);
const EnemyTemplate*data_enemy(int id);
int                 data_num_items(void);
const ItemDef      *data_item(int id);
/* Lookup names/labels for display. */
const char         *data_stat_name(int s);
const char         *data_stat_short(int s);
const char         *data_rarity_name(int r);
int                 data_rarity_color(int r);
const char         *data_slot_name(int s);
int                 data_num_enemies(void);
/* Pick a random item within level range, class, and rarity cap. NULL if none.
 * Only searches static items (Epic/Legendary). For Common-Rare, use data_generate_item. */
const ItemDef      *data_random_drop(int minLvl, int maxLvl, int classMask, int maxRarity);
/* Procedurally generate a Common/Uncommon/Rare item with WoW-style animal suffix.
 * Fills *out with name, stats, price, classMask. Rarity must be <= RARITY_RARE. */
void                data_generate_item(ItemDef *out, int slot, int rarity, int level, int classId);
/* Generate a deterministic shop item (class-optimal suffix, 2x price). */
void                data_shop_item(ItemDef *out, int slot, int level, int classId);
/* Get a specific skill definition: SKILLS[classId][tier][option]. */
const SkillDef     *data_skill(int classId, int tier, int option);
/* Required hero level for the given skill tier (10, 20, 30, 40, 50, 60). */
int                 data_skill_level(int tier);

const AchievementDef *data_achievement(int id);
const TitleDef       *data_title(int id);
const char           *data_affix_name(int id);

void    check_achievements(GameState *gs);

#endif /* GAME_H */
