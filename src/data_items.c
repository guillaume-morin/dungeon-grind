/*
 * data_items.c — Item tables, procedural generation, and shop logic.
 *
 * Common/Uncommon/Rare items are procedurally generated at runtime using
 * WoW-style "{Material} {Base} of the {Animal}" naming. Epic items come
 * in three polarizing variants per slot/class/tier; Legendaries are class-
 * specific with primary+secondary stat distribution.
 *
 * CM_ALL (0) means "any class can equip". Non-zero classMask is a
 * bitmask of (1 << classId). Legendary jewelry is class-specific;
 * Epic jewelry uses CM_ALL.
 *
 * Rarity budget chain (each tier ~1.35x previous):
 *   Common(50) -> Uncommon(75) -> Rare(105) -> Epic(142) -> Legendary(192)
 * Epic tiers: Lv46, Lv55, Lv66, Lv78, Lv90 (Storm). Legendary: Lv100.
 * Mob drop rates are very low (.0012-.0032); bosses always drop with
 * weighted rarity (see combat.c).
 */
#include "game.h"
#include <stdlib.h>
#include <string.h>

#define CM_WAR  (1 << CLASS_WARRIOR)
#define CM_ROG  (1 << CLASS_ROGUE)
#define CM_MAG  (1 << CLASS_MAGE)
#define CM_PRI  (1 << CLASS_PRIEST)
#define CM_ALL  0

/* ── procedural generation tables (Common/Uncommon/Rare) ─────────── */

/*
 * WoW-style "{Material} {Base} of the {Animal}" naming.
 * Each suffix maps two stats with weights, filtered by classMask.
 * Universal suffixes (classMask 0) available to all classes and jewelry.
 */
typedef struct { const char *name; int stat1, stat2, w1, w2, classMask; } Suffix;
#define NUM_SUFFIXES 17
static const Suffix SUFFIXES[NUM_SUFFIXES] = {
    { "of the Bear",    STR, VIT, 60, 40, CM_WAR|CM_ROG },
    { "of the Tiger",   STR, AGI, 50, 50, CM_WAR|CM_ROG },
    { "of the Gorilla", STR, DEF, 50, 50, CM_WAR },
    { "of the Boar",    STR, SPD, 50, 50, CM_WAR|CM_ROG },
    { "of the Monkey",  AGI, VIT, 60, 40, CM_ROG|CM_WAR },
    { "of the Falcon",  AGI, SPD, 50, 50, CM_ROG },
    { "of the Wolf",    AGI, DEF, 50, 50, CM_ROG },
    { "of the Owl",     INT_, WIS, 50, 50, CM_MAG|CM_PRI },
    { "of the Eagle",   INT_, VIT, 60, 40, CM_MAG|CM_PRI },
    { "of the Hawk",    INT_, SPD, 50, 50, CM_MAG },
    { "of the Basilisk",INT_, DEF, 50, 50, CM_MAG },
    { "of the Whale",   WIS, VIT, 50, 50, CM_PRI|CM_MAG },
    { "of the Crane",   WIS, SPD, 50, 50, CM_PRI },
    { "of the Turtle",  WIS, DEF, 50, 50, CM_PRI },
    { "of the Stag",    VIT, SPD, 50, 50, 0 },
    { "of the Crab",    VIT, DEF, 50, 50, 0 },
    { "of the Serpent",  DEF, SPD, 50, 50, 0 },
};

static const char *MATERIALS[3][3] = {
    { "Iron", "Worn", "Crude" },
    { "Steel", "Fine", "Solid" },
    { "Dark", "Shadow", "Arcane" }
};

/* BASE_NAMES[slot][classId][variant] — jewelry rows are class-independent */
static const char *BASE_NAMES[NUM_SLOTS][NUM_CLASSES][3] = {
    [SLOT_WEAPON]  = { {"Sword","Blade","Axe"}, {"Dagger","Shiv","Knife"}, {"Staff","Wand","Rod"}, {"Mace","Hammer","Scepter"} },
    [SLOT_HELMET]  = { {"Helm","Visor","Crown"}, {"Hood","Mask","Cowl"}, {"Circlet","Cap","Tiara"}, {"Coif","Mitre","Halo"} },
    [SLOT_CHEST]   = { {"Plate","Mail","Armor"}, {"Vest","Tunic","Jerkin"}, {"Robe","Mantle","Garb"}, {"Raiment","Surcoat","Cassock"} },
    [SLOT_LEGS]    = { {"Greaves","Tassets","Guards"}, {"Pants","Leggings","Chaps"}, {"Trousers","Kilt","Skirt"}, {"Cuisses","Robes","Breeches"} },
    [SLOT_BOOTS]   = { {"Boots","Treads","Stompers"}, {"Boots","Shoes","Walkers"}, {"Shoes","Slippers","Steps"}, {"Sandals","Steps","Treads"} },
    [SLOT_RING]    = { {"Ring","Band","Loop"}, {"Ring","Band","Loop"}, {"Ring","Band","Loop"}, {"Ring","Band","Loop"} },
    [SLOT_AMULET]  = { {"Charm","Pendant","Locket"}, {"Charm","Pendant","Locket"}, {"Charm","Pendant","Locket"}, {"Charm","Pendant","Locket"} }
};

/* Class-optimal suffixes for deterministic shop items: Bear(W), Monkey(R), Owl(M), Whale(P) */
static const int SHOP_SUFFIX[NUM_CLASSES] = { 0, 4, 7, 11 };

/*
 * Epic items come in three polarizing variants per slot/class/tier:
 *   A (Might):    ~60% primary, ~20% secondary offense — pure damage
 *   B (Guardian): ~40% primary, ~25% VIT, ~25% DEF    — tanky
 *   C (Swift):    ~45% primary, ~30% SPD, budget 5-10% lower (SPD is OP in idle)
 * Legendary: single item per slot/class at Lv100, class-specific jewelry.
 */
