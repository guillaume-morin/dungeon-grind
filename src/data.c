/*
 * data.c — All static game data: classes, skills, enemies, dungeons, items.
 *
 * Everything is const and allocated at compile time (zero malloc).
 * The rest of the codebase accesses data exclusively through the
 * data_*() accessor functions at the bottom of this file.
 *
 * CM_ALL (0) means "any class can equip". Non-zero classMask is a
 * bitmask of (1 << classId). Jewelry (rings, amulets) uses CM_ALL;
 * armor and weapons are class-restricted.
 *
 * Item rarity progression is intentionally per-dungeon-tier:
 * Common(Lv1) → Uncommon(Lv8) → Rare(Lv18,32) → Epic(Lv46+) → Legendary(Lv90).
 * Mob drop rates are very low (.0012–.0032); bosses always drop with
 * weighted rarity (see combat.c).
 */
#include "game.h"
#include <stdlib.h>
#include <string.h>

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
        .baseStats       = { 5, 12, 3, 3, 6, 4, 10 },
        .growthPerLevel  = { 1,  3, 0, 0, 1, 1,  3 },
        .baseHp = 90, .hpPerLevel = 9,
        .baseResource = 100, .maxResource = 100, .resourceRegen = 15
    },
    [CLASS_MAGE] = {
        .name = "Mage", .symbol = "M", .resourceName = "Mana",
        .description = "Master of arcane forces, fragile but devastating.",
        .primaryStat = INT_,
        .baseStats       = { 3, 4, 14, 8, 5, 3, 6 },
        .growthPerLevel  = { 0, 1,  4, 2, 1, 0, 1 },
        .baseHp = 100, .hpPerLevel = 7,
        .baseResource = 100, .maxResource = 150, .resourceRegen = 8
    },
    [CLASS_PRIEST] = {
        .name = "Priest", .symbol = "P", .resourceName = "Mana",
        .description = "Vessel of divine power, enduring and relentless.",
        .primaryStat = WIS,
        .baseStats       = { 4, 4, 8, 14, 8, 5, 4 },
        .growthPerLevel  = { 0, 0, 2,  4, 2, 1, 1 },
        .baseHp = 100, .hpPerLevel = 11,
        .baseResource = 120, .maxResource = 150, .resourceRegen = 10
    }
};

static const int SKILL_LEVELS[MAX_SKILL_TIERS] = { 10, 20, 30, 40, 50, 60 };

