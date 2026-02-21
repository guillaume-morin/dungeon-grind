/*
 * data.c — Class definitions and shared lookup helpers.
 *
 * Game data is split across four files:
 *   data.c         — classes, stat/rarity/slot name lookups
 *   data_skills.c  — skill trees (6 tiers x 2 options x 4 classes)
 *   data_enemies.c — enemy templates, dungeon definitions
 *   data_items.c   — item tables, procedural generation, shop logic
 */
#include "game.h"

static const ClassDef CLASSES[NUM_CLASSES] = {
    [CLASS_WARRIOR] = {
        .name = "Warrior", .symbol = "W", .resourceName = "Rage",
        .description = "Unbreakable in armor, devastating in battle.",
        .primaryStat = STR,
        .baseStats       = { 12, 5, 3, 3, 10, 8, 5 },
        .growthPerLevel  = {  3, 1, 0, 0,  2, 1, 1 },
        .baseHp = 120, .hpPerLevel = 14,
        .baseResource = 0, .maxResource = 100, .resourceRegen = 10
    },
    [CLASS_ROGUE] = {
        .name = "Rogue", .symbol = "R", .resourceName = "Energy",
        .description = "Swift and deadly, striking from the shadows.",
        .primaryStat = AGI,
        .baseStats       = { 5, 12, 3, 3, 7, 4, 10 },
        .growthPerLevel  = {  1,  3, 0, 0,  1, 0,  2 },
        .baseHp = 100, .hpPerLevel = 11,
        .baseResource = 100, .maxResource = 100, .resourceRegen = 18
    },
    [CLASS_MAGE] = {
        .name = "Mage", .symbol = "M", .resourceName = "Mana",
        .description = "Master of arcane forces, fragile but devastating.",
        .primaryStat = INT_,
        .baseStats       = { 3, 4, 14, 5, 5, 2, 6 },
        .growthPerLevel  = {  0, 1,  3, 1,  1, 0, 1 },
        .baseHp = 90, .hpPerLevel = 10,
        .baseResource = 120, .maxResource = 150, .resourceRegen = 12
    },
    [CLASS_PRIEST] = {
        .name = "Priest", .symbol = "P", .resourceName = "Mana",
        .description = "Divine healer who outlasts any foe.",
        .primaryStat = WIS,
        .baseStats       = { 3, 3, 6, 14, 8, 5, 4 },
        .growthPerLevel  = {  0, 0, 1,  3,  2, 1, 0 },
        .baseHp = 100, .hpPerLevel = 11,
        .baseResource = 120, .maxResource = 150, .resourceRegen = 10
    }
};

static const char *STAT_NAMES[]  = { "Strength","Agility","Intellect","Wisdom","Vitality","Defense","Speed" };
static const char *STAT_SHORT[]  = { "STR","AGI","INT","WIS","VIT","DEF","SPD" };
static const char *RARITY_NAMES[]= { "Common","Uncommon","Rare","Epic","Legendary" };
static const int   RARITY_COLS[] = { CP_WHITE, CP_GREEN, CP_BLUE, CP_MAGENTA, CP_YELLOW };
static const char *SLOT_NAMES[]  = { "Weapon","Helmet","Chest","Legs","Boots","Ring","Amulet" };

const ClassDef     *data_class(int id)   { return (id>=0 && id<NUM_CLASSES)  ? &CLASSES[id]  : NULL; }

const char *data_stat_name(int s)    { return (s>=0 && s<NUM_STATS)    ? STAT_NAMES[s]   : "?"; }
const char *data_stat_short(int s)   { return (s>=0 && s<NUM_STATS)    ? STAT_SHORT[s]   : "?"; }
const char *data_rarity_name(int r)  { return (r>=0 && r<NUM_RARITIES) ? RARITY_NAMES[r] : "?"; }
int         data_rarity_color(int r) { return (r>=0 && r<NUM_RARITIES) ? RARITY_COLS[r]  : CP_WHITE; }
const char *data_slot_name(int s)    { return (s>=0 && s<NUM_SLOTS)    ? SLOT_NAMES[s]   : "?"; }