/*                                      slot          rar             lvl   price  STR AGI INT WIS VIT DEF SPD  class */
static const ItemDef ITEMS[] = {
    /* ── Weapons Lv46 ── */
    { "Ashbringer",       SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {50, 15,  0,  0, 12, 10,  0}, CM_WAR },
    { "Boneguard Cleaver",SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {35,  9,  0,  0, 22, 21,  0}, CM_WAR },
    { "Galeborn Sword",   SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {36, 12,  0,  0,  8,  0, 25}, CM_WAR },
    { "Death's Whisper",  SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {12, 50,  0,  0,  0,  0, 19}, CM_ROG },
    { "Deathward Dagger", SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {9, 32,  0,  0, 20, 20,  0}, CM_ROG },
    { "Windreap Shiv",    SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {11, 34,  0,  0,  8,  0, 22}, CM_ROG },
    { "Staff of Eternity",SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {0,  0, 50, 19,  0,  0, 10}, CM_MAG },
    { "Eternal Bastion",  SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {0,  0, 31,  9, 20, 19,  0}, CM_MAG },
    { "Galestaff Eternal",SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {0,  0, 34, 11,  6,  0, 22}, CM_MAG },
    { "Benediction",      SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {0,  0, 19, 50, 12,  0,  0}, CM_PRI },
    { "Aegis of Faith",   SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {0,  0,  9, 32, 20, 20,  0}, CM_PRI },
    { "Tempest Scepter",  SLOT_WEAPON,  RARITY_EPIC,       46, 18750, {0,  0, 11, 34,  6,  0, 24}, CM_PRI },
    /* ── Weapons Lv55 ── */
    { "Molten Greatsword",SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {75,  0,  0,  0, 24, 12,  0}, CM_WAR },
    { "Flameshield Blade",SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {45, 10,  0,  0, 28, 29,  0}, CM_WAR },
    { "Blazestorm Sword", SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {46, 15,  0,  0, 10,  0, 31}, CM_WAR },
    { "Magma Fang",       SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {12, 75,  0,  0,  0,  0, 24}, CM_ROG },
    { "Pyreguard Fang",   SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {10, 45,  0,  0, 28, 29,  0}, CM_ROG },
    { "Cinderwind Shiv",  SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {15, 46,  0,  0, 10,  0, 31}, CM_ROG },
    { "Flameheart Staff", SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {0,  0, 75, 24,  0,  0, 12}, CM_MAG },
    { "Fireward Staff",   SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {0,  0, 45, 10, 28, 29,  0}, CM_MAG },
    { "Stormfire Wand",   SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {0,  0, 46, 15, 10,  0, 31}, CM_MAG },
    { "Ember Scepter",    SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {0,  0, 24, 75, 12,  0,  0}, CM_PRI },
    { "Embershield Mace", SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {0,  0, 10, 45, 28, 29,  0}, CM_PRI },
    { "Pyrestorm Scepter",SLOT_WEAPON,  RARITY_EPIC,       55, 30000, {0,  0, 15, 46, 10,  0, 31}, CM_PRI },
    /* ── Weapons Lv66 ── */
    { "Frostbite Cleaver",SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {81,  0,  0,  0, 25, 15,  0}, CM_WAR },
    { "Frostwall Blade",  SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {49, 12,  0,  0, 30, 30,  0}, CM_WAR },
    { "Frostwind Reaver", SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {51, 18,  0,  0, 10,  0, 34}, CM_WAR },
    { "Icicle Shiv",      SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {12, 81,  0,  0,  0,  0, 25}, CM_ROG },
    { "Iceward Stiletto", SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {12, 48,  0,  0, 30, 29,  0}, CM_ROG },
    { "Icewind Dagger",   SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {16, 50,  0,  0, 11,  0, 32}, CM_ROG },
    { "Frozen Scepter",   SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {0,  0, 81, 25,  0,  0, 12}, CM_MAG },
    { "Frostwall Staff",  SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {0,  0, 48, 12, 30, 29,  0}, CM_MAG },
    { "Frostgale Rod",    SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {0,  0, 50, 16, 11,  0, 32}, CM_MAG },
    { "Glacial Wand",     SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {0,  0, 25, 81, 15,  0,  0}, CM_PRI },
    { "Glacial Bulwark",  SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {0,  0, 12, 49, 30, 30,  0}, CM_PRI },
    { "Glacial Zephyr",   SLOT_WEAPON,  RARITY_EPIC,       66, 56250, {0,  0, 18, 51, 10,  0, 34}, CM_PRI },
    /* ── Weapons Lv78 ── */
    { "Void Reaver",      SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {106,  0,  0,  0, 35, 20,  0}, CM_WAR },
    { "Voidwall Cleaver", SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {65, 16,  0,  0, 40, 40,  0}, CM_WAR },
    { "Voidstorm Blade",  SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {68, 22,  0,  0, 15,  0, 45}, CM_WAR },
    { "Null Blade",       SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {18,106,  0,  0,  0,  0, 35}, CM_ROG },
    { "Nullguard Dagger", SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {16, 64,  0,  0, 40, 39,  0}, CM_ROG },
    { "Nullwind Shiv",    SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {22, 66,  0,  0, 14,  0, 45}, CM_ROG },
    { "Void Orb",         SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {0,  0,106, 35,  0,  0, 18}, CM_MAG },
    { "Voidward Scepter", SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {0,  0, 64, 16, 40, 39,  0}, CM_MAG },
    { "Voidstorm Orb",    SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {0,  0, 66, 22, 14,  0, 45}, CM_MAG },
    { "Void Censer",      SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {0,  0, 35,106, 20,  0,  0}, CM_PRI },
    { "Rift Aegis Mace",  SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {0,  0, 16, 65, 40, 40,  0}, CM_PRI },
    { "Rift Gale Scepter",SLOT_WEAPON,  RARITY_EPIC,       78, 105000, {0,  0, 22, 68, 15,  0, 45}, CM_PRI },

    /* ── Helmets Lv46 ── */
    { "Crown of Ruin",    SLOT_HELMET,  RARITY_EPIC,       46, 13120, {10,  0,  0,  0, 28, 20,  0}, CM_WAR },
    { "Boneguard Helm",   SLOT_HELMET,  RARITY_EPIC,       46, 13120, {22,  6,  0,  0, 15, 14,  0}, CM_WAR },
    { "Galeforged Visor", SLOT_HELMET,  RARITY_EPIC,       46, 13120, {24,  8,  0,  0,  6,  0, 16}, CM_WAR },
    { "Phantom Crown",    SLOT_HELMET,  RARITY_EPIC,       46, 13120, {0, 20,  0,  0,  8,  0, 18}, CM_ROG },
    { "Phantom Bastion",  SLOT_HELMET,  RARITY_EPIC,       46, 13120, {5, 18,  0,  0, 11, 11,  0}, CM_ROG },
    { "Phantom Gale Hood",SLOT_HELMET,  RARITY_EPIC,       46, 13120, {6, 19,  0,  0,  4,  0, 12}, CM_ROG },
    { "Crown of Stars",   SLOT_HELMET,  RARITY_EPIC,       46, 13120, {0,  0, 28, 12,  6,  0,  0}, CM_MAG },
    { "Starwarden Crown", SLOT_HELMET,  RARITY_EPIC,       46, 13120, {0,  0, 19,  5, 11, 11,  0}, CM_MAG },
    { "Stargale Circlet", SLOT_HELMET,  RARITY_EPIC,       46, 13120, {0,  0, 19,  6,  5,  0, 12}, CM_MAG },
    { "Divine Crown",     SLOT_HELMET,  RARITY_EPIC,       46, 13120, {0,  0,  0, 22, 15, 10,  0}, CM_PRI },
    { "Hallowed Bulwark", SLOT_HELMET,  RARITY_EPIC,       46, 13120, {0,  0,  5, 19, 12, 11,  0}, CM_PRI },
    { "Divinewind Mitre", SLOT_HELMET,  RARITY_EPIC,       46, 13120, {0,  0,  6, 20,  4,  0, 14}, CM_PRI },
    /* ── Helmets Lv55 ── */
    { "Molten Warhelm",   SLOT_HELMET,  RARITY_EPIC,       55, 20620, {8,  0,  0,  0, 28, 30,  0}, CM_WAR },
    { "Magma Sentinel",   SLOT_HELMET,  RARITY_EPIC,       55, 20620, {25,  6,  0,  0, 16, 16,  0}, CM_WAR },
    { "Blazewind Visor",  SLOT_HELMET,  RARITY_EPIC,       55, 20620, {28,  9,  0,  0,  6,  0, 18}, CM_WAR },
    { "Magma Visor",      SLOT_HELMET,  RARITY_EPIC,       55, 20620, {0, 24,  0,  0, 10,  0, 20}, CM_ROG },
    { "Magma Aegis Hood", SLOT_HELMET,  RARITY_EPIC,       55, 20620, {6, 21,  0,  0, 14, 14,  0}, CM_ROG },
    { "Magma Gale Mask",  SLOT_HELMET,  RARITY_EPIC,       55, 20620, {8, 22,  0,  0,  5,  0, 15}, CM_ROG },
    { "Flamecrown",       SLOT_HELMET,  RARITY_EPIC,       55, 20620, {0,  0, 30, 15,  6,  0,  0}, CM_MAG },
    { "Flameguard Crown", SLOT_HELMET,  RARITY_EPIC,       55, 20620, {0,  0, 21,  5, 14, 12,  0}, CM_MAG },
    { "Flamegale Circlet",SLOT_HELMET,  RARITY_EPIC,       55, 20620, {0,  0, 21,  8,  5,  0, 15}, CM_MAG },
    { "Ember Mitre",      SLOT_HELMET,  RARITY_EPIC,       55, 20620, {0,  0,  0, 28, 18, 14,  0}, CM_PRI },
    { "Ember Rampart Coif",SLOT_HELMET, RARITY_EPIC,       55, 20620, {0,  0,  6, 24, 15, 14,  0}, CM_PRI },
    { "Emberwind Halo",   SLOT_HELMET,  RARITY_EPIC,       55, 20620, {0,  0,  8, 24,  6,  0, 16}, CM_PRI },
    /* ── Helmets Lv66 ── */
    { "Frostguard Helm",  SLOT_HELMET,  RARITY_EPIC,       66, 37500, {10,  0,  0,  0, 30, 35,  0}, CM_WAR },
    { "Frostwall Helm",   SLOT_HELMET,  RARITY_EPIC,       66, 37500, {30,  8,  0,  0, 19, 19,  0}, CM_WAR },
    { "Frostwind Visor",  SLOT_HELMET,  RARITY_EPIC,       66, 37500, {31, 10,  0,  0,  8,  0, 21}, CM_WAR },
    { "Icestalker Hood",  SLOT_HELMET,  RARITY_EPIC,       66, 37500, {0, 25,  0,  0, 11,  0, 21}, CM_ROG },
    { "Iceward Hood",     SLOT_HELMET,  RARITY_EPIC,       66, 37500, {6, 22,  0,  0, 15, 14,  0}, CM_ROG },
    { "Icewind Cowl",     SLOT_HELMET,  RARITY_EPIC,       66, 37500, {8, 24,  0,  0,  6,  0, 16}, CM_ROG },
    { "Frostweave Crown", SLOT_HELMET,  RARITY_EPIC,       66, 37500, {0,  0, 35, 18,  6,  0,  0}, CM_MAG },
    { "Frostward Diadem", SLOT_HELMET,  RARITY_EPIC,       66, 37500, {0,  0, 24,  6, 15, 14,  0}, CM_MAG },
    { "Frostgale Tiara",  SLOT_HELMET,  RARITY_EPIC,       66, 37500, {0,  0, 25,  9,  5,  0, 16}, CM_MAG },
    { "Glacial Halo",     SLOT_HELMET,  RARITY_EPIC,       66, 37500, {0,  0,  0, 30, 20, 15,  0}, CM_PRI },
    { "Glacial Aegis Coif",SLOT_HELMET, RARITY_EPIC,       66, 37500, {0,  0,  6, 26, 16, 16,  0}, CM_PRI },
    { "Glacial Wind Halo",SLOT_HELMET,  RARITY_EPIC,       66, 37500, {0,  0,  9, 28,  6,  0, 18}, CM_PRI },
    /* ── Helmets Lv78 ── */
    { "Voidforged Visor", SLOT_HELMET,  RARITY_EPIC,       78, 67500, {12,  0,  0,  0, 38, 45,  0}, CM_WAR },
    { "Voidwall Helm",    SLOT_HELMET,  RARITY_EPIC,       78, 67500, {38, 10,  0,  0, 24, 24,  0}, CM_WAR },
    { "Voidstorm Visor",  SLOT_HELMET,  RARITY_EPIC,       78, 67500, {40, 14,  0,  0,  9,  0, 26}, CM_WAR },
    { "Null Mask",        SLOT_HELMET,  RARITY_EPIC,       78, 67500, {0, 35,  0,  0, 15,  0, 28}, CM_ROG },
    { "Nullguard Hood",   SLOT_HELMET,  RARITY_EPIC,       78, 67500, {8, 31,  0,  0, 20, 19,  0}, CM_ROG },
    { "Nullwind Cowl",    SLOT_HELMET,  RARITY_EPIC,       78, 67500, {11, 32,  0,  0,  8,  0, 21}, CM_ROG },
    { "Rift Diadem",      SLOT_HELMET,  RARITY_EPIC,       78, 67500, {0,  0, 45, 22,  9,  0,  0}, CM_MAG },
    { "Rift Aegis Crown", SLOT_HELMET,  RARITY_EPIC,       78, 67500, {0,  0, 30,  8, 19, 20,  0}, CM_MAG },
    { "Rift Gale Circlet",SLOT_HELMET,  RARITY_EPIC,       78, 67500, {0,  0, 32, 11,  6,  0, 21}, CM_MAG },
    { "Void Circlet",     SLOT_HELMET,  RARITY_EPIC,       78, 67500, {0,  0,  0, 38, 25, 20,  0}, CM_PRI },
    { "Voidward Mitre",   SLOT_HELMET,  RARITY_EPIC,       78, 67500, {0,  0,  9, 32, 21, 20,  0}, CM_PRI },
    { "Voidwind Halo",    SLOT_HELMET,  RARITY_EPIC,       78, 67500, {0,  0, 11, 34,  9,  0, 22}, CM_PRI },

    /* ── Chests Lv46 ── */
    { "Aegis of Valor",   SLOT_CHEST,   RARITY_EPIC,       46, 15000, {12,  0,  0,  0, 28, 35,  0}, CM_WAR },
    { "Boneguard Plate",  SLOT_CHEST,   RARITY_EPIC,       46, 15000, {30,  8,  0,  0, 19, 19,  0}, CM_WAR },
    { "Galeborn Mail",    SLOT_CHEST,   RARITY_EPIC,       46, 15000, {31, 10,  0,  0,  8,  0, 21}, CM_WAR },
    { "Phantom Shroud",   SLOT_CHEST,   RARITY_EPIC,       46, 15000, {0, 25,  0,  0, 10,  0, 20}, CM_ROG },
    { "Deathward Vest",   SLOT_CHEST,   RARITY_EPIC,       46, 15000, {5, 22,  0,  0, 14, 14,  0}, CM_ROG },
    { "Windreap Tunic",   SLOT_CHEST,   RARITY_EPIC,       46, 15000, {8, 22,  0,  0,  6,  0, 15}, CM_ROG },
    { "Robe of Eternity", SLOT_CHEST,   RARITY_EPIC,       46, 15000, {0,  0, 35, 18,  6,  0,  0}, CM_MAG },
    { "Eternal Ward Robe",SLOT_CHEST,   RARITY_EPIC,       46, 15000, {0,  0, 24,  6, 15, 14,  0}, CM_MAG },
    { "Galeweave Mantle", SLOT_CHEST,   RARITY_EPIC,       46, 15000, {0,  0, 25,  9,  5,  0, 16}, CM_MAG },
    { "Divine Regalia",   SLOT_CHEST,   RARITY_EPIC,       46, 15000, {0,  0,  0, 28, 18, 12,  0}, CM_PRI },
    { "Hallowed Raiment", SLOT_CHEST,   RARITY_EPIC,       46, 15000, {0,  0,  6, 22, 15, 14,  0}, CM_PRI },
    { "Tempest Surcoat",  SLOT_CHEST,   RARITY_EPIC,       46, 15000, {0,  0,  8, 24,  6,  0, 16}, CM_PRI },
    /* ── Chests Lv55 ── */
    { "Molten Cuirass",   SLOT_CHEST,   RARITY_EPIC,       55, 24380, {12,  0,  0,  0, 42, 48,  0}, CM_WAR },
    { "Flameguard Plate", SLOT_CHEST,   RARITY_EPIC,       55, 24380, {40, 10,  0,  0, 25, 25,  0}, CM_WAR },
    { "Blazewind Mail",   SLOT_CHEST,   RARITY_EPIC,       55, 24380, {42, 14,  0,  0, 10,  0, 29}, CM_WAR },
    { "Magma Tunic",      SLOT_CHEST,   RARITY_EPIC,       55, 24380, {0, 36,  0,  0, 15,  0, 30}, CM_ROG },
    { "Pyreguard Jerkin", SLOT_CHEST,   RARITY_EPIC,       55, 24380, {8, 32,  0,  0, 21, 20,  0}, CM_ROG },
    { "Cinderwind Vest",  SLOT_CHEST,   RARITY_EPIC,       55, 24380, {12, 35,  0,  0,  6,  0, 22}, CM_ROG },
    { "Flamewoven Robe",  SLOT_CHEST,   RARITY_EPIC,       55, 24380, {0,  0, 48, 24,  9,  0,  0}, CM_MAG },
    { "Fireward Mantle",  SLOT_CHEST,   RARITY_EPIC,       55, 24380, {0,  0, 32,  8, 21, 20,  0}, CM_MAG },
    { "Stormfire Robe",   SLOT_CHEST,   RARITY_EPIC,       55, 24380, {0,  0, 35, 12,  6,  0, 22}, CM_MAG },
    { "Ember Vestment",   SLOT_CHEST,   RARITY_EPIC,       55, 24380, {0,  0,  0, 42, 28, 21,  0}, CM_PRI },
    { "Emberward Garb",   SLOT_CHEST,   RARITY_EPIC,       55, 24380, {0,  0,  9, 36, 22, 22,  0}, CM_PRI },
    { "Emberstorm Cassock",SLOT_CHEST,  RARITY_EPIC,       55, 24380, {0,  0, 12, 38,  9,  0, 25}, CM_PRI },
    /* ── Chests Lv66 ── */
    { "Frostplate Armor", SLOT_CHEST,   RARITY_EPIC,       66, 45000, {15,  0,  0,  0, 45, 52,  0}, CM_WAR },
    { "Frostwall Plate",  SLOT_CHEST,   RARITY_EPIC,       66, 45000, {45, 11,  0,  0, 28, 29,  0}, CM_WAR },
    { "Frostwind Armor",  SLOT_CHEST,   RARITY_EPIC,       66, 45000, {48, 16,  0,  0, 10,  0, 31}, CM_WAR },
    { "Iceweave Jerkin",  SLOT_CHEST,   RARITY_EPIC,       66, 45000, {0, 40,  0,  0, 18,  0, 32}, CM_ROG },
    { "Iceward Jerkin",   SLOT_CHEST,   RARITY_EPIC,       66, 45000, {9, 36,  0,  0, 22, 22,  0}, CM_ROG },
    { "Icewind Tunic",    SLOT_CHEST,   RARITY_EPIC,       66, 45000, {12, 38,  0,  0,  9,  0, 25}, CM_ROG },
    { "Frozen Robe",      SLOT_CHEST,   RARITY_EPIC,       66, 45000, {0,  0, 52, 28, 10,  0,  0}, CM_MAG },
    { "Frostward Robe",   SLOT_CHEST,   RARITY_EPIC,       66, 45000, {0,  0, 36,  9, 22, 22,  0}, CM_MAG },
    { "Frostgale Mantle", SLOT_CHEST,   RARITY_EPIC,       66, 45000, {0,  0, 38, 12,  9,  0, 25}, CM_MAG },
    { "Glacial Raiment",  SLOT_CHEST,   RARITY_EPIC,       66, 45000, {0,  0,  0, 45, 30, 22,  0}, CM_PRI },
    { "Glacial Aegis Garb",SLOT_CHEST,  RARITY_EPIC,       66, 45000, {0,  0, 10, 39, 25, 24,  0}, CM_PRI },
    { "Glacial Wind Robe",SLOT_CHEST,   RARITY_EPIC,       66, 45000, {0,  0, 14, 41,  9,  0, 28}, CM_PRI },
    /* ── Chests Lv78 ── */
    { "Voidplate",        SLOT_CHEST,   RARITY_EPIC,       78, 82500, {20,  0,  0,  0, 58, 69,  0}, CM_WAR },
    { "Voidwall Plate",   SLOT_CHEST,   RARITY_EPIC,       78, 82500, {59, 15,  0,  0, 36, 36,  0}, CM_WAR },
    { "Voidstorm Mail",   SLOT_CHEST,   RARITY_EPIC,       78, 82500, {61, 20,  0,  0, 14,  0, 41}, CM_WAR },
    { "Null Shroud",      SLOT_CHEST,   RARITY_EPIC,       78, 82500, {0, 52,  0,  0, 22,  0, 42}, CM_ROG },
    { "Nullguard Vest",   SLOT_CHEST,   RARITY_EPIC,       78, 82500, {11, 48,  0,  0, 30, 29,  0}, CM_ROG },
    { "Nullwind Jerkin",  SLOT_CHEST,   RARITY_EPIC,       78, 82500, {16, 49,  0,  0, 11,  0, 32}, CM_ROG },
    { "Rift Robe",        SLOT_CHEST,   RARITY_EPIC,       78, 82500, {0,  0, 69, 35, 12,  0,  0}, CM_MAG },
    { "Rift Aegis Robe",  SLOT_CHEST,   RARITY_EPIC,       78, 82500, {0,  0, 46, 11, 29, 30,  0}, CM_MAG },
    { "Rift Gale Mantle", SLOT_CHEST,   RARITY_EPIC,       78, 82500, {0,  0, 49, 16, 10,  0, 32}, CM_MAG },
    { "Void Regalia",     SLOT_CHEST,   RARITY_EPIC,       78, 82500, {0,  0,  0, 58, 38, 30,  0}, CM_PRI },
    { "Voidward Raiment", SLOT_CHEST,   RARITY_EPIC,       78, 82500, {0,  0, 12, 50, 31, 31,  0}, CM_PRI },
    { "Voidwind Cassock", SLOT_CHEST,   RARITY_EPIC,       78, 82500, {0,  0, 18, 52, 11,  0, 35}, CM_PRI },

    /* ── Legs Lv46 ── */
    { "Bonecage Greaves", SLOT_LEGS,    RARITY_EPIC,       46,  12000, {10,  0,  0,  0, 22, 30,  0}, CM_WAR },
    { "Boneguard Tassets", SLOT_LEGS,   RARITY_EPIC,       46,  12000, {25,  6,  0,  0, 15, 16,  0}, CM_WAR },
    { "Galeborn Greaves", SLOT_LEGS,    RARITY_EPIC,       46,  12000, {26,  9,  0,  0,  6,  0, 18}, CM_WAR },
    { "Phantom Legwraps", SLOT_LEGS,    RARITY_EPIC,       46,  12000, {0, 20,  0,  0,  5,  0, 18}, CM_ROG },
    { "Deathward Pants",  SLOT_LEGS,    RARITY_EPIC,       46,  12000, {4, 18,  0,  0, 10, 11,  0}, CM_ROG },
    { "Windreap Leggings",SLOT_LEGS,    RARITY_EPIC,       46,  12000, {6, 18,  0,  0,  4,  0, 12}, CM_ROG },
    { "Leggings of Stars",SLOT_LEGS,    RARITY_EPIC,       46,  12000, {0,  0, 25, 12,  0,  0,  6}, CM_MAG },
    { "Starwarden Kilt",  SLOT_LEGS,    RARITY_EPIC,       46,  12000, {0,  0, 18,  5, 11, 10,  0}, CM_MAG },
    { "Stargale Trousers",SLOT_LEGS,    RARITY_EPIC,       46,  12000, {0,  0, 19,  6,  4,  0, 12}, CM_MAG },
    { "Divine Greaves",   SLOT_LEGS,    RARITY_EPIC,       46,  12000, {0,  0,  0, 20, 15, 10,  0}, CM_PRI },
    { "Hallowed Cuisses", SLOT_LEGS,    RARITY_EPIC,       46,  12000, {0,  0,  5, 18, 11, 11,  0}, CM_PRI },
    { "Divinewind Robes", SLOT_LEGS,    RARITY_EPIC,       46,  12000, {0,  0,  6, 19,  4,  0, 12}, CM_PRI },
    /* ── Legs Lv55 ── */
    { "Molten Legguards", SLOT_LEGS,    RARITY_EPIC,       55, 18750, {9,  0,  0,  0, 32, 39,  0}, CM_WAR },
    { "Flameguard Greaves",SLOT_LEGS,   RARITY_EPIC,       55, 18750, {32,  8,  0,  0, 21, 20,  0}, CM_WAR },
    { "Blazewind Greaves",SLOT_LEGS,    RARITY_EPIC,       55, 18750, {35, 12,  0,  0,  6,  0, 22}, CM_WAR },
    { "Magma Leggings",   SLOT_LEGS,    RARITY_EPIC,       55, 18750, {0, 30,  0,  0, 12,  0, 24}, CM_ROG },
    { "Pyreguard Pants",  SLOT_LEGS,    RARITY_EPIC,       55, 18750, {6, 28,  0,  0, 16, 16,  0}, CM_ROG },
    { "Cinderwind Pants", SLOT_LEGS,    RARITY_EPIC,       55, 18750, {9, 28,  0,  0,  8,  0, 18}, CM_ROG },
    { "Flamewoven Pants", SLOT_LEGS,    RARITY_EPIC,       55, 18750, {0,  0, 39, 18,  8,  0,  0}, CM_MAG },
    { "Fireward Trousers",SLOT_LEGS,    RARITY_EPIC,       55, 18750, {0,  0, 25,  6, 16, 16,  0}, CM_MAG },
    { "Stormfire Kilt",   SLOT_LEGS,    RARITY_EPIC,       55, 18750, {0,  0, 28,  9,  6,  0, 18}, CM_MAG },
    { "Ember Greaves",    SLOT_LEGS,    RARITY_EPIC,       55, 18750, {0,  0,  0, 32, 21, 16,  0}, CM_PRI },
    { "Emberward Cuisses",SLOT_LEGS,    RARITY_EPIC,       55, 18750, {0,  0,  8, 29, 18, 16,  0}, CM_PRI },
    { "Emberstorm Robes", SLOT_LEGS,    RARITY_EPIC,       55, 18750, {0,  0, 10, 30,  6,  0, 20}, CM_PRI },
    /* ── Legs Lv66 ── */
    { "Frostguard Greaves",SLOT_LEGS,   RARITY_EPIC,       66, 33750, {12,  0,  0,  0, 35, 42,  0}, CM_WAR },
    { "Frostwall Greaves",SLOT_LEGS,    RARITY_EPIC,       66, 33750, {36,  9,  0,  0, 22, 22,  0}, CM_WAR },
    { "Frostwind Guards", SLOT_LEGS,    RARITY_EPIC,       66, 33750, {38, 12,  0,  0,  9,  0, 25}, CM_WAR },
    { "Icestalker Pants", SLOT_LEGS,    RARITY_EPIC,       66, 33750, {0, 32,  0,  0, 15,  0, 25}, CM_ROG },
    { "Iceward Leggings", SLOT_LEGS,    RARITY_EPIC,       66, 33750, {8, 29,  0,  0, 19, 18,  0}, CM_ROG },
    { "Icewind Chaps",    SLOT_LEGS,    RARITY_EPIC,       66, 33750, {10, 30,  0,  0,  8,  0, 20}, CM_ROG },
    { "Frostweave Legs",  SLOT_LEGS,    RARITY_EPIC,       66, 33750, {0,  0, 42, 22,  8,  0,  0}, CM_MAG },
    { "Frostward Kilt",   SLOT_LEGS,    RARITY_EPIC,       66, 33750, {0,  0, 29,  8, 19, 18,  0}, CM_MAG },
    { "Frostgale Skirt",  SLOT_LEGS,    RARITY_EPIC,       66, 33750, {0,  0, 30, 10,  8,  0, 20}, CM_MAG },
    { "Glacial Cuisses",  SLOT_LEGS,    RARITY_EPIC,       66, 33750, {0,  0,  0, 35, 25, 18,  0}, CM_PRI },
    { "Glacial Ward Legs",SLOT_LEGS,    RARITY_EPIC,       66, 33750, {0,  0,  8, 31, 20, 19,  0}, CM_PRI },
    { "Glacial Wind Robes",SLOT_LEGS,   RARITY_EPIC,       66, 33750, {0,  0, 11, 32,  8,  0, 21}, CM_PRI },
    /* ── Legs Lv78 ── */
    { "Voidforged Greaves",SLOT_LEGS,   RARITY_EPIC,       78, 60000, {15,  0,  0,  0, 45, 55,  0}, CM_WAR },
    { "Voidwall Tassets", SLOT_LEGS,    RARITY_EPIC,       78, 60000, {46, 11,  0,  0, 29, 29,  0}, CM_WAR },
    { "Voidstorm Greaves",SLOT_LEGS,    RARITY_EPIC,       78, 60000, {49, 16,  0,  0, 10,  0, 32}, CM_WAR },
    { "Null Legwraps",    SLOT_LEGS,    RARITY_EPIC,       78, 60000, {0, 42,  0,  0, 18,  0, 35}, CM_ROG },
    { "Nullguard Pants",  SLOT_LEGS,    RARITY_EPIC,       78, 60000, {10, 38,  0,  0, 24, 24,  0}, CM_ROG },
    { "Nullwind Leggings",SLOT_LEGS,    RARITY_EPIC,       78, 60000, {14, 40,  0,  0,  9,  0, 26}, CM_ROG },
    { "Rift Leggings",    SLOT_LEGS,    RARITY_EPIC,       78, 60000, {0,  0, 55, 28, 10,  0,  0}, CM_MAG },
    { "Rift Ward Kilt",   SLOT_LEGS,    RARITY_EPIC,       78, 60000, {0,  0, 38,  9, 22, 24,  0}, CM_MAG },
    { "Rift Gale Skirt",  SLOT_LEGS,    RARITY_EPIC,       78, 60000, {0,  0, 39, 12,  9,  0, 26}, CM_MAG },
    { "Void Greaves",     SLOT_LEGS,    RARITY_EPIC,       78, 60000, {0,  0,  0, 45, 30, 25,  0}, CM_PRI },
    { "Voidward Cuisses", SLOT_LEGS,    RARITY_EPIC,       78, 60000, {0,  0, 10, 40, 25, 25,  0}, CM_PRI },
    { "Voidwind Breeches",SLOT_LEGS,    RARITY_EPIC,       78, 60000, {0,  0, 14, 41, 10,  0, 28}, CM_PRI },

    /* ── Boots Lv46 ── */
    { "Titanstep Boots",  SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {0,  0,  0,  0, 10, 15, 12}, CM_WAR },
    { "Boneguard Treads", SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {15,  4,  0,  0, 10,  9,  0}, CM_WAR },
    { "Galeborn Stompers",SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {16,  5,  0,  0,  4,  0, 10}, CM_WAR },
    { "Phantom Stride",   SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {0, 12,  0,  0,  5,  0, 25}, CM_ROG },
    { "Deathward Boots",  SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {4, 18,  0,  0, 11, 10,  0}, CM_ROG },
    { "Windreap Walkers", SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {6, 18,  0,  0,  4,  0, 12}, CM_ROG },
    { "Steps of Eternity",SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {0,  0, 12,  8,  0,  0, 18}, CM_MAG },
    { "Eternal Ward Shoes",SLOT_BOOTS,  RARITY_EPIC,       46,  11250, {0,  0, 15,  4, 10,  9,  0}, CM_MAG },
    { "Galeweave Steps",  SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {0,  0, 16,  5,  4,  0, 10}, CM_MAG },
    { "Divine Striders",  SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {0,  0,  0, 12, 10,  0, 12}, CM_PRI },
    { "Hallowed Sandals", SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {0,  0,  4, 14,  9,  9,  0}, CM_PRI },
    { "Tempest Steps",    SLOT_BOOTS,   RARITY_EPIC,       46,  11250, {0,  0,  5, 15,  2,  0, 10}, CM_PRI },
    /* ── Boots Lv55 ── */
    { "Molten Sabatons",  SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {0,  0,  0,  0, 15, 21, 18}, CM_WAR },
    { "Flameguard Treads",SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {21,  6,  0,  0, 14, 14,  0}, CM_WAR },
    { "Blazewind Boots",  SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {22,  8,  0,  0,  5,  0, 15}, CM_WAR },
    { "Magma Treads",     SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {0, 18,  0,  0,  9,  0, 28}, CM_ROG },
    { "Pyreguard Boots",  SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {6, 21,  0,  0, 14, 14,  0}, CM_ROG },
    { "Cinderwind Shoes", SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {8, 22,  0,  0,  5,  0, 15}, CM_ROG },
    { "Flamestep Shoes",  SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {0,  0, 18, 12,  0,  0, 21}, CM_MAG },
    { "Fireward Slippers",SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {0,  0, 21,  5, 14, 12,  0}, CM_MAG },
    { "Stormfire Steps",  SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {0,  0, 21,  8,  5,  0, 15}, CM_MAG },
    { "Ember Sandals",    SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {0,  0,  0, 18, 15,  0, 18}, CM_PRI },
    { "Emberward Treads", SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {0,  0,  5, 21, 14, 12,  0}, CM_PRI },
    { "Emberstorm Steps", SLOT_BOOTS,   RARITY_EPIC,       55, 16880, {0,  0,  8, 21,  5,  0, 15}, CM_PRI },
    /* ── Boots Lv66 ── */
    { "Frostguard Boots", SLOT_BOOTS,   RARITY_EPIC,       66, 30000, {0,  0,  0,  0, 18, 22, 20}, CM_WAR },
    { "Frostwall Treads", SLOT_BOOTS,   RARITY_EPIC,       66, 30000, {24,  6,  0,  0, 15, 15,  0}, CM_WAR },
    { "Frostwind Stompers",SLOT_BOOTS,  RARITY_EPIC,       66, 30000, {25,  9,  0,  0,  5,  0, 18}, CM_WAR },
    { "Icestalker Boots", SLOT_BOOTS,   RARITY_EPIC,       66, 30000, {0, 20,  0,  0, 10,  0, 30}, CM_ROG },
    { "Iceward Walkers",  SLOT_BOOTS,   RARITY_EPIC,       66, 30000, {6, 24,  0,  0, 15, 15,  0}, CM_ROG },
    { "Icewind Shoes",    SLOT_BOOTS,   RARITY_EPIC,       66, 30000, {9, 25,  0,  0,  5,  0, 18}, CM_ROG },
    { "Frostweave Shoes", SLOT_BOOTS,   RARITY_EPIC,       66, 30000, {0,  0, 20, 12,  0,  0, 22}, CM_MAG },
    { "Frostward Slippers",SLOT_BOOTS,  RARITY_EPIC,       66, 30000, {0,  0, 22,  5, 14, 14,  0}, CM_MAG },
    { "Frostgale Steps",  SLOT_BOOTS,   RARITY_EPIC,       66, 30000, {0,  0, 22,  8,  6,  0, 15}, CM_MAG },
    { "Glacial Sandals",  SLOT_BOOTS,   RARITY_EPIC,       66, 30000, {0,  0,  0, 20, 15,  0, 20}, CM_PRI },
    { "Glacial Ward Steps",SLOT_BOOTS,  RARITY_EPIC,       66, 30000, {0,  0,  5, 22, 14, 14,  0}, CM_PRI },
    { "Glacial Gale Steps",SLOT_BOOTS,  RARITY_EPIC,       66, 30000, {0,  0,  8, 22,  6,  0, 15}, CM_PRI },
    /* ── Boots Lv78 ── */
    { "Voidforged Boots", SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {0,  0,  0,  0, 22, 30, 25}, CM_WAR },
    { "Voidwall Boots",   SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {31,  8,  0,  0, 20, 19,  0}, CM_WAR },
    { "Voidstorm Treads", SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {32, 11,  0,  0,  8,  0, 21}, CM_WAR },
    { "Null Treads",      SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {0, 25,  0,  0, 12,  0, 40}, CM_ROG },
    { "Nullguard Walkers",SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {8, 31,  0,  0, 20, 19,  0}, CM_ROG },
    { "Nullwind Shoes",   SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {11, 32,  0,  0,  8,  0, 21}, CM_ROG },
    { "Rift Walkers",     SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {0,  0, 25, 18,  0,  0, 30}, CM_MAG },
    { "Rift Ward Shoes",  SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {0,  0, 29,  8, 18, 19,  0}, CM_MAG },
    { "Rift Gale Steps",  SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {0,  0, 30, 10,  8,  0, 20}, CM_MAG },
    { "Void Striders",    SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {0,  0,  0, 25, 20,  0, 25}, CM_PRI },
    { "Voidward Sandals", SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {0,  0,  8, 28, 18, 18,  0}, CM_PRI },
    { "Voidwind Treads",  SLOT_BOOTS,   RARITY_EPIC,       78, 52500, {0,  0, 10, 29,  6,  0, 20}, CM_PRI },

    /* ── Rings (CM_ALL) ── */
    { "Ring of the Lich", SLOT_RING,    RARITY_EPIC,       46, 14250, {0,  0, 19, 19, 10,  0,  0}, CM_ALL },
    { "Lich King's Seal", SLOT_RING,    RARITY_EPIC,       46, 14250, {4,  4,  4,  4, 18, 15,  0}, CM_ALL },
    { "Lich Wind Loop",   SLOT_RING,    RARITY_EPIC,       46, 14250, {5,  5,  5,  5,  6,  0, 18}, CM_ALL },
    { "Molten Band",      SLOT_RING,    RARITY_EPIC,       55, 22500, {18, 18,  0,  0,  0,  0,  9}, CM_ALL },
    { "Molten Seal",      SLOT_RING,    RARITY_EPIC,       55, 22500, {5,  5,  0,  0, 18, 14,  5}, CM_ALL },
    { "Molten Gale Ring", SLOT_RING,    RARITY_EPIC,       55, 22500, {8,  8,  5,  5,  2,  0, 15}, CM_ALL },
    { "Frozen Signet",    SLOT_RING,    RARITY_EPIC,       66, 41250, {0,  0, 20, 20, 10,  0,  0}, CM_ALL },
    { "Frozen Ward Band", SLOT_RING,    RARITY_EPIC,       66, 41250, {4,  4,  4,  4, 20, 15,  0}, CM_ALL },
    { "Frozen Gale Loop", SLOT_RING,    RARITY_EPIC,       66, 41250, {5,  5,  6,  6,  6,  0, 18}, CM_ALL },
    { "Void Ring",        SLOT_RING,    RARITY_EPIC,       78, 75000, {0, 12,  0,  0, 18, 12, 12}, CM_ALL },
    { "Void Aegis Band",  SLOT_RING,    RARITY_EPIC,       78, 75000, {5,  5,  5,  5, 20, 15,  0}, CM_ALL },
    { "Void Gale Ring",   SLOT_RING,    RARITY_EPIC,       78, 75000, {6,  6,  6,  6,  6,  0, 20}, CM_ALL },

    /* ── Amulets (CM_ALL) ── */
    { "Heart of Eternity",SLOT_AMULET,  RARITY_EPIC,       46, 14250, {0,  0,  0, 15, 19, 10,  0}, CM_ALL },
    { "Warden's Locket",  SLOT_AMULET,  RARITY_EPIC,       46, 14250, {4,  4,  4,  4, 15, 14,  0}, CM_ALL },
    { "Windborn Charm",   SLOT_AMULET,  RARITY_EPIC,       46, 14250, {4,  4,  5,  5,  6,  0, 16}, CM_ALL },
    { "Molten Pendant",   SLOT_AMULET,  RARITY_EPIC,       55, 22500, {0,  0,  0, 12, 21,  9,  0}, CM_ALL },
    { "Molten Ward Charm",SLOT_AMULET,  RARITY_EPIC,       55, 22500, {5,  5,  0,  0, 16, 12,  5}, CM_ALL },
    { "Molten Gale Charm",SLOT_AMULET,  RARITY_EPIC,       55, 22500, {6,  6,  5,  5,  2,  0, 15}, CM_ALL },
    { "Frozen Heart",     SLOT_AMULET,  RARITY_EPIC,       66, 41250, {0,  0, 12, 12, 12,  8,  0}, CM_ALL },
    { "Frozen Ward Charm",SLOT_AMULET,  RARITY_EPIC,       66, 41250, {4,  4,  4,  4, 18, 12,  0}, CM_ALL },
    { "Frozen Gale Locket",SLOT_AMULET, RARITY_EPIC,       66, 41250, {5,  5,  5,  5,  6,  0, 15}, CM_ALL },
    { "Void Locket",      SLOT_AMULET,  RARITY_EPIC,       78, 75000, {0,  0, 15, 15, 22, 10,  0}, CM_ALL },
    { "Void Aegis Pendant",SLOT_AMULET, RARITY_EPIC,       78, 75000, {5,  5,  5,  5, 22, 20,  0}, CM_ALL },
    { "Void Gale Locket", SLOT_AMULET,  RARITY_EPIC,       78, 75000, {6,  6,  6,  6,  8,  0, 22}, CM_ALL },

/* ══════ Lv90 Epic Tier (Storm Theme) ══════ */
    /* ── Weapons Lv90 ── */
    { "Stormreaver",          SLOT_WEAPON,  RARITY_EPIC,      90,160000, {112,  0,  0,  0, 38, 22,  0}, CM_WAR },
    { "Stormwall Blade",      SLOT_WEAPON,  RARITY_EPIC,      90,160000, {69, 17,  0,  0, 43, 43,  0}, CM_WAR },
    { "Stormwind Sword",      SLOT_WEAPON,  RARITY_EPIC,      90,160000, {72, 24,  0,  0, 16,  0, 48}, CM_WAR },
    { "Thunderstrike",        SLOT_WEAPON,  RARITY_EPIC,      90,160000, {0,112,  0,  0, 38, 22,  0}, CM_ROG },
    { "Thunderguard Fang",    SLOT_WEAPON,  RARITY_EPIC,      90,160000, {17, 69,  0,  0, 43, 43,  0}, CM_ROG },
    { "Thunderwind Shiv",     SLOT_WEAPON,  RARITY_EPIC,      90,160000, {24, 72,  0,  0, 16,  0, 48}, CM_ROG },
    { "Tempest Staff",        SLOT_WEAPON,  RARITY_EPIC,      90,160000, {0,  0,112,  0, 38, 22,  0}, CM_MAG },
    { "Tempest Aegis Rod",    SLOT_WEAPON,  RARITY_EPIC,      90,160000, {0,  0, 69, 17, 43, 43,  0}, CM_MAG },
    { "Tempest Gale Wand",    SLOT_WEAPON,  RARITY_EPIC,      90,160000, {0,  0, 72, 24, 16,  0, 48}, CM_MAG },
    { "Storm Scepter",        SLOT_WEAPON,  RARITY_EPIC,      90,160000, {0,  0,  0,112, 38, 22,  0}, CM_PRI },
    { "Stormward Mace",       SLOT_WEAPON,  RARITY_EPIC,      90,160000, {0,  0, 17, 69, 43, 43,  0}, CM_PRI },
    { "Stormgale Scepter",    SLOT_WEAPON,  RARITY_EPIC,      90,160000, {0,  0, 24, 72, 16,  0, 48}, CM_PRI },
    /* ── Helmets Lv90 ── */
    { "Stormforged Helm",     SLOT_HELMET,  RARITY_EPIC,      90,110000, {84,  0,  0,  0, 28, 17,  0}, CM_WAR },
    { "Stormwall Helm",       SLOT_HELMET,  RARITY_EPIC,      90,110000, {52, 13,  0,  0, 32, 32,  0}, CM_WAR },
    { "Stormwind Visor",      SLOT_HELMET,  RARITY_EPIC,      90,110000, {54, 18,  0,  0, 12,  0, 36}, CM_WAR },
    { "Thunderstalker Hood",  SLOT_HELMET,  RARITY_EPIC,      90,110000, {0, 84,  0,  0, 28, 17,  0}, CM_ROG },
    { "Thunderguard Hood",    SLOT_HELMET,  RARITY_EPIC,      90,110000, {13, 52,  0,  0, 32, 32,  0}, CM_ROG },
    { "Thunderwind Cowl",     SLOT_HELMET,  RARITY_EPIC,      90,110000, {18, 54,  0,  0, 12,  0, 36}, CM_ROG },
    { "Tempest Crown",        SLOT_HELMET,  RARITY_EPIC,      90,110000, {0,  0, 84,  0, 28, 17,  0}, CM_MAG },
    { "Tempest Ward Crown",   SLOT_HELMET,  RARITY_EPIC,      90,110000, {0,  0, 52, 13, 32, 32,  0}, CM_MAG },
    { "Tempest Gale Tiara",   SLOT_HELMET,  RARITY_EPIC,      90,110000, {0,  0, 54, 18, 12,  0, 36}, CM_MAG },
    { "Storm Halo",           SLOT_HELMET,  RARITY_EPIC,      90,110000, {0,  0,  0, 84, 28, 17,  0}, CM_PRI },
    { "Stormward Coif",       SLOT_HELMET,  RARITY_EPIC,      90,110000, {0,  0, 13, 52, 32, 32,  0}, CM_PRI },
    { "Stormgale Mitre",      SLOT_HELMET,  RARITY_EPIC,      90,110000, {0,  0, 18, 54, 12,  0, 36}, CM_PRI },
    /* ── Chests Lv90 ── */
    { "Stormplate Armor",     SLOT_CHEST,  RARITY_EPIC,      90,140000, {103,  0,  0,  0, 35, 20,  0}, CM_WAR },
    { "Stormwall Plate",      SLOT_CHEST,  RARITY_EPIC,      90,140000, {63, 16,  0,  0, 40, 39,  0}, CM_WAR },
    { "Stormwind Mail",       SLOT_CHEST,  RARITY_EPIC,      90,140000, {66, 22,  0,  0, 15,  0, 44}, CM_WAR },
    { "Thunderweave Vest",    SLOT_CHEST,  RARITY_EPIC,      90,140000, {0,103,  0,  0, 35, 20,  0}, CM_ROG },
    { "Thunderguard Vest",    SLOT_CHEST,  RARITY_EPIC,      90,140000, {16, 63,  0,  0, 40, 39,  0}, CM_ROG },
    { "Thunderwind Tunic",    SLOT_CHEST,  RARITY_EPIC,      90,140000, {22, 66,  0,  0, 15,  0, 44}, CM_ROG },
    { "Tempest Robe",         SLOT_CHEST,  RARITY_EPIC,      90,140000, {0,  0,103,  0, 35, 20,  0}, CM_MAG },
    { "Tempest Ward Robe",    SLOT_CHEST,  RARITY_EPIC,      90,140000, {0,  0, 63, 16, 40, 39,  0}, CM_MAG },
    { "Tempest Gale Mantle",  SLOT_CHEST,  RARITY_EPIC,      90,140000, {0,  0, 66, 22, 15,  0, 44}, CM_MAG },
    { "Storm Vestment",       SLOT_CHEST,  RARITY_EPIC,      90,140000, {0,  0,  0,103, 35, 20,  0}, CM_PRI },
    { "Stormward Raiment",    SLOT_CHEST,  RARITY_EPIC,      90,140000, {0,  0, 16, 63, 40, 39,  0}, CM_PRI },
    { "Stormgale Cassock",    SLOT_CHEST,  RARITY_EPIC,      90,140000, {0,  0, 22, 66, 15,  0, 44}, CM_PRI },
    /* ── Legs Lv90 ── */
    { "Stormforged Greaves",  SLOT_LEGS,  RARITY_EPIC,      90,100000, {79,  0,  0,  0, 27, 16,  0}, CM_WAR },
    { "Stormwall Tassets",    SLOT_LEGS,  RARITY_EPIC,      90,100000, {49, 12,  0,  0, 30, 31,  0}, CM_WAR },
    { "Stormwind Guards",     SLOT_LEGS,  RARITY_EPIC,      90,100000, {51, 17,  0,  0, 11,  0, 34}, CM_WAR },
    { "Thunderstalker Pants", SLOT_LEGS,  RARITY_EPIC,      90,100000, {0, 79,  0,  0, 27, 16,  0}, CM_ROG },
    { "Thunderguard Pants",   SLOT_LEGS,  RARITY_EPIC,      90,100000, {12, 49,  0,  0, 30, 31,  0}, CM_ROG },
    { "Thunderwind Chaps",    SLOT_LEGS,  RARITY_EPIC,      90,100000, {17, 51,  0,  0, 11,  0, 34}, CM_ROG },
    { "Tempest Leggings",     SLOT_LEGS,  RARITY_EPIC,      90,100000, {0,  0, 79,  0, 27, 16,  0}, CM_MAG },
    { "Tempest Ward Kilt",    SLOT_LEGS,  RARITY_EPIC,      90,100000, {0,  0, 49, 12, 30, 31,  0}, CM_MAG },
    { "Tempest Gale Skirt",   SLOT_LEGS,  RARITY_EPIC,      90,100000, {0,  0, 51, 17, 11,  0, 34}, CM_MAG },
    { "Storm Cuisses",        SLOT_LEGS,  RARITY_EPIC,      90,100000, {0,  0,  0, 79, 27, 16,  0}, CM_PRI },
    { "Stormward Robes",      SLOT_LEGS,  RARITY_EPIC,      90,100000, {0,  0, 12, 49, 30, 31,  0}, CM_PRI },
    { "Stormgale Breeches",   SLOT_LEGS,  RARITY_EPIC,      90,100000, {0,  0, 17, 51, 11,  0, 34}, CM_PRI },
    /* ── Boots Lv90 ── */
    { "Stormforged Boots",    SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {56,  0,  0,  0, 19,  0, 11}, CM_WAR },
    { "Stormwall Treads",     SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {34,  9,  0,  0, 22, 21,  0}, CM_WAR },
    { "Stormwind Stompers",   SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {36, 12,  0,  0,  8,  0, 24}, CM_WAR },
    { "Thunderstalker Boots", SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {0, 56,  0,  0, 19,  0, 11}, CM_ROG },
    { "Thunderguard Boots",   SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {9, 34,  0,  0, 22, 21,  0}, CM_ROG },
    { "Thunderwind Shoes",    SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {12, 36,  0,  0,  8,  0, 24}, CM_ROG },
    { "Tempest Walkers",      SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {0,  0, 56,  0, 19,  0, 11}, CM_MAG },
    { "Tempest Ward Shoes",   SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {0,  0, 34,  9, 22, 21,  0}, CM_MAG },
    { "Tempest Gale Steps",   SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {0,  0, 36, 12,  8,  0, 24}, CM_MAG },
    { "Storm Striders",       SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {0,  0,  0, 56, 19,  0, 11}, CM_PRI },
    { "Stormward Sandals",    SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {0,  0,  9, 34, 22, 21,  0}, CM_PRI },
    { "Stormgale Treads",     SLOT_BOOTS,  RARITY_EPIC,      90, 75000, {0,  0, 12, 36,  8,  0, 24}, CM_PRI },
    /* ── Rings Lv90 (CM_ALL) ── */
    { "Storm Signet",         SLOT_RING,  RARITY_EPIC,      90, 65000, {14, 14, 14, 14, 16,  0,  0}, CM_ALL },
    { "Storm Aegis Band",     SLOT_RING,  RARITY_EPIC,      90, 65000, {6,  6,  6,  6, 24, 24,  0}, CM_ALL },
    { "Storm Gale Loop",      SLOT_RING,  RARITY_EPIC,      90, 65000, {8,  8,  8,  8,  8,  0, 28}, CM_ALL },
    /* ── Amulets Lv90 (CM_ALL) ── */
    { "Storm Heart",          SLOT_AMULET,  RARITY_EPIC,      90, 65000, {14, 14, 14, 14, 16,  0,  0}, CM_ALL },
    { "Storm Ward Charm",     SLOT_AMULET,  RARITY_EPIC,      90, 65000, {6,  6,  6,  6, 24, 24,  0}, CM_ALL },
    { "Storm Gale Locket",    SLOT_AMULET,  RARITY_EPIC,      90, 65000, {8,  8,  8,  8,  8,  0, 28}, CM_ALL },

/* ══════ Legendary Items (Rebalanced) ══════ */
    /* ── Legendary weapons (D8 boss exclusive, levelReq 100) ── */
    { "Eternity's Edge",      SLOT_WEAPON,  RARITY_LEGENDARY,     100,260000, {142, 38,  0,  0, 52, 26,  0}, CM_WAR },
    { "Eternal Whisper",      SLOT_WEAPON,  RARITY_LEGENDARY,     100,260000, {38,142,  0,  0, 52, 26,  0}, CM_ROG },
    { "Staff of Ages",        SLOT_WEAPON,  RARITY_LEGENDARY,     100,260000, {0,  0,142, 38, 52, 26,  0}, CM_MAG },
    { "Eternal Grace",        SLOT_WEAPON,  RARITY_LEGENDARY,     100,260000, {0,  0, 38,142, 52, 26,  0}, CM_PRI },
    /* ── Legendary helmets ── */
    { "Helm of Ages",         SLOT_HELMET,  RARITY_LEGENDARY,     100,190000, {74, 43,  0,  0, 54, 23,  0}, CM_WAR },
    { "Timeless Hood",        SLOT_HELMET,  RARITY_LEGENDARY,     100,190000, {43, 74,  0,  0, 54, 23,  0}, CM_ROG },
    { "Crown of Aeons",       SLOT_HELMET,  RARITY_LEGENDARY,     100,190000, {0,  0, 74, 43, 54, 23,  0}, CM_MAG },
    { "Eternal Aureole",      SLOT_HELMET,  RARITY_LEGENDARY,     100,190000, {0,  0, 43, 74, 54, 23,  0}, CM_PRI },
    /* ── Legendary chests ── */
    { "Armor of Eternity",    SLOT_CHEST,  RARITY_LEGENDARY,     100,240000, {90, 53,  0,  0, 66, 28,  0}, CM_WAR },
    { "Timeless Garb",        SLOT_CHEST,  RARITY_LEGENDARY,     100,240000, {53, 90,  0,  0, 66, 28,  0}, CM_ROG },
    { "Robe of Aeons",        SLOT_CHEST,  RARITY_LEGENDARY,     100,240000, {0,  0, 90, 53, 66, 28,  0}, CM_MAG },
    { "Eternal Vestment",     SLOT_CHEST,  RARITY_LEGENDARY,     100,240000, {0,  0, 53, 90, 66, 28,  0}, CM_PRI },
    /* ── Legendary legs ── */
    { "Greaves of Ages",      SLOT_LEGS,  RARITY_LEGENDARY,     100,180000, {70, 40,  0,  0, 51, 22,  0}, CM_WAR },
    { "Timeless Legwraps",    SLOT_LEGS,  RARITY_LEGENDARY,     100,180000, {40, 70,  0,  0, 51, 22,  0}, CM_ROG },
    { "Leggings of Aeons",    SLOT_LEGS,  RARITY_LEGENDARY,     100,180000, {0,  0, 70, 40, 51, 22,  0}, CM_MAG },
    { "Eternal Greaves",      SLOT_LEGS,  RARITY_LEGENDARY,     100,180000, {0,  0, 40, 70, 51, 22,  0}, CM_PRI },
    /* ── Legendary boots ── */
    { "Boots of Eternity",    SLOT_BOOTS,  RARITY_LEGENDARY,     100,130000, {28,  0,  0,  0, 26, 13, 62}, CM_WAR },
    { "Timeless Stride",      SLOT_BOOTS,  RARITY_LEGENDARY,     100,130000, {0, 28,  0,  0, 26, 13, 62}, CM_ROG },
    { "Steps of Aeons",       SLOT_BOOTS,  RARITY_LEGENDARY,     100,130000, {0,  0, 28,  0, 26, 13, 62}, CM_MAG },
    { "Eternal Striders",     SLOT_BOOTS,  RARITY_LEGENDARY,     100,130000, {0,  0,  0, 28, 26, 13, 62}, CM_PRI },
    /* ── Legendary rings (class-specific) ── */
    { "Eternal Signet",       SLOT_RING,  RARITY_LEGENDARY,     100,110000, {43, 27,  0,  0, 22,  0, 16}, CM_WAR },
    { "Timeless Loop",        SLOT_RING,  RARITY_LEGENDARY,     100,110000, {27, 43,  0,  0, 22,  0, 16}, CM_ROG },
    { "Band of Aeons",        SLOT_RING,  RARITY_LEGENDARY,     100,110000, {0,  0, 43, 27, 22,  0, 16}, CM_MAG },
    { "Eternal Seal",         SLOT_RING,  RARITY_LEGENDARY,     100,110000, {0,  0, 27, 43, 22,  0, 16}, CM_PRI },
    /* ── Legendary amulets (class-specific) ── */
    { "Charm of Ages",        SLOT_AMULET,  RARITY_LEGENDARY,     100,110000, {38, 27,  0,  0, 27, 16,  0}, CM_WAR },
    { "Timeless Pendant",     SLOT_AMULET,  RARITY_LEGENDARY,     100,110000, {27, 38,  0,  0, 27, 16,  0}, CM_ROG },
    { "Aeon Locket",          SLOT_AMULET,  RARITY_LEGENDARY,     100,110000, {0,  0, 38, 27, 27, 16,  0}, CM_MAG },
    { "Eternal Talisman",     SLOT_AMULET,  RARITY_LEGENDARY,     100,110000, {0,  0, 27, 38, 27, 16,  0}, CM_PRI },

};