static const SkillDef SKILLS[NUM_CLASSES][MAX_SKILL_TIERS][2] = {
    [CLASS_WARRIOR] = {
        {
            {.name="Rend", .description="Bleed: 50% DoT/tick 4t", .cooldown=10, .resourceCost=23, .buffTicks=4, .buffDmgPct=50},
            {.name="Victory Rush", .description="150% dmg + heal 15%", .cooldown=8, .resourceCost=30, .dmgMul=1.5f, .healPct=15}
        },
        {
            {.name="Bloodthirst", .description="180% dmg + heal 5%", .cooldown=6, .resourceCost=38, .dmgMul=1.8f, .healPct=5},
            {.name="Execute", .description="400% dmg if enemy<20%", .cooldown=12, .resourceCost=45, .dmgMul=4.0f, .enemyHpBelow=20}
        },
        {
            {.name="Second Wind", .description="Heal 5%/tick 3t if HP<35%", .cooldown=15, .resourceCost=0, .buffTicks=3, .buffHealPct=5, .hpBelow=35},
            {.name="Ignore Pain", .description="Shield 25% maxHP", .cooldown=12, .resourceCost=30, .buffTicks=6, .buffShieldPct=25}
        },
        {
            {.name="Recklessness", .description="+100% crit dmg 4t", .cooldown=18, .resourceCost=45, .buffTicks=4, .buffCritBonus=1.0f},
            {.name="Rampage", .description="4 hits at 80% dmg", .cooldown=10, .resourceCost=60, .dmgMul=0.8f, .numHits=4}
        },
        {
            {.name="Shield Wall", .description="Shield 40% HP if HP<40%", .cooldown=25, .resourceCost=0, .buffTicks=5, .buffShieldPct=40, .hpBelow=40},
            {.name="Berserker Rage", .description="+40% dmg 6t", .cooldown=16, .resourceCost=45, .buffTicks=6, .buffDmgMul=0.4f}
        },
        {
            {.name="Bladestorm", .description="5x100% dmg + immune 1t", .cooldown=22, .resourceCost=75, .dmgMul=1.0f, .numHits=5, .buffTicks=1, .buffImmune=1},
            {.name="Avatar", .description="+30% dmg + shield 20% 8t", .cooldown=25, .resourceCost=60, .buffTicks=8, .buffDmgMul=0.3f, .buffShieldPct=20}
        }
    },
    [CLASS_ROGUE] = {
        {
            {.name="Slice and Dice", .description="+35% dmg 5t", .cooldown=12, .resourceCost=30, .buffTicks=5, .buffDmgMul=0.35f},
            {.name="Deadly Poison", .description="DoT 60%/tick 4t", .cooldown=8, .resourceCost=23, .buffTicks=4, .buffDmgPct=60}
        },
        {
            {.name="Rupture", .description="DoT 80%/tick 5t", .cooldown=10, .resourceCost=38, .buffTicks=5, .buffDmgPct=80},
            {.name="Ghostly Strike", .description="180% dmg +20% dodge 3t", .cooldown=8, .resourceCost=38, .dmgMul=1.8f, .buffTicks=3, .buffDodge=0.2f}
        },
        {
            {.name="Adrenaline Rush", .description="+50% dmg 5t", .cooldown=20, .resourceCost=45, .buffTicks=5, .buffDmgMul=0.5f},
            {.name="Cloak of Shadows", .description="Immune 2t if HP<40%", .cooldown=22, .resourceCost=0, .buffTicks=2, .buffImmune=1, .hpBelow=40}
        },
        {
            {.name="Cold Blood", .description="+150% crit dmg 2t", .cooldown=14, .resourceCost=45, .buffTicks=2, .buffCritBonus=1.5f},
            {.name="Marked for Death", .description="+60% crit dmg 5t", .cooldown=16, .resourceCost=38, .buffTicks=5, .buffCritBonus=0.6f}
        },
        {
            {.name="Shadow Blades", .description="200% no-armor +30% dmg 4t", .cooldown=20, .resourceCost=53, .dmgMul=2.0f, .ignoreArmor=1, .buffTicks=4, .buffDmgMul=0.3f},
            {.name="Crimson Vial", .description="Heal 25%", .cooldown=16, .resourceCost=30, .healPct=25}
        },
        {
            {.name="Shadow Dance", .description="+80% dmg +80% critDmg 4t", .cooldown=22, .resourceCost=60, .buffTicks=4, .buffDmgMul=0.8f, .buffCritBonus=0.8f},
            {.name="Death from Above", .description="500% no-armor + stun 2t", .cooldown=20, .resourceCost=75, .dmgMul=5.0f, .ignoreArmor=1, .stunTicks=2}
        }
    },
    [CLASS_MAGE] = {
        {
            {.name="Pyroblast", .description="300% dmg ignore armor", .cooldown=10, .resourceCost=45, .dmgMul=3.0f, .ignoreArmor=1},
            {.name="Living Bomb", .description="DoT 100%/tick 4t", .cooldown=10, .resourceCost=38, .buffTicks=4, .buffDmgPct=100}
        },
        {
            {.name="Ice Barrier", .description="Shield 30% maxHP", .cooldown=14, .resourceCost=38, .buffTicks=8, .buffShieldPct=30},
            {.name="Blazing Barrier", .description="Shield 20% + DoT 40%/t 6t", .cooldown=14, .resourceCost=38, .buffTicks=6, .buffShieldPct=20, .buffDmgPct=40}
        },
        {
            {.name="Icy Veins", .description="+30% dmg 6t", .cooldown=20, .resourceCost=45, .buffTicks=6, .buffDmgMul=0.3f},
            {.name="Combustion", .description="+100% crit dmg 4t", .cooldown=18, .resourceCost=53, .buffTicks=4, .buffCritBonus=1.0f}
        },
        {
            {.name="Arcane Power", .description="+40% dmg 5t", .cooldown=16, .resourceCost=60, .buffTicks=5, .buffDmgMul=0.4f},
            {.name="Evocation", .description="Heal 30% + mana 50%", .cooldown=20, .resourceCost=0, .healPct=30, .manaPct=50}
        },
        {
            {.name="Ice Block", .description="Immune 3t+heal 20% if<30%", .cooldown=28, .resourceCost=0, .buffTicks=3, .buffImmune=1, .healPct=20, .hpBelow=30},
            {.name="Mirror Image", .description="+25% dodge 5t", .cooldown=18, .resourceCost=45, .buffTicks=5, .buffDodge=0.25f}
        },
        {
            {.name="Meteor", .description="600% no-armor + stun 2t", .cooldown=22, .resourceCost=90, .dmgMul=6.0f, .ignoreArmor=1, .stunTicks=2},
            {.name="Glacial Spike", .description="500% dmg + stun 3t", .cooldown=20, .resourceCost=75, .dmgMul=5.0f, .stunTicks=3}
        }
    },
    [CLASS_PRIEST] = {
        {
            {.name="Shadow Word: Pain", .description="DoT 80%/tick 5t", .cooldown=10, .resourceCost=23, .buffTicks=5, .buffDmgPct=80},
            {.name="PW: Shield", .description="Shield 30% maxHP", .cooldown=12, .resourceCost=30, .buffTicks=8, .buffShieldPct=30}
        },
        {
            {.name="Desperate Prayer", .description="Heal 30% if HP<35%", .cooldown=18, .resourceCost=0, .healPct=30, .hpBelow=35},
            {.name="Vampiric Embrace", .description="Heal 4%/t+DoT 60%/t 5t", .cooldown=14, .resourceCost=38, .buffTicks=5, .buffHealPct=4, .buffDmgPct=60}
        },
        {
            {.name="Penance", .description="3x 150% dmg + heal 5%", .cooldown=10, .resourceCost=45, .dmgMul=1.5f, .numHits=3, .healPct=5},
            {.name="Shadowfiend", .description="DoT 120%/t 5t + mana 30%", .cooldown=18, .resourceCost=0, .buffTicks=5, .buffDmgPct=120, .manaPct=30}
        },
        {
            {.name="Guardian Spirit", .description="Immune 2t+heal30% if<25%", .cooldown=28, .resourceCost=0, .buffTicks=2, .buffImmune=1, .healPct=30, .hpBelow=25},
            {.name="Power Infusion", .description="+35% dmg 6t", .cooldown=18, .resourceCost=45, .buffTicks=6, .buffDmgMul=0.35f}
        },
        {
            {.name="Holy Nova", .description="200% dmg + heal 15%", .cooldown=10, .resourceCost=53, .dmgMul=2.0f, .healPct=15},
            {.name="Void Eruption", .description="+40% dmg 6t", .cooldown=20, .resourceCost=60, .buffTicks=6, .buffDmgMul=0.4f}
        },
        {
            {.name="Divine Hymn", .description="Heal 8%/t 5t + immune", .cooldown=28, .resourceCost=75, .buffTicks=5, .buffHealPct=8, .buffImmune=1},
            {.name="SW: Death", .description="500% dmg if enemy<20%", .cooldown=15, .resourceCost=60, .dmgMul=5.0f, .enemyHpBelow=20}
        }
    }
};

