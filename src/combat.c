/*
 * combat.c — Auto-combat tick loop, skill engine, loot system.
 *
 * Key design decisions:
 *
 * MOB_MAX_RARITY[] — per-dungeon cap on normal mob drop rarity. Bosses
 * get +1 tier above the cap. Prevents early dungeons from dropping
 * endgame gear.
 *
 * Boss loot uses weighted rarity selection (RARITY_WEIGHTS), not uniform
 * random. Common drops are 25× more likely than Legendary.
 *
 * apply_skill() is a generic interpreter for SkillDef — it handles damage,
 * multi-hit, armor ignore, stun, buffs, shields, DoTs, heals, and mana
 * restore through field checks rather than per-skill code paths.
 *
 * The "goto enemy_killed" label exists because skills fire mid-tick before
 * the normal attack. If a skill kills the enemy, we jump to the kill
 * reward logic rather than continuing to the normal attack.
 *
 * Absorb shields (shieldHp on Buff) are checked before HP damage in the
 * enemy attack resolution. Multiple shields stack and drain in order.
 *
 * Gold gain is divided by 4 with a floor of 1 to slow economy progression.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static float randf(void) { return (float)rand() / RAND_MAX; } /* [0.0, 1.0] */

/* Apply ±10% random variance to a damage value (minimum variance of 1). */
static int dmg_variance(int base) {
    int var = (int)(base * 0.1f);
    if (var < 1) var = 1;
    return base + (rand() % (var * 2 + 1)) - var;
}

/* Spawn next enemy from the current dungeon's enemy pool, or the boss if kill count >= BOSS_THRESHOLD. */
void combat_spawn(GameState *gs) {
    const DungeonDef *dg = data_dungeon(gs->currentDungeon);
    if (!dg) return;

    int pick;
    int isBoss = (gs->dungeonKills >= BOSS_THRESHOLD);

    if (isBoss) {
        pick = dg->bossIdx;
        gs->bossActive = 1;
    } else {
        if (dg->numEnemies <= 0) return;
        pick = dg->enemyIdx[rand() % dg->numEnemies];
        gs->bossActive = 0;
    }

    const EnemyTemplate *et = data_enemy(pick);
    if (!et) return;

    Enemy *e = &gs->enemy;
    strncpy(e->name, et->name, MAX_NAME - 1);
    e->name[MAX_NAME - 1] = '\0';
    e->templateIdx = pick;
    e->maxHp     = et->hp;
    e->hp        = e->maxHp;
    e->attack    = et->attack;
    e->defense   = et->defense;
    e->speed     = et->speed;
    e->xpReward  = et->xpReward;
    e->goldReward= et->goldReward;
    e->dropChance= et->dropChance;
    e->stunned   = 0;
    e->slowed    = 0;

    gs->isElite = 0;
    if (!isBoss && randf() < 0.05f) {
        gs->isElite = 1;
        char eliteName[MAX_NAME];
        snprintf(eliteName, MAX_NAME, "Elite %s", et->name);
        strncpy(e->name, eliteName, MAX_NAME - 1);
        e->name[MAX_NAME - 1] = '\0';
        e->maxHp     *= 3;
        e->hp         = e->maxHp;
        e->attack    *= 3;
        e->defense   *= 3;
        e->xpReward  *= 3;
        e->goldReward*= 3;
        e->dropChance = 1.0f;
    }

    if (gs->hardModeActive) {
        int a0 = gs->activeAffixes[0], a1 = gs->activeAffixes[1];
        if (a0 == 0 || a1 == 0) { e->maxHp = e->maxHp * 130 / 100; e->hp = e->maxHp; }
        if (a0 == 1 || a1 == 1) { e->attack = e->attack * 120 / 100; }
    }

    gs->hasEnemy = 1;

    char buf[LOG_LINE_W + 1];
    if (isBoss) {
        snprintf(buf, sizeof(buf), "*** BOSS: %s *** (HP:%d)", e->name, e->maxHp);
        ui_log(gs, buf, CP_YELLOW);
    } else if (gs->isElite) {
        snprintf(buf, sizeof(buf), "!! ELITE: %s !! (HP:%d)", e->name, e->maxHp);
        ui_log(gs, buf, CP_YELLOW);
    } else {
        snprintf(buf, sizeof(buf), "A %s appears! (HP:%d)", e->name, e->maxHp);
        ui_log(gs, buf, CP_WHITE);
    }
}

