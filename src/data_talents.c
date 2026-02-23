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
        /* ── Arms: STR scaling, physical burst, crit damage ── */
        { .name = "Arms", .color = CP_RED, .nodeCount = 15, .nodes = {
            NODE("Weapon Mastery",  "+3% STR/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_STR,3),         NONE, NONE),
            NODE("Sharpened Blade", "+3% phys dmg/rank",       0,1, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,3),    NONE, NONE),
            NODE("Tactical Mastery","+2% XP +2% atkSpd/rank",  0,2, 2, 0,-1, 0xFF, P(TB_PCT_XP,2),          P(TB_PCT_ATKSPD,2), NONE),
            NODE("Deep Wounds",     "+3% crit dmg/rank",       1,0, 3, 0, 0, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Mortal Strike",   "Active: 250% + bleed",    1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Deflection",      "+3% dodge +2% crit/rank", 1,2, 2, 0, 2, 0xFF, P(TB_FLAT_DODGE,3),      P(TB_FLAT_CRIT,2), NONE),
            NODE("Imp Mortal Strk", "+2% crit +2% atkSpd/rank",2,0, 3, 0, 3, 0xFF, P(TB_FLAT_CRIT,2),       P(TB_PCT_ATKSPD,2), NONE),
            NODE("Bladestorm",      "Active: 5x100% + immune", 2,1, 1, 1, 4, 1,    NONE,                    NONE, NONE),
            NODE("Blood Frenzy",    "+5% STR +3% HP/rank",     2,2, 2, 0, 5, 0xFF, P(TB_PCT_STR,5),         P(TB_PCT_HP,3), NONE),
            NODE("Taste for Blood", "+3% physDmg +2% critDmg/r",3,0, 3, 0, 6, 0xFF, P(TB_PCT_PHYS_DMG,3),   P(TB_PCT_CRIT_DMG,2), NONE),
            NODE("Second Wind",     "+5% HP/rank",             3,1, 2, 0,-1, 0xFF, P(TB_PCT_HP,5),           NONE, NONE),
            NODE("Endless Rage",    "+3% critDmg +3% atkSpd/r",3,2, 2, 0,-1, 0xFF, P(TB_PCT_CRIT_DMG,3),    P(TB_PCT_ATKSPD,3), NONE),
            NODE("Executioner",     "+4% physDmg +2% crit/rank",4,0, 3, 0, 9, 0xFF, P(TB_PCT_PHYS_DMG,4),   P(TB_FLAT_CRIT,2), NONE),
            NODE("Sweeping Strikes","Active: +50% dmg 5t",     4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Titan's Grip",    "+5% STR +3% critDmg/rank",4,2, 2, 0,11, 0xFF, P(TB_PCT_STR,5),         P(TB_PCT_CRIT_DMG,3), NONE),
        }},
        /* ── Fury: Berserker speed, dual-wield frenzy ── */
        { .name = "Fury", .color = CP_YELLOW, .nodeCount = 15, .nodes = {
            NODE("Cruelty",         "+3% STR/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_STR,3),         NONE, NONE),
            NODE("Unbridled Wrath", "+3% phys dmg/rank",       0,1, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,3),    NONE, NONE),
            NODE("Booming Voice",   "+3% atkSpd +2% crit/rank",0,2, 2, 0,-1, 0xFF, P(TB_PCT_ATKSPD,3),      P(TB_FLAT_CRIT,2), NONE),
            NODE("Bloodthirst",     "Active: 200% + heal 8%",  1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Enrage",          "+3% crit dmg/rank",       1,0, 3, 0, 0, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Dual Wield Spec", "+3% AGI +2% atkSpd/rank", 1,2, 2, 0, 2, 0xFF, P(TB_PCT_AGI,3),         P(TB_PCT_ATKSPD,2), NONE),
            NODE("Flurry",          "+2% crit +3% atkSpd/rank",2,0, 3, 0, 4, 0xFF, P(TB_FLAT_CRIT,2),       P(TB_PCT_ATKSPD,3), NONE),
            NODE("Rampage",         "Active: 4x90% dmg",       2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Bloodsurge",      "+5% STR +3% AGI/rank",    2,2, 2, 0, 5, 0xFF, P(TB_PCT_STR,5),         P(TB_PCT_AGI,3), NONE),
            NODE("Meat Cleaver",    "+3% physDmg +2% HP/rank", 3,0, 3, 0, 6, 0xFF, P(TB_PCT_PHYS_DMG,3),    P(TB_PCT_HP,2), NONE),
            NODE("Fresh Meat",      "+3% critDmg +3% crit/r",  3,1, 2, 0,-1, 0xFF, P(TB_PCT_CRIT_DMG,3),    P(TB_FLAT_CRIT,3), NONE),
            NODE("Berserker Fury",  "+5% atkSpd +3% critDmg/r",3,2, 2, 0,-1, 0xFF, P(TB_PCT_ATKSPD,5),      P(TB_PCT_CRIT_DMG,3), NONE),
            NODE("Recklessness",    "Active: +80% critDmg 5t", 4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Annihilator",     "+4% physDmg +1% crit/rank",4,0, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,4),   P(TB_FLAT_CRIT,1), NONE),
            NODE("Titan's Grip",    "+5% STR +3% atkSpd/rank", 4,2, 2, 0,11, 0xFF, P(TB_PCT_STR,5),         P(TB_PCT_ATKSPD,3), NONE),
        }},
        /* ── Protection: VIT/DEF scaling, HP, armor, block ── */
        { .name = "Protection", .color = CP_CYAN, .nodeCount = 15, .nodes = {
            NODE("Toughness",       "+3% VIT/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_VIT,3),         NONE, NONE),
            NODE("Shield Spec",     "+3% armor/rank",          0,1, 3, 0,-1, 0xFF, P(TB_PCT_ARMOR,3),       NONE, NONE),
            NODE("Anticipation",    "+5% DEF/rank",            0,2, 2, 0,-1, 0xFF, P(TB_PCT_DEF,5),         NONE, NONE),
            NODE("Shield Slam",     "Active: 200% + stun 2t",  1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Improved Shield", "+2% block +2% HP/rank",   1,0, 3, 0, 0, 0xFF, P(TB_FLAT_BLOCK,2),      P(TB_PCT_HP,2), NONE),
            NODE("Defiance",        "+3% HP +2% dodge/rank",   1,2, 2, 0, 2, 0xFF, P(TB_PCT_HP,3),           P(TB_FLAT_DODGE,2), NONE),
            NODE("One-Hand Spec",   "+3% physDmg +2% armor/r", 2,0, 3, 0, 4, 0xFF, P(TB_PCT_PHYS_DMG,3),    P(TB_PCT_ARMOR,2), NONE),
            NODE("Shield Wall",     "Active: shield 60% HP",   2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Iron Will",       "+5% VIT +3% DEF/rank",    2,2, 2, 0, 5, 0xFF, P(TB_PCT_VIT,5),         P(TB_PCT_DEF,3), NONE),
            NODE("Vitality",        "+3% HP +2 flat DR/rank",  3,0, 3, 0, 6, 0xFF, P(TB_PCT_HP,3),           P(TB_FLAT_DMGREDUCE,2), NONE),
            NODE("Indomitable",     "+3% dodge +3% HP/rank",   3,1, 2, 0,-1, 0xFF, P(TB_FLAT_DODGE,3),      P(TB_PCT_HP,3), NONE),
            NODE("Devastate",       "+5% armor +3% physDmg/r", 3,2, 2, 0,-1, 0xFF, P(TB_PCT_ARMOR,5),       P(TB_PCT_PHYS_DMG,3), NONE),
            NODE("Last Stand",      "Active: immune 2t+heal",  4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Bastion",         "+3% VIT +2% block/rank",  4,0, 3, 0, 9, 0xFF, P(TB_PCT_VIT,3),         P(TB_FLAT_BLOCK,2), NONE),
            NODE("Juggernaut",      "+5% HP +3 flat DR/rank",  4,2, 2, 0,11, 0xFF, P(TB_PCT_HP,5),           P(TB_FLAT_DMGREDUCE,3), NONE),
        }},
    }},

    /* ════════════════════════ ROGUE ════════════════════════ */
    [CLASS_ROGUE] = { .trees = {
        /* ── Assassination: Burst crits, poison mastery, armor pen ── */
        { .name = "Assassination", .color = CP_GREEN, .nodeCount = 15, .nodes = {
            NODE("Malice",          "+3% AGI/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_AGI,3),         NONE, NONE),
            NODE("Lethality",       "+3% phys dmg/rank",       0,1, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,3),    NONE, NONE),
            NODE("Ruthlessness",    "+3% atkSpd +2% crit/rank",0,2, 2, 0,-1, 0xFF, P(TB_PCT_ATKSPD,3),      P(TB_FLAT_CRIT,2), NONE),
            NODE("Venom Rush",      "+3% crit dmg/rank",       1,0, 3, 0, 0, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Envenom",         "Active: 280% ignore arm", 1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Improved Poisons","+3% crit +2% critDmg/rank",1,2, 2, 0, 2, 0xFF, P(TB_FLAT_CRIT,3),      P(TB_PCT_CRIT_DMG,2), NONE),
            NODE("Seal Fate",       "+2% crit +2% physDmg/rank",2,0, 3, 0, 3, 0xFF, P(TB_FLAT_CRIT,2),      P(TB_PCT_PHYS_DMG,2), NONE),
            NODE("Vendetta",        "Active: +40% dmg 6t",     2,1, 1, 1, 4, 1,    NONE,                    NONE, NONE),
            NODE("Cold Blood",      "+5% AGI +3% HP/rank",     2,2, 2, 0, 5, 0xFF, P(TB_PCT_AGI,5),         P(TB_PCT_HP,3), NONE),
            NODE("Master Poisoner", "+3% critDmg +2% atkSpd/r",3,0, 3, 0, 6, 0xFF, P(TB_PCT_CRIT_DMG,3),    P(TB_PCT_ATKSPD,2), NONE),
            NODE("Cut to the Bone", "+3% crit +3% physDmg/rank",3,1, 2, 0,-1, 0xFF, P(TB_FLAT_CRIT,3),      P(TB_PCT_PHYS_DMG,3), NONE),
            NODE("Toxicologist",    "+5% physDmg +3% AGI/rank",3,2, 2, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,5),    P(TB_PCT_AGI,3), NONE),
            NODE("Deathmark",       "Active: DoT+300% hit",    4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Master Assassin", "+3% critDmg +2% crit/rank",4,0, 3, 0,-1, 0xFF, P(TB_PCT_CRIT_DMG,3),   P(TB_FLAT_CRIT,2), NONE),
            NODE("Kingsbane",       "+5% AGI +3% critDmg/rank",4,2, 2, 0,11, 0xFF, P(TB_PCT_AGI,5),         P(TB_PCT_CRIT_DMG,3), NONE),
        }},
        /* ── Outlaw: Speed, dodge, gold, versatile attacks ── */
        { .name = "Outlaw", .color = CP_YELLOW, .nodeCount = 15, .nodes = {
            NODE("Lightning Reflex","+3% AGI/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_AGI,3),         NONE, NONE),
            NODE("Quick Draw",      "+3% phys dmg/rank",       0,1, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,3),    NONE, NONE),
            NODE("Riposte",         "+3% dodge +3% gold/rank", 0,2, 2, 0,-1, 0xFF, P(TB_FLAT_DODGE,3),      P(TB_PCT_GOLD,3), NONE),
            NODE("Opportunity",     "+3% atkSpd +2% crit/rank",1,0, 3, 0, 0, 0xFF, P(TB_PCT_ATKSPD,3),      P(TB_FLAT_CRIT,2), NONE),
            NODE("Pistol Shot",     "Active: 180% ignore arm", 1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Ghostly Strike",  "+3% crit +2% atkSpd/rank",1,2, 2, 0, 2, 0xFF, P(TB_FLAT_CRIT,3),       P(TB_PCT_ATKSPD,2), NONE),
            NODE("Combat Potency",  "+3% physDmg +2% atkSpd/r",2,0, 3, 0, 3, 0xFF, P(TB_PCT_PHYS_DMG,3),    P(TB_PCT_ATKSPD,2), NONE),
            NODE("Adrenaline Rush", "Active: +35% dmg/dodge",  2,1, 1, 1, 4, 1,    NONE,                    NONE, NONE),
            NODE("Evasion",         "+5% AGI +3% HP/rank",     2,2, 2, 0, 5, 0xFF, P(TB_PCT_AGI,5),         P(TB_PCT_HP,3), NONE),
            NODE("Killing Spree",   "+3% critDmg +2% atkSpd/r",3,0, 3, 0, 6, 0xFF, P(TB_PCT_CRIT_DMG,3),    P(TB_PCT_ATKSPD,2), NONE),
            NODE("Dirty Tricks",    "+3% dodge +3% critDmg/r", 3,1, 2, 0,-1, 0xFF, P(TB_FLAT_DODGE,3),      P(TB_PCT_CRIT_DMG,3), NONE),
            NODE("Roll the Bones",  "+3% physDmg +3% crit/rank",3,2, 2, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,3),   P(TB_FLAT_CRIT,3), NONE),
            NODE("Blade Flurry",    "Active: 3x120% dmg",      4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("True Bearing",    "+3% atkSpd +2% crit/rank",4,0, 3, 0,-1, 0xFF, P(TB_PCT_ATKSPD,3),      P(TB_FLAT_CRIT,2), NONE),
            NODE("Pirate King",     "+5% AGI +3% dodge/rank",  4,2, 2, 0,11, 0xFF, P(TB_PCT_AGI,5),         P(TB_PCT_DODGE,3), NONE),
        }},
        /* ── Subtlety: Shadow strikes, massive crit dmg, evasion ── */
        { .name = "Subtlety", .color = CP_MAGENTA, .nodeCount = 15, .nodes = {
            NODE("Master of Shadow","+3% AGI/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_AGI,3),         NONE, NONE),
            NODE("Nightstalker",    "+3% phys dmg/rank",       0,1, 3, 0,-1, 0xFF, P(TB_PCT_PHYS_DMG,3),    NONE, NONE),
            NODE("Shadow Focus",    "+3% dodge +2% crit/rank", 0,2, 2, 0,-1, 0xFF, P(TB_FLAT_DODGE,3),      P(TB_FLAT_CRIT,2), NONE),
            NODE("Shadowstrike",    "Active: 300% dmg",        1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Premeditation",   "+5% crit dmg/rank",       1,0, 3, 0, 0, 0xFF, P(TB_PCT_CRIT_DMG,5),    NONE, NONE),
            NODE("Gloomblade",      "+3% crit +3% physDmg/rank",1,2, 2, 0, 2, 0xFF, P(TB_FLAT_CRIT,3),      P(TB_PCT_PHYS_DMG,3), NONE),
            NODE("Find Weakness",   "+3% physDmg +2% crit/rank",2,0, 3, 0, 4, 0xFF, P(TB_PCT_PHYS_DMG,3),   P(TB_FLAT_CRIT,2), NONE),
            NODE("Shadow Dance",    "Active: +50% dmg +crit",  2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Deeper Stratagem","+5% AGI +3% atkSpd/rank", 2,2, 2, 0, 5, 0xFF, P(TB_PCT_AGI,5),         P(TB_PCT_ATKSPD,3), NONE),
            NODE("Dark Shadow",     "+3% critDmg +2% dodge/r", 3,0, 3, 0, 6, 0xFF, P(TB_PCT_CRIT_DMG,3),    P(TB_FLAT_DODGE,2), NONE),
            NODE("Shroud of Night", "+3% crit +3% atkSpd/rank",3,1, 2, 0,-1, 0xFF, P(TB_FLAT_CRIT,3),       P(TB_PCT_ATKSPD,3), NONE),
            NODE("Inevitability",   "+5% atkSpd +3% HP/rank",  3,2, 2, 0,-1, 0xFF, P(TB_PCT_ATKSPD,5),      P(TB_PCT_HP,3), NONE),
            NODE("Death from Above","Active: 500% + stun",      4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Perforated Veins","+3% AGI +2% critDmg/rank",4,0, 3, 0,-1, 0xFF, P(TB_PCT_AGI,3),         P(TB_PCT_CRIT_DMG,2), NONE),
            NODE("Finality",        "+5% critDmg +3% crit/rank",4,2, 2, 0,11, 0xFF, P(TB_PCT_CRIT_DMG,5),   P(TB_FLAT_CRIT,3), NONE),
        }},
    }},

    /* ════════════════════════ MAGE ════════════════════════ */
    [CLASS_MAGE] = { .trees = {
        /* ── Fire: Burst damage, crit strikes, ignite ── */
        { .name = "Fire", .color = CP_RED, .nodeCount = 15, .nodes = {
            NODE("Improved Fireblt","+3% INT/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_INT,3),         NONE, NONE),
            NODE("Ignite",          "+3% spell dmg/rank",      0,1, 3, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,3),   NONE, NONE),
            NODE("Critical Mass",   "+3% crit +2% critDmg/rank",0,2, 2, 0,-1, 0xFF, P(TB_FLAT_CRIT,3),     P(TB_PCT_CRIT_DMG,2), NONE),
            NODE("Pyroblast",       "Active: 350% ignore arm", 1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Fire Power",      "+3% crit dmg/rank",       1,0, 3, 0, 0, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Blazing Speed",   "+3% atkSpd +2% dodge/rank",1,2, 2, 0, 2, 0xFF, P(TB_PCT_ATKSPD,3),    P(TB_FLAT_DODGE,2), NONE),
            NODE("Hot Streak",      "+2% crit +2% atkSpd/rank",2,0, 3, 0, 4, 0xFF, P(TB_FLAT_CRIT,2),       P(TB_PCT_ATKSPD,2), NONE),
            NODE("Combustion",      "Active: +100% critDmg",   2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("World in Flames", "+5% INT +3% HP/rank",     2,2, 2, 0, 5, 0xFF, P(TB_PCT_INT,5),         P(TB_PCT_HP,3), NONE),
            NODE("Firestarter",     "+3% spellDmg +2% critDmg/r",3,0, 3, 0, 6, 0xFF, P(TB_PCT_SPELL_DMG,3), P(TB_PCT_CRIT_DMG,2), NONE),
            NODE("Cauterize",       "+3% HP +2% crit/rank",    3,1, 2, 0,-1, 0xFF, P(TB_PCT_HP,3),           P(TB_FLAT_CRIT,2), NONE),
            NODE("Pyromaniac",      "+5% critDmg +3% INT/rank",3,2, 2, 0,-1, 0xFF, P(TB_PCT_CRIT_DMG,5),    P(TB_PCT_INT,3), NONE),
            NODE("Meteor",          "Active: 600% + stun 2t",  4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Sun King's",      "+4% spellDmg +1% crit/rank",4,0, 3, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,4), P(TB_FLAT_CRIT,1), NONE),
            NODE("Conflagration",   "+5% INT +3% critDmg/rank",4,2, 2, 0,11, 0xFF, P(TB_PCT_INT,5),         P(TB_PCT_CRIT_DMG,3), NONE),
        }},
        /* ── Frost: Defensive caster, control, sustained damage ── */
        { .name = "Frost", .color = CP_CYAN, .nodeCount = 15, .nodes = {
            NODE("Frostbite",       "+3% INT/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_INT,3),         NONE, NONE),
            NODE("Ice Shards",      "+3% spell dmg/rank",      0,1, 3, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,3),   NONE, NONE),
            NODE("Frost Armor",     "+5% armor/rank",          0,2, 2, 0,-1, 0xFF, P(TB_PCT_ARMOR,5),       NONE, NONE),
            NODE("Ice Lance",       "Active: 200% + slow",     1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Brain Freeze",    "+2% crit +2% HP/rank",    1,0, 3, 0, 0, 0xFF, P(TB_FLAT_CRIT,2),       P(TB_PCT_HP,2), NONE),
            NODE("Permafrost",      "+5% dodge +3% armor/rank",1,2, 2, 0, 2, 0xFF, P(TB_PCT_DODGE,5),       P(TB_PCT_ARMOR,3), NONE),
            NODE("Shatter",         "+3% critDmg +2% crit/rank",2,0, 3, 0, 4, 0xFF, P(TB_PCT_CRIT_DMG,3),  P(TB_FLAT_CRIT,2), NONE),
            NODE("Icy Veins",       "Active: +35% dmg 6t",     2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Arctic Winds",    "+5% INT +3% WIS/rank",    2,2, 2, 0, 5, 0xFF, P(TB_PCT_INT,5),         P(TB_PCT_WIS,3), NONE),
            NODE("Fingers of Frost","+3% spellDmg +2% HP/rank",3,0, 3, 0, 6, 0xFF, P(TB_PCT_SPELL_DMG,3),   P(TB_PCT_HP,2), NONE),
            NODE("Ice Barrier",     "+5% HP +2% armor/rank",   3,1, 2, 0,-1, 0xFF, P(TB_PCT_HP,5),           P(TB_PCT_ARMOR,2), NONE),
            NODE("Cold Snap",       "+3% dodge +3% atkSpd/rank",3,2, 2, 0,-1, 0xFF, P(TB_FLAT_DODGE,3),     P(TB_PCT_ATKSPD,3), NONE),
            NODE("Glacial Spike",   "Active: 500% + stun 3t",  4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Comet Storm",     "+3% INT +2% spellDmg/rank",4,0, 3, 0, 9, 0xFF, P(TB_PCT_INT,3),        P(TB_PCT_SPELL_DMG,2), NONE),
            NODE("Deep Freeze",     "+5% HP +3% dodge/rank",   4,2, 2, 0,11, 0xFF, P(TB_PCT_HP,5),           P(TB_FLAT_DODGE,3), NONE),
        }},
        /* ── Arcane: Mana efficiency, versatility, XP ── */
        { .name = "Arcane", .color = CP_MAGENTA, .nodeCount = 15, .nodes = {
            NODE("Arcane Mind",     "+3% INT/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_INT,3),         NONE, NONE),
            NODE("Arcane Focus",    "+3% spell dmg/rank",      0,1, 3, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,3),   NONE, NONE),
            NODE("Arcane Fortitude","+5% WIS/rank",            0,2, 2, 0,-1, 0xFF, P(TB_PCT_WIS,5),         NONE, NONE),
            NODE("Arcane Blast",    "Active: 220% + mana 10%", 1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Arcane Instab.",  "+3% crit +2% critDmg/rank",1,0, 3, 0, 0, 0xFF, P(TB_FLAT_CRIT,3),     P(TB_PCT_CRIT_DMG,2), NONE),
            NODE("Presence of Mind","+3% XP +2% atkSpd/rank",  1,2, 2, 0, 2, 0xFF, P(TB_PCT_XP,3),          P(TB_PCT_ATKSPD,2), NONE),
            NODE("Nether Tempest",  "+3% critDmg +2% atkSpd/r",2,0, 3, 0, 4, 0xFF, P(TB_PCT_CRIT_DMG,3),    P(TB_PCT_ATKSPD,2), NONE),
            NODE("Arcane Power",    "Active: +45% dmg 5t",     2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Arcane Empow.",   "+5% INT +3% WIS/rank",    2,2, 2, 0, 5, 0xFF, P(TB_PCT_INT,5),         P(TB_PCT_WIS,3), NONE),
            NODE("Arcane Harmony",  "+3% spellDmg +2% crit/rank",3,0, 3, 0, 6, 0xFF, P(TB_PCT_SPELL_DMG,3), P(TB_FLAT_CRIT,2), NONE),
            NODE("Mana Shield",     "+5% HP +2% atkSpd/rank",  3,1, 2, 0,-1, 0xFF, P(TB_PCT_HP,5),           P(TB_PCT_ATKSPD,2), NONE),
            NODE("Time Warp",       "+5% atkSpd +2% XP/rank",  3,2, 2, 0,-1, 0xFF, P(TB_PCT_ATKSPD,5),      P(TB_PCT_XP,2), NONE),
            NODE("Evocation",       "Active: heal 35%+mana60%",4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Arcane Mastery",  "+4% spellDmg +1% critDmg/r",4,0, 3, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,4), P(TB_PCT_CRIT_DMG,1), NONE),
            NODE("Enlightenment",   "+5% INT +3% XP/rank",     4,2, 2, 0,11, 0xFF, P(TB_PCT_INT,5),         P(TB_PCT_XP,3), NONE),
        }},
    }},

    /* ════════════════════════ PRIEST ════════════════════════ */
    [CLASS_PRIEST] = { .trees = {
        /* ── Shadow: Aggressive void caster, crits, speed ── */
        { .name = "Shadow", .color = CP_MAGENTA, .nodeCount = 15, .nodes = {
            NODE("Spirit Tap",      "+3% WIS/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_WIS,3),         NONE, NONE),
            NODE("Shadow Focus",    "+3% spell dmg/rank",      0,1, 3, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,3),   NONE, NONE),
            NODE("Darkness",        "+3% atkSpd +2% crit/rank",0,2, 2, 0,-1, 0xFF, P(TB_PCT_ATKSPD,3),      P(TB_FLAT_CRIT,2), NONE),
            NODE("Mind Blast",      "Active: 280% ignore arm", 1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Shadow Weaving",  "+3% crit dmg/rank",       1,0, 3, 0, 0, 0xFF, P(TB_PCT_CRIT_DMG,3),    NONE, NONE),
            NODE("Misery",          "+3% INT +2% atkSpd/rank", 1,2, 2, 0, 2, 0xFF, P(TB_PCT_INT,3),         P(TB_PCT_ATKSPD,2), NONE),
            NODE("Mind Spike",      "+2% crit +2% atkSpd/rank",2,0, 3, 0, 4, 0xFF, P(TB_FLAT_CRIT,2),       P(TB_PCT_ATKSPD,2), NONE),
            NODE("Void Eruption",   "Active: +45% dmg 6t",     2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Vampiric Touch",  "+5% WIS +3% HP/rank",     2,2, 2, 0, 5, 0xFF, P(TB_PCT_WIS,5),         P(TB_PCT_HP,3), NONE),
            NODE("Shadowform",      "+3% spellDmg +2% dodge/r",3,0, 3, 0, 6, 0xFF, P(TB_PCT_SPELL_DMG,3),   P(TB_FLAT_DODGE,2), NONE),
            NODE("Shadow Mastery",  "+3% crit +2% critDmg/rank",3,1, 2, 0,-1, 0xFF, P(TB_FLAT_CRIT,3),     P(TB_PCT_CRIT_DMG,2), NONE),
            NODE("Torment",         "+5% critDmg +3% INT/rank",3,2, 2, 0,-1, 0xFF, P(TB_PCT_CRIT_DMG,5),    P(TB_PCT_INT,3), NONE),
            NODE("SW: Death",       "Active: 500% if <20%",     4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Ancient Madness", "+4% spellDmg +1% atkSpd/r",4,0, 3, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,4),  P(TB_PCT_ATKSPD,1), NONE),
            NODE("Void Lord",       "+5% WIS +3% critDmg/rank",4,2, 2, 0,11, 0xFF, P(TB_PCT_WIS,5),         P(TB_PCT_CRIT_DMG,3), NONE),
        }},
        /* ── Discipline: Heal-through-damage, balanced hybrid ── */
        { .name = "Discipline", .color = CP_YELLOW, .nodeCount = 15, .nodes = {
            NODE("Meditation",      "+3% WIS/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_WIS,3),         NONE, NONE),
            NODE("Twin Disciplines","+3% heal/rank",           0,1, 3, 0,-1, 0xFF, P(TB_PCT_HEAL,3),        NONE, NONE),
            NODE("Mental Agility",  "+5% INT/rank",            0,2, 2, 0,-1, 0xFF, P(TB_PCT_INT,5),         NONE, NONE),
            NODE("Penance",         "Active: 3x160% + heal",   1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Focused Will",    "+3% HP +2% armor/rank",   1,0, 3, 0, 0, 0xFF, P(TB_PCT_HP,3),           P(TB_PCT_ARMOR,2), NONE),
            NODE("Enlightenment",   "+3% XP +2% spellDmg/rank",1,2, 2, 0, 2, 0xFF, P(TB_PCT_XP,3),         P(TB_PCT_SPELL_DMG,2), NONE),
            NODE("Imp Penance",     "+3% spellDmg +2% heal/r", 2,0, 3, 0, 4, 0xFF, P(TB_PCT_SPELL_DMG,3),   P(TB_PCT_HEAL,2), NONE),
            NODE("Power Infusion",  "Active: +35% dmg 6t",     2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Borrowed Time",   "+5% WIS +3% INT/rank",    2,2, 2, 0, 5, 0xFF, P(TB_PCT_WIS,5),         P(TB_PCT_INT,3), NONE),
            NODE("Grace",           "+3% heal +2% HP/rank",    3,0, 3, 0, 6, 0xFF, P(TB_PCT_HEAL,3),        P(TB_PCT_HP,2), NONE),
            NODE("Rapture",         "+3% HP +3% armor/rank",   3,1, 2, 0,-1, 0xFF, P(TB_PCT_HP,3),           P(TB_PCT_ARMOR,3), NONE),
            NODE("Atonement",       "+5% spellDmg +3% critDmg/r",3,2, 2, 0,-1, 0xFF, P(TB_PCT_SPELL_DMG,5), P(TB_PCT_CRIT_DMG,3), NONE),
            NODE("Pain Suppression","Active: immune3t+heal20%", 4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Strength of Soul","+3% heal +2% HP/rank",    4,0, 3, 0,-1, 0xFF, P(TB_PCT_HEAL,3),        P(TB_PCT_HP,2), NONE),
            NODE("Clarity of Will", "+5% WIS +3% spellDmg/rank",4,2, 2, 0,11, 0xFF, P(TB_PCT_WIS,5),        P(TB_PCT_SPELL_DMG,3), NONE),
        }},
        /* ── Holy: Pure healing, maximum survivability ── */
        { .name = "Holy", .color = CP_WHITE, .nodeCount = 15, .nodes = {
            NODE("Holy Spec",       "+3% WIS/rank",            0,0, 3, 0,-1, 0xFF, P(TB_PCT_WIS,3),         NONE, NONE),
            NODE("Divine Fury",     "+3% heal/rank",           0,1, 3, 0,-1, 0xFF, P(TB_PCT_HEAL,3),        NONE, NONE),
            NODE("Blessed Recovery","+5% VIT/rank",            0,2, 2, 0,-1, 0xFF, P(TB_PCT_VIT,5),         NONE, NONE),
            NODE("Holy Fire",       "Active: 200% + DoT",      1,1, 1, 1, 1, 0,    NONE,                    NONE, NONE),
            NODE("Searing Light",   "+3% heal +2% spellDmg/rank",1,0, 3, 0, 0, 0xFF, P(TB_PCT_HEAL,3),     P(TB_PCT_SPELL_DMG,2), NONE),
            NODE("Inspiration",     "+3% HP +2% armor/rank",   1,2, 2, 0, 2, 0xFF, P(TB_PCT_HP,3),           P(TB_PCT_ARMOR,2), NONE),
            NODE("Surge of Light",  "+3% heal +2% dodge/rank", 2,0, 3, 0, 4, 0xFF, P(TB_PCT_HEAL,3),        P(TB_FLAT_DODGE,2), NONE),
            NODE("Guardian Spirit", "Active: immune+heal30%",   2,1, 1, 1, 3, 1,    NONE,                    NONE, NONE),
            NODE("Spiritual Guide", "+5% WIS +3% VIT/rank",    2,2, 2, 0, 5, 0xFF, P(TB_PCT_WIS,5),         P(TB_PCT_VIT,3), NONE),
            NODE("Empowered Heal",  "+3% heal +2% HP/rank",    3,0, 3, 0, 6, 0xFF, P(TB_PCT_HEAL,3),        P(TB_PCT_HP,2), NONE),
            NODE("Body and Soul",   "+3% dodge +3% HP/rank",   3,1, 2, 0,-1, 0xFF, P(TB_FLAT_DODGE,3),      P(TB_PCT_HP,3), NONE),
            NODE("Spirit of Redemp","+5% HP +2% armor/rank",   3,2, 2, 0,-1, 0xFF, P(TB_PCT_HP,5),           P(TB_PCT_ARMOR,2), NONE),
            NODE("Divine Hymn",     "Active: heal8%/t+immune",  4,1, 1, 1,10, 2,    NONE,                    NONE, NONE),
            NODE("Circle of Heal",  "+4% heal +1% WIS/rank",   4,0, 3, 0,-1, 0xFF, P(TB_PCT_HEAL,4),        P(TB_PCT_WIS,1), NONE),
            NODE("Apotheosis",      "+5% WIS +3% VIT/rank",    4,2, 2, 0,11, 0xFF, P(TB_PCT_WIS,5),         P(TB_PCT_VIT,3), NONE),
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