/* ── enemies ──────────────────────────────────────────────────────── */

#define E_GOBLIN_GRUNT    0
#define E_GOBLIN_ARCHER   1
#define E_WOLF            2
#define E_GOBLIN_SHAMAN   3
#define E_ORC_SCOUT       4
#define E_SKELETON         5
#define E_ZOMBIE           6
#define E_GHOUL            7
#define E_BONE_MAGE        8
#define E_CRYPT_WRAITH     9
#define E_DARK_KNIGHT     10
#define E_GARGOYLE        11
#define E_STONE_GOLEM     12
#define E_SHADOW_ARCHER   13
#define E_CURSED_WARDEN   14
#define E_DEMON           15
#define E_SHADOW_LORD     16
#define E_IMP_SWARM       17
#define E_PIT_FIEND       18
#define E_VOID_STALKER    19
#define E_DEATH_KNIGHT    20
#define E_UNDEAD_DRAGON   21
#define E_LICH            22
#define E_BONE_COLOSSUS   23
#define E_SOUL_REAVER     24

#define E_BOSS_GOBLIN_KING 25
#define E_BOSS_ASHVAR      26
#define E_BOSS_BLACKSTONE  27
#define E_BOSS_ABYSSAL     28
#define E_BOSS_LICH_KING   29

#define E_MAGMA_ELEM      30
#define E_FIRE_IMP        31
#define E_LAVA_LURKER     32
#define E_INFERNAL_GUARD  33
#define E_SCORCHED_WYVERN 34
#define E_FROST_REVENANT  35
#define E_ICE_WRAITH      36
#define E_GLACIAL_GOLEM   37
#define E_FROZEN_ARCHER   38
#define E_WINTER_KNIGHT   39
#define E_VOID_SPAWN      40
#define E_ASTRAL_DEVOURER 41
#define E_NULL_WALKER     42
#define E_COSMIC_HORROR   43
#define E_RIFT_GUARDIAN   44
#define E_ETERNAL_SENTINEL 45
#define E_SERAPHIM        46
#define E_TIME_WEAVER     47
#define E_CELESTIAL_DRAGON 48
#define E_PRIMORDIAL_ONE  49

#define E_BOSS_IGNIS      50
#define E_BOSS_CRYOLITH   51
#define E_BOSS_ZHARKUL    52
#define E_BOSS_ETERNAL    53

#define TOTAL_ENEMIES      54

