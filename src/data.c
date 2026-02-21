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
        .growthPerLevel  = {  3, 1, 0, 0,  2, 2, 1 },
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
        .baseHp = 80, .hpPerLevel = 7,
        .baseResource = 100, .maxResource = 150, .resourceRegen = 8
    },
    [CLASS_PRIEST] = {
        .name = "Priest", .symbol = "P", .resourceName = "Mana",
        .description = "Vessel of divine power, enduring and relentless.",
        .primaryStat = WIS,
        .baseStats       = { 4, 4, 8, 14, 8, 5, 4 },
        .growthPerLevel  = { 0, 0, 2,  4, 2, 1, 0 },
        .baseHp = 100, .hpPerLevel = 11,
        .baseResource = 120, .maxResource = 150, .resourceRegen = 10
    }
};

static const int SKILL_LEVELS[MAX_SKILL_TIERS] = { 10, 20, 30, 40, 50, 60 };

static const SkillDef SKILLS[NUM_CLASSES][MAX_SKILL_TIERS][2] = {
    [CLASS_WARRIOR] = {
        {
            {.name="Rend", .description="Bleed: 50% DoT/tick 4t", .cooldown=10, .resourceCost=15, .buffTicks=4, .buffDmgPct=50},
            {.name="Victory Rush", .description="150% dmg + heal 15%", .cooldown=8, .resourceCost=20, .dmgMul=1.5f, .healPct=15}
        },
        {
            {.name="Bloodthirst", .description="180% dmg + heal 5%", .cooldown=6, .resourceCost=25, .dmgMul=1.8f, .healPct=5},
            {.name="Execute", .description="400% dmg if enemy<20%", .cooldown=4, .resourceCost=30, .dmgMul=4.0f, .enemyHpBelow=20}
        },
        {
            {.name="Second Wind", .description="Heal 5%/tick 3t if HP<35%", .cooldown=15, .resourceCost=0, .buffTicks=3, .buffHealPct=5, .hpBelow=35},
            {.name="Ignore Pain", .description="Shield 25% maxHP", .cooldown=12, .resourceCost=20, .buffTicks=6, .buffShieldPct=25}
        },
        {
            {.name="Recklessness", .description="+100% crit dmg 4t", .cooldown=18, .resourceCost=30, .buffTicks=4, .buffCritBonus=1.0f},
            {.name="Rampage", .description="4 hits at 80% dmg", .cooldown=10, .resourceCost=40, .dmgMul=0.8f, .numHits=4}
        },
        {
            {.name="Shield Wall", .description="Shield 40% HP if HP<40%", .cooldown=25, .resourceCost=0, .buffTicks=5, .buffShieldPct=40, .hpBelow=40},
            {.name="Berserker Rage", .description="+40% dmg 6t", .cooldown=16, .resourceCost=30, .buffTicks=6, .buffDmgMul=0.4f}
        },
        {
            {.name="Bladestorm", .description="5x100% dmg + immune 1t", .cooldown=22, .resourceCost=50, .dmgMul=1.0f, .numHits=5, .buffTicks=1, .buffImmune=1},
            {.name="Avatar", .description="+30% dmg + shield 20% 8t", .cooldown=25, .resourceCost=40, .buffTicks=8, .buffDmgMul=0.3f, .buffShieldPct=20}
        }
    },
    [CLASS_ROGUE] = {
        {
            {.name="Slice and Dice", .description="+35% dmg 5t", .cooldown=12, .resourceCost=20, .buffTicks=5, .buffDmgMul=0.35f},
            {.name="Deadly Poison", .description="DoT 60%/tick 4t", .cooldown=8, .resourceCost=15, .buffTicks=4, .buffDmgPct=60}
        },
        {
            {.name="Rupture", .description="DoT 80%/tick 5t", .cooldown=10, .resourceCost=25, .buffTicks=5, .buffDmgPct=80},
            {.name="Ghostly Strike", .description="180% dmg +20% dodge 3t", .cooldown=8, .resourceCost=25, .dmgMul=1.8f, .buffTicks=3, .buffDodge=0.2f}
        },
        {
            {.name="Adrenaline Rush", .description="+50% dmg 5t", .cooldown=20, .resourceCost=30, .buffTicks=5, .buffDmgMul=0.5f},
            {.name="Cloak of Shadows", .description="Immune 2t if HP<40%", .cooldown=22, .resourceCost=0, .buffTicks=2, .buffImmune=1, .hpBelow=40}
        },
        {
            {.name="Cold Blood", .description="+150% crit dmg 2t", .cooldown=14, .resourceCost=30, .buffTicks=2, .buffCritBonus=1.5f},
            {.name="Marked for Death", .description="+60% crit dmg 5t", .cooldown=16, .resourceCost=25, .buffTicks=5, .buffCritBonus=0.6f}
        },
        {
            {.name="Shadow Blades", .description="200% no-armor +30% dmg 4t", .cooldown=20, .resourceCost=35, .dmgMul=2.0f, .ignoreArmor=1, .buffTicks=4, .buffDmgMul=0.3f},
            {.name="Crimson Vial", .description="Heal 25%", .cooldown=16, .resourceCost=20, .healPct=25}
        },
        {
            {.name="Shadow Dance", .description="+80% dmg +80% critDmg 4t", .cooldown=22, .resourceCost=40, .buffTicks=4, .buffDmgMul=0.8f, .buffCritBonus=0.8f},
            {.name="Death from Above", .description="500% no-armor + stun 2t", .cooldown=20, .resourceCost=50, .dmgMul=5.0f, .ignoreArmor=1, .stunTicks=2}
        }
    },
    [CLASS_MAGE] = {
        {
            {.name="Pyroblast", .description="300% dmg ignore armor", .cooldown=10, .resourceCost=30, .dmgMul=3.0f, .ignoreArmor=1},
            {.name="Living Bomb", .description="DoT 100%/tick 4t", .cooldown=10, .resourceCost=25, .buffTicks=4, .buffDmgPct=100}
        },
        {
            {.name="Ice Barrier", .description="Shield 30% maxHP", .cooldown=14, .resourceCost=25, .buffTicks=8, .buffShieldPct=30},
            {.name="Blazing Barrier", .description="Shield 20% + DoT 40%/t 6t", .cooldown=14, .resourceCost=25, .buffTicks=6, .buffShieldPct=20, .buffDmgPct=40}
        },
        {
            {.name="Icy Veins", .description="+30% dmg 6t", .cooldown=20, .resourceCost=30, .buffTicks=6, .buffDmgMul=0.3f},
            {.name="Combustion", .description="+100% crit dmg 4t", .cooldown=18, .resourceCost=35, .buffTicks=4, .buffCritBonus=1.0f}
        },
        {
            {.name="Arcane Power", .description="+40% dmg 5t", .cooldown=16, .resourceCost=40, .buffTicks=5, .buffDmgMul=0.4f},
            {.name="Evocation", .description="Heal 30% + mana 50%", .cooldown=20, .resourceCost=0, .healPct=30, .manaPct=50}
        },
        {
            {.name="Ice Block", .description="Immune 3t+heal 20% if<30%", .cooldown=28, .resourceCost=0, .buffTicks=3, .buffImmune=1, .healPct=20, .hpBelow=30},
            {.name="Mirror Image", .description="+25% dodge 5t", .cooldown=18, .resourceCost=30, .buffTicks=5, .buffDodge=0.25f}
        },
        {
            {.name="Meteor", .description="600% no-armor + stun 2t", .cooldown=22, .resourceCost=60, .dmgMul=6.0f, .ignoreArmor=1, .stunTicks=2},
            {.name="Glacial Spike", .description="500% dmg + stun 3t", .cooldown=20, .resourceCost=50, .dmgMul=5.0f, .stunTicks=3}
        }
    },
    [CLASS_PRIEST] = {
        {
            {.name="Shadow Word: Pain", .description="DoT 80%/tick 5t", .cooldown=10, .resourceCost=15, .buffTicks=5, .buffDmgPct=80},
            {.name="PW: Shield", .description="Shield 30% maxHP", .cooldown=12, .resourceCost=20, .buffTicks=8, .buffShieldPct=30}
        },
        {
            {.name="Desperate Prayer", .description="Heal 30% if HP<35%", .cooldown=18, .resourceCost=0, .healPct=30, .hpBelow=35},
            {.name="Vampiric Embrace", .description="Heal 4%/t+DoT 60%/t 5t", .cooldown=14, .resourceCost=25, .buffTicks=5, .buffHealPct=4, .buffDmgPct=60}
        },
        {
            {.name="Penance", .description="3x 150% dmg + heal 5%", .cooldown=10, .resourceCost=30, .dmgMul=1.5f, .numHits=3, .healPct=5},
            {.name="Shadowfiend", .description="DoT 120%/t 5t + mana 30%", .cooldown=18, .resourceCost=0, .buffTicks=5, .buffDmgPct=120, .manaPct=30}
        },
        {
            {.name="Guardian Spirit", .description="Immune 2t+heal30% if<25%", .cooldown=28, .resourceCost=0, .buffTicks=2, .buffImmune=1, .healPct=30, .hpBelow=25},
            {.name="Power Infusion", .description="+35% dmg 6t", .cooldown=18, .resourceCost=30, .buffTicks=6, .buffDmgMul=0.35f}
        },
        {
            {.name="Holy Nova", .description="200% dmg + heal 15%", .cooldown=10, .resourceCost=35, .dmgMul=2.0f, .healPct=15},
            {.name="Void Eruption", .description="+40% dmg 6t", .cooldown=20, .resourceCost=40, .buffTicks=6, .buffDmgMul=0.4f}
        },
        {
            {.name="Divine Hymn", .description="Heal 8%/t 5t + immune", .cooldown=28, .resourceCost=50, .buffTicks=5, .buffHealPct=8, .buffImmune=1},
            {.name="SW: Death", .description="500% dmg if enemy<20%", .cooldown=8, .resourceCost=40, .dmgMul=5.0f, .enemyHpBelow=20}
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
    { "Goblin Grunt",   0,  45,  6, 2, 4,  18,  5, .0016f,
      {"    ,   ,",  "   (o . o)", "    > v <",  "    /| |\\",  "     | |" }},
    { "Goblin Archer",  0,  38,  9, 1, 5,  20,  6, .0016f,
      {"    ,   ,",  "   (- . -)~}", "    > ^ <",  "    /| |\\",  "     | |" }},
    { "Dire Wolf",      0,  35, 10, 1, 8,  16,  3, .0012f,
      {"    /\\_/\\",  "   ( o.o )", "    > ^ <",  "   /|   |\\", "   ~ ~ ~ ~" }},
    { "Goblin Shaman",  0,  30, 12, 1, 3,  24,  8, .002f,
      {"   , ~ ,",   "  (@ . @) *", "   > ~ < |", "   /| |\\",   "    | |" }},
    { "Orc Scout",      0,  70,  8, 4, 4,  28, 10, .0024f,
      {"     ___",   "  ,-/o o\\",  "  | (__) |", "  | /||\\ |", "   \\|  |/" }},

    { "Skeleton",       1,  80, 14, 4, 5,  42, 14, .0016f,
      {"    _____",  "   / x x \\", "   |  ^  |", "    \\___/",   "    /| |\\" }},
    { "Zombie",         1, 110, 11, 6, 2,  45, 12, .0016f,
      {"    _..._",  "   /x   \\~", "   | .  .|", "    \\_  _/",  "     |  |" }},
    { "Ghoul",          1,  90, 18, 3, 7,  50, 16, .002f,
      {"   /^  ^\\",  "  (  ..  )",  "   \\<  >/",  "   /|  |\\",  "  / |  | \\" }},
    { "Bone Mage",      1,  65, 22, 2, 4,  55, 20, .0024f,
      {" *  _____  *","   / o o \\", "   | ___ |", "    \\___/",   "     | |" }},
    { "Crypt Wraith",   1,  70, 20, 1, 9,  58, 18, .0028f,
      {"  ~(    )~", "  / o  o \\", "  | ==== |", "   \\    /",   "    ~~~~" }},

    { "Dark Knight",    2, 220, 28,18, 4,  95, 35, .002f,
      {"   _===_",   "  /|o  o|\\", "  ||_/\\_||", "  | [==] |", "  |_|  |_|" }},
    { "Gargoyle",       2, 180, 32,14, 6, 100, 38, .002f,
      {"   _/  \\_",  "  / o  o \\", "  \\  /\\  /", "   \\/  \\/",  "   |_/\\_|" }},
    { "Stone Golem",    2, 350, 24,25, 2, 110, 42, .0024f,
      {"  [######]", "  |  ..  |",  "  | [==] |",  "  |  ||  |",  "  |__|__|" }},
    { "Shadow Archer",  2, 160, 36, 8, 8, 105, 40, .002f,
      {"    _/\\_",   "  /' .. '\\}","  |  ||  |", "  |  ||  |",  "   \\_||_/" }},
    { "Cursed Warden",  2, 280, 30,20, 3, 115, 45, .0028f,
      {"  =[====]=", "  |  ><  |",  "  |_/||\\_|", "  | [  ] |",  "  |_|  |_|" }},

    { "Demon",          3, 400, 48,20, 6, 180, 70, .002f,
      {"   }\\  /{",  "  ( o  o )",  "  |\\ \\/ /|", "  |/ || \\|", "   \\_||_/" }},
    { "Shadow Lord",    3, 360, 55,15, 7, 200, 80, .0024f,
      {" ~[======]~","  |  oo  |",  "  | \\  / |", "  |  \\/  |",  "  |______|" }},
    { "Imp Swarm",      3, 250, 42, 8,10, 170, 65, .0016f,
      {" /\\ /\\ /\\", " oo oo oo",   " >< >< ><",  " || || ||",   " ~~ ~~ ~~" }},
    { "Pit Fiend",      3, 500, 52,22, 5, 220, 90, .0028f,
      {" \\|/  \\|/",  " (  oo  )",   " |  /\\  |",  " | |  | |",  " |_|__|_|" }},
    { "Void Stalker",   3, 320, 60,12, 9, 210, 85, .0024f,
      {"  ~.   .~",  " (  @ @  )",  "  \\ === /",   "   |   |",    "   ~   ~" }},

    { "Death Knight",   4, 600, 65,35, 5, 340,130, .0024f,
      {"   _[==]_",  "  /|x  x|\\", "  || /\\ ||", "  |[====]|", "  |_|  |_|" }},
    { "Undead Dragon",  4, 900, 72,28, 6, 420,180, .0028f,
      {" /\\   _/\\_", "/ \\\\=// \\",  "| | oo | |", " \\_\\ __ /_/","   |____|" }},
    { "Lich King",      4, 750, 80,20, 7, 480,200, .0032f,
      {" ~{ /--\\ }~","  |(o)(o)|",  "  | \\__/ |", "  / |  | \\", "   \\|__|/" }},
    { "Bone Colossus",  4,1200, 60,40, 3, 400,160, .0028f,
      {" _|[##]|_",  " ||| oo|||",  " ||| /\\|||", " |||_||_|||","_|_|  |_|_" }},
    { "Soul Reaver",    4, 550, 90,15,10, 460,190, .0032f,
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

    { "Magma Elemental", 5, 1200, 100, 35,  5,  620, 260, .002f,
      {"   .\\\\  //.",  " ~{ o  o }~",  "  \\\\\\==///",  "   \\\\||//",   "    ~~~~" }},
    { "Fire Imp",        5,  900, 120, 25,  8,  580, 240, .0016f,
      {"   ^/  \\^",   " ~( ^^ )~",   "  ( || )",    "   \\/\\/",     "    ||" }},
    { "Lava Lurker",     5, 1500,  95, 45,  4,  700, 300, .002f,
      {"  ___===___",  " /  o o  \\",  " | /\\/\\ |",  "  \\ __ /",    "   |__|" }},
    { "Infernal Guard",  5, 1800, 110, 50,  4,  750, 320, .0024f,
      {"  =[|##|]=",  " ~|| oo||~",  "  || /\\||",   "  |[==]|",    "  |_||_|" }},
    { "Scorched Wyvern", 5, 1100, 130, 28,  9,  680, 280, .0028f,
      {" ^/\\__/\\^",  "/ \\\\><// \\", "|  \\\\//  |", " \\_\\/\\_/",   "   |__|" }},

    { "Frost Revenant",  6, 1800, 145, 50,  6, 1050, 420, .002f,
      {" ~*{    }*~",  "  ( ** )",    "  |\\  /|",   "   \\||/",     "    **" }},
    { "Ice Wraith",      6, 1400, 170, 35,  9,  980, 400, .002f,
      {"  ~*   *~",   " ~( *  * )~",  "  \\ == /",    "   \\  /",     "    **" }},
    { "Glacial Golem",   6, 2800, 130, 70,  3, 1200, 500, .0024f,
      {" *[####]*",   " *|*  *|*",   "  | [] |",    "  |_||_|",    " *|  |*" }},
    { "Frozen Archer",   6, 1500, 180, 40,  8, 1100, 450, .002f,
      {"   *_/\\_*",   " */' ''\\}*",  "  |* ||*|",   "  |  ||  |",  "   \\_||_/" }},
    { "Winter Knight",   6, 2200, 155, 60,  5, 1150, 480, .0028f,
      {"  *[**]*",    " */|*  *|\\*", " *||_/\\_||*", "  |*[]*|",    "  |_|  |_|" }},

    { "Void Spawn",      7, 2800, 200, 65,  7, 1700, 680, .002f,
      {"  ~.    .~",  "  ( .  . )",   "  |  ..  |",  "   \\    /",   "    ...." }},
    { "Astral Devourer", 7, 2200, 240, 50, 10, 1600, 650, .002f,
      {" ~.  __  .~",  "  ( @  @ )",   " ~\\ >< /~",   "   \\  /",     "    .." }},
    { "Null Walker",     7, 3200, 210, 80,  5, 1850, 750, .0024f,
      {"  .[____].",  " .| .  . |.", " .| [  ] |.", "  |  ||  |",  "  |__|__|" }},
    { "Cosmic Horror",   7, 4500, 190, 90,  4, 2000, 800, .0024f,
      {" .}\\    /{.",  ". ( .. ) .",  " .|\\  /|.",   " .|_||_|.",   "   .  ." }},
    { "Rift Guardian",   7, 3000, 260, 70,  8, 1900, 780, .0028f,
      {" .[.===.].",  " .| @  @ |.", " .| \\  / |.", "  | .||. |",  "  |_|__|_|" }},

    { "Eternal Sentinel",8, 4500, 280, 90,  6, 2900,1050, .002f,
      {"  +[====]+",  " +|| ** ||+", " +||_/\\_||+", "  |+[]]+|",   "  |_|  |_|" }},
    { "Seraphim",        8, 3500, 340, 70, 10, 2700,1000, .002f,
      {" +\\|/  \\|/+", " +( ^  ^ )+", "  +\\    /+",  "   +\\||/+",   "    +||+" }},
    { "Time Weaver",     8, 3000, 370, 55, 12, 2800,1020, .0024f,
      {"+~{ @  @ }~+","  +( oo )+",  " +|\\ \\/ /|+", "  +\\|  |/+",  "   + ~~ +" }},
    { "Celestial Dragon",8, 6000, 300,110,  5, 3200,1200, .0028f,
      {"+/\\+=+/\\_+", "+\\\\ \\\\=// /+","|| |oo| ||+", " \\_\\ +/_/+",  "  +|__|+" }},
    { "Primordial One",  8, 5000, 350, 85,  8, 3100,1100, .0024f,
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

/*                                 slot          rar                lvl   price  STR AGI INT WIS VIT DEF SPD  class */
static const ItemDef ITEMS[] = {
    { "Rusty Sword",      SLOT_WEAPON,  RARITY_COMMON,      1,   15, { 3,0,0,0,0,0,0}, CM_WAR },
    { "Worn Dagger",      SLOT_WEAPON,  RARITY_COMMON,      1,   15, { 0,3,0,0,0,0,0}, CM_ROG },
    { "Gnarled Staff",    SLOT_WEAPON,  RARITY_COMMON,      1,   15, { 0,0,3,0,0,0,0}, CM_MAG },
    { "Wooden Mace",      SLOT_WEAPON,  RARITY_COMMON,      1,   15, { 0,0,0,3,0,0,0}, CM_PRI },
    { "Iron Longsword",   SLOT_WEAPON,  RARITY_UNCOMMON,    8,   80, { 7,0,0,0,2,0,0}, CM_WAR },
    { "Steel Stiletto",   SLOT_WEAPON,  RARITY_UNCOMMON,    8,   80, { 0,7,0,0,0,0,2}, CM_ROG },
    { "Arcane Wand",      SLOT_WEAPON,  RARITY_UNCOMMON,    8,   80, { 0,0,7,2,0,0,0}, CM_MAG },
    { "Blessed Scepter",  SLOT_WEAPON,  RARITY_UNCOMMON,    8,   80, { 0,0,2,7,0,0,0}, CM_PRI },
    { "Blacksteel Blade", SLOT_WEAPON,  RARITY_RARE,       18,  320, {14,4,0,0,3,0,0}, CM_WAR },
    { "Shadow Fang",      SLOT_WEAPON,  RARITY_RARE,       18,  320, { 2,14,0,0,0,0,5}, CM_ROG },
    { "Crystal Focus",    SLOT_WEAPON,  RARITY_RARE,       18,  320, { 0,0,14,5,0,0,2}, CM_MAG },
    { "Holy Relic",       SLOT_WEAPON,  RARITY_RARE,       18,  320, { 0,0,5,14,3,0,0}, CM_PRI },
    { "Demonbane",        SLOT_WEAPON,  RARITY_RARE,       32, 1200, {24,8,0,0,6,4,0}, CM_WAR },
    { "Nightfall",        SLOT_WEAPON,  RARITY_RARE,       32, 1200, { 5,24,0,0,0,0,10}, CM_ROG },
    { "Voidstaff",        SLOT_WEAPON,  RARITY_RARE,       32, 1200, { 0,0,24,10,0,0,4}, CM_MAG },
    { "Soulmender",       SLOT_WEAPON,  RARITY_RARE,       32, 1200, { 0,0,10,24,6,0,0}, CM_PRI },
    { "Ashbringer",       SLOT_WEAPON,  RARITY_EPIC,       46, 5000, {40,12,0,0,10,8,0}, CM_WAR },
    { "Death's Whisper",  SLOT_WEAPON,  RARITY_EPIC,       46, 5000, {10,40,0,0,0,0,15}, CM_ROG },
    { "Staff of Eternity",SLOT_WEAPON,  RARITY_EPIC,       46, 5000, { 0,0,40,15,0,0,8}, CM_MAG },
    { "Benediction",      SLOT_WEAPON,  RARITY_EPIC,       46, 5000, { 0,0,15,40,10,0,0}, CM_PRI },

    { "Iron Helm",        SLOT_HELMET,  RARITY_COMMON,      1,   10, { 0,0,0,0,2,1,0}, CM_WAR },
    { "Leather Hood",     SLOT_HELMET,  RARITY_COMMON,      1,   10, { 0,1,0,0,0,0,1}, CM_ROG },
    { "Cloth Cap",        SLOT_HELMET,  RARITY_COMMON,      1,   10, { 0,0,1,1,0,0,0}, CM_MAG },
    { "Linen Coif",       SLOT_HELMET,  RARITY_COMMON,      1,   10, { 0,0,0,1,1,0,0}, CM_PRI },
    { "Steel Greathelm",  SLOT_HELMET,  RARITY_UNCOMMON,    8,   55, { 1,0,0,0,4,3,0}, CM_WAR },
    { "Shadow Cowl",      SLOT_HELMET,  RARITY_UNCOMMON,    8,   55, { 0,3,0,0,1,0,2}, CM_ROG },
    { "Wizard Hat",       SLOT_HELMET,  RARITY_UNCOMMON,    8,   55, { 0,0,4,2,0,0,0}, CM_MAG },
    { "Blessed Hood",     SLOT_HELMET,  RARITY_UNCOMMON,    8,   55, { 0,0,0,3,2,1,0}, CM_PRI },
    { "Darksteel Visor",  SLOT_HELMET,  RARITY_RARE,       18,  220, { 3,0,0,0,8,6,0}, CM_WAR },
    { "Nightstalker Mask",SLOT_HELMET,  RARITY_RARE,       18,  220, { 0,6,0,0,2,0,5}, CM_ROG },
    { "Arcane Circlet",   SLOT_HELMET,  RARITY_RARE,       18,  220, { 0,0,8,4,2,0,0}, CM_MAG },
    { "Holy Mitre",       SLOT_HELMET,  RARITY_RARE,       18,  220, { 0,0,0,7,4,3,0}, CM_PRI },
    { "Abyssal Warhelm",  SLOT_HELMET,  RARITY_RARE,       32,  850, { 5,0,0,0,14,10,0}, CM_WAR },
    { "Veil of Shadows",  SLOT_HELMET,  RARITY_RARE,       32,  850, { 0,10,0,0,4,0,8}, CM_ROG },
    { "Void Diadem",      SLOT_HELMET,  RARITY_RARE,       32,  850, { 0,0,14,6,3,0,0}, CM_MAG },
    { "Seraph's Halo",    SLOT_HELMET,  RARITY_RARE,       32,  850, { 0,0,0,12,8,5,0}, CM_PRI },
    { "Crown of Ruin",    SLOT_HELMET,  RARITY_EPIC,       46, 3500, { 8,0,0,0,22,16,0}, CM_WAR },
    { "Phantom Crown",    SLOT_HELMET,  RARITY_EPIC,       46, 3500, { 0,16,0,0,6,0,14}, CM_ROG },
    { "Crown of Stars",   SLOT_HELMET,  RARITY_EPIC,       46, 3500, { 0,0,22,10,5,0,0}, CM_MAG },
    { "Divine Crown",     SLOT_HELMET,  RARITY_EPIC,       46, 3500, { 0,0,0,18,12,8,0}, CM_PRI },

    { "Rusty Chainmail",  SLOT_CHEST,   RARITY_COMMON,      1,   12, { 0,0,0,0,1,2,0}, CM_WAR },
    { "Leather Vest",     SLOT_CHEST,   RARITY_COMMON,      1,   12, { 0,2,0,0,0,0,1}, CM_ROG },
    { "Tattered Robe",    SLOT_CHEST,   RARITY_COMMON,      1,   12, { 0,0,2,1,0,0,0}, CM_MAG },
    { "Linen Vestment",   SLOT_CHEST,   RARITY_COMMON,      1,   12, { 0,0,0,1,1,1,0}, CM_PRI },
    { "Iron Breastplate", SLOT_CHEST,   RARITY_UNCOMMON,    8,   65, { 1,0,0,0,4,5,0}, CM_WAR },
    { "Studded Jerkin",   SLOT_CHEST,   RARITY_UNCOMMON,    8,   65, { 0,4,0,0,1,0,3}, CM_ROG },
    { "Silk Robe",        SLOT_CHEST,   RARITY_UNCOMMON,    8,   65, { 0,0,5,3,0,0,0}, CM_MAG },
    { "Blessed Surcoat",  SLOT_CHEST,   RARITY_UNCOMMON,    8,   65, { 0,0,0,4,3,2,0}, CM_PRI },
    { "Obsidian Plate",   SLOT_CHEST,   RARITY_RARE,       18,  260, { 3,0,0,0,8,10,0}, CM_WAR },
    { "Shadow Tunic",     SLOT_CHEST,   RARITY_RARE,       18,  260, { 0,8,0,0,3,0,6}, CM_ROG },
    { "Arcane Vestment",  SLOT_CHEST,   RARITY_RARE,       18,  260, { 0,0,10,5,2,0,0}, CM_MAG },
    { "Holy Raiment",     SLOT_CHEST,   RARITY_RARE,       18,  260, { 0,0,0,8,6,4,0}, CM_PRI },
    { "Dreadforge Armor", SLOT_CHEST,   RARITY_RARE,       32,  950, { 6,0,0,0,14,16,0}, CM_WAR },
    { "Nightweave Vest",  SLOT_CHEST,   RARITY_RARE,       32,  950, { 0,12,0,0,5,0,10}, CM_ROG },
    { "Voidweave Robe",   SLOT_CHEST,   RARITY_RARE,       32,  950, { 0,0,16,8,3,0,0}, CM_MAG },
    { "Seraph's Garb",    SLOT_CHEST,   RARITY_RARE,       32,  950, { 0,0,0,14,10,7,0}, CM_PRI },
    { "Aegis of Valor",   SLOT_CHEST,   RARITY_EPIC,       46, 4000, {10,0,0,0,22,28,0}, CM_WAR },
    { "Phantom Shroud",   SLOT_CHEST,   RARITY_EPIC,       46, 4000, { 0,20,0,0,8,0,16}, CM_ROG },
    { "Robe of Eternity", SLOT_CHEST,   RARITY_EPIC,       46, 4000, { 0,0,28,14,5,0,0}, CM_MAG },
    { "Divine Regalia",   SLOT_CHEST,   RARITY_EPIC,       46, 4000, { 0,0,0,22,14,10,0}, CM_PRI },

    { "Chain Leggings",   SLOT_LEGS,    RARITY_COMMON,      1,   10, { 0,0,0,0,2,1,0}, CM_WAR },
    { "Leather Breeches", SLOT_LEGS,    RARITY_COMMON,      1,   10, { 0,1,0,0,0,0,1}, CM_ROG },
    { "Cloth Pants",      SLOT_LEGS,    RARITY_COMMON,      1,   10, { 0,0,1,1,0,0,0}, CM_MAG },
    { "Linen Skirt",      SLOT_LEGS,    RARITY_COMMON,      1,   10, { 0,0,0,1,1,0,0}, CM_PRI },
    { "Iron Greaves",     SLOT_LEGS,    RARITY_UNCOMMON,    8,   50, { 1,0,0,0,3,4,0}, CM_WAR },
    { "Studded Leggings", SLOT_LEGS,    RARITY_UNCOMMON,    8,   50, { 0,3,0,0,1,0,3}, CM_ROG },
    { "Silk Trousers",    SLOT_LEGS,    RARITY_UNCOMMON,    8,   50, { 0,0,4,2,0,0,1}, CM_MAG },
    { "Blessed Greaves",  SLOT_LEGS,    RARITY_UNCOMMON,    8,   50, { 0,0,0,3,2,2,0}, CM_PRI },
    { "Plated Legguards", SLOT_LEGS,    RARITY_RARE,       18,  200, { 3,0,0,0,6,8,0}, CM_WAR },
    { "Shadow Leggings",  SLOT_LEGS,    RARITY_RARE,       18,  200, { 0,6,0,0,2,0,5}, CM_ROG },
    { "Arcane Leggings",  SLOT_LEGS,    RARITY_RARE,       18,  200, { 0,0,8,3,0,0,2}, CM_MAG },
    { "Holy Leggings",    SLOT_LEGS,    RARITY_RARE,       18,  200, { 0,0,0,6,4,3,0}, CM_PRI },
    { "Abyssal Tassets",  SLOT_LEGS,    RARITY_RARE,       32,  800, { 5,0,0,0,10,14,0}, CM_WAR },
    { "Nightweave Pants", SLOT_LEGS,    RARITY_RARE,       32,  800, { 0,10,0,0,3,0,8}, CM_ROG },
    { "Voidweave Pants",  SLOT_LEGS,    RARITY_RARE,       32,  800, { 0,0,12,6,0,0,3}, CM_MAG },
    { "Seraph's Cuisses", SLOT_LEGS,    RARITY_RARE,       32,  800, { 0,0,0,10,8,5,0}, CM_PRI },
    { "Bonecage Greaves", SLOT_LEGS,    RARITY_EPIC,       46, 3200, { 8,0,0,0,18,24,0}, CM_WAR },
    { "Phantom Legwraps", SLOT_LEGS,    RARITY_EPIC,       46, 3200, { 0,16,0,0,4,0,14}, CM_ROG },
    { "Leggings of Stars",SLOT_LEGS,    RARITY_EPIC,       46, 3200, { 0,0,20,10,0,0,5}, CM_MAG },
    { "Divine Greaves",   SLOT_LEGS,    RARITY_EPIC,       46, 3200, { 0,0,0,16,12,8,0}, CM_PRI },

    { "Iron Boots",       SLOT_BOOTS,   RARITY_COMMON,      1,    8, { 0,0,0,0,0,1,1}, CM_WAR },
    { "Soft Boots",       SLOT_BOOTS,   RARITY_COMMON,      1,    8, { 0,1,0,0,0,0,2}, CM_ROG },
    { "Cloth Slippers",   SLOT_BOOTS,   RARITY_COMMON,      1,    8, { 0,0,1,0,0,0,1}, CM_MAG },
    { "Sandals",          SLOT_BOOTS,   RARITY_COMMON,      1,    8, { 0,0,0,1,0,0,1}, CM_PRI },
    { "Steel Sabatons",   SLOT_BOOTS,   RARITY_UNCOMMON,    8,   45, { 0,0,0,0,1,3,2}, CM_WAR },
    { "Leather Boots",    SLOT_BOOTS,   RARITY_UNCOMMON,    8,   45, { 0,2,0,0,0,0,4}, CM_ROG },
    { "Silk Slippers",    SLOT_BOOTS,   RARITY_UNCOMMON,    8,   45, { 0,0,2,1,0,0,3}, CM_MAG },
    { "Blessed Sandals",  SLOT_BOOTS,   RARITY_UNCOMMON,    8,   45, { 0,0,0,2,1,0,2}, CM_PRI },
    { "Darksteel Treads", SLOT_BOOTS,   RARITY_RARE,       18,  180, { 0,0,0,0,3,5,4}, CM_WAR },
    { "Shadowstep Boots", SLOT_BOOTS,   RARITY_RARE,       18,  180, { 0,4,0,0,2,0,7}, CM_ROG },
    { "Arcane Treads",    SLOT_BOOTS,   RARITY_RARE,       18,  180, { 0,0,4,2,0,0,5}, CM_MAG },
    { "Holy Treads",      SLOT_BOOTS,   RARITY_RARE,       18,  180, { 0,0,0,4,3,0,4}, CM_PRI },
    { "Warboots of Ruin", SLOT_BOOTS,   RARITY_RARE,       32,  750, { 0,0,0,0,5,8,6}, CM_WAR },
    { "Nightwalkers",     SLOT_BOOTS,   RARITY_RARE,       32,  750, { 0,6,0,0,3,0,12}, CM_ROG },
    { "Voidwalkers",      SLOT_BOOTS,   RARITY_RARE,       32,  750, { 0,0,6,4,0,0,8}, CM_MAG },
    { "Seraph's Steps",   SLOT_BOOTS,   RARITY_RARE,       32,  750, { 0,0,0,6,5,0,6}, CM_PRI },
    { "Titanstep Boots",  SLOT_BOOTS,   RARITY_EPIC,       46, 3000, { 0,0,0,0,8,12,10}, CM_WAR },
    { "Phantom Stride",   SLOT_BOOTS,   RARITY_EPIC,       46, 3000, { 0,10,0,0,4,0,20}, CM_ROG },
    { "Steps of Eternity",SLOT_BOOTS,   RARITY_EPIC,       46, 3000, { 0,0,10,6,0,0,14}, CM_MAG },
    { "Divine Striders",  SLOT_BOOTS,   RARITY_EPIC,       46, 3000, { 0,0,0,10,8,0,10}, CM_PRI },

    { "Copper Band",      SLOT_RING,    RARITY_COMMON,      1,   12, { 1,1,0,0,0,0,0}, CM_ALL },
    { "Silver Ring",      SLOT_RING,    RARITY_UNCOMMON,    8,   60, { 3,3,0,0,0,0,0}, CM_ALL },
    { "Signet of Might",  SLOT_RING,    RARITY_RARE,       18,  240, { 6,0,0,0,4,3,0}, CM_ALL },
    { "Band of Suffering",SLOT_RING,    RARITY_RARE,       32,  900, { 8,8,0,0,0,0,5}, CM_ALL },
    { "Ring of the Lich", SLOT_RING,    RARITY_EPIC,       46, 3800, { 0,0,15,15,8,0,0}, CM_ALL },

    { "Bone Charm",       SLOT_AMULET,  RARITY_COMMON,      1,   12, { 0,0,0,0,2,0,0}, CM_ALL },
    { "Jade Pendant",     SLOT_AMULET,  RARITY_UNCOMMON,    8,   60, { 0,0,3,3,0,0,0}, CM_ALL },
    { "Bloodstone Amulet",SLOT_AMULET,  RARITY_RARE,       18,  240, { 0,0,0,4,6,3,0}, CM_ALL },
    { "Eye of Shadow",    SLOT_AMULET,  RARITY_RARE,       32,  900, { 0,0,10,8,0,0,3}, CM_ALL },
    { "Heart of Eternity",SLOT_AMULET,  RARITY_EPIC,       46, 3800, { 0,0,0,12,15,8,0}, CM_ALL },

    { "Molten Greatsword",SLOT_WEAPON,  RARITY_EPIC,       55, 8000, {50,0,0,0,16,8,0}, CM_WAR },
    { "Magma Fang",       SLOT_WEAPON,  RARITY_EPIC,       55, 8000, { 8,50,0,0,0,0,16}, CM_ROG },
    { "Flameheart Staff", SLOT_WEAPON,  RARITY_EPIC,       55, 8000, { 0,0,50,16,0,0,8}, CM_MAG },
    { "Ember Scepter",    SLOT_WEAPON,  RARITY_EPIC,       55, 8000, { 0,0,16,50,8,0,0}, CM_PRI },
    { "Frostbite Cleaver",SLOT_WEAPON,  RARITY_EPIC,       66,15000, {65,0,0,0,20,12,0}, CM_WAR },
    { "Icicle Shiv",      SLOT_WEAPON,  RARITY_EPIC,       66,15000, {10,65,0,0,0,0,20}, CM_ROG },
    { "Frozen Scepter",   SLOT_WEAPON,  RARITY_EPIC,       66,15000, { 0,0,65,20,0,0,10}, CM_MAG },
    { "Glacial Wand",     SLOT_WEAPON,  RARITY_EPIC,       66,15000, { 0,0,20,65,12,0,0}, CM_PRI },
    { "Void Reaver",      SLOT_WEAPON,  RARITY_EPIC,       78,28000, {85,0,0,0,28,16,0}, CM_WAR },
    { "Null Blade",       SLOT_WEAPON,  RARITY_EPIC,       78,28000, {14,85,0,0,0,0,28}, CM_ROG },
    { "Void Orb",         SLOT_WEAPON,  RARITY_EPIC,       78,28000, { 0,0,85,28,0,0,14}, CM_MAG },
    { "Void Censer",      SLOT_WEAPON,  RARITY_EPIC,       78,28000, { 0,0,28,85,16,0,0}, CM_PRI },
    { "Eternity's Edge",  SLOT_WEAPON,  RARITY_LEGENDARY,  90,50000, {110,0,0,0,35,20,0}, CM_WAR },
    { "Eternal Whisper",  SLOT_WEAPON,  RARITY_LEGENDARY,  90,50000, {18,110,0,0,0,0,35}, CM_ROG },
    { "Staff of Ages",    SLOT_WEAPON,  RARITY_LEGENDARY,  90,50000, { 0,0,110,35,0,0,18}, CM_MAG },
    { "Eternal Grace",    SLOT_WEAPON,  RARITY_LEGENDARY,  90,50000, { 0,0,35,110,20,0,0}, CM_PRI },

    { "Molten Warhelm",   SLOT_HELMET,  RARITY_EPIC,       55, 5500, { 5,0,0,0,18,20,0}, CM_WAR },
    { "Magma Visor",      SLOT_HELMET,  RARITY_EPIC,       55, 5500, { 0,16,0,0,7,0,13}, CM_ROG },
    { "Flamecrown",       SLOT_HELMET,  RARITY_EPIC,       55, 5500, { 0,0,20,10,4,0,0}, CM_MAG },
    { "Ember Mitre",      SLOT_HELMET,  RARITY_EPIC,       55, 5500, { 0,0,0,18,12,9,0}, CM_PRI },
    { "Frostguard Helm",  SLOT_HELMET,  RARITY_EPIC,       66,10000, { 8,0,0,0,24,28,0}, CM_WAR },
    { "Icestalker Hood",  SLOT_HELMET,  RARITY_EPIC,       66,10000, { 0,20,0,0,9,0,17}, CM_ROG },
    { "Frostweave Crown", SLOT_HELMET,  RARITY_EPIC,       66,10000, { 0,0,28,14,5,0,0}, CM_MAG },
    { "Glacial Halo",     SLOT_HELMET,  RARITY_EPIC,       66,10000, { 0,0,0,24,16,12,0}, CM_PRI },
    { "Voidforged Visor", SLOT_HELMET,  RARITY_EPIC,       78,18000, {10,0,0,0,30,36,0}, CM_WAR },
    { "Null Mask",        SLOT_HELMET,  RARITY_EPIC,       78,18000, { 0,28,0,0,12,0,22}, CM_ROG },
    { "Rift Diadem",      SLOT_HELMET,  RARITY_EPIC,       78,18000, { 0,0,36,18,7,0,0}, CM_MAG },
    { "Void Circlet",     SLOT_HELMET,  RARITY_EPIC,       78,18000, { 0,0,0,30,20,16,0}, CM_PRI },
    { "Helm of Ages",     SLOT_HELMET,  RARITY_LEGENDARY,  90,32000, {14,0,0,0,38,46,0}, CM_WAR },
    { "Timeless Hood",    SLOT_HELMET,  RARITY_LEGENDARY,  90,32000, { 0,35,0,0,16,0,28}, CM_ROG },
    { "Crown of Aeons",   SLOT_HELMET,  RARITY_LEGENDARY,  90,32000, { 0,0,46,24,9,0,0}, CM_MAG },
    { "Eternal Aureole",  SLOT_HELMET,  RARITY_LEGENDARY,  90,32000, { 0,0,0,38,25,20,0}, CM_PRI },

    { "Molten Cuirass",   SLOT_CHEST,   RARITY_EPIC,       55, 6500, { 8,0,0,0,28,32,0}, CM_WAR },
    { "Magma Tunic",      SLOT_CHEST,   RARITY_EPIC,       55, 6500, { 0,24,0,0,10,0,20}, CM_ROG },
    { "Flamewoven Robe",  SLOT_CHEST,   RARITY_EPIC,       55, 6500, { 0,0,32,16,6,0,0}, CM_MAG },
    { "Ember Vestment",   SLOT_CHEST,   RARITY_EPIC,       55, 6500, { 0,0,0,28,18,14,0}, CM_PRI },
    { "Frostplate Armor", SLOT_CHEST,   RARITY_EPIC,       66,12000, {12,0,0,0,36,42,0}, CM_WAR },
    { "Iceweave Jerkin",  SLOT_CHEST,   RARITY_EPIC,       66,12000, { 0,32,0,0,14,0,26}, CM_ROG },
    { "Frozen Robe",      SLOT_CHEST,   RARITY_EPIC,       66,12000, { 0,0,42,22,8,0,0}, CM_MAG },
    { "Glacial Raiment",  SLOT_CHEST,   RARITY_EPIC,       66,12000, { 0,0,0,36,24,18,0}, CM_PRI },
    { "Voidplate",        SLOT_CHEST,   RARITY_EPIC,       78,22000, {16,0,0,0,46,55,0}, CM_WAR },
    { "Null Shroud",      SLOT_CHEST,   RARITY_EPIC,       78,22000, { 0,42,0,0,18,0,34}, CM_ROG },
    { "Rift Robe",        SLOT_CHEST,   RARITY_EPIC,       78,22000, { 0,0,55,28,10,0,0}, CM_MAG },
    { "Void Regalia",     SLOT_CHEST,   RARITY_EPIC,       78,22000, { 0,0,0,46,30,24,0}, CM_PRI },
    { "Armor of Eternity",SLOT_CHEST,   RARITY_LEGENDARY,  90,40000, {22,0,0,0,58,70,0}, CM_WAR },
    { "Timeless Garb",    SLOT_CHEST,   RARITY_LEGENDARY,  90,40000, { 0,54,0,0,24,0,44}, CM_ROG },
    { "Robe of Aeons",    SLOT_CHEST,   RARITY_LEGENDARY,  90,40000, { 0,0,70,36,14,0,0}, CM_MAG },
    { "Eternal Vestment", SLOT_CHEST,   RARITY_LEGENDARY,  90,40000, { 0,0,0,58,38,30,0}, CM_PRI },

    { "Molten Legguards", SLOT_LEGS,    RARITY_EPIC,       55, 5000, { 6,0,0,0,22,26,0}, CM_WAR },
    { "Magma Leggings",   SLOT_LEGS,    RARITY_EPIC,       55, 5000, { 0,20,0,0,8,0,16}, CM_ROG },
    { "Flamewoven Pants", SLOT_LEGS,    RARITY_EPIC,       55, 5000, { 0,0,26,12,5,0,0}, CM_MAG },
    { "Ember Greaves",    SLOT_LEGS,    RARITY_EPIC,       55, 5000, { 0,0,0,22,14,11,0}, CM_PRI },
    { "Frostguard Greaves",SLOT_LEGS,   RARITY_EPIC,       66, 9000, {10,0,0,0,28,34,0}, CM_WAR },
    { "Icestalker Pants", SLOT_LEGS,    RARITY_EPIC,       66, 9000, { 0,26,0,0,12,0,20}, CM_ROG },
    { "Frostweave Legs",  SLOT_LEGS,    RARITY_EPIC,       66, 9000, { 0,0,34,18,6,0,0}, CM_MAG },
    { "Glacial Cuisses",  SLOT_LEGS,    RARITY_EPIC,       66, 9000, { 0,0,0,28,20,14,0}, CM_PRI },
    { "Voidforged Greaves",SLOT_LEGS,   RARITY_EPIC,       78,16000, {12,0,0,0,36,44,0}, CM_WAR },
    { "Null Legwraps",    SLOT_LEGS,    RARITY_EPIC,       78,16000, { 0,34,0,0,14,0,28}, CM_ROG },
    { "Rift Leggings",    SLOT_LEGS,    RARITY_EPIC,       78,16000, { 0,0,44,22,8,0,0}, CM_MAG },
    { "Void Greaves",     SLOT_LEGS,    RARITY_EPIC,       78,16000, { 0,0,0,36,24,20,0}, CM_PRI },
    { "Greaves of Ages",  SLOT_LEGS,    RARITY_LEGENDARY,  90,28000, {18,0,0,0,46,56,0}, CM_WAR },
    { "Timeless Legwraps",SLOT_LEGS,    RARITY_LEGENDARY,  90,28000, { 0,44,0,0,20,0,36}, CM_ROG },
    { "Leggings of Aeons",SLOT_LEGS,    RARITY_LEGENDARY,  90,28000, { 0,0,56,28,12,0,0}, CM_MAG },
    { "Eternal Greaves",  SLOT_LEGS,    RARITY_LEGENDARY,  90,28000, { 0,0,0,46,30,24,0}, CM_PRI },

    { "Molten Sabatons",  SLOT_BOOTS,   RARITY_EPIC,       55, 4500, { 0,0,0,0,10,14,12}, CM_WAR },
    { "Magma Treads",     SLOT_BOOTS,   RARITY_EPIC,       55, 4500, { 0,12,0,0,6,0,18}, CM_ROG },
    { "Flamestep Shoes",  SLOT_BOOTS,   RARITY_EPIC,       55, 4500, { 0,0,12,8,0,0,14}, CM_MAG },
    { "Ember Sandals",    SLOT_BOOTS,   RARITY_EPIC,       55, 4500, { 0,0,0,12,10,0,12}, CM_PRI },
    { "Frostguard Boots", SLOT_BOOTS,   RARITY_EPIC,       66, 8000, { 0,0,0,0,14,18,16}, CM_WAR },
    { "Icestalker Boots", SLOT_BOOTS,   RARITY_EPIC,       66, 8000, { 0,16,0,0,8,0,24}, CM_ROG },
    { "Frostweave Shoes", SLOT_BOOTS,   RARITY_EPIC,       66, 8000, { 0,0,16,10,0,0,18}, CM_MAG },
    { "Glacial Sandals",  SLOT_BOOTS,   RARITY_EPIC,       66, 8000, { 0,0,0,16,12,0,16}, CM_PRI },
    { "Voidforged Boots", SLOT_BOOTS,   RARITY_EPIC,       78,14000, { 0,0,0,0,18,24,20}, CM_WAR },
    { "Null Treads",      SLOT_BOOTS,   RARITY_EPIC,       78,14000, { 0,20,0,0,10,0,32}, CM_ROG },
    { "Rift Walkers",     SLOT_BOOTS,   RARITY_EPIC,       78,14000, { 0,0,20,14,0,0,24}, CM_MAG },
    { "Void Striders",    SLOT_BOOTS,   RARITY_EPIC,       78,14000, { 0,0,0,20,16,0,20}, CM_PRI },
    { "Boots of Eternity",SLOT_BOOTS,   RARITY_LEGENDARY,  90,25000, { 0,0,0,0,24,30,26}, CM_WAR },
    { "Timeless Stride",  SLOT_BOOTS,   RARITY_LEGENDARY,  90,25000, { 0,26,0,0,14,0,40}, CM_ROG },
    { "Steps of Aeons",   SLOT_BOOTS,   RARITY_LEGENDARY,  90,25000, { 0,0,26,18,0,0,30}, CM_MAG },
    { "Eternal Striders", SLOT_BOOTS,   RARITY_LEGENDARY,  90,25000, { 0,0,0,26,20,0,26}, CM_PRI },

    { "Molten Band",      SLOT_RING,    RARITY_EPIC,       55, 6000, {12,12,0,0,0,0,6}, CM_ALL },
    { "Frozen Signet",    SLOT_RING,    RARITY_EPIC,       66,11000, { 0,0,16,16,8,0,0}, CM_ALL },
    { "Void Ring",        SLOT_RING,    RARITY_EPIC,       78,20000, { 0,10,0,0,14,10,10}, CM_ALL },
    { "Eternal Band",     SLOT_RING,    RARITY_LEGENDARY,  90,36000, {15,15,15,15,0,0,0}, CM_ALL },

    { "Molten Pendant",   SLOT_AMULET,  RARITY_EPIC,       55, 6000, { 0,0,0,8,14,6,0}, CM_ALL },
    { "Frozen Heart",     SLOT_AMULET,  RARITY_EPIC,       66,11000, { 0,0,10,10,10,6,0}, CM_ALL },
    { "Void Locket",      SLOT_AMULET,  RARITY_EPIC,       78,20000, { 0,0,12,12,18,8,0}, CM_ALL },
    { "Eternal Charm",    SLOT_AMULET,  RARITY_LEGENDARY,  90,36000, { 0,0,16,16,22,12,0}, CM_ALL }
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

/* Pick a random item matching level range, class, and rarity cap. Uniform random among candidates. */
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