void check_achievements(GameState *gs) {
    Hero *h = &gs->hero;
    int before[4];
    for (int i = 0; i < 4; i++) before[i] = h->achievements[i];

    if (h->totalKills >= 1)    ACH_SET(h, 0);
    if (h->totalKills >= 100)  ACH_SET(h, 1);
    if (h->totalKills >= 1000) ACH_SET(h, 2);
    if (h->totalKills >= 5000) ACH_SET(h, 3);

    int bossSum = 0;
    int allBosses = 1;
    for (int d = 0; d < NUM_DUNGEONS; d++) {
        bossSum += h->bossKills[d];
        if (h->bossKills[d] == 0) allBosses = 0;
    }
    if (bossSum >= 1)  ACH_SET(h, 4);
    if (bossSum >= 10) ACH_SET(h, 5);
    if (allBosses)     ACH_SET(h, 6);

    if (h->level >= 10) ACH_SET(h, 7);
    if (h->level >= 25) ACH_SET(h, 8);
    if (h->level >= 50) ACH_SET(h, 9);
    if (h->level >= 75) ACH_SET(h, 10);
    if (h->level >= 99) ACH_SET(h, 11);

    int allEquipped = 1;
    int allEpic = 1;
    int allLegendary = 1;
    for (int s = 0; s < NUM_SLOTS; s++) {
        if (!h->hasEquip[s]) { allEquipped = 0; allEpic = 0; allLegendary = 0; break; }
        if (h->equipment[s].rarity < RARITY_EPIC) allEpic = 0;
        if (h->equipment[s].rarity < RARITY_LEGENDARY) allLegendary = 0;
    }
    if (allEquipped)  ACH_SET(h, 12);
    if (allEpic)      ACH_SET(h, 13);
    if (allLegendary) ACH_SET(h, 14);

    if (h->totalGoldEarned >= 10000)  ACH_SET(h, 15);
    if (h->totalGoldEarned >= 100000) ACH_SET(h, 16);
    if (h->deaths >= 10) ACH_SET(h, 17);
    if (h->eliteKills >= 1) ACH_SET(h, 18);
    if (h->hardModeClears >= 1) ACH_SET(h, 19);

    for (int i = 0; i < 4; i++) {
        uint32_t fresh = h->achievements[i] & ~before[i];
        while (fresh) {
            int bit = __builtin_ctz(fresh);
            int idx = i * 32 + bit;
            if (idx < NUM_ACHIEVEMENTS) {
                const AchievementDef *ad = data_achievement(idx);
                if (ad) {
                    char buf[LOG_LINE_W + 1];
                    snprintf(buf, sizeof(buf), "[ACHIEVEMENT] %s!", ad->name);
                    ui_log(gs, buf, CP_YELLOW);
                }
            }
            fresh &= fresh - 1;
        }
    }
}

void combat_tick_buffs(GameState *gs) {
    Hero *h = &gs->hero;
    for (int i = h->numBuffs - 1; i >= 0; i--) {
        Buff *b = &h->buffs[i];
        if (b->healPerTick > 0) {
            int prevHp = h->hp;
            hero_heal(h, b->healPerTick);
            gs->combatHealed += h->hp - prevHp;
        }
        if (b->dmgPerTick > 0 && gs->hasEnemy) {
            gs->enemy.hp -= b->dmgPerTick;
        }
        b->ticksLeft--;
        if (b->ticksLeft <= 0) {
            for (int j = i; j < h->numBuffs - 1; j++)
                h->buffs[j] = h->buffs[j + 1];
            h->numBuffs--;
        }
    }
}

