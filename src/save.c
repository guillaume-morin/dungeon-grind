/*
 * save.c — Binary save/load with versioned format.
 *
 * Format: [4-byte magic][4-byte version][Hero struct][dungeon state]
 *
 * SAVE_MAGIC ("DGRV") + SAVE_VER enable forward-compatible rejection:
 * if we change the Hero struct layout (add/remove fields), we bump
 * SAVE_VER and old saves are cleanly rejected rather than silently
 * loading corrupted data.
 *
 * The Hero struct is written/read as a single fwrite/fread. This is why
 * ItemDef uses char[] instead of const char* — pointers would be
 * meaningless after reload.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* Windows uses _mkdir (no mode arg) and APPDATA instead of HOME. */
#if defined(_WIN32)
#include <direct.h>
#define PLATFORM_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define PLATFORM_MKDIR(path) mkdir(path, 0755)
#endif

#define SAVE_MAGIC  0x44475256  /* "DGRV" */
#define SAVE_VER    10
#define SAVE_VER_V8  8
#define SAVE_VER_V9  9

/* Return platform-specific save directory: ~/.dungeon-grind or %APPDATA%\.dungeon-grind. */
static const char *save_dir(void) {
    static char path[4096];
#if defined(_WIN32)
    const char *home = getenv("APPDATA");
    if (!home) home = getenv("USERPROFILE");
#else
    const char *home = getenv("HOME");
#endif
    if (!home) home = ".";
    snprintf(path, sizeof(path), "%s/.dungeon-grind", home);
    return path;
}

static const char *save_slot_path(int slot) {
    static char path[4096];
    snprintf(path, sizeof(path), "%s/save%d.dat", save_dir(), slot + 1);
    return path;
}

/* Write [magic][version][Hero][dungeon state] to save file. Creates save dir if needed. */
int save_game(GameState *gs) {
    if (!gs->heroCreated) return 0;
    PLATFORM_MKDIR(save_dir());

    gs->hero.lastSaveTime = (int64_t)time(NULL);

    FILE *f = fopen(save_slot_path(gs->saveSlot), "wb");
    if (!f) return 0;

    unsigned int magic = SAVE_MAGIC;
    unsigned int ver   = SAVE_VER;
    fwrite(&magic, sizeof(magic), 1, f);
    fwrite(&ver,   sizeof(ver),   1, f);
    fwrite(&gs->hero, sizeof(Hero), 1, f);
    fwrite(&gs->currentDungeon, sizeof(int), 1, f);
    fwrite(&gs->inDungeon, sizeof(int), 1, f);
    fwrite(&gs->dungeonKills, sizeof(int), 1, f);
    fwrite(&gs->hardModeActive, sizeof(int), 1, f);

    fclose(f);
    return 1;
}

/*
 * Load save file: validate magic + version, read Hero + dungeon state.
 * Rejects incompatible saves (version mismatch) rather than loading corrupted data.
 * Resets runtime combat state (buffs, boss/death timers) and respawns if in dungeon.
 */