static const int NUM_ITEMS_TOTAL = sizeof(ITEMS) / sizeof(ITEMS[0]);

int                 data_num_items(void) { return NUM_ITEMS_TOTAL; }
const ItemDef      *data_item(int id)    { return (id>=0 && id<NUM_ITEMS_TOTAL) ? &ITEMS[id] : NULL; }

/* ── procedural item generation ───────────────────────────────────── */

/*
 * Generate a random Common/Uncommon/Rare item with WoW-style naming.
 * Budget formula: max(2, (2 + level*1.1) * slotWeight * rarityWeight).
 * Stats split between suffix's two stats using its w1/w2 weights.
 * classMask 0 for jewelry (equippable by all), else (1 << classId).
 */
void data_generate_item(ItemDef *out, int slot, int rarity, int level, int classId) {
    memset(out, 0, sizeof(ItemDef));
    out->slot    = slot;
    out->rarity  = rarity;
    out->levelReq = level;
    int cm = (slot == SLOT_RING || slot == SLOT_AMULET) ? 0 : (1 << classId);
    out->classMask = cm;

    /* collect eligible suffixes — universal (classMask 0) always included */
    int cands[NUM_SUFFIXES], nc = 0;
    for (int i = 0; i < NUM_SUFFIXES; i++)
        if (SUFFIXES[i].classMask == 0 || cm == 0 || (SUFFIXES[i].classMask & cm))
            cands[nc++] = i;
    int si = cands[rand() % nc];
    const Suffix *suf = &SUFFIXES[si];

    const char *mat  = MATERIALS[rarity][rand() % 3];
    const char *base = BASE_NAMES[slot][classId][rand() % 3];
    snprintf(out->name, MAX_NAME, "%s %s %s", mat, base, suf->name);

    static const int SLOT_MUL[NUM_SLOTS] = { 120, 90, 110, 85, 60, 50, 50 };
    static const int RAR_MUL[3]          = { 50, 75, 105 };
    static const int RAR_FLOOR[3]        = { 2, 3, 5 };
    int budget = (int)((2.0f + level * 1.1f) * SLOT_MUL[slot] / 100.0f
                       * RAR_MUL[rarity] / 100.0f + 0.5f);
    if (budget < RAR_FLOOR[rarity]) budget = RAR_FLOOR[rarity];

    out->stats[suf->stat1] = budget * suf->w1 / 100;
    if (out->stats[suf->stat1] < 1 && budget >= 2) out->stats[suf->stat1] = 1;
    out->stats[suf->stat2] = budget - out->stats[suf->stat1];

    static const int RAR_PRICE[3] = { 1, 3, 8 };
    out->price = budget * (3 + level / 2) * RAR_PRICE[rarity] / 10;
    out->price = ((out->price + 5) / 10) * 10;
    if (out->price < 10) out->price = 10;
}

