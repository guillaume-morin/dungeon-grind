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
/* Windows uses _mkdir (no mode arg) and APPDATA instead of HOME. */
#if defined(_WIN32)
#include <direct.h>
#define PLATFORM_MKDIR(path) _mkdir(path)
#else
#include <sys/stat.h>
#define PLATFORM_MKDIR(path) mkdir(path, 0755)
#endif

#define SAVE_MAGIC  0x44475256  /* "DGRV" */
#define SAVE_VER    5

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
int save_game(const GameState *gs) {
    if (!gs->heroCreated) return 0;
    PLATFORM_MKDIR(save_dir());

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
    if (fread(&ver,   sizeof(ver),   1, f) != 1 || ver   != SAVE_VER)  { fclose(f); return 0; }

    Hero loaded;
    if (fread(&loaded, sizeof(Hero), 1, f) != 1) { fclose(f); return 0; }
    gs->hero = loaded;

    if (fread(&gs->currentDungeon, sizeof(int), 1, f) != 1) gs->currentDungeon = 0;
    if (fread(&gs->inDungeon, sizeof(int), 1, f) != 1) gs->inDungeon = 0;
    if (fread(&gs->dungeonKills, sizeof(int), 1, f) != 1) gs->dungeonKills = 0;

    fclose(f);

    gs->heroCreated = 1;
    gs->screen = SCR_MAIN;
    gs->hasEnemy = 0;
    gs->paused = 0;
    gs->deathTimer = 0;
    gs->bossActive = 0;
    gs->bossTimer = 0;
    gs->hero.numBuffs = 0;

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
        if (fread(&ver,   sizeof(ver),   1, f) != 1 || ver   != SAVE_VER)   { fclose(f); continue; }

        Hero h;
        if (fread(&h, sizeof(Hero), 1, f) != 1) { fclose(f); continue; }

        info->exists  = 1;
        strncpy(info->name, h.name, MAX_NAME - 1);
        info->level   = h.level;
        info->classId = h.classId;
        fclose(f);
    }
}

