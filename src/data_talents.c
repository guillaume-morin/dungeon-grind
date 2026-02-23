#include "game.h"
#include <stddef.h>

/* Active skills granted by talent nodes — referenced by skillIdx. */
static const SkillDef TALENT_SKILLS[NUM_CLASSES][NUM_TALENT_TREES][4] = {
    [CLASS_WARRIOR] = {
        /* Arms */
        {
            {.name="Mortal Strike",  .description="250% dmg, bleed 40%/t 3t", .cooldown=8,  .resourceCost=35, .dmgMul=2.5f, .buffTicks=3, .buffDmgPct=40},
            {.name="Bladestorm",     .description="5x100% dmg + immune 1t",   .cooldown=22, .resourceCost=75, .dmgMul=1.0f, .numHits=5, .buffTicks=1, .buffImmune=1},
            {.name="Sweeping Strikes",.description="+50% dmg +20% crit 5t",   .cooldown=18, .resourceCost=50, .buffTicks=5, .buffDmgMul=0.5f, .buffCritBonus=0.2f},
        },
        /* Fury */
        {
            {.name="Bloodthirst",    .description="200% dmg + heal 8%",       .cooldown=6,  .resourceCost=30, .dmgMul=2.0f, .healPct=8},
            {.name="Rampage",        .description="4x90% dmg",                .cooldown=10, .resourceCost=55, .dmgMul=0.9f, .numHits=4},
            {.name="Recklessness",   .description="+80% crit dmg 5t",         .cooldown=20, .resourceCost=40, .buffTicks=5, .buffCritBonus=0.8f},
        },
        /* Protection */
        {
            {.name="Shield Slam",    .description="200% dmg + stun 2t",       .cooldown=8,  .resourceCost=30, .dmgMul=2.0f, .stunTicks=2},
            {.name="Shield Wall",    .description="Shield 60% HP if<40%",     .cooldown=25, .resourceCost=0,  .buffTicks=5, .buffShieldPct=60, .hpBelow=40},
            {.name="Last Stand",     .description="Immune 2t + heal 25%",     .cooldown=30, .resourceCost=0,  .buffTicks=2, .buffImmune=1, .healPct=25, .hpBelow=30},
        },
    },
    [CLASS_ROGUE] = {
        /* Assassination */
        {
            {.name="Envenom",        .description="280% dmg ignore armor",    .cooldown=8,  .resourceCost=35, .dmgMul=2.8f, .ignoreArmor=1},
            {.name="Vendetta",       .description="+40% dmg 6t",              .cooldown=20, .resourceCost=45, .buffTicks=6, .buffDmgMul=0.4f},
            {.name="Deathmark",      .description="DoT 100%/t 5t + 300% hit",.cooldown=22, .resourceCost=60, .dmgMul=3.0f, .buffTicks=5, .buffDmgPct=100},
        },
        /* Outlaw */
        {
            {.name="Pistol Shot",    .description="180% dmg ignore armor",    .cooldown=5,  .resourceCost=20, .dmgMul=1.8f, .ignoreArmor=1},
            {.name="Adrenaline Rush",.description="+35% dmg +25% dodge 5t",   .cooldown=20, .resourceCost=40, .buffTicks=5, .buffDmgMul=0.35f, .buffDodge=0.25f},
            {.name="Blade Flurry",   .description="3x120% dmg",              .cooldown=10, .resourceCost=45, .dmgMul=1.2f, .numHits=3},
        },
        /* Subtlety */
        {
            {.name="Shadowstrike",   .description="300% dmg from stealth",    .cooldown=8,  .resourceCost=40, .dmgMul=3.0f},
            {.name="Shadow Dance",   .description="+50% dmg +60% critDmg 4t", .cooldown=22, .resourceCost=55, .buffTicks=4, .buffDmgMul=0.5f, .buffCritBonus=0.6f},
            {.name="Death from Above",.description="500% ignore armor +stun 2t",.cooldown=20,.resourceCost=70, .dmgMul=5.0f, .ignoreArmor=1, .stunTicks=2},
        },
    },
    [CLASS_MAGE] = {
        /* Fire */
        {
            {.name="Pyroblast",      .description="350% dmg ignore armor",    .cooldown=10, .resourceCost=50, .dmgMul=3.5f, .ignoreArmor=1},
            {.name="Combustion",     .description="+100% crit dmg 4t",        .cooldown=18, .resourceCost=50, .buffTicks=4, .buffCritBonus=1.0f},
            {.name="Meteor",         .description="600% ignore armor +stun 2t",.cooldown=22,.resourceCost=85, .dmgMul=6.0f, .ignoreArmor=1, .stunTicks=2},
        },
        /* Frost */
        {
            {.name="Ice Lance",      .description="200% dmg + slow 2t",       .cooldown=5,  .resourceCost=25, .dmgMul=2.0f, .stunTicks=1},
            {.name="Icy Veins",      .description="+35% dmg 6t",              .cooldown=20, .resourceCost=40, .buffTicks=6, .buffDmgMul=0.35f},
            {.name="Glacial Spike",  .description="500% dmg + stun 3t",       .cooldown=20, .resourceCost=70, .dmgMul=5.0f, .stunTicks=3},
        },
        /* Arcane */
        {
            {.name="Arcane Blast",   .description="220% dmg + mana 10%",      .cooldown=6,  .resourceCost=30, .dmgMul=2.2f, .manaPct=10},
            {.name="Arcane Power",   .description="+45% dmg 5t",              .cooldown=18, .resourceCost=55, .buffTicks=5, .buffDmgMul=0.45f},
            {.name="Evocation",      .description="Heal 35% + mana 60%",      .cooldown=25, .resourceCost=0,  .healPct=35, .manaPct=60},
        },
    },
    [CLASS_PRIEST] = {
        /* Shadow */
        {
            {.name="Mind Blast",     .description="280% dmg ignore armor",    .cooldown=8,  .resourceCost=35, .dmgMul=2.8f, .ignoreArmor=1},
            {.name="Void Eruption",  .description="+45% dmg 6t",              .cooldown=20, .resourceCost=55, .buffTicks=6, .buffDmgMul=0.45f},
            {.name="SW: Death",      .description="500% dmg if enemy<20%",    .cooldown=15, .resourceCost=55, .dmgMul=5.0f, .enemyHpBelow=20},
        },
        /* Discipline */
        {
            {.name="Penance",        .description="3x160% dmg + heal 5%",     .cooldown=10, .resourceCost=40, .dmgMul=1.6f, .numHits=3, .healPct=5},
            {.name="Power Infusion", .description="+35% dmg 6t",              .cooldown=18, .resourceCost=40, .buffTicks=6, .buffDmgMul=0.35f},
            {.name="Pain Suppression",.description="Immune 3t + heal 20%",    .cooldown=28, .resourceCost=0,  .buffTicks=3, .buffImmune=1, .healPct=20, .hpBelow=25},
        },
        /* Holy */
        {
            {.name="Holy Fire",      .description="200% dmg + DoT 60%/t 3t",  .cooldown=8,  .resourceCost=30, .dmgMul=2.0f, .buffTicks=3, .buffDmgPct=60},
            {.name="Guardian Spirit", .description="Immune 2t+heal 30% if<25%",.cooldown=28, .resourceCost=0,  .buffTicks=2, .buffImmune=1, .healPct=30, .hpBelow=25},
            {.name="Divine Hymn",    .description="Heal 8%/t 5t + immune",     .cooldown=28, .resourceCost=70, .buffTicks=5, .buffHealPct=8, .buffImmune=1},
        },
    },
};