/*
 * Deterministic shop item — same as procedural but fixed suffix/material/base
 * so the shop is stable per class. Uses class-optimal suffix (Bear/Monkey/Owl/Whale),
 * first material variant, first base name. Price doubled as shop markup.
 */
void data_shop_item(ItemDef *out, int slot, int level, int classId) {
    memset(out, 0, sizeof(ItemDef));
    out->slot    = slot;
    out->levelReq = level;
    int cm = (slot == SLOT_RING || slot == SLOT_AMULET) ? 0 : (1 << classId);
    out->classMask = cm;

    int rarity = (level < 8) ? RARITY_COMMON
               : (level < 18) ? RARITY_UNCOMMON : RARITY_RARE;
    out->rarity = rarity;

    const Suffix *suf = &SUFFIXES[SHOP_SUFFIX[classId]];
    const char *mat   = MATERIALS[rarity][0];
    const char *base  = BASE_NAMES[slot][classId][0];
    snprintf(out->name, MAX_NAME, "%s %s %s", mat, base, suf->name);

    static const int SLOT_MUL[NUM_SLOTS] = { 120, 90, 110, 85, 60, 50, 50 };
    static const int RAR_MUL[3]          = { 50, 75, 105 };
    int budget = (int)((2.0f + level * 1.1f) * SLOT_MUL[slot] / 100.0f
                       * RAR_MUL[rarity] / 100.0f * 0.75f + 0.5f);
    if (budget < 1) budget = 1;

    out->stats[suf->stat1] = budget * suf->w1 / 100;
    if (out->stats[suf->stat1] < 1 && budget >= 2) out->stats[suf->stat1] = 1;
    out->stats[suf->stat2] = budget - out->stats[suf->stat1];

    static const int RAR_PRICE[3] = { 1, 3, 8 };
    out->price = budget * (3 + level / 2) * RAR_PRICE[rarity] / 10;
    out->price *= 2;  /* shop markup */
    out->price = ((out->price + 5) / 10) * 10;
    if (out->price < 10) out->price = 10;
}

/* Pick a random Epic/Legendary item from ITEMS[] matching level range and class. */
const ItemDef *data_random_drop(int minLvl, int maxLvl, int classMask, int maxRarity) {
    int candidates[NUM_ITEMS_TOTAL];
    int n = 0;
    for (int i = 0; i < NUM_ITEMS_TOTAL; i++) {
        if (ITEMS[i].levelReq >= minLvl && ITEMS[i].levelReq <= maxLvl &&
            ITEMS[i].rarity <= maxRarity) {
            if (ITEMS[i].classMask == 0 || (ITEMS[i].classMask & classMask))
                candidates[n++] = i;
        }
    }
    if (n == 0) return NULL;
    return &ITEMS[candidates[rand() % n]];
}