static const EnemyTemplate ENEMIES[TOTAL_ENEMIES] = {
    { "Goblin Grunt",   0,  45,  6, 2, 4,  18,  5, .008f,
      {"    ,   ,",  "   (o . o)", "    > v <",  "    /| |\\",  "     | |" }},
    { "Goblin Archer",  0,  38,  9, 1, 5,  20,  6, .008f,
      {"    ,   ,",  "   (- . -)~}", "    > ^ <",  "    /| |\\",  "     | |" }},
    { "Dire Wolf",      0,  35, 10, 1, 8,  16,  3, .006f,
      {"    /\\_/\\",  "   ( o.o )", "    > ^ <",  "   /|   |\\", "   ~ ~ ~ ~" }},
    { "Goblin Shaman",  0,  30, 12, 1, 3,  24,  8, .01f,
      {"   , ~ ,",   "  (@ . @) *", "   > ~ < |", "   /| |\\",   "    | |" }},
    { "Orc Scout",      0,  70,  8, 4, 4,  28, 10, .012f,
      {"     ___",   "  ,-/o o\\",  "  | (__) |", "  | /||\\ |", "   \\|  |/" }},

    { "Skeleton",       1,  80, 14, 4, 5,  42, 14, .008f,
      {"    _____",  "   / x x \\", "   |  ^  |", "    \\___/",   "    /| |\\" }},
    { "Zombie",         1, 110, 11, 6, 2,  45, 12, .008f,
      {"    _..._",  "   /x   \\~", "   | .  .|", "    \\_  _/",  "     |  |" }},
    { "Ghoul",          1,  90, 18, 3, 7,  50, 16, .01f,
      {"   /^  ^\\",  "  (  ..  )",  "   \\<  >/",  "   /|  |\\",  "  / |  | \\" }},
    { "Bone Mage",      1,  65, 22, 2, 4,  55, 20, .012f,
      {" *  _____  *","   / o o \\", "   | ___ |", "    \\___/",   "     | |" }},
    { "Crypt Wraith",   1,  70, 20, 1, 9,  58, 18, .014f,
      {"  ~(    )~", "  / o  o \\", "  | ==== |", "   \\    /",   "    ~~~~" }},

    { "Dark Knight",    2, 220, 28,18, 4,  95, 35, .01f,
      {"   _===_",   "  /|o  o|\\", "  ||_/\\_||", "  | [==] |", "  |_|  |_|" }},
    { "Gargoyle",       2, 180, 32,14, 6, 100, 38, .01f,
      {"   _/  \\_",  "  / o  o \\", "  \\  /\\  /", "   \\/  \\/",  "   |_/\\_|" }},
    { "Stone Golem",    2, 350, 24,25, 2, 110, 42, .012f,
      {"  [######]", "  |  ..  |",  "  | [==] |",  "  |  ||  |",  "  |__|__|" }},
    { "Shadow Archer",  2, 160, 36, 8, 8, 105, 40, .01f,
      {"    _/\\_",   "  /' .. '\\}","  |  ||  |", "  |  ||  |",  "   \\_||_/" }},
    { "Cursed Warden",  2, 280, 30,20, 3, 115, 45, .014f,
      {"  =[====]=", "  |  ><  |",  "  |_/||\\_|", "  | [  ] |",  "  |_|  |_|" }},

    { "Demon",          3, 400, 48,20, 6, 180, 70, .01f,
      {"   }\\  /{",  "  ( o  o )",  "  |\\ \\/ /|", "  |/ || \\|", "   \\_||_/" }},
    { "Shadow Lord",    3, 360, 55,15, 7, 200, 80, .012f,
      {" ~[======]~","  |  oo  |",  "  | \\  / |", "  |  \\/  |",  "  |______|" }},
    { "Imp Swarm",      3, 250, 42, 8,10, 170, 65, .008f,
      {" /\\ /\\ /\\", " oo oo oo",   " >< >< ><",  " || || ||",   " ~~ ~~ ~~" }},
    { "Pit Fiend",      3, 500, 52,22, 5, 220, 90, .014f,
      {" \\|/  \\|/",  " (  oo  )",   " |  /\\  |",  " | |  | |",  " |_|__|_|" }},
    { "Void Stalker",   3, 320, 60,12, 9, 210, 85, .012f,
      {"  ~.   .~",  " (  @ @  )",  "  \\ === /",   "   |   |",    "   ~   ~" }},

    { "Death Knight",   4, 600, 65,35, 5, 340,130, .012f,
      {"   _[==]_",  "  /|x  x|\\", "  || /\\ ||", "  |[====]|", "  |_|  |_|" }},
    { "Undead Dragon",  4, 900, 72,28, 6, 420,180, .014f,
      {" /\\   _/\\_", "/ \\\\=// \\",  "| | oo | |", " \\_\\ __ /_/","   |____|" }},
    { "Lich King",      4, 750, 80,20, 7, 480,200, .016f,
      {" ~{ /--\\ }~","  |(o)(o)|",  "  | \\__/ |", "  / |  | \\", "   \\|__|/" }},
    { "Bone Colossus",  4,1200, 60,40, 3, 400,160, .014f,
      {" _|[##]|_",  " ||| oo|||",  " ||| /\\|||", " |||_||_|||","_|_|  |_|_" }},
    { "Soul Reaver",    4, 550, 90,15,10, 460,190, .016f,
      {" ~{      }~","  ( @  @ )",  "  |\\-\"\"-/|", "   \\|  |/",  "    ~~~~" }},

    { "Goblin King",    0, 200, 18, 8, 3, 120, 60, .50f,
      {"   _/===\\_",  "  /|o  o|\\", "  | >ww< |",  "  |[KING]|",  "  |_|  |_|" }},
    { "Lord Ashvar",    1, 400, 35,12, 5, 300,120, .50f,
      {" ~{_/==\\_}~", "  | X  X |",  "  | \\==/ |",  "  |[BONE]|",  "  |__|__|" }},
    { "The Warden",     2, 700, 50,30, 4, 500,200, .50f,
      {" =[|/==\\|]=", "  || ** ||",  "  || /\\ ||",  "  |[WARD]|",  "  |_|__|_|" }},
    { "Abyssal Lord",   3,1000, 75,28, 6, 800,350, .50f,
      {" }\\|/==\\|/{","  ( @  @ )",  "  |\\=/\\=/|", "  |[LORD]|",  "  |_|__|_|" }},
    { "The Lich God",   4,2000,110,45, 7,1500,700, .50f,
      {" ~{/====\\}~","  |(O)(O)|",  "  | \\==/ |",  "  |[LICH]|",  "  |_|__|_|" }},

    { "Magma Elemental", 5, 1200, 100, 35,  5,  620, 260, .01f,
      {"   .\\\\  //.",  " ~{ o  o }~",  "  \\\\\\==///",  "   \\\\||//",   "    ~~~~" }},
    { "Fire Imp",        5,  900, 120, 25,  8,  580, 240, .008f,
      {"   ^/  \\^",   " ~( ^^ )~",   "  ( || )",    "   \\/\\/",     "    ||" }},
    { "Lava Lurker",     5, 1500,  95, 45,  4,  700, 300, .01f,
      {"  ___===___",  " /  o o  \\",  " | /\\/\\ |",  "  \\ __ /",    "   |__|" }},
    { "Infernal Guard",  5, 1800, 110, 50,  4,  750, 320, .012f,
      {"  =[|##|]=",  " ~|| oo||~",  "  || /\\||",   "  |[==]|",    "  |_||_|" }},
    { "Scorched Wyvern", 5, 1100, 130, 28,  9,  680, 280, .014f,
      {" ^/\\__/\\^",  "/ \\\\><// \\", "|  \\\\//  |", " \\_\\/\\_/",   "   |__|" }},

    { "Frost Revenant",  6, 1800, 145, 50,  6, 1050, 420, .01f,
      {" ~*{    }*~",  "  ( ** )",    "  |\\  /|",   "   \\||/",     "    **" }},
    { "Ice Wraith",      6, 1400, 170, 35,  9,  980, 400, .01f,
      {"  ~*   *~",   " ~( *  * )~",  "  \\ == /",    "   \\  /",     "    **" }},
    { "Glacial Golem",   6, 2800, 130, 70,  3, 1200, 500, .012f,
      {" *[####]*",   " *|*  *|*",   "  | [] |",    "  |_||_|",    " *|  |*" }},
    { "Frozen Archer",   6, 1500, 180, 40,  8, 1100, 450, .01f,
      {"   *_/\\_*",   " */' ''\\}*",  "  |* ||*|",   "  |  ||  |",  "   \\_||_/" }},
    { "Winter Knight",   6, 2200, 155, 60,  5, 1150, 480, .014f,
      {"  *[**]*",    " */|*  *|\\*", " *||_/\\_||*", "  |*[]*|",    "  |_|  |_|" }},

    { "Void Spawn",      7, 2800, 200, 65,  7, 1700, 680, .01f,
      {"  ~.    .~",  "  ( .  . )",   "  |  ..  |",  "   \\    /",   "    ...." }},
    { "Astral Devourer", 7, 2200, 240, 50, 10, 1600, 650, .01f,
      {" ~.  __  .~",  "  ( @  @ )",   " ~\\ >< /~",   "   \\  /",     "    .." }},
    { "Null Walker",     7, 3200, 210, 80,  5, 1850, 750, .012f,
      {"  .[____].",  " .| .  . |.", " .| [  ] |.", "  |  ||  |",  "  |__|__|" }},
    { "Cosmic Horror",   7, 4500, 190, 90,  4, 2000, 800, .012f,
      {" .}\\    /{.",  ". ( .. ) .",  " .|\\  /|.",   " .|_||_|.",   "   .  ." }},
    { "Rift Guardian",   7, 3000, 260, 70,  8, 1900, 780, .014f,
      {" .[.===.].",  " .| @  @ |.", " .| \\  / |.", "  | .||. |",  "  |_|__|_|" }},

    { "Eternal Sentinel",8, 4500, 280, 90,  6, 2900,1050, .01f,
      {"  +[====]+",  " +|| ** ||+", " +||_/\\_||+", "  |+[]]+|",   "  |_|  |_|" }},
    { "Seraphim",        8, 3500, 340, 70, 10, 2700,1000, .01f,
      {" +\\|/  \\|/+", " +( ^  ^ )+", "  +\\    /+",  "   +\\||/+",   "    +||+" }},
    { "Time Weaver",     8, 3000, 370, 55, 12, 2800,1020, .012f,
      {"+~{ @  @ }~+","  +( oo )+",  " +|\\ \\/ /|+", "  +\\|  |/+",  "   + ~~ +" }},
    { "Celestial Dragon",8, 6000, 300,110,  5, 3200,1200, .014f,
      {"+/\\+=+/\\_+", "+\\\\ \\\\=// /+","|| |oo| ||+", " \\_\\ +/_/+",  "  +|__|+" }},
    { "Primordial One",  8, 5000, 350, 85,  8, 3100,1100, .012f,
      {"+{  ____  }+","+(  O  O  )+","+ | \\__/ | +","+  \\|  |/  +","  + ++++ +" }},

    { "Ignis the Molten",5, 4500, 180, 55,  5, 2400,1000, .50f,
      {"  ~\\\\|//~",   " ~{ o  o }~",  " ~|\\\\||//|~", " ~|[FIRE]|~", "  ~|_||_|~" }},
    { "Cryolith",        6, 8000, 260, 80,  6, 4200,1800, .50f,
      {" **/|==|\\**",  " *|| ** ||*", " *|| /\\ ||*", " *|[FROST]*", " *|_|  |_|*"}},
    { "Zhar'Kul the Void",7,14000, 380,100, 7, 7000,3000, .50f,
      {".~{/====\\}~.",".(  @  @  ).",". | \\../ | .",".|[ VOID ]|.",". |_|  |_| ."}},
    { "The Eternal One",  8,22000, 520,130, 8,12000,5000, .50f,
      {"+~{|====|}~+","+|(  **  )|+","+ | \\++/ | +","+|[ETERNAL]+","+ |_|  |_| +"}}
};

