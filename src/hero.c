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
 * Talent soft cap: past SOFT_CAP (30), overflow is halved
 * (SOFT_CAP + (overflow / 2)). Every point past 30 still counts
 * (no dead zones from integer division). The UI searches for the minimum
 * increment that actually changes a derived stat.
 *
 * hero_equip() auto-moves the old equipped item to inventory. The CALLER
 * must then remove the source item from inventory to avoid duplication.
 */
#include "game.h"
#include <string.h>
#include <math.h>

/* Zero-initialize hero and set class defaults. skillChoices[] start at -1 (unchosen). */
Hero hero_create(int classId, const char *name) {
    Hero h;
    memset(&h, 0, sizeof(h));
    strncpy(h.name, name, MAX_NAME - 1);
    h.name[MAX_NAME - 1] = '\0';
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

/*
 * Single source of truth for all derived combat stats. Recomputed from scratch
 * each call (no caching) to avoid stale values after talent/equip changes.
 *
 * Stat pipeline:
 *   1. For each stat: base + soft-capped talents + equipment sum
 *      Soft cap: past 30 pts, overflow is halved via integer division.
 *   2. Primary-stat-based damage (class-specific: mage INT*1.5+WIS*0.5, priest WIS*1.2+INT*0.5, rogue AGI*1.4+STR*0.4)
 *   3. Derived rates: crit (AGI/300), dodge (AGI/300), block (DEF/250, warrior only),
 *      DR (DEF/(DEF+100)), tick rate (asymptotic 150+650/(1+SPD*0.02)),
 *      XP multiplier (1 + min(INT*0.005, 0.50)), flat damage reduce (WIS*0.15)
 */
EStats hero_effective_stats(const Hero *h) {
    EStats es;
    memset(&es, 0, sizeof(es));
    const ClassDef *cd = data_class(h->classId);

    int talFlat[NUM_STATS] = {0};
    int talFlatHp = 0, talFlatCrit = 0, talFlatDodge = 0, talFlatBlock = 0, talFlatDR = 0;
    int pctPhysDmg=0, pctSpellDmg=0, pctHeal=0, pctHp=0, pctCritDmg=0;
    int pctDodge=0, pctArmor=0, pctAtkSpd=0, pctXp=0, pctGold=0;
    int pctStat[NUM_STATS] = {0};

    for (int t = 0; t < NUM_TALENT_TREES; t++) {
        const TalentTreeDef *td = data_talent_tree(h->classId, t);
        if (!td) continue;
        for (int n = 0; n < td->nodeCount; n++) {
            int ranks = h->talentRanks[t][n];
            if (ranks == 0) continue;
            const TalentNodeDef *nd = &td->nodes[n];
            for (int b = 0; b < MAX_TALENT_BONUSES; b++) {
                int val = nd->bonus[b].perRank * ranks;
                switch (nd->bonus[b].type) {
                case TB_NONE: break;
                case TB_FLAT_STR: talFlat[STR] += val; break;
                case TB_FLAT_AGI: talFlat[AGI] += val; break;
                case TB_FLAT_INT: talFlat[INT_]+= val; break;
                case TB_FLAT_WIS: talFlat[WIS] += val; break;
                case TB_FLAT_VIT: talFlat[VIT] += val; break;
                case TB_FLAT_DEF: talFlat[DEF] += val; break;
                case TB_FLAT_SPD: talFlat[SPD] += val; break;
                case TB_FLAT_HP:       talFlatHp    += val; break;
                case TB_FLAT_CRIT:     talFlatCrit  += val; break;
                case TB_FLAT_DODGE:    talFlatDodge += val; break;
                case TB_FLAT_BLOCK:    talFlatBlock += val; break;
                case TB_FLAT_DMGREDUCE:talFlatDR    += val; break;
                case TB_PCT_PHYS_DMG:  pctPhysDmg   += val; break;
                case TB_PCT_SPELL_DMG: pctSpellDmg  += val; break;
                case TB_PCT_HEAL:      pctHeal      += val; break;
                case TB_PCT_HP:        pctHp        += val; break;
                case TB_PCT_CRIT_DMG:  pctCritDmg   += val; break;
                case TB_PCT_DODGE:     pctDodge     += val; break;
                case TB_PCT_ARMOR:     pctArmor     += val; break;
                case TB_PCT_ATKSPD:    pctAtkSpd    += val; break;
                case TB_PCT_XP:        pctXp        += val; break;
                case TB_PCT_GOLD:      pctGold      += val; break;
                case TB_PCT_STR: pctStat[STR] += val; break;
                case TB_PCT_AGI: pctStat[AGI] += val; break;
                case TB_PCT_INT: pctStat[INT_]+= val; break;
                case TB_PCT_WIS: pctStat[WIS] += val; break;
                case TB_PCT_VIT: pctStat[VIT] += val; break;
                case TB_PCT_DEF: pctStat[DEF] += val; break;
                }
            }
        }
    }

    for (int i = 0; i < NUM_STATS; i++) {
        int base = cd->baseStats[i];
        int oldTal = h->talents[i];
        if (oldTal > SOFT_CAP) oldTal = SOFT_CAP + (oldTal - SOFT_CAP) / 2;

        int eq = 0;
        for (int s = 0; s < NUM_SLOTS; s++)
            if (h->hasEquip[s]) eq += h->equipment[s].stats[i];

        es.stats[i] = base + oldTal + talFlat[i] + eq;
    }

    /* Apply % stat talents — scales with equipment, applied before deriving combat stats */
    for (int i = 0; i < NUM_STATS; i++)
        if (pctStat[i] > 0)
            es.stats[i] = es.stats[i] * (100 + pctStat[i]) / 100;

    int pri = es.stats[cd->primaryStat];
    es.maxHp = cd->baseHp + es.stats[VIT] * 5 + es.stats[STR] * 2 + talFlatHp;
    es.damage = (int)(pri * 1.5f + es.stats[AGI] * 0.2f);
    es.critChance = fminf(0.50f, es.stats[AGI] / 300.0f + talFlatCrit / 100.0f);
    es.critMult   = 1.5f;
    es.dodgeChance = fminf(0.35f, es.stats[AGI] / 300.0f + talFlatDodge / 100.0f);
    es.blockChance = (h->classId == CLASS_WARRIOR)
        ? fminf(0.30f, es.stats[DEF] / 250.0f + talFlatBlock / 100.0f) : 0;
    es.blockReduction = 0.30f;
    es.dmgReduction   = fminf(0.50f, es.stats[DEF] / (es.stats[DEF] + 100.0f));
    es.tickRate = 150 + (int)(650.0f / (1.0f + es.stats[SPD] * 0.02f));
    es.healPower = 0;
    es.xpMultiplier = 1.0f + fminf(0.50f, es.stats[INT_] * 0.005f);
    es.flatDmgReduce = (int)(es.stats[WIS] * 0.15f) + talFlatDR;

    switch (h->classId) {
    case CLASS_MAGE:
        es.damage = (int)(es.stats[INT_] * 1.5f + es.stats[WIS] * 0.4f);
        break;
    case CLASS_PRIEST:
        es.damage = (int)(es.stats[WIS] * 1.4f + es.stats[INT_] * 0.2f);
        es.healPower = (int)(es.stats[WIS] * 1.5f + es.stats[INT_] * 0.3f);
        break;
    case CLASS_ROGUE:
        es.damage = (int)(es.stats[AGI] * 1.5f + es.stats[STR] * 0.2f);
        es.critMult = 1.8f;
        break;
    default:
        break;
    }

    int isPhysical = (h->classId == CLASS_WARRIOR || h->classId == CLASS_ROGUE);
    if (isPhysical && pctPhysDmg > 0)
        es.damage = es.damage * (100 + pctPhysDmg) / 100;
    if (!isPhysical && pctSpellDmg > 0)
        es.damage = es.damage * (100 + pctSpellDmg) / 100;
    if (pctHeal > 0 && es.healPower > 0)
        es.healPower = es.healPower * (100 + pctHeal) / 100;
    if (pctHp > 0)
        es.maxHp = es.maxHp * (100 + pctHp) / 100;
    if (pctCritDmg > 0)
        es.critMult += pctCritDmg / 100.0f;
    if (pctDodge > 0)
        es.dodgeChance = fminf(0.50f, es.dodgeChance + pctDodge / 100.0f);
    if (pctArmor > 0)
        es.dmgReduction = fminf(0.60f, es.dmgReduction * (100 + pctArmor) / 100.0f);
    if (pctAtkSpd > 0)
        es.tickRate = (int)(es.tickRate * 100.0f / (100 + pctAtkSpd));
    if (pctXp > 0)
        es.xpMultiplier *= (100 + pctXp) / 100.0f;
    (void)pctGold;

    int achBonusHp = 0, achBonusDmgPct = 0, achBonusXpPct = 0;
    for (int a = 0; a < NUM_ACHIEVEMENTS; a++) {
        if (!ACH_HAS(h, a)) continue;
        const AchievementDef *ad = data_achievement(a);
        if (!ad) continue;
        switch (ad->bonusType) {
        case 0: achBonusHp     += ad->bonusValue; break;
        case 1: achBonusDmgPct += ad->bonusValue; break;
        case 2: achBonusXpPct  += ad->bonusValue; break;
        }
    }
    es.maxHp += achBonusHp;
    if (achBonusDmgPct > 0)
        es.damage = es.damage * (100 + achBonusDmgPct) / 100;
    if (achBonusXpPct > 0)
        es.xpMultiplier *= (100 + achBonusXpPct) / 100.0f;

    return es;
}

/* Quadratic + cubic XP curve: early levels are quick, late levels are grindy.
 * Lv10: 850, Lv50: 50,050, Lv90: 259,250. Total 1→99 ≈ 8.7M XP. */
int hero_xp_needed(int level) {
    return 5 * level * level + (int)(0.3f * level * level * level) + 50;
}

float hero_xp_pct(const Hero *h) {
    int needed = hero_xp_needed(h->level);
    if (needed <= 0) return 0;
    return (float)h->xp / needed;
}

/*
 * Grant XP with auto-level-up loop: drains XP threshold, increments level,
 * awards 1 talent point, and fully restores HP + resource each level.
 * Returns 1 if at least one level was gained.
 */
int hero_add_xp(GameState *gs, int amount) {
    Hero *h = &gs->hero;
    h->xp += amount;
    h->totalXpEarned += amount;
    int leveled = 0;

    while (h->xp >= hero_xp_needed(h->level) && h->level < MAX_LEVEL) {
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

static const int TIER_GATE[TALENT_TIERS] = { 0, 5, 10, 15, 20 };

int hero_total_talent_points_spent(const Hero *h) {
    int total = 0;
    for (int t = 0; t < NUM_TALENT_TREES; t++)
        total += h->talentTreePoints[t];
    return total;
}

int hero_can_invest_talent(const Hero *h, int tree, int node) {
    if (h->talentPoints <= 0) return 0;
    if (tree < 0 || tree >= NUM_TALENT_TREES) return 0;

    const TalentNodeDef *nd = data_talent_node(h->classId, tree, node);
    if (!nd) return 0;
    if (h->talentRanks[tree][node] >= nd->maxRank) return 0;
    if (h->talentTreePoints[tree] < TIER_GATE[nd->tier]) return 0;
    if (nd->prereqNode >= 0) {
        const TalentNodeDef *pre = data_talent_node(h->classId, tree, nd->prereqNode);
        if (!pre || h->talentRanks[tree][nd->prereqNode] < pre->maxRank) return 0;
    }
    return 1;
}

int hero_invest_talent(Hero *h, int tree, int node) {
    if (!hero_can_invest_talent(h, tree, node)) return 0;
    h->talentRanks[tree][node]++;
    h->talentTreePoints[tree]++;
    h->talentPoints--;
    EStats es = hero_effective_stats(h);
    h->maxHp = es.maxHp;
    if (h->hp > h->maxHp) h->hp = h->maxHp;
    return 1;
}

static int can_uninvest_node(const Hero *h, int tree, int node) {
    if (h->talentRanks[tree][node] <= 0) return 0;

    const TalentTreeDef *td = data_talent_tree(h->classId, tree);
    if (!td) return 0;

    uint8_t simRanks[TALENT_NODES_PER_TREE];
    for (int i = 0; i < TALENT_NODES_PER_TREE; i++)
        simRanks[i] = h->talentRanks[tree][i];
    simRanks[node]--;

    int simTreePts = h->talentTreePoints[tree] - 1;

    for (int i = 0; i < td->nodeCount; i++) {
        if (simRanks[i] == 0) continue;
        const TalentNodeDef *nd = &td->nodes[i];

        if (simTreePts < TIER_GATE[nd->tier] && nd->tier > 0) {
            int ptsBelow = 0;
            for (int j = 0; j < td->nodeCount; j++)
                if (td->nodes[j].tier < nd->tier) ptsBelow += simRanks[j];
            if (ptsBelow < TIER_GATE[nd->tier]) return 0;
        }

        if (nd->prereqNode >= 0) {
            const TalentNodeDef *pre = &td->nodes[nd->prereqNode];
            if (simRanks[nd->prereqNode] < pre->maxRank) return 0;
        }
    }
    return 1;
}

int hero_uninvest_talent(Hero *h, int tree, int node) {
    if (!can_uninvest_node(h, tree, node)) return 0;
    h->talentRanks[tree][node]--;
    h->talentTreePoints[tree]--;
    h->talentPoints++;
    EStats es = hero_effective_stats(h);
    h->maxHp = es.maxHp;
    if (h->hp > h->maxHp) h->hp = h->maxHp;
    return 1;
}

void hero_reset_talents(Hero *h) {
    int refunded = 0;
    for (int t = 0; t < NUM_TALENT_TREES; t++) {
        refunded += h->talentTreePoints[t];
        h->talentTreePoints[t] = 0;
        for (int n = 0; n < TALENT_NODES_PER_TREE; n++)
            h->talentRanks[t][n] = 0;
    }
    h->talentPoints += refunded;
    memset(h->activeSkillCooldowns, 0, sizeof(h->activeSkillCooldowns));
    EStats es = hero_effective_stats(h);
    h->maxHp = es.maxHp;
    if (h->hp > h->maxHp) h->hp = h->maxHp;
}

int hero_collect_active_skills(const Hero *h, const SkillDef **out, int *cooldowns, int max) {
    int count = 0;
    for (int t = 0; t < NUM_TALENT_TREES && count < max; t++) {
        const TalentTreeDef *td = data_talent_tree(h->classId, t);
        if (!td) continue;
        for (int n = 0; n < td->nodeCount && count < max; n++) {
            if (h->talentRanks[t][n] == 0) continue;
            const TalentNodeDef *nd = &td->nodes[n];
            if (!nd->isActive) continue;
            const SkillDef *sk = data_talent_skill(h->classId, t, n);
            if (!sk) continue;
            out[count] = sk;
            cooldowns[count] = h->activeSkillCooldowns[count];
            count++;
        }
    }
    return count;
}

/*
 * Equip an item. If the slot is occupied, the old item is auto-moved to inventory.
 * IMPORTANT: The caller must remove the source item from inventory afterward,
 * because this function copies *item into the slot (it doesn't move it).
 * Returns 0 if level requirement not met or inventory full (can't stash old item).
 */
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
