/*
 * data_enemies.c — Enemy templates and dungeon definitions.
 *
 * Enemy indices (E_*) are local to this file; DUNGEONS references them
 * for enemy rosters and boss assignments. ASCII art is 5 lines × 18 cols.
 */
#include "game.h"

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

    { "Goblin King",    0, 200, 25, 8, 3, 120, 60, .50f,
      {"   _/===\\_",  "  /|o  o|\\", "  | >ww< |",  "  |[KING]|",  "  |_|  |_|" }},
    { "Lord Ashvar",    1, 400, 49,12, 5, 300,120, .50f,
      {" ~{_/==\\_}~", "  | X  X |",  "  | \\==/ |",  "  |[BONE]|",  "  |__|__|" }},
    { "The Warden",     2, 700, 70,30, 4, 500,200, .50f,
      {" =[|/==\\|]=", "  || ** ||",  "  || /\\ ||",  "  |[WARD]|",  "  |_|__|_|" }},
    { "Abyssal Lord",   3,1000,105,28, 6, 800,350, .50f,
      {" }\\|/==\\|/{","  ( @  @ )",  "  |\\=/\\=/|", "  |[LORD]|",  "  |_|__|_|" }},
    { "The Lich God",   4,2000,154,45, 7,1500,700, .50f,
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

    { "Ignis the Molten",5, 4500, 252, 55,  5, 2400,1000, .50f,
      {"  ~\\\\|//~",   " ~{ o  o }~",  " ~|\\\\||//|~", " ~|[FIRE]|~", "  ~|_||_|~" }},
    { "Cryolith",        6, 8000, 364, 80,  6, 4200,1800, .50f,
      {" **/|==|\\**",  " *|| ** ||*", " *|| /\\ ||*", " *|[FROST]*", " *|_|  |_|*"}},
    { "Zhar'Kul the Void",7,14000, 532,100, 7, 7000,3000, .50f,
      {".~{/====\\}~.",".(  @  @  ).",". | \\../ | .",".|[ VOID ]|.",". |_|  |_| ."}},
    { "The Eternal One",  8,22000, 728,130, 8,12000,5000, .50f,
      {"+~{|====|}~+","+|(  **  )|+","+ | \\++/ | +","+|[ETERNAL]+","+ |_|  |_| +"}}
};

static const DungeonDef DUNGEONS[NUM_DUNGEONS] = {
    { "Goblin Warren",      1, "Fetid tunnels crawling with goblins.",
      { E_GOBLIN_GRUNT, E_GOBLIN_ARCHER, E_WOLF, E_GOBLIN_SHAMAN, E_ORC_SCOUT }, 5, E_BOSS_GOBLIN_KING, CP_GREEN },
    { "Crypt of Ashvar",    5, "Ancient tombs where the dead refuse rest.",
      { E_SKELETON, E_ZOMBIE, E_GHOUL, E_BONE_MAGE, E_CRYPT_WRAITH }, 5, E_BOSS_ASHVAR, CP_CYAN },
    { "Blackstone Keep",   12, "A fortress of dark stone and darker purpose.",
      { E_DARK_KNIGHT, E_GARGOYLE, E_STONE_GOLEM, E_SHADOW_ARCHER, E_CURSED_WARDEN }, 5, E_BOSS_BLACKSTONE, CP_YELLOW },
    { "The Abyssal Rift",  22, "A tear in reality seeping with hellfire.",
      { E_DEMON, E_SHADOW_LORD, E_IMP_SWARM, E_PIT_FIEND, E_VOID_STALKER }, 5, E_BOSS_ABYSSAL, CP_MAGENTA },
    { "Throne of the Lich",35, "The seat of undying power. Few return.",
      { E_DEATH_KNIGHT, E_UNDEAD_DRAGON, E_LICH, E_BONE_COLOSSUS, E_SOUL_REAVER }, 5, E_BOSS_LICH_KING, CP_WHITE },
    { "The Molten Core",     45, "Rivers of magma beneath a dying volcano.",
      { E_MAGMA_ELEM, E_FIRE_IMP, E_LAVA_LURKER, E_INFERNAL_GUARD, E_SCORCHED_WYVERN }, 5, E_BOSS_IGNIS, CP_RED },
    { "Frozen Citadel",      55, "An ancient fortress locked in eternal winter.",
      { E_FROST_REVENANT, E_ICE_WRAITH, E_GLACIAL_GOLEM, E_FROZEN_ARCHER, E_WINTER_KNIGHT }, 5, E_BOSS_CRYOLITH, CP_BLUE },
    { "The Void Nexus",      68, "A tear between dimensions, leaking raw chaos.",
      { E_VOID_SPAWN, E_ASTRAL_DEVOURER, E_NULL_WALKER, E_COSMIC_HORROR, E_RIFT_GUARDIAN }, 5, E_BOSS_ZHARKUL, CP_MAGENTA },
    { "Throne of Eternity",  82, "Beyond time itself. The final test of a hero.",
      { E_ETERNAL_SENTINEL, E_SERAPHIM, E_TIME_WEAVER, E_CELESTIAL_DRAGON, E_PRIMORDIAL_ONE }, 5, E_BOSS_ETERNAL, CP_YELLOW }
};

const DungeonDef   *data_dungeon(int id) { return (id>=0 && id<NUM_DUNGEONS) ? &DUNGEONS[id] : NULL; }
const EnemyTemplate*data_enemy(int id)   { return (id>=0 && id<TOTAL_ENEMIES)? &ENEMIES[id]  : NULL; }
int                 data_num_enemies(void) { return TOTAL_ENEMIES; }
