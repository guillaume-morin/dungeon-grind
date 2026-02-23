/*
 * data_items.c — Item tables, procedural generation, and shop logic.
 *
 * Common/Uncommon/Rare items are procedurally generated at runtime using
 * WoW-style "{Material} {Base} of the {Animal}" naming. Epic items come
 * in three polarizing variants per slot/class/tier; Legendaries are unique.
 *
 * CM_ALL (0) means "any class can equip". Non-zero classMask is a
 * bitmask of (1 << classId). Jewelry (rings, amulets) uses CM_ALL;
 * armor and weapons are class-restricted.
 *
 * Item rarity progression is intentionally per-dungeon-tier:
 * Common(Lv1) -> Uncommon(Lv8) -> Rare(Lv18,32) -> Epic(Lv46+) -> Legendary(Lv90).
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
 * Legendary: single unique item per slot/class at Lv90.
 */
/*                                      slot          rar             lvl   price  STR AGI INT WIS VIT DEF SPD  class */
static const ItemDef ITEMS[] = {
    /* ── Weapons Lv46 ── */
    { "Ashbringer",       SLOT_WEAPON,  RARITY_EPIC,       46, 15000, {40,12,0,0,10,8,0}, CM_WAR },
    { "Boneguard Cleaver",SLOT_WEAPON,  RARITY_EPIC,       46, 15000, {28,7,0,0,18,17,0}, CM_WAR },
    { "Galeborn Sword",   SLOT_WEAPON,  RARITY_EPIC,       46, 15000, {29,10,0,0,6,0,20}, CM_WAR },
    { "Death's Whisper",  SLOT_WEAPON,  RARITY_EPIC,       46, 15000, {10,40,0,0,0,0,15}, CM_ROG },
    { "Deathward Dagger", SLOT_WEAPON,  RARITY_EPIC,       46, 15000, { 7,26,0,0,16,16,0}, CM_ROG },
    { "Windreap Shiv",    SLOT_WEAPON,  RARITY_EPIC,       46, 15000, { 9,27,0,0,6,0,18}, CM_ROG },
    { "Staff of Eternity",SLOT_WEAPON,  RARITY_EPIC,       46, 15000, { 0,0,40,15,0,0,8}, CM_MAG },
    { "Eternal Bastion",  SLOT_WEAPON,  RARITY_EPIC,       46, 15000, { 0,0,25,7,16,15,0}, CM_MAG },
    { "Galestaff Eternal",SLOT_WEAPON,  RARITY_EPIC,       46, 15000, { 0,0,27,9,5,0,18}, CM_MAG },
    { "Benediction",      SLOT_WEAPON,  RARITY_EPIC,       46, 15000, { 0,0,15,40,10,0,0}, CM_PRI },
    { "Aegis of Faith",   SLOT_WEAPON,  RARITY_EPIC,       46, 15000, { 0,0,7,26,16,16,0}, CM_PRI },
    { "Tempest Scepter",  SLOT_WEAPON,  RARITY_EPIC,       46, 15000, { 0,0,9,27,5,0,19}, CM_PRI },
    /* ── Weapons Lv55 ── */
    { "Molten Greatsword",SLOT_WEAPON,  RARITY_EPIC,       55, 24000, {60,0,0,0,19,10,0}, CM_WAR },
    { "Flameshield Blade",SLOT_WEAPON,  RARITY_EPIC,       55, 24000, {36,8,0,0,22,23,0}, CM_WAR },
    { "Blazestorm Sword", SLOT_WEAPON,  RARITY_EPIC,       55, 24000, {37,12,0,0,8,0,25}, CM_WAR },
    { "Magma Fang",       SLOT_WEAPON,  RARITY_EPIC,       55, 24000, {10,60,0,0,0,0,19}, CM_ROG },
    { "Pyreguard Fang",   SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 8,36,0,0,22,23,0}, CM_ROG },
    { "Cinderwind Shiv",  SLOT_WEAPON,  RARITY_EPIC,       55, 24000, {12,37,0,0,8,0,25}, CM_ROG },
    { "Flameheart Staff", SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,60,19,0,0,10}, CM_MAG },
    { "Fireward Staff",   SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,36,8,22,23,0}, CM_MAG },
    { "Stormfire Wand",   SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,37,12,8,0,25}, CM_MAG },
    { "Ember Scepter",    SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,19,60,10,0,0}, CM_PRI },
    { "Embershield Mace", SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,8,36,22,23,0}, CM_PRI },
    { "Pyrestorm Scepter",SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,12,37,8,0,25}, CM_PRI },
    /* ── Weapons Lv66 ── */
    { "Frostbite Cleaver",SLOT_WEAPON,  RARITY_EPIC,       66, 45000, {65,0,0,0,20,12,0}, CM_WAR },
    { "Frostwall Blade",  SLOT_WEAPON,  RARITY_EPIC,       66, 45000, {39,10,0,0,24,24,0}, CM_WAR },
    { "Frostwind Reaver", SLOT_WEAPON,  RARITY_EPIC,       66, 45000, {41,14,0,0,8,0,27}, CM_WAR },
    { "Icicle Shiv",      SLOT_WEAPON,  RARITY_EPIC,       66, 45000, {10,65,0,0,0,0,20}, CM_ROG },
    { "Iceward Stiletto", SLOT_WEAPON,  RARITY_EPIC,       66, 45000, {10,38,0,0,24,23,0}, CM_ROG },
    { "Icewind Dagger",   SLOT_WEAPON,  RARITY_EPIC,       66, 45000, {13,40,0,0,9,0,26}, CM_ROG },
    { "Frozen Scepter",   SLOT_WEAPON,  RARITY_EPIC,       66, 45000, { 0,0,65,20,0,0,10}, CM_MAG },
    { "Frostwall Staff",  SLOT_WEAPON,  RARITY_EPIC,       66, 45000, { 0,0,38,10,24,23,0}, CM_MAG },
    { "Frostgale Rod",    SLOT_WEAPON,  RARITY_EPIC,       66, 45000, { 0,0,40,13,9,0,26}, CM_MAG },
    { "Glacial Wand",     SLOT_WEAPON,  RARITY_EPIC,       66, 45000, { 0,0,20,65,12,0,0}, CM_PRI },
    { "Glacial Bulwark",  SLOT_WEAPON,  RARITY_EPIC,       66, 45000, { 0,0,10,39,24,24,0}, CM_PRI },
    { "Glacial Zephyr",   SLOT_WEAPON,  RARITY_EPIC,       66, 45000, { 0,0,14,41,8,0,27}, CM_PRI },
    /* ── Weapons Lv78 ── */
    { "Void Reaver",      SLOT_WEAPON,  RARITY_EPIC,       78, 84000, {85,0,0,0,28,16,0}, CM_WAR },
    { "Voidwall Cleaver", SLOT_WEAPON,  RARITY_EPIC,       78, 84000, {52,13,0,0,32,32,0}, CM_WAR },
    { "Voidstorm Blade",  SLOT_WEAPON,  RARITY_EPIC,       78, 84000, {54,18,0,0,12,0,36}, CM_WAR },
    { "Null Blade",       SLOT_WEAPON,  RARITY_EPIC,       78, 84000, {14,85,0,0,0,0,28}, CM_ROG },
    { "Nullguard Dagger", SLOT_WEAPON,  RARITY_EPIC,       78, 84000, {13,51,0,0,32,31,0}, CM_ROG },
    { "Nullwind Shiv",    SLOT_WEAPON,  RARITY_EPIC,       78, 84000, {18,53,0,0,11,0,36}, CM_ROG },
    { "Void Orb",         SLOT_WEAPON,  RARITY_EPIC,       78, 84000, { 0,0,85,28,0,0,14}, CM_MAG },
    { "Voidward Scepter", SLOT_WEAPON,  RARITY_EPIC,       78, 84000, { 0,0,51,13,32,31,0}, CM_MAG },
    { "Voidstorm Orb",    SLOT_WEAPON,  RARITY_EPIC,       78, 84000, { 0,0,53,18,11,0,36}, CM_MAG },
    { "Void Censer",      SLOT_WEAPON,  RARITY_EPIC,       78, 84000, { 0,0,28,85,16,0,0}, CM_PRI },
    { "Rift Aegis Mace",  SLOT_WEAPON,  RARITY_EPIC,       78, 84000, { 0,0,13,52,32,32,0}, CM_PRI },
    { "Rift Gale Scepter",SLOT_WEAPON,  RARITY_EPIC,       78, 84000, { 0,0,18,54,12,0,36}, CM_PRI },

    /* ── Helmets Lv46 ── */
    { "Crown of Ruin",    SLOT_HELMET,  RARITY_EPIC,       46, 10500, { 8,0,0,0,22,16,0}, CM_WAR },
    { "Boneguard Helm",   SLOT_HELMET,  RARITY_EPIC,       46, 10500, {18,5,0,0,12,11,0}, CM_WAR },
    { "Galeforged Visor", SLOT_HELMET,  RARITY_EPIC,       46, 10500, {19,6,0,0,5,0,13}, CM_WAR },
    { "Phantom Crown",    SLOT_HELMET,  RARITY_EPIC,       46, 10500, { 0,16,0,0,6,0,14}, CM_ROG },
    { "Phantom Bastion",  SLOT_HELMET,  RARITY_EPIC,       46, 10500, { 4,14,0,0,9,9,0}, CM_ROG },
    { "Phantom Gale Hood",SLOT_HELMET,  RARITY_EPIC,       46, 10500, { 5,15,0,0,3,0,10}, CM_ROG },
    { "Crown of Stars",   SLOT_HELMET,  RARITY_EPIC,       46, 10500, { 0,0,22,10,5,0,0}, CM_MAG },
    { "Starwarden Crown", SLOT_HELMET,  RARITY_EPIC,       46, 10500, { 0,0,15,4,9,9,0}, CM_MAG },
    { "Stargale Circlet", SLOT_HELMET,  RARITY_EPIC,       46, 10500, { 0,0,15,5,4,0,10}, CM_MAG },
    { "Divine Crown",     SLOT_HELMET,  RARITY_EPIC,       46, 10500, { 0,0,0,18,12,8,0}, CM_PRI },
    { "Hallowed Bulwark", SLOT_HELMET,  RARITY_EPIC,       46, 10500, { 0,0,4,15,10,9,0}, CM_PRI },
    { "Divinewind Mitre", SLOT_HELMET,  RARITY_EPIC,       46, 10500, { 0,0,5,16,3,0,11}, CM_PRI },
    /* ── Helmets Lv55 ── */
    { "Molten Warhelm",   SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 6,0,0,0,22,24,0}, CM_WAR },
    { "Magma Sentinel",   SLOT_HELMET,  RARITY_EPIC,       55, 16500, {20,5,0,0,13,13,0}, CM_WAR },
    { "Blazewind Visor",  SLOT_HELMET,  RARITY_EPIC,       55, 16500, {22,7,0,0,5,0,14}, CM_WAR },
    { "Magma Visor",      SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,19,0,0,8,0,16}, CM_ROG },
    { "Magma Aegis Hood", SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 5,17,0,0,11,11,0}, CM_ROG },
    { "Magma Gale Mask",  SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 6,18,0,0,4,0,12}, CM_ROG },
    { "Flamecrown",       SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,0,24,12,5,0,0}, CM_MAG },
    { "Flameguard Crown", SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,0,17,4,11,10,0}, CM_MAG },
    { "Flamegale Circlet",SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,0,17,6,4,0,12}, CM_MAG },
    { "Ember Mitre",      SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,0,0,22,14,11,0}, CM_PRI },
    { "Ember Rampart Coif",SLOT_HELMET, RARITY_EPIC,       55, 16500, { 0,0,5,19,12,11,0}, CM_PRI },
    { "Emberwind Halo",   SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,0,6,19,5,0,13}, CM_PRI },
    /* ── Helmets Lv66 ── */
    { "Frostguard Helm",  SLOT_HELMET,  RARITY_EPIC,       66, 30000, { 8,0,0,0,24,28,0}, CM_WAR },
    { "Frostwall Helm",   SLOT_HELMET,  RARITY_EPIC,       66, 30000, {24,6,0,0,15,15,0}, CM_WAR },
    { "Frostwind Visor",  SLOT_HELMET,  RARITY_EPIC,       66, 30000, {25,8,0,0,6,0,17}, CM_WAR },
    { "Icestalker Hood",  SLOT_HELMET,  RARITY_EPIC,       66, 30000, { 0,20,0,0,9,0,17}, CM_ROG },
    { "Iceward Hood",     SLOT_HELMET,  RARITY_EPIC,       66, 30000, { 5,18,0,0,12,11,0}, CM_ROG },
    { "Icewind Cowl",     SLOT_HELMET,  RARITY_EPIC,       66, 30000, { 6,19,0,0,5,0,13}, CM_ROG },
    { "Frostweave Crown", SLOT_HELMET,  RARITY_EPIC,       66, 30000, { 0,0,28,14,5,0,0}, CM_MAG },
    { "Frostward Diadem", SLOT_HELMET,  RARITY_EPIC,       66, 30000, { 0,0,19,5,12,11,0}, CM_MAG },
    { "Frostgale Tiara",  SLOT_HELMET,  RARITY_EPIC,       66, 30000, { 0,0,20,7,4,0,13}, CM_MAG },
    { "Glacial Halo",     SLOT_HELMET,  RARITY_EPIC,       66, 30000, { 0,0,0,24,16,12,0}, CM_PRI },
    { "Glacial Aegis Coif",SLOT_HELMET, RARITY_EPIC,       66, 30000, { 0,0,5,21,13,13,0}, CM_PRI },
    { "Glacial Wind Halo",SLOT_HELMET,  RARITY_EPIC,       66, 30000, { 0,0,7,22,5,0,14}, CM_PRI },
    /* ── Helmets Lv78 ── */
    { "Voidforged Visor", SLOT_HELMET,  RARITY_EPIC,       78, 54000, {10,0,0,0,30,36,0}, CM_WAR },
    { "Voidwall Helm",    SLOT_HELMET,  RARITY_EPIC,       78, 54000, {30,8,0,0,19,19,0}, CM_WAR },
    { "Voidstorm Visor",  SLOT_HELMET,  RARITY_EPIC,       78, 54000, {32,11,0,0,7,0,21}, CM_WAR },
    { "Null Mask",        SLOT_HELMET,  RARITY_EPIC,       78, 54000, { 0,28,0,0,12,0,22}, CM_ROG },
    { "Nullguard Hood",   SLOT_HELMET,  RARITY_EPIC,       78, 54000, { 6,25,0,0,16,15,0}, CM_ROG },
    { "Nullwind Cowl",    SLOT_HELMET,  RARITY_EPIC,       78, 54000, { 9,26,0,0,6,0,17}, CM_ROG },
    { "Rift Diadem",      SLOT_HELMET,  RARITY_EPIC,       78, 54000, { 0,0,36,18,7,0,0}, CM_MAG },
    { "Rift Aegis Crown", SLOT_HELMET,  RARITY_EPIC,       78, 54000, { 0,0,24,6,15,16,0}, CM_MAG },
    { "Rift Gale Circlet",SLOT_HELMET,  RARITY_EPIC,       78, 54000, { 0,0,26,9,5,0,17}, CM_MAG },
    { "Void Circlet",     SLOT_HELMET,  RARITY_EPIC,       78, 54000, { 0,0,0,30,20,16,0}, CM_PRI },
    { "Voidward Mitre",   SLOT_HELMET,  RARITY_EPIC,       78, 54000, { 0,0,7,26,17,16,0}, CM_PRI },
    { "Voidwind Halo",    SLOT_HELMET,  RARITY_EPIC,       78, 54000, { 0,0,9,27,7,0,18}, CM_PRI },

    /* ── Chests Lv46 ── */
    { "Aegis of Valor",   SLOT_CHEST,   RARITY_EPIC,       46, 12000, {10,0,0,0,22,28,0}, CM_WAR },
    { "Boneguard Plate",  SLOT_CHEST,   RARITY_EPIC,       46, 12000, {24,6,0,0,15,15,0}, CM_WAR },
    { "Galeborn Mail",    SLOT_CHEST,   RARITY_EPIC,       46, 12000, {25,8,0,0,6,0,17}, CM_WAR },
    { "Phantom Shroud",   SLOT_CHEST,   RARITY_EPIC,       46, 12000, { 0,20,0,0,8,0,16}, CM_ROG },
    { "Deathward Vest",   SLOT_CHEST,   RARITY_EPIC,       46, 12000, { 4,18,0,0,11,11,0}, CM_ROG },
    { "Windreap Tunic",   SLOT_CHEST,   RARITY_EPIC,       46, 12000, { 6,18,0,0,5,0,12}, CM_ROG },
    { "Robe of Eternity", SLOT_CHEST,   RARITY_EPIC,       46, 12000, { 0,0,28,14,5,0,0}, CM_MAG },
    { "Eternal Ward Robe",SLOT_CHEST,   RARITY_EPIC,       46, 12000, { 0,0,19,5,12,11,0}, CM_MAG },
    { "Galeweave Mantle", SLOT_CHEST,   RARITY_EPIC,       46, 12000, { 0,0,20,7,4,0,13}, CM_MAG },
    { "Divine Regalia",   SLOT_CHEST,   RARITY_EPIC,       46, 12000, { 0,0,0,22,14,10,0}, CM_PRI },
    { "Hallowed Raiment", SLOT_CHEST,   RARITY_EPIC,       46, 12000, { 0,0,5,18,12,11,0}, CM_PRI },
    { "Tempest Surcoat",  SLOT_CHEST,   RARITY_EPIC,       46, 12000, { 0,0,6,19,5,0,13}, CM_PRI },
    /* ── Chests Lv55 ── */
    { "Molten Cuirass",   SLOT_CHEST,   RARITY_EPIC,       55, 19500, {10,0,0,0,34,38,0}, CM_WAR },
    { "Flameguard Plate", SLOT_CHEST,   RARITY_EPIC,       55, 19500, {32,8,0,0,20,20,0}, CM_WAR },
    { "Blazewind Mail",   SLOT_CHEST,   RARITY_EPIC,       55, 19500, {34,11,0,0,8,0,23}, CM_WAR },
    { "Magma Tunic",      SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,29,0,0,12,0,24}, CM_ROG },
    { "Pyreguard Jerkin", SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 6,26,0,0,17,16,0}, CM_ROG },
    { "Cinderwind Vest",  SLOT_CHEST,   RARITY_EPIC,       55, 19500, {10,28,0,0,5,0,18}, CM_ROG },
    { "Flamewoven Robe",  SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,0,38,19,7,0,0}, CM_MAG },
    { "Fireward Mantle",  SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,0,26,6,17,16,0}, CM_MAG },
    { "Stormfire Robe",   SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,0,28,10,5,0,18}, CM_MAG },
    { "Ember Vestment",   SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,0,0,34,22,17,0}, CM_PRI },
    { "Emberward Garb",   SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,0,7,29,18,18,0}, CM_PRI },
    { "Emberstorm Cassock",SLOT_CHEST,  RARITY_EPIC,       55, 19500, { 0,0,10,30,7,0,20}, CM_PRI },
    /* ── Chests Lv66 ── */
    { "Frostplate Armor", SLOT_CHEST,   RARITY_EPIC,       66, 36000, {12,0,0,0,36,42,0}, CM_WAR },
    { "Frostwall Plate",  SLOT_CHEST,   RARITY_EPIC,       66, 36000, {36,9,0,0,22,23,0}, CM_WAR },
    { "Frostwind Armor",  SLOT_CHEST,   RARITY_EPIC,       66, 36000, {38,13,0,0,8,0,25}, CM_WAR },
    { "Iceweave Jerkin",  SLOT_CHEST,   RARITY_EPIC,       66, 36000, { 0,32,0,0,14,0,26}, CM_ROG },
    { "Iceward Jerkin",   SLOT_CHEST,   RARITY_EPIC,       66, 36000, { 7,29,0,0,18,18,0}, CM_ROG },
    { "Icewind Tunic",    SLOT_CHEST,   RARITY_EPIC,       66, 36000, {10,30,0,0,7,0,20}, CM_ROG },
    { "Frozen Robe",      SLOT_CHEST,   RARITY_EPIC,       66, 36000, { 0,0,42,22,8,0,0}, CM_MAG },
    { "Frostward Robe",   SLOT_CHEST,   RARITY_EPIC,       66, 36000, { 0,0,29,7,18,18,0}, CM_MAG },
    { "Frostgale Mantle", SLOT_CHEST,   RARITY_EPIC,       66, 36000, { 0,0,30,10,7,0,20}, CM_MAG },
    { "Glacial Raiment",  SLOT_CHEST,   RARITY_EPIC,       66, 36000, { 0,0,0,36,24,18,0}, CM_PRI },
    { "Glacial Aegis Garb",SLOT_CHEST,  RARITY_EPIC,       66, 36000, { 0,0,8,31,20,19,0}, CM_PRI },
    { "Glacial Wind Robe",SLOT_CHEST,   RARITY_EPIC,       66, 36000, { 0,0,11,33,7,0,22}, CM_PRI },
    /* ── Chests Lv78 ── */
    { "Voidplate",        SLOT_CHEST,   RARITY_EPIC,       78, 66000, {16,0,0,0,46,55,0}, CM_WAR },
    { "Voidwall Plate",   SLOT_CHEST,   RARITY_EPIC,       78, 66000, {47,12,0,0,29,29,0}, CM_WAR },
    { "Voidstorm Mail",   SLOT_CHEST,   RARITY_EPIC,       78, 66000, {49,16,0,0,11,0,33}, CM_WAR },
    { "Null Shroud",      SLOT_CHEST,   RARITY_EPIC,       78, 66000, { 0,42,0,0,18,0,34}, CM_ROG },
    { "Nullguard Vest",   SLOT_CHEST,   RARITY_EPIC,       78, 66000, { 9,38,0,0,24,23,0}, CM_ROG },
    { "Nullwind Jerkin",  SLOT_CHEST,   RARITY_EPIC,       78, 66000, {13,39,0,0,9,0,26}, CM_ROG },
    { "Rift Robe",        SLOT_CHEST,   RARITY_EPIC,       78, 66000, { 0,0,55,28,10,0,0}, CM_MAG },
    { "Rift Aegis Robe",  SLOT_CHEST,   RARITY_EPIC,       78, 66000, { 0,0,37,9,23,24,0}, CM_MAG },
    { "Rift Gale Mantle", SLOT_CHEST,   RARITY_EPIC,       78, 66000, { 0,0,39,13,8,0,26}, CM_MAG },
    { "Void Regalia",     SLOT_CHEST,   RARITY_EPIC,       78, 66000, { 0,0,0,46,30,24,0}, CM_PRI },
    { "Voidward Raiment", SLOT_CHEST,   RARITY_EPIC,       78, 66000, { 0,0,10,40,25,25,0}, CM_PRI },
    { "Voidwind Cassock", SLOT_CHEST,   RARITY_EPIC,       78, 66000, { 0,0,14,42,9,0,28}, CM_PRI },

    /* ── Legs Lv46 ── */
    { "Bonecage Greaves", SLOT_LEGS,    RARITY_EPIC,       46,  9600, { 8,0,0,0,18,24,0}, CM_WAR },
    { "Boneguard Tassets", SLOT_LEGS,   RARITY_EPIC,       46,  9600, {20,5,0,0,12,13,0}, CM_WAR },
    { "Galeborn Greaves", SLOT_LEGS,    RARITY_EPIC,       46,  9600, {21,7,0,0,5,0,14}, CM_WAR },
    { "Phantom Legwraps", SLOT_LEGS,    RARITY_EPIC,       46,  9600, { 0,16,0,0,4,0,14}, CM_ROG },
    { "Deathward Pants",  SLOT_LEGS,    RARITY_EPIC,       46,  9600, { 3,14,0,0,8,9,0}, CM_ROG },
    { "Windreap Leggings",SLOT_LEGS,    RARITY_EPIC,       46,  9600, { 5,14,0,0,3,0,10}, CM_ROG },
    { "Leggings of Stars",SLOT_LEGS,    RARITY_EPIC,       46,  9600, { 0,0,20,10,0,0,5}, CM_MAG },
    { "Starwarden Kilt",  SLOT_LEGS,    RARITY_EPIC,       46,  9600, { 0,0,14,4,9,8,0}, CM_MAG },
    { "Stargale Trousers",SLOT_LEGS,    RARITY_EPIC,       46,  9600, { 0,0,15,5,3,0,10}, CM_MAG },
    { "Divine Greaves",   SLOT_LEGS,    RARITY_EPIC,       46,  9600, { 0,0,0,16,12,8,0}, CM_PRI },
    { "Hallowed Cuisses", SLOT_LEGS,    RARITY_EPIC,       46,  9600, { 0,0,4,14,9,9,0}, CM_PRI },
    { "Divinewind Robes", SLOT_LEGS,    RARITY_EPIC,       46,  9600, { 0,0,5,15,3,0,10}, CM_PRI },
    /* ── Legs Lv55 ── */
    { "Molten Legguards", SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 7,0,0,0,26,31,0}, CM_WAR },
    { "Flameguard Greaves",SLOT_LEGS,   RARITY_EPIC,       55, 15000, {26,6,0,0,17,16,0}, CM_WAR },
    { "Blazewind Greaves",SLOT_LEGS,    RARITY_EPIC,       55, 15000, {28,10,0,0,5,0,18}, CM_WAR },
    { "Magma Leggings",   SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,24,0,0,10,0,19}, CM_ROG },
    { "Pyreguard Pants",  SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 5,22,0,0,13,13,0}, CM_ROG },
    { "Cinderwind Pants", SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 7,22,0,0,6,0,14}, CM_ROG },
    { "Flamewoven Pants", SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,31,14,6,0,0}, CM_MAG },
    { "Fireward Trousers",SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,20,5,13,13,0}, CM_MAG },
    { "Stormfire Kilt",   SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,22,7,5,0,14}, CM_MAG },
    { "Ember Greaves",    SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,0,26,17,13,0}, CM_PRI },
    { "Emberward Cuisses",SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,6,23,14,13,0}, CM_PRI },
    { "Emberstorm Robes", SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,8,24,5,0,16}, CM_PRI },
    /* ── Legs Lv66 ── */
    { "Frostguard Greaves",SLOT_LEGS,   RARITY_EPIC,       66, 27000, {10,0,0,0,28,34,0}, CM_WAR },
    { "Frostwall Greaves",SLOT_LEGS,    RARITY_EPIC,       66, 27000, {29,7,0,0,18,18,0}, CM_WAR },
    { "Frostwind Guards", SLOT_LEGS,    RARITY_EPIC,       66, 27000, {30,10,0,0,7,0,20}, CM_WAR },
    { "Icestalker Pants", SLOT_LEGS,    RARITY_EPIC,       66, 27000, { 0,26,0,0,12,0,20}, CM_ROG },
    { "Iceward Leggings", SLOT_LEGS,    RARITY_EPIC,       66, 27000, { 6,23,0,0,15,14,0}, CM_ROG },
    { "Icewind Chaps",    SLOT_LEGS,    RARITY_EPIC,       66, 27000, { 8,24,0,0,6,0,16}, CM_ROG },
    { "Frostweave Legs",  SLOT_LEGS,    RARITY_EPIC,       66, 27000, { 0,0,34,18,6,0,0}, CM_MAG },
    { "Frostward Kilt",   SLOT_LEGS,    RARITY_EPIC,       66, 27000, { 0,0,23,6,15,14,0}, CM_MAG },
    { "Frostgale Skirt",  SLOT_LEGS,    RARITY_EPIC,       66, 27000, { 0,0,24,8,6,0,16}, CM_MAG },
    { "Glacial Cuisses",  SLOT_LEGS,    RARITY_EPIC,       66, 27000, { 0,0,0,28,20,14,0}, CM_PRI },
    { "Glacial Ward Legs",SLOT_LEGS,    RARITY_EPIC,       66, 27000, { 0,0,6,25,16,15,0}, CM_PRI },
    { "Glacial Wind Robes",SLOT_LEGS,   RARITY_EPIC,       66, 27000, { 0,0,9,26,6,0,17}, CM_PRI },
    /* ── Legs Lv78 ── */
    { "Voidforged Greaves",SLOT_LEGS,   RARITY_EPIC,       78, 48000, {12,0,0,0,36,44,0}, CM_WAR },
    { "Voidwall Tassets", SLOT_LEGS,    RARITY_EPIC,       78, 48000, {37,9,0,0,23,23,0}, CM_WAR },
    { "Voidstorm Greaves",SLOT_LEGS,    RARITY_EPIC,       78, 48000, {39,13,0,0,8,0,26}, CM_WAR },
    { "Null Legwraps",    SLOT_LEGS,    RARITY_EPIC,       78, 48000, { 0,34,0,0,14,0,28}, CM_ROG },
    { "Nullguard Pants",  SLOT_LEGS,    RARITY_EPIC,       78, 48000, { 8,30,0,0,19,19,0}, CM_ROG },
    { "Nullwind Leggings",SLOT_LEGS,    RARITY_EPIC,       78, 48000, {11,32,0,0,7,0,21}, CM_ROG },
    { "Rift Leggings",    SLOT_LEGS,    RARITY_EPIC,       78, 48000, { 0,0,44,22,8,0,0}, CM_MAG },
    { "Rift Ward Kilt",   SLOT_LEGS,    RARITY_EPIC,       78, 48000, { 0,0,30,7,18,19,0}, CM_MAG },
    { "Rift Gale Skirt",  SLOT_LEGS,    RARITY_EPIC,       78, 48000, { 0,0,31,10,7,0,21}, CM_MAG },
    { "Void Greaves",     SLOT_LEGS,    RARITY_EPIC,       78, 48000, { 0,0,0,36,24,20,0}, CM_PRI },
    { "Voidward Cuisses", SLOT_LEGS,    RARITY_EPIC,       78, 48000, { 0,0,8,32,20,20,0}, CM_PRI },
    { "Voidwind Breeches",SLOT_LEGS,    RARITY_EPIC,       78, 48000, { 0,0,11,33,8,0,22}, CM_PRI },

    /* ── Boots Lv46 ── */
    { "Titanstep Boots",  SLOT_BOOTS,   RARITY_EPIC,       46,  9000, { 0,0,0,0,8,12,10}, CM_WAR },
    { "Boneguard Treads", SLOT_BOOTS,   RARITY_EPIC,       46,  9000, {12,3,0,0,8,7,0}, CM_WAR },
    { "Galeborn Stompers",SLOT_BOOTS,   RARITY_EPIC,       46,  9000, {13,4,0,0,3,0,8}, CM_WAR },
    { "Phantom Stride",   SLOT_BOOTS,   RARITY_EPIC,       46,  9000, { 0,10,0,0,4,0,20}, CM_ROG },
    { "Deathward Boots",  SLOT_BOOTS,   RARITY_EPIC,       46,  9000, { 3,14,0,0,9,8,0}, CM_ROG },
    { "Windreap Walkers", SLOT_BOOTS,   RARITY_EPIC,       46,  9000, { 5,14,0,0,3,0,10}, CM_ROG },
    { "Steps of Eternity",SLOT_BOOTS,   RARITY_EPIC,       46,  9000, { 0,0,10,6,0,0,14}, CM_MAG },
    { "Eternal Ward Shoes",SLOT_BOOTS,  RARITY_EPIC,       46,  9000, { 0,0,12,3,8,7,0}, CM_MAG },
    { "Galeweave Steps",  SLOT_BOOTS,   RARITY_EPIC,       46,  9000, { 0,0,13,4,3,0,8}, CM_MAG },
    { "Divine Striders",  SLOT_BOOTS,   RARITY_EPIC,       46,  9000, { 0,0,0,10,8,0,10}, CM_PRI },
    { "Hallowed Sandals", SLOT_BOOTS,   RARITY_EPIC,       46,  9000, { 0,0,3,11,7,7,0}, CM_PRI },
    { "Tempest Steps",    SLOT_BOOTS,   RARITY_EPIC,       46,  9000, { 0,0,4,12,2,0,8}, CM_PRI },
    /* ── Boots Lv55 ── */
    { "Molten Sabatons",  SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,0,0,12,17,14}, CM_WAR },
    { "Flameguard Treads",SLOT_BOOTS,   RARITY_EPIC,       55, 13500, {17,5,0,0,11,11,0}, CM_WAR },
    { "Blazewind Boots",  SLOT_BOOTS,   RARITY_EPIC,       55, 13500, {18,6,0,0,4,0,12}, CM_WAR },
    { "Magma Treads",     SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,14,0,0,7,0,22}, CM_ROG },
    { "Pyreguard Boots",  SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 5,17,0,0,11,11,0}, CM_ROG },
    { "Cinderwind Shoes", SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 6,18,0,0,4,0,12}, CM_ROG },
    { "Flamestep Shoes",  SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,14,10,0,0,17}, CM_MAG },
    { "Fireward Slippers",SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,17,4,11,10,0}, CM_MAG },
    { "Stormfire Steps",  SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,17,6,4,0,12}, CM_MAG },
    { "Ember Sandals",    SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,0,14,12,0,14}, CM_PRI },
    { "Emberward Treads", SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,4,17,11,10,0}, CM_PRI },
    { "Emberstorm Steps", SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,6,17,4,0,12}, CM_PRI },
    /* ── Boots Lv66 ── */
    { "Frostguard Boots", SLOT_BOOTS,   RARITY_EPIC,       66, 24000, { 0,0,0,0,14,18,16}, CM_WAR },
    { "Frostwall Treads", SLOT_BOOTS,   RARITY_EPIC,       66, 24000, {19,5,0,0,12,12,0}, CM_WAR },
    { "Frostwind Stompers",SLOT_BOOTS,  RARITY_EPIC,       66, 24000, {20,7,0,0,4,0,14}, CM_WAR },
    { "Icestalker Boots", SLOT_BOOTS,   RARITY_EPIC,       66, 24000, { 0,16,0,0,8,0,24}, CM_ROG },
    { "Iceward Walkers",  SLOT_BOOTS,   RARITY_EPIC,       66, 24000, { 5,19,0,0,12,12,0}, CM_ROG },
    { "Icewind Shoes",    SLOT_BOOTS,   RARITY_EPIC,       66, 24000, { 7,20,0,0,4,0,14}, CM_ROG },
    { "Frostweave Shoes", SLOT_BOOTS,   RARITY_EPIC,       66, 24000, { 0,0,16,10,0,0,18}, CM_MAG },
    { "Frostward Slippers",SLOT_BOOTS,  RARITY_EPIC,       66, 24000, { 0,0,18,4,11,11,0}, CM_MAG },
    { "Frostgale Steps",  SLOT_BOOTS,   RARITY_EPIC,       66, 24000, { 0,0,18,6,5,0,12}, CM_MAG },
    { "Glacial Sandals",  SLOT_BOOTS,   RARITY_EPIC,       66, 24000, { 0,0,0,16,12,0,16}, CM_PRI },
    { "Glacial Ward Steps",SLOT_BOOTS,  RARITY_EPIC,       66, 24000, { 0,0,4,18,11,11,0}, CM_PRI },
    { "Glacial Gale Steps",SLOT_BOOTS,  RARITY_EPIC,       66, 24000, { 0,0,6,18,5,0,12}, CM_PRI },
    /* ── Boots Lv78 ── */
    { "Voidforged Boots", SLOT_BOOTS,   RARITY_EPIC,       78, 42000, { 0,0,0,0,18,24,20}, CM_WAR },
    { "Voidwall Boots",   SLOT_BOOTS,   RARITY_EPIC,       78, 42000, {25,6,0,0,16,15,0}, CM_WAR },
    { "Voidstorm Treads", SLOT_BOOTS,   RARITY_EPIC,       78, 42000, {26,9,0,0,6,0,17}, CM_WAR },
    { "Null Treads",      SLOT_BOOTS,   RARITY_EPIC,       78, 42000, { 0,20,0,0,10,0,32}, CM_ROG },
    { "Nullguard Walkers",SLOT_BOOTS,   RARITY_EPIC,       78, 42000, { 6,25,0,0,16,15,0}, CM_ROG },
    { "Nullwind Shoes",   SLOT_BOOTS,   RARITY_EPIC,       78, 42000, { 9,26,0,0,6,0,17}, CM_ROG },
    { "Rift Walkers",     SLOT_BOOTS,   RARITY_EPIC,       78, 42000, { 0,0,20,14,0,0,24}, CM_MAG },
    { "Rift Ward Shoes",  SLOT_BOOTS,   RARITY_EPIC,       78, 42000, { 0,0,23,6,14,15,0}, CM_MAG },
    { "Rift Gale Steps",  SLOT_BOOTS,   RARITY_EPIC,       78, 42000, { 0,0,24,8,6,0,16}, CM_MAG },
    { "Void Striders",    SLOT_BOOTS,   RARITY_EPIC,       78, 42000, { 0,0,0,20,16,0,20}, CM_PRI },
    { "Voidward Sandals", SLOT_BOOTS,   RARITY_EPIC,       78, 42000, { 0,0,6,22,14,14,0}, CM_PRI },
    { "Voidwind Treads",  SLOT_BOOTS,   RARITY_EPIC,       78, 42000, { 0,0,8,23,5,0,16}, CM_PRI },

    /* ── Rings (CM_ALL) ── */
    { "Ring of the Lich", SLOT_RING,    RARITY_EPIC,       46, 11400, { 0,0,15,15,8,0,0}, CM_ALL },
    { "Lich King's Seal", SLOT_RING,    RARITY_EPIC,       46, 11400, { 3,3,3,3,14,12,0}, CM_ALL },
    { "Lich Wind Loop",   SLOT_RING,    RARITY_EPIC,       46, 11400, { 4,4,4,4,5,0,14}, CM_ALL },
    { "Molten Band",      SLOT_RING,    RARITY_EPIC,       55, 18000, {14,14,0,0,0,0,7}, CM_ALL },
    { "Molten Seal",      SLOT_RING,    RARITY_EPIC,       55, 18000, { 4,4,0,0,14,11,4}, CM_ALL },
    { "Molten Gale Ring", SLOT_RING,    RARITY_EPIC,       55, 18000, { 6,6,4,4,2,0,12}, CM_ALL },
    { "Frozen Signet",    SLOT_RING,    RARITY_EPIC,       66, 33000, { 0,0,16,16,8,0,0}, CM_ALL },
    { "Frozen Ward Band", SLOT_RING,    RARITY_EPIC,       66, 33000, { 3,3,3,3,16,12,0}, CM_ALL },
    { "Frozen Gale Loop", SLOT_RING,    RARITY_EPIC,       66, 33000, { 4,4,5,5,5,0,14}, CM_ALL },
    { "Void Ring",        SLOT_RING,    RARITY_EPIC,       78, 60000, { 0,10,0,0,14,10,10}, CM_ALL },
    { "Void Aegis Band",  SLOT_RING,    RARITY_EPIC,       78, 60000, { 4,4,4,4,16,12,0}, CM_ALL },
    { "Void Gale Ring",   SLOT_RING,    RARITY_EPIC,       78, 60000, { 5,5,5,5,5,0,16}, CM_ALL },

    /* ── Amulets (CM_ALL) ── */
    { "Heart of Eternity",SLOT_AMULET,  RARITY_EPIC,       46, 11400, { 0,0,0,12,15,8,0}, CM_ALL },
    { "Warden's Locket",  SLOT_AMULET,  RARITY_EPIC,       46, 11400, { 3,3,3,3,12,11,0}, CM_ALL },
    { "Windborn Charm",   SLOT_AMULET,  RARITY_EPIC,       46, 11400, { 3,3,4,4,5,0,13}, CM_ALL },
    { "Molten Pendant",   SLOT_AMULET,  RARITY_EPIC,       55, 18000, { 0,0,0,10,17,7,0}, CM_ALL },
    { "Molten Ward Charm",SLOT_AMULET,  RARITY_EPIC,       55, 18000, { 4,4,0,0,13,10,4}, CM_ALL },
    { "Molten Gale Charm",SLOT_AMULET,  RARITY_EPIC,       55, 18000, { 5,5,4,4,2,0,12}, CM_ALL },
    { "Frozen Heart",     SLOT_AMULET,  RARITY_EPIC,       66, 33000, { 0,0,10,10,10,6,0}, CM_ALL },
    { "Frozen Ward Charm",SLOT_AMULET,  RARITY_EPIC,       66, 33000, { 3,3,3,3,14,10,0}, CM_ALL },
    { "Frozen Gale Locket",SLOT_AMULET, RARITY_EPIC,       66, 33000, { 4,4,4,4,5,0,12}, CM_ALL },
    { "Void Locket",      SLOT_AMULET,  RARITY_EPIC,       78, 60000, { 0,0,12,12,18,8,0}, CM_ALL },
    { "Void Aegis Pendant",SLOT_AMULET, RARITY_EPIC,       78, 60000, { 4,4,4,4,18,16,0}, CM_ALL },
    { "Void Gale Locket", SLOT_AMULET,  RARITY_EPIC,       78, 60000, { 5,5,5,5,6,0,18}, CM_ALL },

    /* ── Legendary weapons (D8 boss exclusive, levelReq 99) ── */
    { "Eternity's Edge",  SLOT_WEAPON,  RARITY_LEGENDARY, 100,250000, {110,0,0,0,35,20,0}, CM_WAR },
    { "Eternal Whisper",  SLOT_WEAPON,  RARITY_LEGENDARY, 100,250000, {18,110,0,0,0,0,35}, CM_ROG },
    { "Staff of Ages",    SLOT_WEAPON,  RARITY_LEGENDARY, 100,250000, { 0,0,110,35,0,0,18}, CM_MAG },
    { "Eternal Grace",    SLOT_WEAPON,  RARITY_LEGENDARY, 100,250000, { 0,0,35,110,20,0,0}, CM_PRI },
    /* ── Legendary helmets ── */
    { "Helm of Ages",     SLOT_HELMET,  RARITY_LEGENDARY, 100,160000, {14,0,0,0,38,46,0}, CM_WAR },
    { "Timeless Hood",    SLOT_HELMET,  RARITY_LEGENDARY, 100,160000, { 0,35,0,0,16,0,28}, CM_ROG },
    { "Crown of Aeons",   SLOT_HELMET,  RARITY_LEGENDARY, 100,160000, { 0,0,46,24,9,0,0}, CM_MAG },
    { "Eternal Aureole",  SLOT_HELMET,  RARITY_LEGENDARY, 100,160000, { 0,0,0,38,25,20,0}, CM_PRI },
    /* ── Legendary chests ── */
    { "Armor of Eternity",SLOT_CHEST,   RARITY_LEGENDARY, 100,200000, {22,0,0,0,58,70,0}, CM_WAR },
    { "Timeless Garb",    SLOT_CHEST,   RARITY_LEGENDARY, 100,200000, { 0,54,0,0,24,0,44}, CM_ROG },
    { "Robe of Aeons",    SLOT_CHEST,   RARITY_LEGENDARY, 100,200000, { 0,0,70,36,14,0,0}, CM_MAG },
    { "Eternal Vestment", SLOT_CHEST,   RARITY_LEGENDARY, 100,200000, { 0,0,0,58,38,30,0}, CM_PRI },
    /* ── Legendary legs ── */
    { "Greaves of Ages",  SLOT_LEGS,    RARITY_LEGENDARY, 100,140000, {18,0,0,0,46,56,0}, CM_WAR },
    { "Timeless Legwraps",SLOT_LEGS,    RARITY_LEGENDARY, 100,140000, { 0,44,0,0,20,0,36}, CM_ROG },
    { "Leggings of Aeons",SLOT_LEGS,    RARITY_LEGENDARY, 100,140000, { 0,0,56,28,12,0,0}, CM_MAG },
    { "Eternal Greaves",  SLOT_LEGS,    RARITY_LEGENDARY, 100,140000, { 0,0,0,46,30,24,0}, CM_PRI },
    /* ── Legendary boots ── */
    { "Boots of Eternity",SLOT_BOOTS,   RARITY_LEGENDARY, 100,125000, { 0,0,0,0,24,30,26}, CM_WAR },
    { "Timeless Stride",  SLOT_BOOTS,   RARITY_LEGENDARY, 100,125000, { 0,26,0,0,14,0,40}, CM_ROG },
    { "Steps of Aeons",   SLOT_BOOTS,   RARITY_LEGENDARY, 100,125000, { 0,0,26,18,0,0,30}, CM_MAG },
    { "Eternal Striders", SLOT_BOOTS,   RARITY_LEGENDARY, 100,125000, { 0,0,0,26,20,0,26}, CM_PRI },
    /* ── Legendary jewelry ── */
    { "Eternal Band",     SLOT_RING,    RARITY_LEGENDARY, 100,180000, {15,15,15,15,0,0,0}, CM_ALL },
    { "Eternal Charm",    SLOT_AMULET,  RARITY_LEGENDARY, 100,180000, { 0,0,16,16,22,12,0}, CM_ALL }
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