#define P(t,pr) {.type=(t), .perRank=(pr)}
#define NONE {.type=TB_NONE, .perRank=0}
#define NODE(n,d,ti,c,mr,act,pre,si,b0,b1,b2) \
    {.name=n,.desc=d,.tier=ti,.col=c,.maxRank=mr,.isActive=act,.prereqNode=pre,.skillIdx=si,.bonus={b0,b1,b2}}

static const ClassTalentDef CLASS_TALENTS[NUM_CLASSES] = {
    /* ════════════════════════ WARRIOR ════════════════════════ */
    [CLASS_WARRIOR] = { .trees = {
        /* ── Arms: Physical damage and bleed mastery ── */
        { .name = "Arms", .color = CP_RED, .nodeCount = 15, .nodes = {
            NODE("Weapon Mastery",  "+3 STR/rank",             0,0, 5, 0,-1, 0xFF, P(TB_FLAT_STR,3),        NONE, NONE),
            NODE("Sharpened Blade", "+2% phys dmg/rank",       0,1, 5, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,2),    NONE, NONE),
            NODE("Tactical Mastery","+1% XP/rank",             0,2, 3, 0,-1, 0xFF, P(TB_PCT_XP,1),          NONE, NONE),
            NODE("Deep Wounds",     "+1% crit/rank",           1,0, 5, 0, 0, 0xFF, P(TB_FLAT_CRIT,1),       NONE, NONE),
            NODE("Mortal Strike",   "Active: 250% + bleed",    1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Battle Cry",      "+2% phys dmg/rank",       1,2, 3, 0, 2, 0xFF, P(TB_PCT_PHYS_DMG,2),    NONE, NONE),
            NODE("Taste for Blood", "+3% crit dmg/rank",       2,0, 5, 0, 3, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Bladestorm",      "Active: 5x100% + immune", 2,1, 1, 1, 4, 1,    NONE,                    NONE, NONE),
            NODE("Overpower",       "+2 STR +1 AGI/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_STR,2),        P(TB_FLAT_AGI,1), NONE),
            NODE("Wrecking Crew",   "+3% phys dmg/rank",       3,0, 5, 0, 6, 0xFF, P(TB_PCT_PHYS_DMG,3),    NONE, NONE),
            NODE("Second Wind",     "+2% HP/rank",             3,1, 3, 0,-1, 0xFF, P(TB_PCT_HP,2),           NONE, NONE),
            NODE("Endless Rage",    "+2% atk spd/rank",        3,2, 3, 0,-1, 0xFF, P(TB_PCT_ATKSPD,2),      NONE, NONE),
            NODE("Executioner",     "+4% phys dmg/rank",       4,0, 3, 0, 9, 0xFF, P(TB_PCT_PHYS_DMG,4),    NONE, NONE),
            NODE("Sweeping Strikes","Active: +50% dmg 5t",     4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Titan's Grip",    "+5 STR +3% crit dmg/rank",4,2, 3, 0,11, 0xFF, P(TB_FLAT_STR,5),        P(TB_PCT_CRIT_DMG,3), NONE),
        }},
        /* ── Fury: Dual-wield frenzy, crit, and attack speed ── */
        { .name = "Fury", .color = CP_YELLOW, .nodeCount = 15, .nodes = {
            NODE("Cruelty",         "+1% crit/rank",           0,0, 5, 0,-1, 0xFF, P(TB_FLAT_CRIT,1),       NONE, NONE),
            NODE("Unbridled Wrath", "+3 AGI/rank",             0,1, 5, 0,-1, 0xFF, P(TB_FLAT_AGI,3),        NONE, NONE),
            NODE("Booming Voice",   "+2% atk spd/rank",        0,2, 3, 0,-1, 0xFF, P(TB_PCT_ATKSPD,2),     NONE, NONE),
            NODE("Bloodthirst",     "Active: 200% + heal 8%",  1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Enrage",          "+2% phys dmg/rank",       1,0, 5, 0, 0, 0xFF, P(TB_PCT_PHYS_DMG,2),    NONE, NONE),
            NODE("Dual Wield Spec", "+3% crit dmg/rank",       1,2, 3, 0, 2, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Flurry",          "+3% atk spd/rank",        2,0, 5, 0, 4, 0xFF, P(TB_PCT_ATKSPD,3),     NONE, NONE),
            NODE("Rampage",         "Active: 4x90% dmg",       2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Bloodsurge",      "+2 STR +1 VIT/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_STR,2),        P(TB_FLAT_VIT,1), NONE),
            NODE("Meat Cleaver",    "+3% phys dmg/rank",       3,0, 5, 0, 6, 0xFF, P(TB_PCT_PHYS_DMG,3),    NONE, NONE),
            NODE("Fresh Meat",      "+2% HP/rank",             3,1, 3, 0,-1, 0xFF, P(TB_PCT_HP,2),           NONE, NONE),
            NODE("Berserker Fury",  "+1% crit +1% critDmg/r",  3,2, 5, 0,-1, 0xFF, P(TB_FLAT_CRIT,1),       P(TB_PCT_CRIT_DMG,1), NONE),
            NODE("Recklessness",    "Active: +80% critDmg 5t", 4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Annihilator",     "+5% phys dmg/rank",       4,0, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,5),    NONE, NONE),
            NODE("Unhinged",        "+5 STR +3% atkSpd/rank",  4,2, 3, 0,11, 0xFF, P(TB_FLAT_STR,5),        P(TB_PCT_ATKSPD,3), NONE),
        }},
        /* ── Protection: Survivability, block, damage reduction ── */
        { .name = "Protection", .color = CP_CYAN, .nodeCount = 15, .nodes = {
            NODE("Toughness",       "+3 VIT/rank",             0,0, 5, 0,-1, 0xFF, P(TB_FLAT_VIT,3),        NONE, NONE),
            NODE("Shield Spec",     "+1% block/rank",          0,1, 5, 0,-1, 0xFF, P(TB_FLAT_BLOCK,1),      NONE, NONE),
            NODE("Anticipation",    "+3 DEF/rank",             0,2, 5, 0,-1, 0xFF, P(TB_FLAT_DEF,3),        NONE, NONE),
            NODE("Shield Slam",     "Active: 200% + stun 2t",  1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Improved Shield",  "+2% armor/rank",         1,0, 5, 0, 0, 0xFF, P(TB_PCT_ARMOR,2),       NONE, NONE),
            NODE("Defiance",        "+2% HP/rank",             1,2, 3, 0, 2, 0xFF, P(TB_PCT_HP,2),           NONE, NONE),
            NODE("One-Hand Spec",   "+2% phys dmg/rank",       2,0, 5, 0, 4, 0xFF, P(TB_PCT_PHYS_DMG,2),    NONE, NONE),
            NODE("Shield Wall",     "Active: shield 60% HP",   2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Iron Will",       "+2 DEF +2 VIT/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_DEF,2),        P(TB_FLAT_VIT,2), NONE),
            NODE("Vitality",        "+3% HP/rank",             3,0, 5, 0, 6, 0xFF, P(TB_PCT_HP,3),           NONE, NONE),
            NODE("Indomitable",     "+1% dodge/rank",          3,1, 3, 0,-1, 0xFF, P(TB_FLAT_DODGE,1),      NONE, NONE),
            NODE("Devastate",       "+2% phys dmg +1 flat DR", 3,2, 5, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,2),    P(TB_FLAT_DMGREDUCE,1), NONE),
            NODE("Last Stand",      "Active: immune 2t+heal",  4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Impenetrable",    "+4% armor +2% HP/rank",   4,0, 3, 0, 9, 0xFF, P(TB_PCT_ARMOR,4),       P(TB_PCT_HP,2), NONE),
            NODE("Juggernaut",      "+5 DEF +5 VIT/rank",      4,2, 3, 0,11, 0xFF, P(TB_FLAT_DEF,5),        P(TB_FLAT_VIT,5), NONE),
        }},
    }},

    /* ════════════════════════ ROGUE ════════════════════════ */
    [CLASS_ROGUE] = { .trees = {
        /* ── Assassination: Poison, crit, burst damage ── */
        { .name = "Assassination", .color = CP_GREEN, .nodeCount = 15, .nodes = {
            NODE("Malice",          "+1% crit/rank",           0,0, 5, 0,-1, 0xFF, P(TB_FLAT_CRIT,1),       NONE, NONE),
            NODE("Lethality",       "+3 AGI/rank",             0,1, 5, 0,-1, 0xFF, P(TB_FLAT_AGI,3),        NONE, NONE),
            NODE("Ruthlessness",    "+2% phys dmg/rank",       0,2, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,2),    NONE, NONE),
            NODE("Venom Rush",      "+2% atk spd/rank",        1,0, 5, 0, 0, 0xFF, P(TB_PCT_ATKSPD,2),     NONE, NONE),
            NODE("Envenom",         "Active: 280% ignore arm", 1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Improved Poisons","+3% crit dmg/rank",       1,2, 3, 0, 2, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Seal Fate",       "+2% crit/rank",           2,0, 5, 0, 3, 0xFF, P(TB_FLAT_CRIT,2),       NONE, NONE),
            NODE("Vendetta",        "Active: +40% dmg 6t",     2,1, 1, 1, 4, 1,    NONE,                    NONE, NONE),
            NODE("Cold Blood",      "+2 AGI +1 STR/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_AGI,2),        P(TB_FLAT_STR,1), NONE),
            NODE("Master Poisoner", "+3% phys dmg/rank",       3,0, 5, 0, 6, 0xFF, P(TB_PCT_PHYS_DMG,3),    NONE, NONE),
            NODE("Cut to the Bone", "+2% dodge/rank",          3,1, 3, 0,-1, 0xFF, P(TB_FLAT_DODGE,2),      NONE, NONE),
            NODE("Toxicologist",    "+2% crit dmg +1 AGI/r",   3,2, 5, 0,-1, 0xFF, P(TB_PCT_CRIT_DMG,2),    P(TB_FLAT_AGI,1), NONE),
            NODE("Deathmark",       "Active: DoT+300% hit",    4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Master Assassin", "+5% phys dmg/rank",       4,0, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,5),    NONE, NONE),
            NODE("Kingsbane",       "+5 AGI +3% critDmg/rank", 4,2, 3, 0,11, 0xFF, P(TB_FLAT_AGI,5),        P(TB_PCT_CRIT_DMG,3), NONE),
        }},
        /* ── Outlaw: Speed, dodge, versatile attacks ── */
        { .name = "Outlaw", .color = CP_YELLOW, .nodeCount = 15, .nodes = {
            NODE("Lightning Reflex","+3 AGI/rank",             0,0, 5, 0,-1, 0xFF, P(TB_FLAT_AGI,3),        NONE, NONE),
            NODE("Quick Draw",      "+2% atk spd/rank",        0,1, 5, 0,-1, 0xFF, P(TB_PCT_ATKSPD,2),     NONE, NONE),
            NODE("Riposte",         "+1% dodge/rank",          0,2, 5, 0,-1, 0xFF, P(TB_FLAT_DODGE,1),      NONE, NONE),
            NODE("Opportunity",     "+2% phys dmg/rank",       1,0, 5, 0, 0, 0xFF, P(TB_PCT_PHYS_DMG,2),    NONE, NONE),
            NODE("Pistol Shot",     "Active: 180% ignore arm", 1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Ghostly Strike",  "+1% crit +1% dodge/r",    1,2, 3, 0, 2, 0xFF, P(TB_FLAT_CRIT,1),       P(TB_FLAT_DODGE,1), NONE),
            NODE("Combat Potency",  "+3% atk spd/rank",        2,0, 5, 0, 3, 0xFF, P(TB_PCT_ATKSPD,3),     NONE, NONE),
            NODE("Adrenaline Rush", "Active: +35% dmg/dodge",  2,1, 1, 1, 4, 1,    NONE,                    NONE, NONE),
            NODE("Evasion",         "+2 AGI +1 SPD/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_AGI,2),        P(TB_FLAT_SPD,1), NONE),
            NODE("Killing Spree",   "+3% phys dmg/rank",       3,0, 5, 0, 6, 0xFF, P(TB_PCT_PHYS_DMG,3),    NONE, NONE),
            NODE("Loaded Dice",     "+2% crit dmg/rank",       3,1, 3, 0,-1, 0xFF, P(TB_PCT_CRIT_DMG,2),    NONE, NONE),
            NODE("Alacrity",        "+3% atk spd +1 SPD/r",    3,2, 3, 0,-1, 0xFF, P(TB_PCT_ATKSPD,3),     P(TB_FLAT_SPD,1), NONE),
            NODE("Blade Flurry",    "Active: 3x120% dmg",      4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("True Bearing",    "+5% phys dmg/rank",       4,0, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,5),    NONE, NONE),
            NODE("Ace Up Sleeve",   "+5 AGI +4% atkSpd/rank",  4,2, 3, 0,11, 0xFF, P(TB_FLAT_AGI,5),        P(TB_PCT_ATKSPD,4), NONE),
        }},
        /* ── Subtlety: Shadow damage, stealth, burst ── */
        { .name = "Subtlety", .color = CP_MAGENTA, .nodeCount = 15, .nodes = {
            NODE("Master of Shadow","+3 AGI/rank",             0,0, 5, 0,-1, 0xFF, P(TB_FLAT_AGI,3),        NONE, NONE),
            NODE("Nightstalker",    "+1% crit/rank",           0,1, 5, 0,-1, 0xFF, P(TB_FLAT_CRIT,1),       NONE, NONE),
            NODE("Shadow Focus",    "+2% phys dmg/rank",       0,2, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,2),    NONE, NONE),
            NODE("Shadowstrike",    "Active: 300% dmg",        1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Premeditation",   "+3% crit dmg/rank",       1,0, 5, 0, 0, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Gloomblade",      "+2% phys dmg/rank",       1,2, 5, 0, 2, 0xFF, P(TB_PCT_PHYS_DMG,2),    NONE, NONE),
            NODE("Find Weakness",   "+2% crit/rank",           2,0, 5, 0, 4, 0xFF, P(TB_FLAT_CRIT,2),       NONE, NONE),
            NODE("Shadow Dance",    "Active: +50% dmg +crit",  2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Deeper Stratagem","+2 AGI +2% critDmg/r",    2,2, 5, 0, 5, 0xFF, P(TB_FLAT_AGI,2),        P(TB_PCT_CRIT_DMG,2), NONE),
            NODE("Dark Shadow",     "+3% phys dmg/rank",       3,0, 5, 0, 6, 0xFF, P(TB_PCT_PHYS_DMG,3),    NONE, NONE),
            NODE("Shroud of Night", "+2% dodge/rank",          3,1, 3, 0,-1, 0xFF, P(TB_FLAT_DODGE,2),      NONE, NONE),
            NODE("Inevitability",   "+2% atk spd +1% crit/r",  3,2, 5, 0,-1, 0xFF, P(TB_PCT_ATKSPD,2),     P(TB_FLAT_CRIT,1), NONE),
            NODE("Death from Above","Active: 500% + stun",      4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Perforated Veins","+5% crit dmg/rank",       4,0, 3, 0,-1, 0xFF, P(TB_PCT_CRIT_DMG,5),    NONE, NONE),
            NODE("Finality",        "+5 AGI +5% physDmg/rank", 4,2, 3, 0,11, 0xFF, P(TB_FLAT_AGI,5),        P(TB_PCT_PHYS_DMG,5), NONE),
        }},
    }},

    /* ════════════════════════ MAGE ════════════════════════ */
    [CLASS_MAGE] = { .trees = {
        /* ── Fire: Burst damage, crit, ignite ── */
        { .name = "Fire", .color = CP_RED, .nodeCount = 15, .nodes = {
            NODE("Improved Fireblt","+3 INT/rank",             0,0, 5, 0,-1, 0xFF, P(TB_FLAT_INT,3),        NONE, NONE),
            NODE("Ignite",          "+2% spell dmg/rank",      0,1, 5, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,2),   NONE, NONE),
            NODE("Critical Mass",   "+1% crit/rank",           0,2, 3, 0,-1, 0xFF, P(TB_FLAT_CRIT,1),       NONE, NONE),
            NODE("Pyroblast",       "Active: 350% ignore arm", 1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Fire Power",      "+2% spell dmg/rank",      1,0, 5, 0, 0, 0xFF, P(TB_PCT_SPELL_DMG,2),   NONE, NONE),
            NODE("Blazing Speed",   "+2% atk spd/rank",        1,2, 3, 0, 2, 0xFF, P(TB_PCT_ATKSPD,2),     NONE, NONE),
            NODE("Hot Streak",      "+2% crit/rank",           2,0, 5, 0, 4, 0xFF, P(TB_FLAT_CRIT,2),       NONE, NONE),
            NODE("Combustion",      "Active: +100% critDmg",   2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("World in Flames", "+2 INT +1 WIS/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_INT,2),        P(TB_FLAT_WIS,1), NONE),
            NODE("Firestarter",     "+3% spell dmg/rank",      3,0, 5, 0, 6, 0xFF, P(TB_PCT_SPELL_DMG,3),   NONE, NONE),
            NODE("Cauterize",       "+2% HP/rank",             3,1, 3, 0,-1, 0xFF, P(TB_PCT_HP,2),           NONE, NONE),
            NODE("Pyromaniac",      "+3% crit dmg/rank",       3,2, 5, 0,-1, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Meteor",          "Active: 600% + stun 2t",  4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Sun King's",      "+5% spell dmg/rank",      4,0, 3, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,5),   NONE, NONE),
            NODE("Inferno",         "+5 INT +3% critDmg/rank", 4,2, 3, 0,11, 0xFF, P(TB_FLAT_INT,5),        P(TB_PCT_CRIT_DMG,3), NONE),
        }},
        /* ── Frost: Control, defense, sustained damage ── */
        { .name = "Frost", .color = CP_CYAN, .nodeCount = 15, .nodes = {
            NODE("Frostbite",       "+3 INT/rank",             0,0, 5, 0,-1, 0xFF, P(TB_FLAT_INT,3),        NONE, NONE),
            NODE("Ice Shards",      "+2% spell dmg/rank",      0,1, 5, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,2),   NONE, NONE),
            NODE("Frost Armor",     "+3 DEF/rank",             0,2, 5, 0,-1, 0xFF, P(TB_FLAT_DEF,3),        NONE, NONE),
            NODE("Ice Lance",       "Active: 200% + slow",     1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Brain Freeze",    "+1% crit/rank",           1,0, 5, 0, 0, 0xFF, P(TB_FLAT_CRIT,1),       NONE, NONE),
            NODE("Permafrost",      "+2% armor/rank",          1,2, 5, 0, 2, 0xFF, P(TB_PCT_ARMOR,2),       NONE, NONE),
            NODE("Shatter",         "+3% crit dmg/rank",       2,0, 5, 0, 4, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Icy Veins",       "Active: +35% dmg 6t",     2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Arctic Winds",    "+2 INT +2 WIS/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_INT,2),        P(TB_FLAT_WIS,2), NONE),
            NODE("Fingers of Frost","+2% spell dmg/rank",      3,0, 5, 0, 6, 0xFF, P(TB_PCT_SPELL_DMG,2),   NONE, NONE),
            NODE("Ice Barrier",     "+3% HP/rank",             3,1, 3, 0,-1, 0xFF, P(TB_PCT_HP,3),           NONE, NONE),
            NODE("Cold Snap",       "+2% dodge +1% atk spd/r", 3,2, 3, 0,-1, 0xFF, P(TB_FLAT_DODGE,2),      P(TB_PCT_ATKSPD,1), NONE),
            NODE("Glacial Spike",   "Active: 500% + stun 3t",  4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Comet Storm",     "+5% spell dmg/rank",      4,0, 3, 0, 9, 0xFF, P(TB_PCT_SPELL_DMG,5),   NONE, NONE),
            NODE("Deep Freeze",     "+5 INT +3% armor/rank",   4,2, 3, 0,11, 0xFF, P(TB_FLAT_INT,5),        P(TB_PCT_ARMOR,3), NONE),
        }},
        /* ── Arcane: Mana, versatility, XP ── */
        { .name = "Arcane", .color = CP_MAGENTA, .nodeCount = 15, .nodes = {
            NODE("Arcane Mind",     "+3 INT/rank",             0,0, 5, 0,-1, 0xFF, P(TB_FLAT_INT,3),        NONE, NONE),
            NODE("Arcane Focus",    "+2% spell dmg/rank",      0,1, 5, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,2),   NONE, NONE),
            NODE("Arcane Fortitude","+3 WIS/rank",             0,2, 5, 0,-1, 0xFF, P(TB_FLAT_WIS,3),        NONE, NONE),
            NODE("Arcane Blast",    "Active: 220% + mana 10%", 1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Arcane Instab.",  "+2% spell dmg/rank",      1,0, 5, 0, 0, 0xFF, P(TB_PCT_SPELL_DMG,2),   NONE, NONE),
            NODE("Presence of Mind","+2% XP/rank",             1,2, 3, 0, 2, 0xFF, P(TB_PCT_XP,2),          NONE, NONE),
            NODE("Nether Tempest",  "+2% crit dmg/rank",       2,0, 5, 0, 4, 0xFF, P(TB_PCT_CRIT_DMG,2),    NONE, NONE),
            NODE("Arcane Power",    "Active: +45% dmg 5t",     2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Arcane Empow.",   "+2 INT +1 WIS/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_INT,2),        P(TB_FLAT_WIS,1), NONE),
            NODE("Arcane Harmony",  "+3% spell dmg/rank",      3,0, 5, 0, 6, 0xFF, P(TB_PCT_SPELL_DMG,3),   NONE, NONE),
            NODE("Mana Shield",     "+2% HP/rank",             3,1, 3, 0,-1, 0xFF, P(TB_PCT_HP,2),           NONE, NONE),
            NODE("Time Warp",       "+3% atk spd/rank",        3,2, 3, 0,-1, 0xFF, P(TB_PCT_ATKSPD,3),     NONE, NONE),
            NODE("Evocation",       "Active: heal 35%+mana60%",4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Arcane Mastery",  "+5% spell dmg/rank",      4,0, 3, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,5),   NONE, NONE),
            NODE("Enlightenment",   "+5 INT +3% XP/rank",      4,2, 3, 0,11, 0xFF, P(TB_FLAT_INT,5),        P(TB_PCT_XP,3), NONE),
        }},
    }},

    /* ════════════════════════ PRIEST ════════════════════════ */
    [CLASS_PRIEST] = { .trees = {
        /* ── Shadow: Damage-over-time, burst, self-sustain ── */
        { .name = "Shadow", .color = CP_MAGENTA, .nodeCount = 15, .nodes = {
            NODE("Spirit Tap",      "+3 WIS/rank",             0,0, 5, 0,-1, 0xFF, P(TB_FLAT_WIS,3),        NONE, NONE),
            NODE("Shadow Focus",    "+2% spell dmg/rank",      0,1, 5, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,2),   NONE, NONE),
            NODE("Darkness",        "+1% crit/rank",           0,2, 3, 0,-1, 0xFF, P(TB_FLAT_CRIT,1),       NONE, NONE),
            NODE("Mind Blast",      "Active: 280% ignore arm", 1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Shadow Weaving",  "+2% spell dmg/rank",      1,0, 5, 0, 0, 0xFF, P(TB_PCT_SPELL_DMG,2),   NONE, NONE),
            NODE("Misery",          "+2% atk spd/rank",        1,2, 3, 0, 2, 0xFF, P(TB_PCT_ATKSPD,2),     NONE, NONE),
            NODE("Shadow Mastery",  "+3% crit dmg/rank",       2,0, 5, 0, 4, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Void Eruption",   "Active: +45% dmg 6t",     2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Vampiric Touch",  "+2 WIS +1 INT/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_WIS,2),        P(TB_FLAT_INT,1), NONE),
            NODE("Auspicious Sprt", "+3% spell dmg/rank",      3,0, 5, 0, 6, 0xFF, P(TB_PCT_SPELL_DMG,3),   NONE, NONE),
            NODE("Shadowform",      "+2% dodge/rank",          3,1, 3, 0,-1, 0xFF, P(TB_FLAT_DODGE,2),      NONE, NONE),
            NODE("Torment",         "+2% crit dmg +1 WIS/r",   3,2, 5, 0,-1, 0xFF, P(TB_PCT_CRIT_DMG,2),    P(TB_FLAT_WIS,1), NONE),
            NODE("SW: Death",       "Active: 500% if <20%",     4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Ancient Madness", "+5% spell dmg/rank",      4,0, 3, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,5),   NONE, NONE),
            NODE("Void Lord",       "+5 WIS +3% critDmg/rank", 4,2, 3, 0,11, 0xFF, P(TB_FLAT_WIS,5),        P(TB_PCT_CRIT_DMG,3), NONE),
        }},
        /* ── Discipline: Heal-through-damage hybrid ── */
        { .name = "Discipline", .color = CP_YELLOW, .nodeCount = 15, .nodes = {
            NODE("Meditation",      "+3 WIS/rank",             0,0, 5, 0,-1, 0xFF, P(TB_FLAT_WIS,3),        NONE, NONE),
            NODE("Twin Disciplines","+2% heal/rank",           0,1, 5, 0,-1, 0xFF, P(TB_PCT_HEAL,2),        NONE, NONE),
            NODE("Mental Agility",  "+3 INT/rank",             0,2, 5, 0,-1, 0xFF, P(TB_FLAT_INT,3),        NONE, NONE),
            NODE("Penance",         "Active: 3x160% + heal",   1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Focused Will",    "+2% HP/rank",             1,0, 5, 0, 0, 0xFF, P(TB_PCT_HP,2),           NONE, NONE),
            NODE("Enlightenment",   "+2% XP/rank",             1,2, 3, 0, 2, 0xFF, P(TB_PCT_XP,2),          NONE, NONE),
            NODE("Divine Aegis",    "+2% armor/rank",          2,0, 5, 0, 4, 0xFF, P(TB_PCT_ARMOR,2),       NONE, NONE),
            NODE("Power Infusion",  "Active: +35% dmg 6t",     2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Borrowed Time",   "+2 WIS +2 INT/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_WIS,2),        P(TB_FLAT_INT,2), NONE),
            NODE("Grace",           "+3% heal/rank",           3,0, 5, 0, 6, 0xFF, P(TB_PCT_HEAL,3),        NONE, NONE),
            NODE("Rapture",         "+3% HP/rank",             3,1, 3, 0,-1, 0xFF, P(TB_PCT_HP,3),           NONE, NONE),
            NODE("Atonement",       "+2% spell dmg +1% heal/r",3,2, 5, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,2),   P(TB_PCT_HEAL,1), NONE),
            NODE("Pain Suppression","Active: immune3t+heal20%", 4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Strength of Soul","+5% heal/rank",           4,0, 3, 0,-1, 0xFF, P(TB_PCT_HEAL,5),        NONE, NONE),
            NODE("Clarity of Will", "+5 WIS +5 INT/rank",      4,2, 3, 0,11, 0xFF, P(TB_FLAT_WIS,5),        P(TB_FLAT_INT,5), NONE),
        }},
        /* ── Holy: Pure healing, survivability ── */
        { .name = "Holy", .color = CP_WHITE, .nodeCount = 15, .nodes = {
            NODE("Holy Spec",       "+3 WIS/rank",             0,0, 5, 0,-1, 0xFF, P(TB_FLAT_WIS,3),        NONE, NONE),
            NODE("Divine Fury",     "+2% heal/rank",           0,1, 5, 0,-1, 0xFF, P(TB_PCT_HEAL,2),        NONE, NONE),
            NODE("Blessed Recovery","+3 VIT/rank",             0,2, 5, 0,-1, 0xFF, P(TB_FLAT_VIT,3),        NONE, NONE),
            NODE("Holy Fire",       "Active: 200% + DoT",      1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Searing Light",   "+2% spell dmg/rank",      1,0, 5, 0, 0, 0xFF, P(TB_PCT_SPELL_DMG,2),   NONE, NONE),
            NODE("Inspiration",     "+2% HP/rank",             1,2, 3, 0, 2, 0xFF, P(TB_PCT_HP,2),           NONE, NONE),
            NODE("Surge of Light",  "+3% heal/rank",           2,0, 5, 0, 4, 0xFF, P(TB_PCT_HEAL,3),        NONE, NONE),
            NODE("Guardian Spirit", "Active: immune+heal30%",   2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Spiritual Guide", "+2 WIS +2 VIT/rank",      2,2, 5, 0, 5, 0xFF, P(TB_FLAT_WIS,2),        P(TB_FLAT_VIT,2), NONE),
            NODE("Empowered Heal",  "+3% heal +1% spellDmg/r", 3,0, 5, 0, 6, 0xFF, P(TB_PCT_HEAL,3),        P(TB_PCT_SPELL_DMG,1), NONE),
            NODE("Body and Soul",   "+2% dodge/rank",          3,1, 3, 0,-1, 0xFF, P(TB_FLAT_DODGE,2),      NONE, NONE),
            NODE("Spirit of Redemp","+3% HP/rank",             3,2, 3, 0,-1, 0xFF, P(TB_PCT_HP,3),           NONE, NONE),
            NODE("Divine Hymn",     "Active: heal8%/t+immune",  4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Circle of Healing","+5% heal/rank",          4,0, 3, 0,-1, 0xFF, P(TB_PCT_HEAL,5),        NONE, NONE),
            NODE("Apotheosis",      "+5 WIS +5 VIT/rank",      4,2, 3, 0,11, 0xFF, P(TB_FLAT_WIS,5),        P(TB_FLAT_VIT,5), NONE),
        }},
    }},
};

#undef P
#undef NONE
#undef NODE

const ClassTalentDef *data_class_talents(int classId) {
    if (classId < 0 || classId >= NUM_CLASSES) return NULL;
    return &CLASS_TALENTS[classId];
}

const TalentTreeDef *data_talent_tree(int classId, int tree) {
    if (classId < 0 || classId >= NUM_CLASSES) return NULL;
    if (tree < 0 || tree >= NUM_TALENT_TREES) return NULL;
    return &CLASS_TALENTS[classId].trees[tree];
}

const TalentNodeDef *data_talent_node(int classId, int tree, int node) {
    const TalentTreeDef *t = data_talent_tree(classId, tree);
    if (!t || node < 0 || node >= t->nodeCount) return NULL;
    return &t->nodes[node];
}

const SkillDef *data_talent_skill(int classId, int tree, int node) {
    const TalentNodeDef *nd = data_talent_node(classId, tree, node);
    if (!nd || !nd->isActive || nd->skillIdx == 0xFF) return NULL;
    if (nd->skillIdx >= 4) return NULL;
    return &TALENT_SKILLS[classId][tree][nd->skillIdx];
}