/* Check if any active buff has a specific boolean flag set. Exactly one check param should be 1. */
static int has_buff_flag(const Hero *h, int checkImmune, int checkManaShield, int checkDodge, int checkDoubleDmg) {
    for (int i = 0; i < h->numBuffs; i++) {
        if (checkImmune    && h->buffs[i].immune)      return 1;
        if (checkManaShield&& h->buffs[i].manaShield)  return 1;
        if (checkDodge     && h->buffs[i].dodgeNext)   return 1;
        if (checkDoubleDmg && h->buffs[i].doubleDmgNext) return 1;
    }
    return 0;
}

/* Sum a float field across all active buffs. field: 0=damageMul, 1=dodgeBonus, 2=critDmgBonus. */
static float total_buff_val(const Hero *h, int field) {
    float total = 0;
    for (int i = 0; i < h->numBuffs; i++) {
        switch (field) {
        case 0: total += h->buffs[i].damageMul;   break;
        case 1: total += h->buffs[i].dodgeBonus;  break;
        case 2: total += h->buffs[i].critDmgBonus; break;
        }
    }
    return total;
}

/* Consume (clear) a one-shot buff flag from the first buff that has it. */
static void consume_buff_flag(Hero *h, int consumeDodge, int consumeDouble) {
    for (int i = 0; i < h->numBuffs; i++) {
        if (consumeDodge  && h->buffs[i].dodgeNext)    { h->buffs[i].dodgeNext = 0; return; }
        if (consumeDouble && h->buffs[i].doubleDmgNext){ h->buffs[i].doubleDmgNext = 0; return; }
    }
}

/* Per-dungeon rarity cap for normal mob drops. Bosses get +1 tier (see try_boss_loot). */
static const int MOB_MAX_RARITY[NUM_DUNGEONS] = {
    RARITY_UNCOMMON, RARITY_UNCOMMON, RARITY_RARE, RARITY_RARE,
    RARITY_EPIC, RARITY_EPIC, RARITY_EPIC, RARITY_EPIC, RARITY_EPIC
};

/*
 * Boss guaranteed drop with weighted rarity selection.
 * Uses procedural generation for Common-Rare, static ITEMS[] for Epic-Legendary.
 * RARITY_WEIGHTS {50,30,12,6,2} make Common 25× more likely than Legendary.
 * Boss rarity cap = dungeon's mob cap + 1 (bosses can drop one tier higher).
 */
static void try_boss_loot(GameState *gs) {
    Hero *h = &gs->hero;
    if (h->invCount >= MAX_INVENTORY) return;
    const DungeonDef *dg = data_dungeon(gs->currentDungeon);
    if (!dg) return;

    int classMask = (1 << h->classId);
    int bossMaxRar = MOB_MAX_RARITY[gs->currentDungeon] + 1;
    if (bossMaxRar > RARITY_LEGENDARY) bossMaxRar = RARITY_LEGENDARY;

     int hasRarity[NUM_RARITIES] = {0};
    for (int r = 0; r <= bossMaxRar; r++)
        hasRarity[r] = 1;
    for (int i = 0; i < data_num_items(); i++) {
        const ItemDef *it = data_item(i);
        if (it->levelReq >= dg->levelReq && it->levelReq <= dg->levelReq + 20 &&
            it->rarity <= bossMaxRar &&
            (it->classMask == 0 || (it->classMask & classMask)))
            hasRarity[it->rarity] = 1;
    }

    static const int RARITY_WEIGHTS[NUM_RARITIES] = { 50, 30, 12, 6, 2 };
    int totalWeight = 0;
    for (int r = 0; r < NUM_RARITIES; r++)
        if (r <= bossMaxRar && hasRarity[r]) totalWeight += RARITY_WEIGHTS[r];
    if (totalWeight == 0) return;

    int roll = rand() % totalWeight;
    int chosenRarity = 0, accum = 0;
    for (int r = 0; r < NUM_RARITIES; r++) {
        if (r > bossMaxRar || !hasRarity[r]) continue;
        accum += RARITY_WEIGHTS[r];
        if (roll < accum) { chosenRarity = r; break; }
    }

    ItemDef drop;
    if (chosenRarity <= RARITY_RARE) {
        int slot = rand() % NUM_SLOTS;
        data_generate_item(&drop, slot, chosenRarity, dg->levelReq, h->classId);
    } else {
        int candidates[256];
        int nc = 0;
        for (int i = 0; i < data_num_items(); i++) {
            const ItemDef *it = data_item(i);
            if (it->rarity == chosenRarity &&
                it->levelReq >= dg->levelReq && it->levelReq <= dg->levelReq + 20 &&
                (it->classMask == 0 || (it->classMask & classMask)))
                candidates[nc++] = i;
        }
        if (nc == 0) return;
        drop = *data_item(candidates[rand() % nc]);
    }

    h->inventory[h->invCount++] = drop;
    char buf[LOG_LINE_W + 1];
    snprintf(buf, sizeof(buf), "[BOSS LOOT] %s!", drop.name);
    ui_log(gs, buf, data_rarity_color(drop.rarity));
}

