/*
 * main.c — Game loop.
 *
 * Uses non-blocking getch (nodelay) with usleep(16000) for ~60fps polling.
 * Combat ticks are timer-gated: the tick rate comes from hero SPD stat,
 * but is overridden to fixed values during death (800ms) and boss-clear
 * delay (1000ms). The boss timer check must come BEFORE the !hasEnemy
 * gate in combat_tick, otherwise the dungeon-restart timer never fires.
 */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
/* Platform-specific time and sleep primitives. */
#if defined(_WIN32)
#include <windows.h>
static long now_ms(void) { return (long)GetTickCount(); }
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <sys/time.h>
#include <unistd.h>
static long now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000L + tv.tv_usec / 1000L;
}
#define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

int main(void) {
    srand((unsigned)time(NULL));

    GameState gs;
    memset(&gs, 0, sizeof(gs));
    gs.running = 1;
    gs.screen = SCR_SAVE_SELECT;

    ui_init(&gs);
    save_refresh_slots(&gs);

    long lastTick = now_ms();
    long lastSave = now_ms();

    while (gs.running) {
        int ch = getch();
        ui_handle_key(&gs, ch);

        long cur = now_ms();

        if (gs.heroCreated && gs.inDungeon && !gs.paused) {
            EStats es = hero_effective_stats(&gs.hero);
            int rate = es.tickRate;
            if (gs.deathTimer > 0) rate = 800;
            if (gs.bossTimer > 0) rate = 1000;

            if (cur - lastTick >= rate) {
                combat_tick(&gs);
                lastTick = cur;
            }
        }

        if (gs.heroCreated && cur - lastSave >= 30000) {
            save_game(&gs);
            lastSave = cur;
        }

        ui_render(&gs);

        SLEEP_MS(16);
    }

    save_game(&gs);
    ui_destroy(&gs);
    return 0;
}