/* ── dungeons ─────────────────────────────────────────────────────── */

static const DungeonDef DUNGEONS[NUM_DUNGEONS] = {
    { "Goblin Warren",      1, "Fetid tunnels crawling with goblins.",
      { E_GOBLIN_GRUNT, E_GOBLIN_ARCHER, E_WOLF, E_GOBLIN_SHAMAN, E_ORC_SCOUT }, 5, E_BOSS_GOBLIN_KING, CP_GREEN },
    { "Crypt of Ashvar",    8, "Ancient tombs where the dead refuse rest.",
      { E_SKELETON, E_ZOMBIE, E_GHOUL, E_BONE_MAGE, E_CRYPT_WRAITH }, 5, E_BOSS_ASHVAR, CP_CYAN },
    { "Blackstone Keep",   18, "A fortress of dark stone and darker purpose.",
      { E_DARK_KNIGHT, E_GARGOYLE, E_STONE_GOLEM, E_SHADOW_ARCHER, E_CURSED_WARDEN }, 5, E_BOSS_BLACKSTONE, CP_YELLOW },
    { "The Abyssal Rift",  32, "A tear in reality seeping with hellfire.",
      { E_DEMON, E_SHADOW_LORD, E_IMP_SWARM, E_PIT_FIEND, E_VOID_STALKER }, 5, E_BOSS_ABYSSAL, CP_MAGENTA },
    { "Throne of the Lich",46, "The seat of undying power. Few return.",
      { E_DEATH_KNIGHT, E_UNDEAD_DRAGON, E_LICH, E_BONE_COLOSSUS, E_SOUL_REAVER }, 5, E_BOSS_LICH_KING, CP_WHITE },
    { "The Molten Core",     55, "Rivers of magma beneath a dying volcano.",
      { E_MAGMA_ELEM, E_FIRE_IMP, E_LAVA_LURKER, E_INFERNAL_GUARD, E_SCORCHED_WYVERN }, 5, E_BOSS_IGNIS, CP_RED },
    { "Frozen Citadel",      66, "An ancient fortress locked in eternal winter.",
      { E_FROST_REVENANT, E_ICE_WRAITH, E_GLACIAL_GOLEM, E_FROZEN_ARCHER, E_WINTER_KNIGHT }, 5, E_BOSS_CRYOLITH, CP_CYAN },
    { "The Void Nexus",      78, "A tear between dimensions, leaking raw chaos.",
      { E_VOID_SPAWN, E_ASTRAL_DEVOURER, E_NULL_WALKER, E_COSMIC_HORROR, E_RIFT_GUARDIAN }, 5, E_BOSS_ZHARKUL, CP_MAGENTA },
    { "Throne of Eternity",  90, "Beyond time itself. The final test of a hero.",
      { E_ETERNAL_SENTINEL, E_SERAPHIM, E_TIME_WEAVER, E_CELESTIAL_DRAGON, E_PRIMORDIAL_ONE }, 5, E_BOSS_ETERNAL, CP_YELLOW }
};