/* Normal mob loot: roll against enemy's dropChance, then generate procedural item. */
static void try_loot_drop(GameState *gs) {
    Hero *h = &gs->hero;
    Enemy *e = &gs->enemy;
    if (randf() >= e->dropChance) return;
    if (h->invCount >= MAX_INVENTORY) return;

    const DungeonDef *dg = data_dungeon(gs->currentDungeon);
    if (!dg) return;

    int maxRar = MOB_MAX_RARITY[gs->currentDungeon];
    int rarity = rand() % (maxRar + 1);
    int slot = rand() % NUM_SLOTS;
    
    ItemDef drop;
    data_generate_item(&drop, slot, rarity, dg->levelReq, h->classId);
    
    h->inventory[h->invCount++] = drop;
    char buf[LOG_LINE_W + 1];
    snprintf(buf, sizeof(buf), "[LOOT] %s!", drop.name);
    ui_log(gs, buf, data_rarity_color(drop.rarity));
}

/*
 * Generic SkillDef interpreter — executes any skill by reading its data fields:
 *   dmgMul > 0  → deal damage (with numHits, ignoreArmor)
 *   stunTicks   → stun enemy
 *   buffTicks   → create Buff with shield/DoT/HoT/dmgMul/dodge/crit/immune
 *   healPct     → instant HP heal (% of maxHp)
 *   manaPct     → instant resource restore (% of maxResource)
 * This avoids per-skill code paths — all 48 skills run through the same logic.
 */
