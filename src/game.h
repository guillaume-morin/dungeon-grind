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
#define MAX_LEVEL       100

#define NUM_ACHIEVEMENTS 20
#define NUM_TITLES       10
#define NUM_AFFIXES      6

#define NUM_TALENT_TREES   3
#define TALENT_NODES_PER_TREE 15
#define TALENT_TIERS       5
#define MAX_TALENT_BONUSES 3
#define MAX_ACTIVE_SKILLS  8

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
    SCR_ACHIEVEMENTS, SCR_TITLES, SCR_BULK_SELL, SCR_ENCY_AFFIXES,
    SCR_TALENTS, SCR_DEBUG
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
#define CP_SEL_GREEN   13
#define CP_SEL_BLUE    14
#define CP_SEL_MAGENTA 15
#define CP_SEL_YELLOW  16
#define CP_SEL_CYAN    17
#define CP_SEL_RED     18

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

/* Talent bonus types: 0x00–0x0F flat (applied first), 0x10+ percentage (scale total). */
enum TalentBonusType {
    TB_NONE = 0,
    TB_FLAT_STR, TB_FLAT_AGI, TB_FLAT_INT, TB_FLAT_WIS,
    TB_FLAT_VIT, TB_FLAT_DEF, TB_FLAT_SPD,
    TB_FLAT_HP, TB_FLAT_CRIT, TB_FLAT_DODGE, TB_FLAT_BLOCK,
    TB_FLAT_DMGREDUCE,
    TB_PCT_PHYS_DMG = 0x10, TB_PCT_SPELL_DMG, TB_PCT_HEAL,
    TB_PCT_HP, TB_PCT_CRIT_DMG, TB_PCT_DODGE, TB_PCT_ARMOR,
    TB_PCT_ATKSPD, TB_PCT_XP, TB_PCT_GOLD,
    TB_PCT_STR, TB_PCT_AGI, TB_PCT_INT, TB_PCT_WIS, TB_PCT_VIT, TB_PCT_DEF
};

typedef struct {
    uint8_t type;
    int16_t perRank;
} TalentBonus;

typedef struct {
    char     name[20];
    char     desc[48];
    uint8_t  tier;
    uint8_t  col;
    uint8_t  maxRank;
    uint8_t  isActive;
    int8_t   prereqNode;
    uint8_t  skillIdx;
    TalentBonus bonus[MAX_TALENT_BONUSES];
} TalentNodeDef;

typedef struct {
    char          name[16];
    uint8_t       nodeCount;
    uint8_t       color;
    TalentNodeDef nodes[TALENT_NODES_PER_TREE];
} TalentTreeDef;

typedef struct {
    TalentTreeDef trees[NUM_TALENT_TREES];
} ClassTalentDef;

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

    int  skillChoices[MAX_SKILL_TIERS];
    int  skillCooldowns[MAX_SKILL_TIERS];

    uint8_t talentRanks[NUM_TALENT_TREES][TALENT_NODES_PER_TREE];
    int  talentTreePoints[NUM_TALENT_TREES];
    int  activeSkillCooldowns[MAX_ACTIVE_SKILLS];

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
    int  autoSellThreshold; /* 0=off, 1..4 = auto-sell items below that rarity */
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

    int     talentTreeIdx;
    int     talentNodeIdx;

    int64_t offlineShowUntil;
    int     offlineXp;
    int     offlineGold;
    int     offlineMin;

    char    nameBuf[MAX_NAME];
    int     nameLen;
    int     debugMode;

    WINDOW *wHeader;
    WINDOW *wLeft;
    WINDOW *wEnemy;
    WINDOW *wLog;
} GameState;

/* ── hero.c ───────────────────────────────────────────────────────── */

Hero    hero_create(int classId, const char *name);
EStats  hero_effective_stats(const Hero *h);
int     hero_xp_needed(int level);
float   hero_xp_pct(const Hero *h);
int     hero_add_xp(GameState *gs, int amount);
int     hero_equip(Hero *h, const ItemDef *item);
void    hero_unequip(Hero *h, int slot);
void    hero_heal(Hero *h, int amount);
void    hero_restore_resource(Hero *h, int amount);

int     hero_can_invest_talent(const Hero *h, int tree, int node);
int     hero_invest_talent(Hero *h, int tree, int node);
int     hero_uninvest_talent(Hero *h, int tree, int node);
void    hero_reset_talents(Hero *h);
int     hero_total_talent_points_spent(const Hero *h);
int     hero_collect_active_skills(const Hero *h, const SkillDef **out, int *cooldowns, int max);

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
const char         *data_affix_name(int id);
const ClassTalentDef *data_class_talents(int classId);
const TalentTreeDef  *data_talent_tree(int classId, int tree);
const TalentNodeDef  *data_talent_node(int classId, int tree, int node);
const SkillDef       *data_talent_skill(int classId, int tree, int node);
const char        **data_class_art(int classId);

void    check_achievements(GameState *gs);

#endif /* GAME_H */