int load_game(GameState *gs) {
    FILE *f = fopen(save_slot_path(gs->saveSlot), "rb");
    if (!f) return 0;

    unsigned int magic, ver;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != SAVE_MAGIC) { fclose(f); return 0; }
    if (fread(&ver,   sizeof(ver),   1, f) != 1)                        { fclose(f); return 0; }
    if (ver != SAVE_VER && ver != SAVE_VER_V9 && ver != SAVE_VER_V8)     { fclose(f); return 0; }

    Hero loaded;
    memset(&loaded, 0, sizeof(Hero));

    if (ver == SAVE_VER) {
        if (fread(&loaded, sizeof(Hero), 1, f) != 1) { fclose(f); return 0; }
    } else if (ver == SAVE_VER_V9) {
        /* V9→V10: same struct layout, but talent tree deps/cols changed — reset ranks */
        if (fread(&loaded, sizeof(Hero), 1, f) != 1) { fclose(f); return 0; }
        for (int t = 0; t < NUM_TALENT_TREES; t++) {
            loaded.talentPoints += loaded.talentTreePoints[t];
            loaded.talentTreePoints[t] = 0;
            for (int n = 0; n < TALENT_NODES_PER_TREE; n++)
                loaded.talentRanks[t][n] = 0;
        }
        memset(loaded.activeSkillCooldowns, 0, sizeof(loaded.activeSkillCooldowns));
    } else {
        /* V8→V10: old stat allocation + possible struct size mismatch */
        long dataStart = ftell(f);
        if (fread(&loaded, sizeof(Hero), 1, f) != 1) {
            fseek(f, dataStart, SEEK_SET);
            fread(&loaded, 1, sizeof(Hero) - sizeof(loaded.talentRanks) - sizeof(loaded.talentTreePoints) - sizeof(loaded.activeSkillCooldowns), f);
        }
        int refunded = 0;
        for (int i = 0; i < NUM_STATS; i++) {
            refunded += loaded.talents[i];
            loaded.talents[i] = 0;
        }
        loaded.talentPoints += refunded;
        for (int i = 0; i < MAX_SKILL_TIERS; i++) {
            loaded.skillChoices[i] = -1;
            loaded.skillCooldowns[i] = 0;
        }
        memset(loaded.talentRanks, 0, sizeof(loaded.talentRanks));
        memset(loaded.talentTreePoints, 0, sizeof(loaded.talentTreePoints));
        memset(loaded.activeSkillCooldowns, 0, sizeof(loaded.activeSkillCooldowns));
    }
    gs->hero = loaded;

    if (fread(&gs->currentDungeon, sizeof(int), 1, f) != 1) gs->currentDungeon = 0;
    if (fread(&gs->inDungeon, sizeof(int), 1, f) != 1) gs->inDungeon = 0;
    if (fread(&gs->dungeonKills, sizeof(int), 1, f) != 1) gs->dungeonKills = 0;
    if (fread(&gs->hardModeActive, sizeof(int), 1, f) != 1) gs->hardModeActive = 0;

    fclose(f);

    gs->heroCreated = 1;
    gs->screen = SCR_MAIN;
    gs->hasEnemy = 0;
    gs->paused = 0;
    gs->deathTimer = 0;
    gs->bossActive = 0;
    gs->bossTimer = 0;
    gs->hero.numBuffs = 0;

    /* Offline progression: award ~half of active play rate */
    if (gs->inDungeon && gs->hero.lastSaveTime > 0) {
        int64_t now = (int64_t)time(NULL);
        int64_t elapsed = now - gs->hero.lastSaveTime;
        int64_t raw_min = elapsed > 0 ? elapsed / 60 : 0;
        int minutes = raw_min > 480 ? 480 : (int)raw_min;  /* cap at 8 hours */
        int totalXp = 0, totalGold = 0;
        if (minutes >= 1) {
            const DungeonDef *dg = data_dungeon(gs->currentDungeon);
            if (dg) {
                int avgXp = 0, avgGold = 0;
                for (int i = 0; i < dg->numEnemies; i++) {
                    const EnemyTemplate *et = data_enemy(dg->enemyIdx[i]);
                    avgXp   += et->xpReward;
                    avgGold += et->goldReward / 4;
                }
                avgXp   /= dg->numEnemies;
                avgGold /= dg->numEnemies;
                int killsPerMin = 5;  /* half of active rate */
                totalXp   = avgXp   * killsPerMin * minutes;
                totalGold = avgGold * killsPerMin * minutes;
                if (totalGold < 1) totalGold = 1;
                gs->hero.gold += totalGold;
                gs->hero.totalGoldEarned += totalGold;
                hero_add_xp(gs, totalXp);
            }
        }
        gs->offlineShowUntil = (int64_t)time(NULL) + 10;
        gs->offlineXp   = totalXp;
        gs->offlineGold = totalGold;
        gs->offlineMin  = minutes;
    }

    if (gs->inDungeon) combat_spawn(gs);

    EStats es = hero_effective_stats(&gs->hero);
    gs->hero.maxHp = es.maxHp;
    if (gs->hero.hp > gs->hero.maxHp) gs->hero.hp = gs->hero.maxHp;

    return 1;
}

/* Delete a save file by slot index. */
void save_delete_slot(int slot) {
    if (slot < 0 || slot >= NUM_SAVE_SLOTS) return;
    remove(save_slot_path(slot));
}

/* Scan all 3 save files and populate slotInfo[] (name, level, class) for the save-select screen. */
void save_refresh_slots(GameState *gs) {
    for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
        SaveSlotInfo *info = &gs->slotInfo[i];
        memset(info, 0, sizeof(*info));

        FILE *f = fopen(save_slot_path(i), "rb");
        if (!f) continue;

        unsigned int magic, ver;
        if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != SAVE_MAGIC) { fclose(f); continue; }
        if (fread(&ver,   sizeof(ver),   1, f) != 1 || (ver != SAVE_VER && ver != SAVE_VER_V9 && ver != SAVE_VER_V8)) { fclose(f); continue; }

        Hero h;
        memset(&h, 0, sizeof(Hero));
        if (fread(&h, sizeof(Hero), 1, f) < 1 && ver == SAVE_VER) { fclose(f); continue; }

         info->exists  = 1;
        strncpy(info->name, h.name, MAX_NAME - 1);
        info->name[MAX_NAME - 1] = '\0';
        info->level   = h.level;
        info->classId = h.classId;
        fclose(f);
    }
}