static void apply_skill(GameState *gs, const SkillDef *sk) {
    Hero *h = &gs->hero;
    Enemy *e = &gs->enemy;
    EStats es = hero_effective_stats(h);
    char buf[LOG_LINE_W + 1];
    int totalDmg = 0;

    if (sk->dmgMul > 0) {
        int hits = sk->numHits > 1 ? sk->numHits : 1;
        for (int i = 0; i < hits; i++) {
            int d = dmg_variance((int)(es.damage * sk->dmgMul));
            if (!sk->ignoreArmor) {
                int arm = (int)((float)e->defense * e->defense / (e->defense + 100.0f));
                d -= arm;
            }
            if (d < 1) d = 1;
            e->hp -= d;
            totalDmg += d;
        }
    }

    if (sk->stunTicks > 0) e->stunned = sk->stunTicks;

    if (sk->buffTicks > 0 && h->numBuffs < MAX_BUFFS) {
        Buff b;
        memset(&b, 0, sizeof(b));
        strncpy(b.name, sk->name, MAX_NAME - 1);
        b.ticksLeft    = sk->buffTicks;
        b.damageMul    = sk->buffDmgMul;
        b.dodgeBonus   = sk->buffDodge;
        b.critDmgBonus = sk->buffCritBonus;
        b.immune       = sk->buffImmune;
        if (sk->buffHealPct > 0)
            b.healPerTick = es.maxHp * sk->buffHealPct / 100;
        if (sk->buffDmgPct > 0) {
            b.dmgPerTick = es.damage * sk->buffDmgPct / 100;
            if (b.dmgPerTick < 1) b.dmgPerTick = 1;
        }
        if (sk->buffShieldPct > 0)
            b.shieldHp = es.maxHp * sk->buffShieldPct / 100;
        h->buffs[h->numBuffs++] = b;
    }

    if (totalDmg > 0) gs->combatDmgDealt += totalDmg;

    if (sk->healPct > 0) {
        int heal = es.maxHp * sk->healPct / 100;
        if (gs->hardModeActive) {
            int a0 = gs->activeAffixes[0], a1 = gs->activeAffixes[1];
            if (a0 == 2 || a1 == 2) heal = heal * 80 / 100;
        }
        int prevHp = h->hp;
        hero_heal(h, heal);
        gs->combatHealed += h->hp - prevHp;
    }

    if (sk->manaPct > 0) {
        int mana = h->maxResource * sk->manaPct / 100;
        hero_restore_resource(h, mana);
    }

    if (totalDmg > 0 && sk->healPct > 0) {
        snprintf(buf, sizeof(buf), "[%s] %d dmg, healed %d",
                 sk->name, totalDmg, es.maxHp * sk->healPct / 100);
    } else if (totalDmg > 0) {
        snprintf(buf, sizeof(buf), "[%s] %d damage!", sk->name, totalDmg);
    } else if (sk->healPct > 0) {
        snprintf(buf, sizeof(buf), "[%s] Healed %d!",
                 sk->name, es.maxHp * sk->healPct / 100);
    } else {
        snprintf(buf, sizeof(buf), "[%s] %s", sk->name, sk->description);
    }
    ui_log(gs, buf, CP_CYAN);
}

/*
 * Fire one skill per tick, highest tier first. For each tier, checks:
 * skill chosen, level requirement, cooldown ready, resource available,
 * hpBelow threshold (hero HP %), enemyHpBelow threshold (enemy HP %).
 * Returns after the first skill fires (one skill per tick).
 */
void combat_try_skills(GameState *gs) {
    Hero *h = &gs->hero;
    Enemy *e = &gs->enemy;

    const SkillDef *skills[MAX_ACTIVE_SKILLS];
    int cooldowns[MAX_ACTIVE_SKILLS];
    int nSkills = hero_collect_active_skills(h, skills, cooldowns, MAX_ACTIVE_SKILLS);

    for (int i = nSkills - 1; i >= 0; i--) {
        if (h->activeSkillCooldowns[i] > 0) continue;
        const SkillDef *sk = skills[i];
        if (!sk || !sk->name) continue;
        if (h->resource < sk->resourceCost) continue;

        if (sk->hpBelow > 0) {
            int pct = h->hp * 100 / (h->maxHp > 0 ? h->maxHp : 1);
            if (pct >= sk->hpBelow) continue;
        }
        if (sk->enemyHpBelow > 0) {
            int pct = e->hp * 100 / (e->maxHp > 0 ? e->maxHp : 1);
            if (pct >= sk->enemyHpBelow) continue;
        }

        h->resource -= sk->resourceCost;
        h->activeSkillCooldowns[i] = sk->cooldown;
        apply_skill(gs, sk);
        return;
    }

    for (int t = MAX_SKILL_TIERS - 1; t >= 0; t--) {
        if (h->skillChoices[t] < 0 || h->skillChoices[t] > 1) continue;
        if (h->level < data_skill_level(t)) continue;
        if (h->skillCooldowns[t] > 0) continue;

        const SkillDef *sk = data_skill(h->classId, t, h->skillChoices[t]);
        if (!sk || !sk->name) continue;
        if (h->resource < sk->resourceCost) continue;

        if (sk->hpBelow > 0) {
            int pct = h->hp * 100 / (h->maxHp > 0 ? h->maxHp : 1);
            if (pct >= sk->hpBelow) continue;
        }
        if (sk->enemyHpBelow > 0) {
            int pct = e->hp * 100 / (e->maxHp > 0 ? e->maxHp : 1);
            if (pct >= sk->enemyHpBelow) continue;
        }

        h->resource -= sk->resourceCost;
        h->skillCooldowns[t] = sk->cooldown;
        apply_skill(gs, sk);
        return;
    }
}

