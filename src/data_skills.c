/*
 * data_skills.c — Skill tree definitions for all four classes.
 *
 * Each class has 6 tiers (unlocked at levels 10-60) with 2 options per tier.
 * Skills are data-driven: combat.c interprets the fields generically.
 */
#include "game.h"

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
