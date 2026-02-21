/*
 * hero.c — Hero creation, stat computation, talent allocation, equip logic.
 *
 * Key design decisions:
 *
 * hero_effective_stats() is the SINGLE SOURCE OF TRUTH for all derived
 * combat stats. It recomputes everything from base + talents + equipment
 * each call. This avoids stale-cache bugs at the cost of recalculation
 * (trivial for 7 stats × 7 slots).
 *
 * Talent soft cap: past SOFT_CAP (30), overflow is halved with integer
 * division: SOFT_CAP + (overflow / 2). This means every OTHER point past
 * 30 produces no visible stat change (e.g. 31→15+30=45, 32→16+30=46,
 * but the float formulas may not produce a different result for +1).
 * The UI searches for the minimum increment that actually changes a stat.
 *
 * hero_equip() auto-moves the old equipped item to inventory. The CALLER
 * must then remove the source item from inventory to avoid duplication.
 */
#include "game.h"
#include <string.h>
#include <math.h>

Hero hero_create(int classId, const char *name) {
    Hero h;
    memset(&h, 0, sizeof(h));
    strncpy(h.name, name, MAX_NAME - 1);
    h.classId = classId;
    h.level = 1;

    const ClassDef *cd = data_class(classId);
    h.hp       = cd->baseHp;
    h.maxHp    = cd->baseHp;
    h.resource = cd->baseResource;
    h.maxResource = cd->maxResource;
    h.highestLevel = 1;

    for (int i = 0; i < MAX_SKILL_TIERS; i++)
        h.skillChoices[i] = -1;

    return h;
}

EStats hero_effective_stats(const Hero *h) {
    EStats es;
    memset(&es, 0, sizeof(es));
    const ClassDef *cd = data_class(h->classId);

    for (int i = 0; i < NUM_STATS; i++) {
        int base = cd->baseStats[i];
        int tal  = h->talents[i];
        if (tal > SOFT_CAP) tal = SOFT_CAP + (tal - SOFT_CAP) / 2;

        int eq = 0;
        for (int s = 0; s < NUM_SLOTS; s++)
            if (h->hasEquip[s]) eq += h->equipment[s].stats[i];

        es.stats[i] = base + tal + eq;
    }

    int pri = es.stats[cd->primaryStat];
    es.maxHp = cd->baseHp + es.stats[VIT] * 5 + es.stats[STR];
    es.damage = (int)(pri * 1.5f + es.stats[STR] * 0.3f);
    es.critChance = fminf(0.50f, es.stats[AGI] / 200.0f);
    es.critMult   = 1.5f;
    es.dodgeChance = fminf(0.35f, es.stats[AGI] / 300.0f);
    es.blockChance = (h->classId == CLASS_WARRIOR) ? fminf(0.30f, es.stats[DEF] / 250.0f) : 0;
    es.blockReduction = 0.30f;
    es.dmgReduction   = fminf(0.50f, es.stats[DEF] / (es.stats[DEF] + 100.0f));
    es.tickRate = 800 - es.stats[SPD] * 8;
    if (es.tickRate < 300) es.tickRate = 300;
    es.healPower = 0;
    es.xpMultiplier = 1.0f + es.stats[INT_] * 0.002f;
    es.flatDmgReduce = (int)(es.stats[WIS] * 0.15f);

    switch (h->classId) {
    case CLASS_MAGE:
        es.damage = (int)(es.stats[INT_] * 1.8f + es.stats[WIS] * 0.3f);
        break;
    case CLASS_PRIEST:
        es.damage = (int)(es.stats[WIS] * 1.2f + es.stats[INT_] * 0.5f);
        es.healPower = (int)(es.stats[WIS] * 1.5f + es.stats[INT_] * 0.3f);
        break;
    case CLASS_ROGUE:
        es.damage = (int)(es.stats[AGI] * 1.4f + es.stats[STR] * 0.4f);
        es.critMult = 1.8f;
        break;
    default:
        break;
    }
    return es;
}

int hero_xp_needed(int level) {
    return 50 * level + 50;
}

float hero_xp_pct(const Hero *h) {
    int needed = hero_xp_needed(h->level);
    if (needed <= 0) return 0;
    return (float)h->xp / needed;
}

int hero_add_xp(GameState *gs, int amount) {
    Hero *h = &gs->hero;
    h->xp += amount;
    h->totalXpEarned += amount;
    int leveled = 0;

    while (h->xp >= hero_xp_needed(h->level)) {
        h->xp -= hero_xp_needed(h->level);
        h->level++;
        h->talentPoints++;
        leveled = 1;
        if (h->level > h->highestLevel) h->highestLevel = h->level;

        EStats es = hero_effective_stats(h);
        h->maxHp = es.maxHp;
        h->hp = h->maxHp;
        h->resource = h->maxResource;

        char buf[LOG_LINE_W + 1];
        snprintf(buf, sizeof(buf), "** LEVEL %d! ** (+1 Talent Point)", h->level);
        ui_log(gs, buf, CP_YELLOW);


    }
    return leveled;
}

int hero_alloc_talent(Hero *h, int stat) {
    if (h->talentPoints <= 0 || stat < 0 || stat >= NUM_STATS) return 0;
    h->talents[stat]++;
    h->talentPoints--;
    EStats es = hero_effective_stats(h);
    h->maxHp = es.maxHp;
    return 1;
}

int hero_equip(Hero *h, const ItemDef *item) {
    if (!item || item->levelReq > h->level) return 0;
    int slot = item->slot;

    if (h->hasEquip[slot]) {
        if (h->invCount >= MAX_INVENTORY) return 0;
        h->inventory[h->invCount++] = h->equipment[slot];
    }

    h->equipment[slot] = *item;
    h->hasEquip[slot] = 1;

    EStats es = hero_effective_stats(h);
    h->maxHp = es.maxHp;
    if (h->hp > h->maxHp) h->hp = h->maxHp;
    return 1;
}

void hero_unequip(Hero *h, int slot) {
    if (slot < 0 || slot >= NUM_SLOTS || !h->hasEquip[slot]) return;
    if (h->invCount >= MAX_INVENTORY) return;

    h->inventory[h->invCount++] = h->equipment[slot];
    h->hasEquip[slot] = 0;
    memset(&h->equipment[slot], 0, sizeof(ItemDef));

    EStats es = hero_effective_stats(h);
    h->maxHp = es.maxHp;
    if (h->hp > h->maxHp) h->hp = h->maxHp;
}

void hero_heal(Hero *h, int amount) {
    h->hp += amount;
    if (h->hp > h->maxHp) h->hp = h->maxHp;
}

void hero_restore_resource(Hero *h, int amount) {
    h->resource += amount;
    if (h->resource > h->maxResource) h->resource = h->maxResource;
}