/*
 * One combat tick — the core game loop step. Sequence:
 *   1. Boss/death timer gates (return early if counting down)
 *   2. Decrement skill cooldowns, regen resource
 *   3. Tick buffs (HoT/DoT), try skills (may kill enemy → goto enemy_killed)
 *   4. Normal attack: buff multipliers → crit check → enemy armor (DEF²/(DEF+100)) → deal damage
 *   5. If enemy dies: award gold (/4, min 1) + XP, loot, heal VIT*2, respawn/boss delay
 *   6. Enemy retaliation: dodge → immune → calculate damage → block → shield absorb → HP loss
 *   7. Hero death: lose 10% gold, reset dungeon kills, start revive timer
 *
 * The "goto enemy_killed" label bridges skill-kills into the same reward path as normal kills.
 */
void combat_tick(GameState *gs) {
    if (!gs->inDungeon || gs->paused) return;
    if (gs->bossTimer > 0) {
        gs->bossTimer--;
        if (gs->bossTimer <= 0) {
            gs->dungeonKills = 0;
            gs->bossActive = 0;
            ui_log(gs, "The dungeon stirs again...", CP_YELLOW);
            combat_spawn(gs);
        }
        return;
    }
    if (!gs->hasEnemy) return;
    if (gs->deathTimer > 0) {
        gs->deathTimer--;
        if (gs->deathTimer <= 0) {
            gs->dungeonKills = 0;
            EStats es = hero_effective_stats(&gs->hero);
            gs->hero.hp = es.maxHp;
            gs->hero.maxHp = es.maxHp;
            gs->hero.resource = gs->hero.maxResource;
            gs->hero.numBuffs = 0;
            ui_log(gs, "You rise again...", CP_GREEN);
            combat_spawn(gs);
        }
        return;
    }

    Hero *h = &gs->hero;
    Enemy *e = &gs->enemy;
    EStats es = hero_effective_stats(h);
    char buf[LOG_LINE_W + 1];

    gs->tickCount++;
    gs->combatTicks++;

    for (int t = 0; t < MAX_SKILL_TIERS; t++)
        if (h->skillCooldowns[t] > 0) h->skillCooldowns[t]--;
    for (int i = 0; i < MAX_ACTIVE_SKILLS; i++)
        if (h->activeSkillCooldowns[i] > 0) h->activeSkillCooldowns[i]--;

    const ClassDef *cd = data_class(h->classId);
    hero_restore_resource(h, cd->resourceRegen);

    if (gs->hardModeActive) {
        int a0 = gs->activeAffixes[0], a1 = gs->activeAffixes[1];
        if (a0 == 5 || a1 == 5) {
            int drain = h->maxHp / 100;
            if (drain < 1) drain = 1;
            h->hp -= drain;
        }
    }

    combat_tick_buffs(gs);
    combat_try_skills(gs);

    if (e->hp <= 0) goto enemy_killed;

    float dmgMul = 1.0f + total_buff_val(h, 0);
    int baseDmg = dmg_variance((int)(es.damage * dmgMul));
    int doubleDmg = has_buff_flag(h, 0, 0, 0, 1);
    if (doubleDmg) { baseDmg *= 2; consume_buff_flag(h, 0, 1); }

    int isCrit = randf() < es.critChance;
    if (isCrit) {
        float cm = es.critMult + total_buff_val(h, 2);
        baseDmg = (int)(baseDmg * cm);
    }

    int armored = (int)((float)e->defense * e->defense / (e->defense + 100.0f));
    int finalDmg = baseDmg - armored;
    if (finalDmg < 1) finalDmg = 1;
    e->hp -= finalDmg;
    gs->combatDmgDealt += finalDmg;

    if (isCrit)
        snprintf(buf, sizeof(buf), "You crit %s for %d! [CRIT]", e->name, finalDmg);
    else
        snprintf(buf, sizeof(buf), "You hit %s for %d.", e->name, finalDmg);
    ui_log(gs, buf, isCrit ? CP_YELLOW : CP_WHITE);

enemy_killed:
    if (e->hp <= 0) {
        h->totalKills++;
        if (gs->isElite) h->eliteKills++;
        int gold = e->goldReward / 4;
        if (gold < 1) gold = 1;
        if (gs->hardModeActive) gold = gold * 150 / 100;
        h->gold += gold;
        h->totalGoldEarned += gold;

        int xp = (int)(e->xpReward * es.xpMultiplier * 1.15f);
        if (gs->hardModeActive) xp = xp * 150 / 100;

        if (gs->bossActive) {
            h->bossKills[gs->currentDungeon]++;
            if (gs->hardModeActive) {
                h->hardModeClears++;
                gs->activeAffixes[0] = rand() % NUM_AFFIXES;
                do { gs->activeAffixes[1] = rand() % NUM_AFFIXES; }
                while (gs->activeAffixes[1] == gs->activeAffixes[0]);
            }
            if (gs->currentDungeon >= 0 && gs->currentDungeon < NUM_DUNGEONS && !h->hardMode[gs->currentDungeon])
                h->hardMode[gs->currentDungeon] = 1;
            snprintf(buf, sizeof(buf), "*** %s SLAIN! *** +%dXP +%dG",
                     e->name, xp, gold);
            ui_log(gs, buf, CP_YELLOW);
            try_boss_loot(gs);
            hero_add_xp(gs, xp);
            int regen = es.stats[VIT] * 2;
            if (gs->hardModeActive) {
                int a0 = gs->activeAffixes[0], a1 = gs->activeAffixes[1];
                if (a0 == 2 || a1 == 2) regen = regen * 80 / 100;
            }
            { int prevHp = h->hp; hero_heal(h, regen); gs->combatHealed += h->hp - prevHp; }
            gs->hasEnemy = 0;
            gs->bossTimer = BOSS_DELAY;
            ui_log(gs, "The dungeon falls silent...", CP_MAGENTA);
            if (gs->hardModeActive) {
                snprintf(buf, sizeof(buf), "New affixes: %s + %s",
                         data_affix_name(gs->activeAffixes[0]),
                         data_affix_name(gs->activeAffixes[1]));
                ui_log(gs, buf, CP_MAGENTA);
            }
        } else {
            gs->dungeonKills++;
            snprintf(buf, sizeof(buf), "Slew %s! +%dXP +%dG",
                     e->name, xp, gold);
            ui_log(gs, buf, gs->isElite ? CP_YELLOW : CP_GREEN);
            if (gs->isElite) {
                try_boss_loot(gs);
            } else {
                try_loot_drop(gs);
            }
            hero_add_xp(gs, xp);
            int regen = es.stats[VIT] * 2;
            if (gs->hardModeActive) {
                int a0 = gs->activeAffixes[0], a1 = gs->activeAffixes[1];
                if (a0 == 2 || a1 == 2) regen = regen * 80 / 100;
            }
            { int prevHp = h->hp; hero_heal(h, regen); gs->combatHealed += h->hp - prevHp; }
            combat_spawn(gs);
        }
        check_achievements(gs);
        return;
    }

    int frenzied = gs->hardModeActive &&
        (gs->activeAffixes[0] == 4 || gs->activeAffixes[1] == 4);
    if (e->stunned > 0 && !frenzied) {
        e->stunned--;
        ui_log(gs, "Enemy is stunned!", CP_CYAN);
        return;
    }
    if (e->slowed > 0 && !frenzied) {
        e->slowed--;
        ui_log(gs, "Enemy is slowed...", CP_CYAN);
        return;
    }
    e->stunned = 0;
    e->slowed = 0;

    int dodgeNext = has_buff_flag(h, 0, 0, 1, 0);
    float totalDodge = es.dodgeChance + total_buff_val(h, 1);
    if (dodgeNext || randf() < totalDodge) {
        if (dodgeNext) consume_buff_flag(h, 1, 0);
        snprintf(buf, sizeof(buf), "%s attacks - DODGE!", e->name);
        ui_log(gs, buf, CP_CYAN);
        return;
    }

    if (has_buff_flag(h, 1, 0, 0, 0)) {
        snprintf(buf, sizeof(buf), "%s attacks - IMMUNE!", e->name);
        ui_log(gs, buf, CP_YELLOW);
        return;
    }

    int eDmg = dmg_variance(e->attack);
    int reduced = (int)(eDmg * es.dmgReduction);
    eDmg -= reduced;
    eDmg -= es.flatDmgReduce;
    if (eDmg < 1) eDmg = 1;

    if (es.blockChance > 0 && randf() < es.blockChance) {
        eDmg = (int)(eDmg * (1.0f - es.blockReduction));
        if (eDmg < 1) eDmg = 1;
        snprintf(buf, sizeof(buf), "%s hits you for %d. [BLOCK]", e->name, eDmg);
        ui_log(gs, buf, CP_YELLOW);
    } else {
        snprintf(buf, sizeof(buf), "%s hits you for %d.", e->name, eDmg);
        ui_log(gs, buf, CP_RED);
    }

    if (gs->hardModeActive) {
        int a0 = gs->activeAffixes[0], a1 = gs->activeAffixes[1];
        if (a0 == 3 || a1 == 3) {
            int thorns = eDmg / 10;
            if (thorns < 1) thorns = 1;
            e->hp -= thorns;
        }
    }

    gs->combatDmgTaken += eDmg;

    if (has_buff_flag(h, 0, 1, 0, 0) && h->resource >= eDmg) {
        h->resource -= eDmg;
        snprintf(buf, sizeof(buf), "Mana Shield absorbs %d damage!", eDmg);
        ui_log(gs, buf, CP_MAGENTA);
    } else {
        for (int i = 0; i < h->numBuffs && eDmg > 0; i++) {
            if (h->buffs[i].shieldHp > 0) {
                if (h->buffs[i].shieldHp >= eDmg) {
                    h->buffs[i].shieldHp -= eDmg;
                    eDmg = 0;
                } else {
                    eDmg -= h->buffs[i].shieldHp;
                    h->buffs[i].shieldHp = 0;
                }
            }
        }
        if (eDmg > 0) h->hp -= eDmg;
    }

    if (h->hp <= 0) {
        h->hp = 0;
        h->deaths++;
        int goldLoss = h->gold / 10;
        h->gold -= goldLoss;
        if (h->gold < 0) h->gold = 0;
        h->numBuffs = 0;
        gs->bossActive = 0;
        gs->combatDmgDealt = 0;
        gs->combatDmgTaken = 0;
        gs->combatHealed = 0;
        gs->combatTicks = 0;
        if (goldLoss > 0)
            snprintf(buf, sizeof(buf), "Died at %d/%d! Lost %d gold.", gs->dungeonKills, BOSS_THRESHOLD, goldLoss);
        else
            snprintf(buf, sizeof(buf), "Died at %d/%d! Reviving...", gs->dungeonKills, BOSS_THRESHOLD);
        ui_log(gs, buf, CP_RED);
        gs->deathTimer = 4;
        check_achievements(gs);
    }
}