/* ── items ────────────────────────────────────────────────────────── */

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
    { "Molten Greatsword",SLOT_WEAPON,  RARITY_EPIC,       55, 24000, {50,0,0,0,16,8,0}, CM_WAR },
    { "Flameshield Blade",SLOT_WEAPON,  RARITY_EPIC,       55, 24000, {30,7,0,0,18,19,0}, CM_WAR },
    { "Blazestorm Sword", SLOT_WEAPON,  RARITY_EPIC,       55, 24000, {31,10,0,0,7,0,21}, CM_WAR },
    { "Magma Fang",       SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 8,50,0,0,0,0,16}, CM_ROG },
    { "Pyreguard Fang",   SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 7,30,0,0,18,19,0}, CM_ROG },
    { "Cinderwind Shiv",  SLOT_WEAPON,  RARITY_EPIC,       55, 24000, {10,31,0,0,7,0,21}, CM_ROG },
    { "Flameheart Staff", SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,50,16,0,0,8}, CM_MAG },
    { "Fireward Staff",   SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,30,7,18,19,0}, CM_MAG },
    { "Stormfire Wand",   SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,31,10,7,0,21}, CM_MAG },
    { "Ember Scepter",    SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,16,50,8,0,0}, CM_PRI },
    { "Embershield Mace", SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,7,30,18,19,0}, CM_PRI },
    { "Pyrestorm Scepter",SLOT_WEAPON,  RARITY_EPIC,       55, 24000, { 0,0,10,31,7,0,21}, CM_PRI },
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
    { "Molten Warhelm",   SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 5,0,0,0,18,20,0}, CM_WAR },
    { "Magma Sentinel",   SLOT_HELMET,  RARITY_EPIC,       55, 16500, {17,4,0,0,11,11,0}, CM_WAR },
    { "Blazewind Visor",  SLOT_HELMET,  RARITY_EPIC,       55, 16500, {18,6,0,0,4,0,12}, CM_WAR },
    { "Magma Visor",      SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,16,0,0,7,0,13}, CM_ROG },
    { "Magma Aegis Hood", SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 4,14,0,0,9,9,0}, CM_ROG },
    { "Magma Gale Mask",  SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 5,15,0,0,3,0,10}, CM_ROG },
    { "Flamecrown",       SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,0,20,10,4,0,0}, CM_MAG },
    { "Flameguard Crown", SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,0,14,3,9,8,0}, CM_MAG },
    { "Flamegale Circlet",SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,0,14,5,3,0,10}, CM_MAG },
    { "Ember Mitre",      SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,0,0,18,12,9,0}, CM_PRI },
    { "Ember Rampart Coif",SLOT_HELMET, RARITY_EPIC,       55, 16500, { 0,0,4,16,10,9,0}, CM_PRI },
    { "Emberwind Halo",   SLOT_HELMET,  RARITY_EPIC,       55, 16500, { 0,0,5,16,4,0,11}, CM_PRI },
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
    { "Molten Cuirass",   SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 8,0,0,0,28,32,0}, CM_WAR },
    { "Flameguard Plate", SLOT_CHEST,   RARITY_EPIC,       55, 19500, {27,7,0,0,17,17,0}, CM_WAR },
    { "Blazewind Mail",   SLOT_CHEST,   RARITY_EPIC,       55, 19500, {28,9,0,0,7,0,19}, CM_WAR },
    { "Magma Tunic",      SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,24,0,0,10,0,20}, CM_ROG },
    { "Pyreguard Jerkin", SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 5,22,0,0,14,13,0}, CM_ROG },
    { "Cinderwind Vest",  SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 8,23,0,0,4,0,15}, CM_ROG },
    { "Flamewoven Robe",  SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,0,32,16,6,0,0}, CM_MAG },
    { "Fireward Mantle",  SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,0,22,5,14,13,0}, CM_MAG },
    { "Stormfire Robe",   SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,0,23,8,4,0,15}, CM_MAG },
    { "Ember Vestment",   SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,0,0,28,18,14,0}, CM_PRI },
    { "Emberward Garb",   SLOT_CHEST,   RARITY_EPIC,       55, 19500, { 0,0,6,24,15,15,0}, CM_PRI },
    { "Emberstorm Cassock",SLOT_CHEST,  RARITY_EPIC,       55, 19500, { 0,0,8,25,6,0,17}, CM_PRI },
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
    { "Molten Legguards", SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 6,0,0,0,22,26,0}, CM_WAR },
    { "Flameguard Greaves",SLOT_LEGS,   RARITY_EPIC,       55, 15000, {22,5,0,0,14,13,0}, CM_WAR },
    { "Blazewind Greaves",SLOT_LEGS,    RARITY_EPIC,       55, 15000, {23,8,0,0,4,0,15}, CM_WAR },
    { "Magma Leggings",   SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,20,0,0,8,0,16}, CM_ROG },
    { "Pyreguard Pants",  SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 4,18,0,0,11,11,0}, CM_ROG },
    { "Cinderwind Pants", SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 6,18,0,0,5,0,12}, CM_ROG },
    { "Flamewoven Pants", SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,26,12,5,0,0}, CM_MAG },
    { "Fireward Trousers",SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,17,4,11,11,0}, CM_MAG },
    { "Stormfire Kilt",   SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,18,6,4,0,12}, CM_MAG },
    { "Ember Greaves",    SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,0,22,14,11,0}, CM_PRI },
    { "Emberward Cuisses",SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,5,19,12,11,0}, CM_PRI },
    { "Emberstorm Robes", SLOT_LEGS,    RARITY_EPIC,       55, 15000, { 0,0,7,20,4,0,13}, CM_PRI },
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
    { "Molten Sabatons",  SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,0,0,10,14,12}, CM_WAR },
    { "Flameguard Treads",SLOT_BOOTS,   RARITY_EPIC,       55, 13500, {14,4,0,0,9,9,0}, CM_WAR },
    { "Blazewind Boots",  SLOT_BOOTS,   RARITY_EPIC,       55, 13500, {15,5,0,0,3,0,10}, CM_WAR },
    { "Magma Treads",     SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,12,0,0,6,0,18}, CM_ROG },
    { "Pyreguard Boots",  SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 4,14,0,0,9,9,0}, CM_ROG },
    { "Cinderwind Shoes", SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 5,15,0,0,3,0,10}, CM_ROG },
    { "Flamestep Shoes",  SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,12,8,0,0,14}, CM_MAG },
    { "Fireward Slippers",SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,14,3,9,8,0}, CM_MAG },
    { "Stormfire Steps",  SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,14,5,3,0,10}, CM_MAG },
    { "Ember Sandals",    SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,0,12,10,0,12}, CM_PRI },
    { "Emberward Treads", SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,3,14,9,8,0}, CM_PRI },
    { "Emberstorm Steps", SLOT_BOOTS,   RARITY_EPIC,       55, 13500, { 0,0,5,14,3,0,10}, CM_PRI },
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
    { "Molten Band",      SLOT_RING,    RARITY_EPIC,       55, 18000, {12,12,0,0,0,0,6}, CM_ALL },
    { "Molten Seal",      SLOT_RING,    RARITY_EPIC,       55, 18000, { 3,3,0,0,12,9,3}, CM_ALL },
    { "Molten Gale Ring", SLOT_RING,    RARITY_EPIC,       55, 18000, { 5,5,3,3,2,0,10}, CM_ALL },
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
    { "Molten Pendant",   SLOT_AMULET,  RARITY_EPIC,       55, 18000, { 0,0,0,8,14,6,0}, CM_ALL },
    { "Molten Ward Charm",SLOT_AMULET,  RARITY_EPIC,       55, 18000, { 3,3,0,0,11,8,3}, CM_ALL },
    { "Molten Gale Charm",SLOT_AMULET,  RARITY_EPIC,       55, 18000, { 4,4,3,3,2,0,10}, CM_ALL },
    { "Frozen Heart",     SLOT_AMULET,  RARITY_EPIC,       66, 33000, { 0,0,10,10,10,6,0}, CM_ALL },
    { "Frozen Ward Charm",SLOT_AMULET,  RARITY_EPIC,       66, 33000, { 3,3,3,3,14,10,0}, CM_ALL },
    { "Frozen Gale Locket",SLOT_AMULET, RARITY_EPIC,       66, 33000, { 4,4,4,4,5,0,12}, CM_ALL },
    { "Void Locket",      SLOT_AMULET,  RARITY_EPIC,       78, 60000, { 0,0,12,12,18,8,0}, CM_ALL },
    { "Void Aegis Pendant",SLOT_AMULET, RARITY_EPIC,       78, 60000, { 4,4,4,4,18,16,0}, CM_ALL },
    { "Void Gale Locket", SLOT_AMULET,  RARITY_EPIC,       78, 60000, { 5,5,5,5,6,0,18}, CM_ALL },

    /* ── Legendary weapons ── */
    { "Eternity's Edge",  SLOT_WEAPON,  RARITY_LEGENDARY,  90,250000, {110,0,0,0,35,20,0}, CM_WAR },
    { "Eternal Whisper",  SLOT_WEAPON,  RARITY_LEGENDARY,  90,250000, {18,110,0,0,0,0,35}, CM_ROG },
    { "Staff of Ages",    SLOT_WEAPON,  RARITY_LEGENDARY,  90,250000, { 0,0,110,35,0,0,18}, CM_MAG },
    { "Eternal Grace",    SLOT_WEAPON,  RARITY_LEGENDARY,  90,250000, { 0,0,35,110,20,0,0}, CM_PRI },
    /* ── Legendary helmets ── */
    { "Helm of Ages",     SLOT_HELMET,  RARITY_LEGENDARY,  90,160000, {14,0,0,0,38,46,0}, CM_WAR },
    { "Timeless Hood",    SLOT_HELMET,  RARITY_LEGENDARY,  90,160000, { 0,35,0,0,16,0,28}, CM_ROG },
    { "Crown of Aeons",   SLOT_HELMET,  RARITY_LEGENDARY,  90,160000, { 0,0,46,24,9,0,0}, CM_MAG },
    { "Eternal Aureole",  SLOT_HELMET,  RARITY_LEGENDARY,  90,160000, { 0,0,0,38,25,20,0}, CM_PRI },
    /* ── Legendary chests ── */
    { "Armor of Eternity",SLOT_CHEST,   RARITY_LEGENDARY,  90,200000, {22,0,0,0,58,70,0}, CM_WAR },
    { "Timeless Garb",    SLOT_CHEST,   RARITY_LEGENDARY,  90,200000, { 0,54,0,0,24,0,44}, CM_ROG },
    { "Robe of Aeons",    SLOT_CHEST,   RARITY_LEGENDARY,  90,200000, { 0,0,70,36,14,0,0}, CM_MAG },
    { "Eternal Vestment", SLOT_CHEST,   RARITY_LEGENDARY,  90,200000, { 0,0,0,58,38,30,0}, CM_PRI },
    /* ── Legendary legs ── */
    { "Greaves of Ages",  SLOT_LEGS,    RARITY_LEGENDARY,  90,140000, {18,0,0,0,46,56,0}, CM_WAR },
    { "Timeless Legwraps",SLOT_LEGS,    RARITY_LEGENDARY,  90,140000, { 0,44,0,0,20,0,36}, CM_ROG },
    { "Leggings of Aeons",SLOT_LEGS,    RARITY_LEGENDARY,  90,140000, { 0,0,56,28,12,0,0}, CM_MAG },
    { "Eternal Greaves",  SLOT_LEGS,    RARITY_LEGENDARY,  90,140000, { 0,0,0,46,30,24,0}, CM_PRI },
    /* ── Legendary boots ── */
    { "Boots of Eternity",SLOT_BOOTS,   RARITY_LEGENDARY,  90,125000, { 0,0,0,0,24,30,26}, CM_WAR },
    { "Timeless Stride",  SLOT_BOOTS,   RARITY_LEGENDARY,  90,125000, { 0,26,0,0,14,0,40}, CM_ROG },
    { "Steps of Aeons",   SLOT_BOOTS,   RARITY_LEGENDARY,  90,125000, { 0,0,26,18,0,0,30}, CM_MAG },
    { "Eternal Striders", SLOT_BOOTS,   RARITY_LEGENDARY,  90,125000, { 0,0,0,26,20,0,26}, CM_PRI },
    /* ── Legendary jewelry ── */
    { "Eternal Band",     SLOT_RING,    RARITY_LEGENDARY,  90,180000, {15,15,15,15,0,0,0}, CM_ALL },
    { "Eternal Charm",    SLOT_AMULET,  RARITY_LEGENDARY,  90,180000, { 0,0,16,16,22,12,0}, CM_ALL }
};

static const int NUM_ITEMS_TOTAL = sizeof(ITEMS) / sizeof(ITEMS[0]);

/* ── lookup helpers ───────────────────────────────────────────────── */

static const char *STAT_NAMES[]  = { "Strength","Agility","Intellect","Wisdom","Vitality","Defense","Speed" };
static const char *STAT_SHORT[]  = { "STR","AGI","INT","WIS","VIT","DEF","SPD" };
static const char *RARITY_NAMES[]= { "Common","Uncommon","Rare","Epic","Legendary" };
static const int   RARITY_COLS[] = { CP_WHITE, CP_GREEN, CP_BLUE, CP_MAGENTA, CP_YELLOW };
static const char *SLOT_NAMES[]  = { "Weapon","Helmet","Chest","Legs","Boots","Ring","Amulet" };

const ClassDef     *data_class(int id)   { return (id>=0 && id<NUM_CLASSES)  ? &CLASSES[id]  : NULL; }
const DungeonDef   *data_dungeon(int id) { return (id>=0 && id<NUM_DUNGEONS) ? &DUNGEONS[id] : NULL; }
const EnemyTemplate*data_enemy(int id)   { return (id>=0 && id<TOTAL_ENEMIES)? &ENEMIES[id]  : NULL; }
int                 data_num_items(void) { return NUM_ITEMS_TOTAL; }
const ItemDef      *data_item(int id)    { return (id>=0 && id<NUM_ITEMS_TOTAL) ? &ITEMS[id] : NULL; }
int                 data_num_enemies(void) { return TOTAL_ENEMIES; }

const char *data_stat_name(int s)    { return (s>=0 && s<NUM_STATS)    ? STAT_NAMES[s]   : "?"; }
const char *data_stat_short(int s)   { return (s>=0 && s<NUM_STATS)    ? STAT_SHORT[s]   : "?"; }
const char *data_rarity_name(int r)  { return (r>=0 && r<NUM_RARITIES) ? RARITY_NAMES[r] : "?"; }
int         data_rarity_color(int r) { return (r>=0 && r<NUM_RARITIES) ? RARITY_COLS[r]  : CP_WHITE; }
const char *data_slot_name(int s)    { return (s>=0 && s<NUM_SLOTS)    ? SLOT_NAMES[s]   : "?"; }

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
    static const int RAR_MUL[3]          = { 60, 85, 120 };
    int budget = (int)((2.0f + level * 1.1f) * SLOT_MUL[slot] / 100.0f
                       * RAR_MUL[rarity] / 100.0f);
    if (budget < 2) budget = 2;

    out->stats[suf->stat1] = budget * suf->w1 / 100;
    out->stats[suf->stat2] = budget - out->stats[suf->stat1];

    static const int RAR_PRICE[3] = { 1, 3, 8 };
    out->price = budget * (3 + level / 2) * RAR_PRICE[rarity] / 10;
    if (out->price < 1) out->price = 1;
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
    static const int RAR_MUL[3]          = { 60, 85, 120 };
    int budget = (int)((2.0f + level * 1.1f) * SLOT_MUL[slot] / 100.0f
                       * RAR_MUL[rarity] / 100.0f);
    if (budget < 2) budget = 2;

    out->stats[suf->stat1] = budget * suf->w1 / 100;
    out->stats[suf->stat2] = budget - out->stats[suf->stat1];

    static const int RAR_PRICE[3] = { 1, 3, 8 };
    out->price = budget * (3 + level / 2) * RAR_PRICE[rarity] / 10;
    if (out->price < 1) out->price = 1;
    out->price *= 2;  /* shop markup */
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

const SkillDef *data_skill(int classId, int tier, int option) {
    if (classId < 0 || classId >= NUM_CLASSES) return NULL;
    if (tier < 0 || tier >= MAX_SKILL_TIERS) return NULL;
    if (option < 0 || option > 1) return NULL;
    return &SKILLS[classId][tier][option];
}

int data_skill_level(int tier) {
    if (tier < 0 || tier >= MAX_SKILL_TIERS) return 999;
    return SKILL_LEVELS[tier];
}
