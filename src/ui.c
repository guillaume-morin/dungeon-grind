/*
 * ui.c — All ncurses rendering and input handling (~2150 lines).
 *
 * Layout: 80×24 terminal. Left panel (28 cols) for menus, right panel
 * split into enemy display (top 8 rows) and combat log (bottom).
 * The right "enemy" panel doubles as a context-sensitive detail view:
 * item stats in equipment/shop, stat preview in character screen,
 * skill description in skills screen, and encyclopedia details.
 * render_enemy_panel() dispatches to the correct detail renderer
 * based on gs->screen.
 *
 * render_stat_detail() handles the talent soft-cap display: it searches
 * for the minimum talent point increment (1–20) that actually produces
 * a visible stat change, accounting for integer division dead zones
 * in the soft cap formula.
 */
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Draw a filled/empty bar using ACS_BLOCK and ACS_BULLET characters. */
static void draw_bar(WINDOW *w, int y, int x, int width, int cur, int max, int cpFull, int cpEmpty) {
    if (max <= 0) max = 1;
    int filled = (cur * width) / max;
    if (filled > width) filled = width;
    if (filled < 0) filled = 0;
    wmove(w, y, x);
    wattron(w, COLOR_PAIR(cpFull));
    for (int i = 0; i < filled; i++) waddch(w, ACS_BLOCK);
    wattroff(w, COLOR_PAIR(cpFull));
    wattron(w, COLOR_PAIR(cpEmpty));
    for (int i = filled; i < width; i++) waddch(w, ACS_BULLET);
    wattroff(w, COLOR_PAIR(cpEmpty));
}

void ui_init(GameState *gs) {
#ifndef PDC_BUILD
    set_escdelay(25);
#endif
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    start_color();

    /* use_default_colors() allows -1 as "terminal default background".
     * PDCurses on Windows has no transparent background — use black. */
    int bg = COLOR_BLACK;
#if !defined(_WIN32)
    if (use_default_colors() == OK) bg = -1;
#endif

    init_pair(CP_DEFAULT,  COLOR_WHITE,   bg);
    init_pair(CP_RED,      COLOR_RED,     bg);
    init_pair(CP_GREEN,    COLOR_GREEN,   bg);
    init_pair(CP_YELLOW,   COLOR_YELLOW,  bg);
    init_pair(CP_BLUE,     COLOR_BLUE,    bg);
    init_pair(CP_MAGENTA,  COLOR_MAGENTA, bg);
    init_pair(CP_CYAN,     COLOR_CYAN,    bg);
    init_pair(CP_WHITE,    COLOR_WHITE,   bg);
    init_pair(CP_HEADER,   COLOR_BLACK,   COLOR_YELLOW);
    init_pair(CP_BORDER,   COLOR_YELLOW,  bg);
    init_pair(CP_SELECTED, COLOR_BLACK,   COLOR_WHITE);
    init_pair(CP_ORANGE,   COLOR_RED,     bg);

    gs->wHeader = newwin(HEADER_H, SCREEN_W, 0, 0);
    gs->wLeft   = newwin(PANEL_H, LEFT_W, HEADER_H, 0);
    gs->wEnemy  = newwin(ENEMY_DISPLAY_H, RIGHT_W, HEADER_H, LEFT_W);
    gs->wLog    = newwin(LOG_H, RIGHT_W, HEADER_H + ENEMY_DISPLAY_H, LEFT_W);
}

void ui_destroy(GameState *gs) {
    if (gs->wHeader) delwin(gs->wHeader);
    if (gs->wLeft)   delwin(gs->wLeft);
    if (gs->wEnemy)  delwin(gs->wEnemy);
    if (gs->wLog)    delwin(gs->wLog);
    endwin();
}

void ui_log(GameState *gs, const char *text, int color) {
    if (gs->logCount < MAX_LOG) {
        LogLine *l = &gs->log[gs->logCount++];
        strncpy(l->text, text, LOG_LINE_W);
        l->text[LOG_LINE_W] = '\0';
        l->color = color;
    } else {
        for (int i = 1; i < MAX_LOG; i++)
            gs->log[i - 1] = gs->log[i];
        LogLine *l = &gs->log[MAX_LOG - 1];
        strncpy(l->text, text, LOG_LINE_W);
        l->text[LOG_LINE_W] = '\0';
        l->color = color;
    }
}

static void render_header(GameState *gs) {
    WINDOW *w = gs->wHeader;
    werase(w);
    wbkgd(w, COLOR_PAIR(CP_HEADER));
    wmove(w, 0, 0);

    if (!gs->heroCreated) {
        mvwprintw(w, 0, (SCREEN_W - 18) / 2, " DUNGEON  GRIND ");
    } else {
        const ClassDef *cd = data_class(gs->hero.classId);
        const char *dname = "Town";
        char dnameBuf[48];
        if (gs->inDungeon) {
            const DungeonDef *dg = data_dungeon(gs->currentDungeon);
            if (dg) {
                if (gs->hardModeActive)
                    snprintf(dnameBuf, sizeof(dnameBuf), "%s (Hard)", dg->name);
                else
                    snprintf(dnameBuf, sizeof(dnameBuf), "%s", dg->name);
                dname = dnameBuf;
            }
        }
        if (gs->hero.activeTitle > 0 && gs->hero.activeTitle <= NUM_TITLES) {
            const TitleDef *td = data_title(gs->hero.activeTitle - 1);
            if (td)
                mvwprintw(w, 0, 1, " %s %s Lv.%d ", gs->hero.name, td->name, gs->hero.level);
            else
                mvwprintw(w, 0, 1, " %s (%s) Lv.%d ", gs->hero.name, cd->name, gs->hero.level);
        } else {
            mvwprintw(w, 0, 1, " %s (%s) Lv.%d ", gs->hero.name, cd->name, gs->hero.level);
        }
        mvwprintw(w, 0, SCREEN_W - (int)strlen(dname) - 3, " %s ", dname);
        if (gs->paused) {
            mvwprintw(w, 0, SCREEN_W / 2 - 4, "[PAUSED]");
        } else if (gs->inDungeon) {
            if (gs->bossActive)
                mvwprintw(w, 0, SCREEN_W / 2 - 3, "[BOSS]");
            else if (gs->bossTimer > 0)
                mvwprintw(w, 0, SCREEN_W / 2 - 6, "[CLEARED!]");
            else
                mvwprintw(w, 0, SCREEN_W / 2 - 4, " %d/%d ", gs->dungeonKills, BOSS_THRESHOLD);
        }
    }
    wnoutrefresh(w);
}

static void render_save_select(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Save Slots");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    int row = 4;
    for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
        int sel = (i == gs->menuIdx);
        SaveSlotInfo *info = &gs->slotInfo[i];

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else if (info->exists) wattron(w, COLOR_PAIR(CP_WHITE));
        else wattron(w, COLOR_PAIR(CP_DEFAULT));

        if (info->exists) {
            const ClassDef *cd = data_class(info->classId);
            mvwprintw(w, row, 1, "%s%d. %s %-12.12s Lv.%d",
                      sel ? " > " : "   ", i + 1,
                      cd->symbol, info->name, info->level);
        } else {
            mvwprintw(w, row, 1, "%s%d. -- Empty --",
                      sel ? " > " : "   ", i + 1);
        }
        wattroff(w, COLOR_PAIR(CP_SELECTED));
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
        row += 2;
    }

    row += 1;
    int newSel = (gs->menuIdx == NUM_SAVE_SLOTS);
    if (newSel) wattron(w, COLOR_PAIR(CP_SELECTED));
    else wattron(w, COLOR_PAIR(CP_RED));
    mvwprintw(w, row, 1, "%sERASE SLOT", newSel ? " > " : "   ");
    if (newSel) wattroff(w, COLOR_PAIR(CP_SELECTED));
    else wattroff(w, COLOR_PAIR(CP_RED));

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Enter] Select");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_new_slot(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Erase Save Slot");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    int row = 4;
    for (int i = 0; i < NUM_SAVE_SLOTS; i++) {
        int sel = (i == gs->menuIdx);
        SaveSlotInfo *info = &gs->slotInfo[i];

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else if (info->exists) wattron(w, COLOR_PAIR(CP_ORANGE));
        else wattron(w, COLOR_PAIR(CP_DEFAULT));

        if (info->exists) {
            const ClassDef *cd = data_class(info->classId);
            mvwprintw(w, row, 1, "%s%d. %s %-12.12s Lv.%d",
                      sel ? " > " : "   ", i + 1,
                      cd->symbol, info->name, info->level);
        } else {
            mvwprintw(w, row, 1, "%s%d. -- Empty --",
                      sel ? " > " : "   ", i + 1);
        }
        wattroff(w, COLOR_PAIR(CP_SELECTED));
        wattroff(w, COLOR_PAIR(CP_ORANGE));
        wattroff(w, COLOR_PAIR(CP_DEFAULT));

        if (info->exists && !sel) {
            wattron(w, COLOR_PAIR(CP_RED));
            mvwprintw(w, row + 1, 5, "DELETE");
            wattroff(w, COLOR_PAIR(CP_RED));
        }
        row += 3;
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_class_select(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Choose Your Class");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    for (int i = 0; i < NUM_CLASSES; i++) {
        const ClassDef *cd = data_class(i);
        if (i == gs->menuIdx) {
            wattron(w, COLOR_PAIR(CP_SELECTED));
            mvwprintw(w, 3 + i, 1, " > %s %-19.19s", cd->symbol, cd->name);
            wattroff(w, COLOR_PAIR(CP_SELECTED));
        } else {
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, 3 + i, 1, "   %s %-19.19s", cd->symbol, cd->name);
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Enter] Select");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_name_input(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    const ClassDef *cd = data_class(gs->hero.classId);
    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Name Your %s", cd->name);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, 4, 2, "> %s_", gs->nameBuf);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 3, 2, "[Enter] Confirm");
    mvwprintw(w, PANEL_H - 2, 2, "[Bksp] Delete");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static const char *MAIN_MENU[] = { "DUNGEON", "CHARACTER", "SKILLS", "EQUIPMENT", "SHOP", "ENCYCLOPEDIA", "QUIT" };
#define MAIN_MENU_N 7

static void render_main_menu(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    Hero *h = &gs->hero;
    EStats es = hero_effective_stats(h);

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "%s Lv.%d", h->name, h->level);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_RED));
    mvwprintw(w, 2, 2, "HP ");
    wattroff(w, COLOR_PAIR(CP_RED));
    draw_bar(w, 2, 5, 10, h->hp, es.maxHp, CP_RED, CP_DEFAULT);
    wprintw(w, " %d", h->hp);

    wattron(w, COLOR_PAIR(CP_YELLOW));
    mvwprintw(w, 3, 2, "XP ");
    wattroff(w, COLOR_PAIR(CP_YELLOW));
    draw_bar(w, 3, 5, 10, h->xp, hero_xp_needed(h->level), CP_YELLOW, CP_DEFAULT);

    wattron(w, COLOR_PAIR(CP_YELLOW));
    mvwprintw(w, 4, 2, "Gold: %d", h->gold);
    wattroff(w, COLOR_PAIR(CP_YELLOW));

    if (h->talentPoints > 0) {
        wattron(w, COLOR_PAIR(CP_GREEN) | A_BOLD);
        mvwprintw(w, 4, 15, "+%dTP", h->talentPoints);
        wattroff(w, COLOR_PAIR(CP_GREEN) | A_BOLD);
    }

    mvwhline(w, 5, 1, ACS_HLINE, LEFT_W - 2);

    for (int i = 0; i < MAIN_MENU_N; i++) {
        if (i == gs->menuIdx) {
            wattron(w, COLOR_PAIR(CP_SELECTED));
            mvwprintw(w, 7 + i * 2, 1, " > %-23s", MAIN_MENU[i]);
            wattroff(w, COLOR_PAIR(CP_SELECTED));
        } else {
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, 7 + i * 2, 1, "   %-23s", MAIN_MENU[i]);
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
    }
    wnoutrefresh(w);
}

static void render_dungeon_select(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Select Dungeon");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    if (gs->inDungeon) {
        wattron(w, COLOR_PAIR(CP_GREEN));
        mvwprintw(w, 2, 2, "[In dungeon]");
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }

    if (gs->wantHardMode) {
        wattron(w, COLOR_PAIR(CP_MAGENTA));
        mvwprintw(w, 2, 16, "[HARD]");
        wattroff(w, COLOR_PAIR(CP_MAGENTA));
    }

    int row = 4;
    for (int i = 0; i < NUM_DUNGEONS; i++) {
        const DungeonDef *dg = data_dungeon(i);
        int unlocked = gs->hero.level >= dg->levelReq;
        int cur = (gs->inDungeon && gs->currentDungeon == i);
        int hm = gs->hero.hardMode[i];

        if (i == gs->menuIdx) {
            wattron(w, COLOR_PAIR(CP_SELECTED));
            mvwprintw(w, row, 1, " > %-23.23s", dg->name);
            wattroff(w, COLOR_PAIR(CP_SELECTED));
        } else if (!unlocked) {
            wattron(w, COLOR_PAIR(CP_RED));
            mvwprintw(w, row, 1, "   Lv.%d ???", dg->levelReq);
            wattroff(w, COLOR_PAIR(CP_RED));
        } else {
            wattron(w, COLOR_PAIR(cur ? CP_GREEN : CP_DEFAULT));
            mvwprintw(w, row, 1, "   %-23.23s", dg->name);
            wattroff(w, COLOR_PAIR(cur ? CP_GREEN : CP_DEFAULT));
        }
        if (hm && unlocked) {
            wattron(w, COLOR_PAIR(CP_MAGENTA));
            mvwprintw(w, row, LEFT_W - 4, "H");
            wattroff(w, COLOR_PAIR(CP_MAGENTA));
        }
        row++;
    }

    if (gs->inDungeon) {
        int exitRow = row + 1;
        int exitIdx = NUM_DUNGEONS;
        if (gs->menuIdx == exitIdx) {
            wattron(w, COLOR_PAIR(CP_SELECTED));
            mvwprintw(w, exitRow, 1, " > EXIT DUNGEON        ");
            wattroff(w, COLOR_PAIR(CP_SELECTED));
        } else {
            wattron(w, COLOR_PAIR(CP_RED));
            mvwprintw(w, exitRow, 1, "   EXIT DUNGEON");
            wattroff(w, COLOR_PAIR(CP_RED));
        }
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 3, 2, "[H] Hard Mode");
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

/* ── equipment bag sort/filter ─────────────────────────────────────── */

#define NUM_EQUIP_SORTS 3
static const char *EQUIP_SORT_NAMES[] = { "Rarity", "Slot", "Level" };
static const char *SLOT_ABBR[] = { "Wpn", "Hlm", "Cht", "Leg", "Bts", "Rng", "Aml" };
static const char  SLOT_CHAR[] = { 'W',   'H',   'C',   'L',   'B',   'R',   'A' };

static const Hero *s_sortHero;
static int s_sortMode;

static int inv_cmp(const void *a, const void *b) {
    int ia = *(const int *)a, ib = *(const int *)b;
    const ItemDef *A = &s_sortHero->inventory[ia];
    const ItemDef *B = &s_sortHero->inventory[ib];
    switch (s_sortMode) {
    case 0:
        if (A->rarity != B->rarity) return B->rarity - A->rarity;
        if (A->levelReq != B->levelReq) return B->levelReq - A->levelReq;
        return ia - ib;
    case 1:
        if (A->slot != B->slot) return A->slot - B->slot;
        if (A->rarity != B->rarity) return B->rarity - A->rarity;
        return ia - ib;
    case 2:
        if (A->levelReq != B->levelReq) return B->levelReq - A->levelReq;
        if (A->rarity != B->rarity) return B->rarity - A->rarity;
        return ia - ib;
    }
    return 0;
}

static int inv_build_view(const Hero *h, int filter, int sort, int *out, int max) {
    int n = 0;
    for (int i = 0; i < h->invCount && n < max; i++) {
        if (filter >= 0 && h->inventory[i].slot != filter) continue;
        out[n++] = i;
    }
    s_sortHero = h;
    s_sortMode = sort;
    if (n > 1) qsort(out, n, sizeof(int), inv_cmp);
    return n;
}

static void render_equipment(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Equipment  Gold:%d", gs->hero.gold);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    int row = 3;
    for (int s = 0; s < NUM_SLOTS; s++) {
        int sel = (s == gs->menuIdx);

        if (gs->hero.hasEquip[s]) {
            int rc = data_rarity_color(gs->hero.equipment[s].rarity);
            if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
            else     wattron(w, COLOR_PAIR(rc));
            mvwprintw(w, row, 1, "%s%-4s%-18.18s",
                      sel ? " > " : "   ", SLOT_ABBR[s], gs->hero.equipment[s].name);
        } else {
            if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
            else     wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, row, 1, "%s%-4s--",
                      sel ? " > " : "   ", SLOT_ABBR[s]);
        }
        wattroff(w, COLOR_PAIR(CP_SELECTED));
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
        row++;
    }

    mvwhline(w, row, 1, ACS_HLINE, LEFT_W - 2);
    row++;

    int filter = gs->equipFilter > 0 ? gs->equipFilter - 1 : -1;
    int viewIdx[MAX_INVENTORY];
    int viewN = inv_build_view(&gs->hero, filter, gs->equipSort, viewIdx, MAX_INVENTORY);

    const char *fn = gs->equipFilter == 0 ? "All" : data_slot_name(gs->equipFilter - 1);
    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, row, 2, "<%s> (%d)", fn, viewN);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    const char *sn = EQUIP_SORT_NAMES[gs->equipSort];
    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, row, LEFT_W - 2 - (int)strlen(sn), "%s", sn);
    wattroff(w, COLOR_PAIR(CP_DEFAULT));
    row++;

    int maxBagRows = PANEL_H - row - 4;
    int bagSel = gs->menuIdx - NUM_SLOTS;
    int bagScroll = 0;
    if (bagSel >= maxBagRows) bagScroll = bagSel - maxBagRows + 1;

    for (int vi = 0; vi < viewN && vi - bagScroll < maxBagRows; vi++) {
        if (vi < bagScroll) continue;
        int invI = viewIdx[vi];
        int sel = (gs->menuIdx == NUM_SLOTS + vi);
        int rc = data_rarity_color(gs->hero.inventory[invI].rarity);

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else     wattron(w, COLOR_PAIR(rc));

        if (gs->equipFilter > 0)
            mvwprintw(w, row, 1, "%s%-23.23s",
                      sel ? " > " : "   ", gs->hero.inventory[invI].name);
        else
            mvwprintw(w, row, 1, "%s%c %-21.21s",
                      sel ? " > " : "   ", SLOT_CHAR[gs->hero.inventory[invI].slot],
                      gs->hero.inventory[invI].name);

        wattroff(w, COLOR_PAIR(CP_SELECTED));
        wattroff(w, COLOR_PAIR(rc));
        row++;
    }

    if (viewN == 0 && gs->hero.invCount > 0) {
        wattron(w, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(w, row, 3, "(none for this slot)");
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
    } else if (viewN == 0) {
        wattron(w, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(w, row, 3, "(empty)");
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
    }

    int inSlots = gs->menuIdx < NUM_SLOTS;
    wattron(w, COLOR_PAIR(CP_CYAN));
    if (inSlots && gs->hero.hasEquip[gs->menuIdx])
        mvwprintw(w, PANEL_H - 5, 2, "[Enter] Unequip");
    else if (!inSlots && bagSel >= 0 && bagSel < viewN)
        mvwprintw(w, PANEL_H - 5, 2, "[Enter] Equip");
    mvwprintw(w, PANEL_H - 4, 2, "[X]Sell  [Z]Bulk Sell");
    mvwprintw(w, PANEL_H - 3, 2, "[S]ort  [</>]Filter");
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

#define NUM_SHOP_TIERS 9
static const struct { int level; int rarity; } SHOP_TIERS[NUM_SHOP_TIERS] = {
    {1, RARITY_COMMON}, {8, RARITY_UNCOMMON},
    {18, RARITY_RARE}, {32, RARITY_RARE}, {40, RARITY_RARE},
    {46, RARITY_EPIC}, {55, RARITY_EPIC}, {66, RARITY_EPIC}, {78, RARITY_EPIC}
};

static int shop_item_count(GameState *gs) {
    (void)gs;
    return NUM_SHOP_TIERS;
}

static void shop_get_item(GameState *gs, int idx, ItemDef *out) {
    memset(out, 0, sizeof(ItemDef));
    if (idx < 0 || idx >= NUM_SHOP_TIERS) return;
    int lvl = SHOP_TIERS[idx].level;
    int rar = SHOP_TIERS[idx].rarity;
    
    if (rar <= RARITY_RARE) {
        data_shop_item(out, gs->shopSlot, lvl, gs->hero.classId);
    } else {
        int classMask = (1 << gs->hero.classId);
        for (int i = 0; i < data_num_items(); i++) {
            const ItemDef *it = data_item(i);
            if (it->slot == gs->shopSlot && it->rarity == rar && it->levelReq == lvl &&
                (it->classMask == 0 || (it->classMask & classMask))) {
                *out = *it;
                return;
            }
        }
        data_shop_item(out, gs->shopSlot, lvl, gs->hero.classId);
    }
}

static void render_shop(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Shop  Gold:%d", gs->hero.gold);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, 2, 2, "<%s>", data_slot_name(gs->shopSlot));
    mvwprintw(w, 2, 16, "[</>]");
    wattroff(w, COLOR_PAIR(CP_WHITE));

    int row = 4;
    for (int i = 0; i < NUM_SHOP_TIERS; i++) {
        ItemDef it;
        shop_get_item(gs, i, &it);
        if (it.name[0] == '\0') continue;
        
        int sel = (i == gs->menuIdx);
        int canBuy = (gs->hero.gold >= it.price && gs->hero.level >= it.levelReq);
        int rc = data_rarity_color(it.rarity);

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else     wattron(w, COLOR_PAIR(canBuy ? rc : CP_RED));

        mvwprintw(w, row, 1, "%s%-16.16s %4dg",
                  sel ? ">" : " ", it.name, it.price);

        if (sel) wattroff(w, COLOR_PAIR(CP_SELECTED));
        else     wattroff(w, COLOR_PAIR(canBuy ? rc : CP_RED));
        row++;
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 3, 2, "[Enter] Buy+Equip");
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_character(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    Hero *h = &gs->hero;
    const ClassDef *cd = data_class(h->classId);
    EStats es = hero_effective_stats(h);

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    char suffix[32];
    snprintf(suffix, sizeof(suffix), " (%s) Lv.%d", cd->name, h->level);
    int nameMax = LEFT_W - 3 - (int)strlen(suffix);
    if (nameMax < 3) nameMax = 3;
    mvwprintw(w, 1, 2, "%.*s%s", nameMax, h->name, suffix);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    int row = 2;
    mvwprintw(w, row, 2, "XP ");
    draw_bar(w, row, 5, 10, h->xp, hero_xp_needed(h->level), CP_YELLOW, CP_DEFAULT);
    wattron(w, COLOR_PAIR(CP_WHITE));
    wprintw(w, " %d/%d", h->xp, hero_xp_needed(h->level));
    wattroff(w, COLOR_PAIR(CP_WHITE));
    row++;

    if (h->talentPoints > 0) {
        wattron(w, COLOR_PAIR(CP_GREEN) | A_BOLD);
        mvwprintw(w, row, 2, "Talent Points: %d", h->talentPoints);
        wattroff(w, COLOR_PAIR(CP_GREEN) | A_BOLD);
    }
    row++;

    for (int i = 0; i < NUM_STATS; i++) {
        if (i == gs->menuIdx) {
            wattron(w, COLOR_PAIR(CP_SELECTED));
            mvwprintw(w, row, 1, " > %-4s %3d", data_stat_short(i), es.stats[i]);
            if (h->talents[i] > 0) wprintw(w, " (+%d)", h->talents[i]);
            wprintw(w, "%*s", LEFT_W - 20, "");
            wattroff(w, COLOR_PAIR(CP_SELECTED));
        } else {
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, row, 1, "   %-4s %3d", data_stat_short(i), es.stats[i]);
            if (h->talents[i] > 0) {
                wattron(w, COLOR_PAIR(CP_GREEN));
                wprintw(w, " (+%d)", h->talents[i]);
                wattroff(w, COLOR_PAIR(CP_GREEN));
            }
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
        row++;
    }

    row++;
    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, row++, 2, "DMG %-4d  Crit %.1f%%", es.damage, es.critChance * 100);
    mvwprintw(w, row++, 2, "Dodge %.1f%% DR %.1f%%", es.dodgeChance * 100, es.dmgReduction * 100);
    if (es.blockChance > 0)
        mvwprintw(w, row++, 2, "Block %.1f%%", es.blockChance * 100);
    mvwprintw(w, row++, 2, "Atk %.2f/s", 1000.0f / es.tickRate);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    row++;
    wattron(w, COLOR_PAIR(CP_GREEN));
    mvwprintw(w, row++, 2, "K:%d D:%d G:%d", h->totalKills, h->deaths, h->totalGoldEarned);
    wattroff(w, COLOR_PAIR(CP_GREEN));

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 4, 2, "[Enter] +1 Talent");
    mvwprintw(w, PANEL_H - 3, 2, "[T] Titles");
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_confirm_quit(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_RED) | A_BOLD);
    mvwprintw(w, 4, 4, "Really quit?");
    wattroff(w, COLOR_PAIR(CP_RED) | A_BOLD);

    const char *opts[] = { "No, keep playing", "Yes, save & quit" };
    for (int i = 0; i < 2; i++) {
        if (i == gs->menuIdx)
            wattron(w, COLOR_PAIR(CP_SELECTED));
        else
            wattron(w, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(w, 7 + i * 2, 2, "%s %s", i == gs->menuIdx ? ">" : " ", opts[i]);
        if (i == gs->menuIdx)
            wattroff(w, COLOR_PAIR(CP_SELECTED));
        else
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
    }
    wnoutrefresh(w);
}

/* Collect all non-boss enemy indices across all dungeons, ordered by dungeon. */
static int ency_normal_enemies(int *out, int max) {
    int n = 0;
    for (int d = 0; d < NUM_DUNGEONS && n < max; d++) {
        const DungeonDef *dg = data_dungeon(d);
        for (int e = 0; e < dg->numEnemies && n < max; e++)
            out[n++] = dg->enemyIdx[e];
    }
    return n;
}

static int ency_bosses(int *out, int max) {
    int n = 0;
    for (int d = 0; d < NUM_DUNGEONS && n < max; d++) {
        out[n++] = data_dungeon(d)->bossIdx;
    }
    return n;
}

/* Collect item indices for a given slot, filtered by classMask. Used by encyclopedia + shop. */
static int ency_items_for_slot(int slot, int classMask, int *out, int max) {
    int n = 0;
    for (int i = 0; i < data_num_items() && n < max; i++) {
        const ItemDef *it = data_item(i);
        if (it->slot != slot) continue;
        if (it->classMask != 0 && !(it->classMask & classMask)) continue;
        out[n++] = i;
    }
    return n;
}

static const char *ency_enemy_dungeon(int templateIdx) {
    const EnemyTemplate *et = data_enemy(templateIdx);
    if (!et) return "???";
    int di = et->dungeonIdx;
    const DungeonDef *dg = data_dungeon(di);
    return dg ? dg->name : "???";
}

static void render_encyclopedia(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Encyclopedia");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    const char *categories[] = { "Classes", "Skills", "Stats", "Items", "Dungeons", "Enemies", "Bosses", "Combat", "Achievements" };
    for (int i = 0; i < 9; i++) {
        if (i == gs->menuIdx) {
            wattron(w, COLOR_PAIR(CP_SELECTED));
            mvwprintw(w, 3 + i, 1, " > %-23s", categories[i]);
            wattroff(w, COLOR_PAIR(CP_SELECTED));
        } else {
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, 3 + i, 1, "   %-23s", categories[i]);
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_encyclopedia_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    const char *descriptions[] = {
        "Browse all four hero classes.",
        "View all skill choices for your class.",
        "Learn what each stat does.",
        "View equipment organized by slot.",
        "Explore dungeons and their inhabitants.",
        "Study the creatures in each dungeon.",
        "Face the guardians that rule each dungeon.",
        "How damage, ticks, and combat mechanics work.",
        "View milestones and their permanent bonuses."
    };

    if (gs->menuIdx >= 0 && gs->menuIdx < 9) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, 2, 2, "%s", descriptions[gs->menuIdx]);
        wattroff(w, COLOR_PAIR(CP_WHITE));
    }

    wnoutrefresh(w);
}

static void render_ency_classes(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Classes");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    for (int i = 0; i < NUM_CLASSES; i++) {
        const ClassDef *cd = data_class(i);
        if (i == gs->menuIdx) {
            wattron(w, COLOR_PAIR(CP_SELECTED));
            mvwprintw(w, 3 + i * 2, 1, " > %-23s", cd->name);
            wattroff(w, COLOR_PAIR(CP_SELECTED));
        } else {
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, 3 + i * 2, 1, "   %-23s", cd->name);
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_ency_classes_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    if (gs->menuIdx < 0 || gs->menuIdx >= NUM_CLASSES) {
        wnoutrefresh(w);
        return;
    }

    const ClassDef *cd = data_class(gs->menuIdx);
    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "%s", cd->name);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, 2, 2, "%s", cd->description);
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, 3, 2, "STR %2d  AGI %2d  INT %2d  WIS %2d",
              cd->baseStats[STR], cd->baseStats[AGI], cd->baseStats[INT_], cd->baseStats[WIS]);
    mvwprintw(w, 4, 2, "VIT %2d  DEF %2d  SPD %2d",
              cd->baseStats[VIT], cd->baseStats[DEF], cd->baseStats[SPD]);
    mvwprintw(w, 5, 2, "HP: %d +%d/Lv  %s: %d/%d",
              cd->baseHp, cd->hpPerLevel, cd->resourceName, cd->baseResource, cd->maxResource);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    char growthBuf[128];
    int pos = 0;
    pos += snprintf(growthBuf + pos, sizeof(growthBuf) - pos, "Growth:");
    for (int s = 0; s < NUM_STATS; s++) {
        if (cd->growthPerLevel[s] > 0) {
            pos += snprintf(growthBuf + pos, sizeof(growthBuf) - pos, " %s+%d",
                           data_stat_short(s), cd->growthPerLevel[s]);
        }
    }
    wattron(w, COLOR_PAIR(CP_GREEN));
    mvwprintw(w, 6, 2, "%s", growthBuf);
    wattroff(w, COLOR_PAIR(CP_GREEN));

    wnoutrefresh(w);
}

static void render_ency_stats(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Stats");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    for (int i = 0; i < NUM_STATS; i++) {
        if (i == gs->menuIdx) {
            wattron(w, COLOR_PAIR(CP_SELECTED));
            mvwprintw(w, 3 + i, 1, " > %-23s", data_stat_name(i));
            wattroff(w, COLOR_PAIR(CP_SELECTED));
        } else {
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, 3 + i, 1, "   %-23s", data_stat_name(i));
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_ency_stats_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    if (gs->menuIdx < 0 || gs->menuIdx >= NUM_STATS) {
        wnoutrefresh(w);
        return;
    }

    const char *statDescs[NUM_STATS][3] = {
        { "Increases physical damage", "+2 max HP per point", "Primary: Warrior" },
        { "Increases crit and dodge chance", "Increases attack speed", "Primary: Rogue" },
        { "Increases spell damage", "+0.2% XP bonus per point", "Primary: Mage" },
        { "Increases heal power", "Flat DR of WIS*0.15", "Primary: Priest" },
        { "Increases max HP", "Heals VIT*2 HP per kill", "Benefits all classes" },
        { "Reduces damage taken", "Block chance (Warrior)", "Benefits all classes" },
        { "Increases attack speed", "Higher SPD = more attacks/sec", "Benefits all classes" }
    };

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "%s (%s)", data_stat_name(gs->menuIdx), data_stat_short(gs->menuIdx));
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, 3, 2, "%s", statDescs[gs->menuIdx][0]);
    mvwprintw(w, 4, 2, "%s", statDescs[gs->menuIdx][1]);
    mvwprintw(w, 5, 2, "%s", statDescs[gs->menuIdx][2]);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    wnoutrefresh(w);
}

static void render_ency_items(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Items");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, 2, 2, "<%s>", data_slot_name(gs->encycSlot));
    mvwprintw(w, 2, 16, "[</>]");
    wattroff(w, COLOR_PAIR(CP_WHITE));

    int items[200];
    int cm = (1 << gs->hero.classId);
    int nItems = ency_items_for_slot(gs->encycSlot, cm, items, 200);

    int maxRows = PANEL_H - 7;
    int scroll = 0;
    if (gs->menuIdx >= maxRows) scroll = gs->menuIdx - maxRows + 1;

    int row = 4;
    for (int i = 0; i < nItems && (i - scroll) < maxRows; i++) {
        if (i < scroll) continue;
        const ItemDef *it = data_item(items[i]);
        int sel = (i == gs->menuIdx);
        int rc = data_rarity_color(it->rarity);

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else     wattron(w, COLOR_PAIR(rc));

        mvwprintw(w, row, 1, "%s%-21.21s", sel ? " > " : "   ", it->name);

        if (sel) wattroff(w, COLOR_PAIR(CP_SELECTED));
        else     wattroff(w, COLOR_PAIR(rc));
        row++;
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_ency_enemies(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Enemies");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    int enemies[50];
    int nEnemies = ency_normal_enemies(enemies, 50);

    int maxRows = PANEL_H - 6;
    int scroll = 0;
    if (gs->menuIdx >= maxRows) scroll = gs->menuIdx - maxRows + 1;

    int row = 3;
    for (int i = 0; i < nEnemies && (i - scroll) < maxRows; i++) {
        if (i < scroll) continue;
        const EnemyTemplate *et = data_enemy(enemies[i]);
        int sel = (i == gs->menuIdx);

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else     wattron(w, COLOR_PAIR(CP_DEFAULT));

        mvwprintw(w, row, 1, "%s%-21.21s", sel ? " > " : "   ", et->name);

        if (sel) wattroff(w, COLOR_PAIR(CP_SELECTED));
        else     wattroff(w, COLOR_PAIR(CP_DEFAULT));
        row++;
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_ency_enemies_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    int enemies[50];
    int nEnemies = ency_normal_enemies(enemies, 50);

    if (gs->menuIdx < 0 || gs->menuIdx >= nEnemies) {
        wnoutrefresh(w);
        return;
    }

    const EnemyTemplate *et = data_enemy(enemies[gs->menuIdx]);
    if (!et) {
        wnoutrefresh(w);
        return;
    }

    wattron(w, COLOR_PAIR(CP_WHITE));
    for (int i = 0; i < ART_LINES; i++) {
        int xoff = (RIGHT_W - ART_COLS) / 2 - 2;
        if (xoff < 1) xoff = 1;
        mvwprintw(w, 1 + i, xoff, "%s", et->art[i]);
    }
    wattroff(w, COLOR_PAIR(CP_WHITE));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 6, 2, "%-20s", et->name);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    wattron(w, COLOR_PAIR(CP_WHITE));
    wprintw(w, " HP%d ATK%d DEF%d", et->hp, et->attack, et->defense);
    mvwprintw(w, 7, 2, "SPD%d  XP%d  Gold%d  %s",
              et->speed, et->xpReward, et->goldReward, ency_enemy_dungeon(enemies[gs->menuIdx]));
    wattroff(w, COLOR_PAIR(CP_WHITE));

    wnoutrefresh(w);
}

static void render_ency_bosses(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Bosses");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    int bosses[10];
    int nBosses = ency_bosses(bosses, 10);

    for (int i = 0; i < nBosses; i++) {
        const EnemyTemplate *et = data_enemy(bosses[i]);
        int sel = (i == gs->menuIdx);

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else     wattron(w, COLOR_PAIR(CP_MAGENTA));

        mvwprintw(w, 3 + i * 2, 1, "%s%-21.21s", sel ? " > " : "   ", et->name);

        if (sel) wattroff(w, COLOR_PAIR(CP_SELECTED));
        else     wattroff(w, COLOR_PAIR(CP_MAGENTA));
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_ency_bosses_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    int bosses[10];
    int nBosses = ency_bosses(bosses, 10);

    if (gs->menuIdx < 0 || gs->menuIdx >= nBosses) {
        wnoutrefresh(w);
        return;
    }

    const EnemyTemplate *et = data_enemy(bosses[gs->menuIdx]);
    if (!et) {
        wnoutrefresh(w);
        return;
    }

    wattron(w, COLOR_PAIR(CP_WHITE));
    for (int i = 0; i < ART_LINES; i++) {
        int xoff = (RIGHT_W - ART_COLS) / 2 - 2;
        if (xoff < 1) xoff = 1;
        mvwprintw(w, 1 + i, xoff, "%s", et->art[i]);
    }
    wattroff(w, COLOR_PAIR(CP_WHITE));

    wattron(w, COLOR_PAIR(CP_MAGENTA) | A_BOLD);
    mvwprintw(w, 6, 2, "%-20s", et->name);
    wattroff(w, COLOR_PAIR(CP_MAGENTA) | A_BOLD);
    wattron(w, COLOR_PAIR(CP_WHITE));
    wprintw(w, " HP%d ATK%d DEF%d", et->hp, et->attack, et->defense);
    mvwprintw(w, 7, 2, "SPD%d  XP%d  Gold%d  %s",
              et->speed, et->xpReward, et->goldReward, ency_enemy_dungeon(bosses[gs->menuIdx]));
    wattroff(w, COLOR_PAIR(CP_WHITE));

    wnoutrefresh(w);
}

static void render_ency_skills(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    const ClassDef *cls = data_class(gs->hero.classId);
    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Skills (%s)", cls ? cls->name : "");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    for (int t = 0; t < MAX_SKILL_TIERS; t++) {
        int reqLvl = data_skill_level(t);
        int hdrRow = 3 + t * 3;

        wattron(w, COLOR_PAIR(CP_YELLOW));
        mvwprintw(w, hdrRow, 2, "Lv%d:", reqLvl);
        wattroff(w, COLOR_PAIR(CP_YELLOW));

        for (int opt = 0; opt < 2; opt++) {
            int idx = t * 2 + opt;
            int row = hdrRow + 1 + opt;
            int isSel = (idx == gs->menuIdx);
            const SkillDef *sk = data_skill(gs->hero.classId, t, opt);

            if (isSel) wattron(w, COLOR_PAIR(CP_SELECTED));
            else       wattron(w, COLOR_PAIR(CP_DEFAULT));

            mvwprintw(w, row, 2, "%s%s", isSel ? "> " : "  ", sk ? sk->name : "???");

            if (isSel) wattroff(w, COLOR_PAIR(CP_SELECTED));
            else       wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_ency_skills_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    int total = MAX_SKILL_TIERS * 2;
    if (gs->menuIdx < 0 || gs->menuIdx >= total) { wnoutrefresh(w); return; }

    int tier = gs->menuIdx / 2;
    int option = gs->menuIdx % 2;
    const SkillDef *sk = data_skill(gs->hero.classId, tier, option);
    if (!sk) { wnoutrefresh(w); return; }

    const ClassDef *cls = data_class(gs->hero.classId);

    wattron(w, COLOR_PAIR(CP_WHITE) | A_BOLD);
    mvwprintw(w, 1, 2, "%s", sk->name);
    wattroff(w, COLOR_PAIR(CP_WHITE) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, 3, 2, "%s", sk->description);
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    wattron(w, COLOR_PAIR(CP_YELLOW));
    mvwprintw(w, 4, 2, "CD: %d  Cost: %d %s",
              sk->cooldown, sk->resourceCost,
              cls ? cls->resourceName : "");
    wattroff(w, COLOR_PAIR(CP_YELLOW));

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, 6, 2, "Lv %d", data_skill_level(tier));
    wattroff(w, COLOR_PAIR(CP_CYAN));

    wnoutrefresh(w);
}

static void render_ency_dungeons(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Dungeons");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    for (int i = 0; i < NUM_DUNGEONS; i++) {
        const DungeonDef *dg = data_dungeon(i);
        int sel = (i == gs->menuIdx);

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else     wattron(w, COLOR_PAIR(dg->colorPair));

        mvwprintw(w, 3 + i, 1, "%sLv%-2d %-18.18s",
                  sel ? " > " : "   ", dg->levelReq, dg->name);

        if (sel) wattroff(w, COLOR_PAIR(CP_SELECTED));
        else     wattroff(w, COLOR_PAIR(dg->colorPair));
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_ency_dungeons_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    if (gs->menuIdx < 0 || gs->menuIdx >= NUM_DUNGEONS) {
        wnoutrefresh(w);
        return;
    }

    const DungeonDef *dg = data_dungeon(gs->menuIdx);
    if (!dg) { wnoutrefresh(w); return; }

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "%s (Lv %d)", dg->name, dg->levelReq);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, 2, 2, "%s", dg->description);
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, 3, 2, "Enemies:");
    wattroff(w, COLOR_PAIR(CP_WHITE));

    int row = 4;
    for (int e = 0; e < dg->numEnemies && row < 9; e++) {
        const EnemyTemplate *et = data_enemy(dg->enemyIdx[e]);
        if (!et) continue;
        wattron(w, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(w, row++, 3, "%s", et->name);
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
    }

    const EnemyTemplate *boss = data_enemy(dg->bossIdx);
    if (boss) {
        wattron(w, COLOR_PAIR(CP_MAGENTA));
        mvwprintw(w, row, 2, "Boss: %s", boss->name);
        wattroff(w, COLOR_PAIR(CP_MAGENTA));
    }

    wnoutrefresh(w);
}

static void render_ency_combat(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Combat Mechanics");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    const char *topics[] = {
        "Tick System", "Hero Damage", "Enemy Armor", "Critical Hits",
        "Dodge & Evasion", "Block (Warrior)", "Damage Reduction",
        "Buffs & Effects", "Shields & Ward", "Stun & Slow",
        "Death & Revive", "Boss Encounters", "Loot System",
        "Resource & Regen"
    };
    int nTopics = 14;

    for (int i = 0; i < nTopics; i++) {
        if (i == gs->menuIdx) {
            wattron(w, COLOR_PAIR(CP_SELECTED));
            mvwprintw(w, 3 + i, 1, " > %-23s", topics[i]);
            wattroff(w, COLOR_PAIR(CP_SELECTED));
        } else {
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, 3 + i, 1, "   %-23s", topics[i]);
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_ency_combat_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    const char *topics[] = {
        "Tick System", "Hero Damage", "Enemy Armor", "Critical Hits",
        "Dodge & Evasion", "Block (Warrior)", "Damage Reduction",
        "Buffs & Effects", "Shields & Ward", "Stun & Slow",
        "Death & Revive", "Boss Encounters", "Loot System",
        "Resource & Regen"
    };

    if (gs->menuIdx < 0 || gs->menuIdx >= 14) {
        wnoutrefresh(w);
        return;
    }

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "%s", topics[gs->menuIdx]);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_WHITE));
    int row = 3;
    switch (gs->menuIdx) {
    case 0: /* Tick System */
        mvwprintw(w, row++, 2, "Combat runs in timed attack cycles.");
        mvwprintw(w, row++, 2, "Each cycle: cooldowns, buffs, skills, hit.");
        mvwprintw(w, row++, 2, "Base ~1.25 atk/s, scales with SPD stat.");
        mvwprintw(w, row++, 2, "Higher SPD = more attacks per second.");
        break;
    case 1: /* Hero Damage — condense to 4 lines (wEnemy has 4 content rows) */
        mvwprintw(w, row++, 2, "War STR*1.5  Rog AGI*1.4+STR*0.4");
        mvwprintw(w, row++, 2, "Mage INT*1.5+WIS*0.5  Pri WIS*1.2");
        mvwprintw(w, row++, 2, "Rogue gets 1.8x crit multiplier.");
        mvwprintw(w, row++, 2, "+/-10%% random variance on every hit.");
        break;
    case 2: /* Enemy Armor */
        mvwprintw(w, row++, 2, "Enemy damage reduction:");
        mvwprintw(w, row++, 2, "  armor = DEF^2 / (DEF + 100)");
        mvwprintw(w, row++, 2, "Your hit = baseDmg - armor (min 1)");
        mvwprintw(w, row++, 2, "Some skills ignore armor entirely.");
        break;
    case 3: /* Critical Hits */
        mvwprintw(w, row++, 2, "Crit chance: AGI / 300 (max 50%%)");
        mvwprintw(w, row++, 2, "Crit multiplier: 1.5x (Rogue: 1.8x)");
        mvwprintw(w, row++, 2, "Buff critDmgBonus stacks additively.");
        mvwprintw(w, row++, 2, "Crit check happens after dmg variance.");
        break;
    case 4: /* Dodge & Evasion */
        mvwprintw(w, row++, 2, "Dodge chance: AGI / 300 (max 35%%)");
        mvwprintw(w, row++, 2, "Buff dodgeBonus stacks additively.");
        mvwprintw(w, row++, 2, "dodgeNext buff: guaranteed next dodge.");
        mvwprintw(w, row++, 2, "Dodged attacks deal zero damage.");
        break;
    case 5: /* Block (Warrior) */
        mvwprintw(w, row++, 2, "Warrior only. DEF / 250 (max 30%%)");
        mvwprintw(w, row++, 2, "Blocks reduce damage by 30%%.");
        mvwprintw(w, row++, 2, "Block check is after dodge check.");
        mvwprintw(w, row++, 2, "Other classes cannot block.");
        break;
    case 6: /* Damage Reduction */
        mvwprintw(w, row++, 2, "%% DR: DEF / (DEF + 100) (max 50%%)");
        mvwprintw(w, row++, 2, "Flat DR: WIS * 0.15");
        mvwprintw(w, row++, 2, "Applied before block, after variance.");
        mvwprintw(w, row++, 2, "Both stack (%% first, flat second).");
        break;
    case 7: /* Buffs & Effects */
        mvwprintw(w, row++, 2, "Skills create timed buffs (ticksLeft).");
        mvwprintw(w, row++, 2, "damageMul, HoT, DoT, dodge, crit...");
        mvwprintw(w, row++, 2, "Buffs tick each combat step.");
        mvwprintw(w, row++, 2, "Max 10 active buffs at once.");
        break;
    case 8: /* Shields & Ward */
        mvwprintw(w, row++, 2, "buffShieldPct: absorb shield (HP%%).");
        mvwprintw(w, row++, 2, "Shields drain before HP damage.");
        mvwprintw(w, row++, 2, "Multiple shields stack, drain in order.");
        mvwprintw(w, row++, 2, "Mana Shield: damage costs mana instead.");
        break;
    case 9: /* Stun & Slow */
        mvwprintw(w, row++, 2, "Stunned enemies skip their attack.");
        mvwprintw(w, row++, 2, "Slowed enemies also skip their turn.");
        mvwprintw(w, row++, 2, "Duration in ticks, decrements each tick.");
        mvwprintw(w, row++, 2, "Hero still attacks stunned enemies.");
        break;
    case 10: /* Death & Revive */
        mvwprintw(w, row++, 2, "On death: lose 10%% gold, clear buffs.");
        mvwprintw(w, row++, 2, "Revive after 4 ticks at full HP/mana.");
        mvwprintw(w, row++, 2, "Dungeon kill count resets to 0.");
        mvwprintw(w, row++, 2, "Boss progress is lost on death.");
        break;
    case 11: /* Boss Encounters */
        mvwprintw(w, row++, 2, "Boss spawns after 50 normal kills.");
        mvwprintw(w, row++, 2, "6 tick delay after boss kill.");
        mvwprintw(w, row++, 2, "Kill count resets after boss phase.");
        mvwprintw(w, row++, 2, "Bosses are always the dungeon guardian.");
        break;
    case 12: /* Loot System */
        mvwprintw(w, row++, 2, "Normal: roll vs enemy dropChance.");
        mvwprintw(w, row++, 2, "Boss: guaranteed weighted rarity drop.");
        mvwprintw(w, row++, 2, "Weights 50/30/12/6/2 (Com > Legendary)");
        mvwprintw(w, row++, 2, "Dungeon caps max normal drop rarity.");
        break;
    case 13: /* Resource & Regen */
        mvwprintw(w, row++, 2, "Each class has a resource (Rage, etc).");
        mvwprintw(w, row++, 2, "Regen per tick: class resourceRegen.");
        mvwprintw(w, row++, 2, "VIT heals VIT*2 HP on each kill.");
        mvwprintw(w, row++, 2, "Level-up fully restores HP and resource.");
        break;
    }
    wattroff(w, COLOR_PAIR(CP_WHITE));

    wnoutrefresh(w);
}

static const ItemDef *equip_highlighted(const GameState *gs) {
    if (gs->menuIdx < NUM_SLOTS) {
        if (gs->hero.hasEquip[gs->menuIdx])
            return &gs->hero.equipment[gs->menuIdx];
    } else {
        int filter = gs->equipFilter > 0 ? gs->equipFilter - 1 : -1;
        int viewIdx[MAX_INVENTORY];
        int viewN = inv_build_view(&gs->hero, filter, gs->equipSort, viewIdx, MAX_INVENTORY);
        int bi = gs->menuIdx - NUM_SLOTS;
        if (bi >= 0 && bi < viewN)
            return &gs->hero.inventory[viewIdx[bi]];
    }
    return NULL;
}

/* Render item stats in the right panel, with optional comparison deltas against currently equipped (cmp). */
static void render_item_detail(GameState *gs, const ItemDef *it, const ItemDef *cmp) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    if (!it) {
        wnoutrefresh(w);
        return;
    }

    int rc = data_rarity_color(it->rarity);
    wattron(w, COLOR_PAIR(rc) | A_BOLD);
    mvwprintw(w, 1, 2, "%s", it->name);
    wattroff(w, COLOR_PAIR(rc) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, 1, 2 + (int)strlen(it->name) + 1, "(%s %s)",
              data_rarity_name(it->rarity), data_slot_name(it->slot));
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    if (it->levelReq > 0) {
        int canUse = gs->hero.level >= it->levelReq;
        wattron(w, COLOR_PAIR(canUse ? CP_WHITE : CP_RED));
        mvwprintw(w, 2, 2, "Requires Lv.%d", it->levelReq);
        wattroff(w, COLOR_PAIR(canUse ? CP_WHITE : CP_RED));
    }

    if (cmp) {
        int erc = data_rarity_color(cmp->rarity);
        wattron(w, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(w, 3, 2, "Worn:");
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
        wattron(w, COLOR_PAIR(erc));
        wprintw(w, " %s", cmp->name);
        wattroff(w, COLOR_PAIR(erc));
    }

    int col = 2;
    int row = cmp ? 5 : 4;
    for (int i = 0; i < NUM_STATS; i++) {
        int sv = it->stats[i];
        int ev = cmp ? cmp->stats[i] : 0;
        if (sv == 0 && ev == 0) continue;
        int diff = sv - ev;

        if (sv > 0) {
            wattron(w, COLOR_PAIR(CP_GREEN));
            mvwprintw(w, row, col, "%-4s%+d", data_stat_short(i), sv);
            wattroff(w, COLOR_PAIR(CP_GREEN));
        } else {
            wattron(w, COLOR_PAIR(CP_WHITE));
            mvwprintw(w, row, col, "%-4s  ", data_stat_short(i));
            wattroff(w, COLOR_PAIR(CP_WHITE));
        }

        if (cmp && diff != 0) {
            int dc = diff > 0 ? CP_GREEN : CP_RED;
            wattron(w, COLOR_PAIR(dc));
            wprintw(w, "(%+d)", diff);
            wattroff(w, COLOR_PAIR(dc));
        }

        col += 16;
        if (col + 14 > RIGHT_W - 2) { col = 2; row++; }
    }

    wnoutrefresh(w);
}

/*
 * Character screen right panel: shows what adding talent points will do.
 * Handles the soft-cap dead zone: past 30 talent points, integer division
 * means some +1 increments produce zero visible stat change. This function
 * searches for the minimum increment (1–20) that actually changes any of
 * the 10 derived fields by temporarily adding points, computing stats,
 * and comparing. Shows "Next bonus: +N pts" when in a dead zone.
 */
static void render_stat_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    int stat = gs->menuIdx;
    if (stat < 0 || stat >= NUM_STATS) { wnoutrefresh(w); return; }

    Hero *h = &gs->hero;
    EStats cur = hero_effective_stats(h);

    int inc;
    EStats nxt;
    for (inc = 1; inc <= 20; inc++) {
        h->talents[stat] += inc;
        nxt = hero_effective_stats(h);
        h->talents[stat] -= inc;
        if (nxt.damage != cur.damage || nxt.maxHp != cur.maxHp ||
            nxt.critChance != cur.critChance || nxt.dodgeChance != cur.dodgeChance ||
            nxt.dmgReduction != cur.dmgReduction || nxt.blockChance != cur.blockChance ||
            nxt.tickRate != cur.tickRate || nxt.healPower != cur.healPower ||
            nxt.xpMultiplier != cur.xpMultiplier || nxt.flatDmgReduce != cur.flatDmgReduce)
            break;
    }

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "%s", data_stat_name(stat));
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    wattron(w, COLOR_PAIR(CP_WHITE));
    wprintw(w, "  %d", cur.stats[stat]);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    if (inc == 1) {
        wattron(w, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(w, 2, 2, "+1 point gives:");
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
    } else if (inc <= 20) {
        wattron(w, COLOR_PAIR(CP_YELLOW));
        mvwprintw(w, 2, 2, "Next bonus: +%d pts", inc);
        wattroff(w, COLOR_PAIR(CP_YELLOW));
    } else {
        wattron(w, COLOR_PAIR(CP_RED));
        mvwprintw(w, 2, 2, "At maximum effect");
        wattroff(w, COLOR_PAIR(CP_RED));
    }

    int row = 3;
    int col2 = RIGHT_W / 2;

    if (nxt.damage != cur.damage) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, 2, "DMG %d", cur.damage);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " > %d (+%d)", nxt.damage, nxt.damage - cur.damage);
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }
    if (nxt.maxHp != cur.maxHp) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, col2, "HP %d", cur.maxHp);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " > %d", nxt.maxHp);
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }
    if (nxt.damage != cur.damage || nxt.maxHp != cur.maxHp) row++;

    if (nxt.critChance != cur.critChance) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, 2, "Crit %.1f%%", cur.critChance * 100);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " > %.1f%%", nxt.critChance * 100);
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }
    if (nxt.dodgeChance != cur.dodgeChance) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, col2, "Dodge %.1f%%", cur.dodgeChance * 100);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " > %.1f%%", nxt.dodgeChance * 100);
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }
    if (nxt.critChance != cur.critChance || nxt.dodgeChance != cur.dodgeChance) row++;

    if (nxt.dmgReduction != cur.dmgReduction) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, 2, "DR %.1f%%", cur.dmgReduction * 100);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " > %.1f%%", nxt.dmgReduction * 100);
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }
    if (nxt.blockChance != cur.blockChance) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, col2, "Block %.1f%%", cur.blockChance * 100);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " > %.1f%%", nxt.blockChance * 100);
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }
    if (nxt.dmgReduction != cur.dmgReduction || nxt.blockChance != cur.blockChance) row++;

    if (nxt.tickRate != cur.tickRate) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, 2, "Atk %.2f/s", 1000.0f / cur.tickRate);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " > %.2f/s", 1000.0f / nxt.tickRate);
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }
    if (nxt.healPower != cur.healPower) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, col2, "Heal %d", cur.healPower);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " > %d", nxt.healPower);
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }
    if (nxt.tickRate != cur.tickRate || nxt.healPower != cur.healPower) row++;

    if (nxt.xpMultiplier != cur.xpMultiplier) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, 2, "XP +%.1f%%", (cur.xpMultiplier - 1.0f) * 100);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " > +%.1f%%", (nxt.xpMultiplier - 1.0f) * 100);
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }
    if (nxt.flatDmgReduce != cur.flatDmgReduce) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, col2, "Ward %d", cur.flatDmgReduce);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_GREEN));
        wprintw(w, " > %d", nxt.flatDmgReduce);
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }

    wnoutrefresh(w);
}

static void render_skill_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    int total = MAX_SKILL_TIERS * 2;
    if (gs->menuIdx < 0 || gs->menuIdx >= total) { wnoutrefresh(w); return; }

    int tier = gs->menuIdx / 2;
    int option = gs->menuIdx % 2;
    const SkillDef *sk = data_skill(gs->hero.classId, tier, option);
    if (!sk) { wnoutrefresh(w); return; }

    int reqLvl = data_skill_level(tier);
    int isLocked = gs->hero.level < reqLvl;
    int chosen = gs->hero.skillChoices[tier];

    wattron(w, COLOR_PAIR(CP_WHITE) | A_BOLD);
    mvwprintw(w, 1, 2, "%s", sk->name);
    wattroff(w, COLOR_PAIR(CP_WHITE) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, 3, 2, "%s", sk->description);
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    const ClassDef *cls = data_class(gs->hero.classId);
    wattron(w, COLOR_PAIR(CP_YELLOW));
    mvwprintw(w, 4, 2, "CD: %d  Cost: %d %s",
              sk->cooldown, sk->resourceCost,
              cls ? cls->resourceName : "");
    wattroff(w, COLOR_PAIR(CP_YELLOW));

    if (isLocked) {
        wattron(w, COLOR_PAIR(CP_RED));
        mvwprintw(w, 6, 2, "Locked (Lv %d)", reqLvl);
        wattroff(w, COLOR_PAIR(CP_RED));
    } else if (chosen == option) {
        wattron(w, COLOR_PAIR(CP_CYAN));
        mvwprintw(w, 6, 2, "Chosen");
        wattroff(w, COLOR_PAIR(CP_CYAN));
    } else if (chosen >= 0) {
        wattron(w, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(w, 6, 2, "Not chosen");
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
    } else {
        wattron(w, COLOR_PAIR(CP_GREEN));
        mvwprintw(w, 6, 2, "Available");
        wattroff(w, COLOR_PAIR(CP_GREEN));
    }

    wnoutrefresh(w);
}

static void render_skills(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Skills");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    for (int t = 0; t < MAX_SKILL_TIERS; t++) {
        int reqLvl = data_skill_level(t);
        int isLocked = gs->hero.level < reqLvl;
        int chosen = gs->hero.skillChoices[t];
        int hdrRow = 2 + t * 3;

        wattron(w, COLOR_PAIR(isLocked ? CP_RED : CP_YELLOW));
        mvwprintw(w, hdrRow, 2, "Lv%d:", reqLvl);
        wattroff(w, COLOR_PAIR(isLocked ? CP_RED : CP_YELLOW));

        for (int opt = 0; opt < 2; opt++) {
            int idx = t * 2 + opt;
            int row = hdrRow + 1 + opt;
            int isSel = (idx == gs->menuIdx);
            const SkillDef *sk = data_skill(gs->hero.classId, t, opt);

            const char *pfx;
            if (isSel)              pfx = "> ";
            else if (chosen == opt) pfx = "* ";
            else                    pfx = "  ";

            if (isSel)
                wattron(w, COLOR_PAIR(CP_SELECTED));
            else if (isLocked)
                wattron(w, COLOR_PAIR(CP_RED));
            else if (chosen == opt)
                wattron(w, COLOR_PAIR(CP_CYAN));
            else if (chosen >= 0)
                wattron(w, COLOR_PAIR(CP_DEFAULT));
            else
                wattron(w, COLOR_PAIR(CP_WHITE));

            mvwprintw(w, row, 2, "%s%s", pfx, sk ? sk->name : "???");

            if (isSel)
                wattroff(w, COLOR_PAIR(CP_SELECTED));
            else if (isLocked)
                wattroff(w, COLOR_PAIR(CP_RED));
            else if (chosen == opt)
                wattroff(w, COLOR_PAIR(CP_CYAN));
            else if (chosen >= 0)
                wattroff(w, COLOR_PAIR(CP_DEFAULT));
            else
                wattroff(w, COLOR_PAIR(CP_WHITE));
        }
    }

    int hasAny = 0;
    for (int t = 0; t < MAX_SKILL_TIERS; t++)
        if (gs->hero.skillChoices[t] >= 0) { hasAny = 1; break; }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 3, 2, "[Enter] Choose");
    if (hasAny) {
        int resetCost = gs->hero.level * gs->hero.level * 2;
        int canReset = gs->hero.gold >= resetCost;
        wattroff(w, COLOR_PAIR(CP_CYAN));
        wattron(w, COLOR_PAIR(canReset ? CP_CYAN : CP_RED));
        mvwprintw(w, PANEL_H - 3, 17, "[R] Reset %dG", resetCost);
        wattroff(w, COLOR_PAIR(canReset ? CP_CYAN : CP_RED));
        wattron(w, COLOR_PAIR(CP_CYAN));
    }
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_achievements(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Achievements");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    int maxRows = PANEL_H - 5;
    int scroll = 0;
    if (gs->menuIdx >= maxRows) scroll = gs->menuIdx - maxRows + 1;

    int row = 3;
    for (int i = 0; i < NUM_ACHIEVEMENTS && (i - scroll) < maxRows; i++) {
        if (i < scroll) continue;
        const AchievementDef *ad = data_achievement(i);
        int has = ACH_HAS(&gs->hero, i);
        int sel = (i == gs->menuIdx);

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else if (has) wattron(w, COLOR_PAIR(CP_GREEN));
        else wattron(w, COLOR_PAIR(CP_DEFAULT));

        mvwprintw(w, row, 1, "%s%c %-21.21s",
                  sel ? " > " : "   ",
                  has ? '*' : ' ',
                  ad->name);

        wattroff(w, COLOR_PAIR(CP_SELECTED));
        wattroff(w, COLOR_PAIR(CP_GREEN));
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
        row++;
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_achievement_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    if (gs->menuIdx < 0 || gs->menuIdx >= NUM_ACHIEVEMENTS) {
        wnoutrefresh(w);
        return;
    }

    const AchievementDef *ad = data_achievement(gs->menuIdx);
    int has = ACH_HAS(&gs->hero, gs->menuIdx);

    wattron(w, COLOR_PAIR(has ? CP_GREEN : CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "%s %s", has ? "[*]" : "[ ]", ad->name);
    wattroff(w, COLOR_PAIR(has ? CP_GREEN : CP_YELLOW) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, 3, 2, "%s", ad->description);
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    const char *bonusNames[] = { "Max HP", "Damage", "XP" };
    const char *bonusFmt[]   = { "+%d HP", "+%d%% DMG", "+%d%% XP" };
    wattron(w, COLOR_PAIR(CP_CYAN));
    char bonusBuf[32];
    snprintf(bonusBuf, sizeof(bonusBuf), bonusFmt[ad->bonusType], ad->bonusValue);
    mvwprintw(w, 5, 2, "Bonus: %s (%s)", bonusBuf, bonusNames[ad->bonusType]);
    wattroff(w, COLOR_PAIR(CP_CYAN));

    wnoutrefresh(w);
}

static void render_titles(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Titles");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    int sel = (gs->menuIdx == 0);
    if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
    else wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, 3, 1, "%s(None)%*s",
              sel ? " > " : "   ", LEFT_W - 12, "");
    wattroff(w, COLOR_PAIR(CP_SELECTED));
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    for (int i = 0; i < NUM_TITLES; i++) {
        const TitleDef *td = data_title(i);
        int unlocked = (td->achievementIdx >= 0 && ACH_HAS(&gs->hero, td->achievementIdx));
        int active = (gs->hero.activeTitle == i + 1);
        sel = (gs->menuIdx == i + 1);

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else if (active) wattron(w, COLOR_PAIR(CP_CYAN));
        else if (unlocked) wattron(w, COLOR_PAIR(CP_GREEN));
        else wattron(w, COLOR_PAIR(CP_DEFAULT));

        mvwprintw(w, 4 + i, 1, "%s%c %-21.21s",
                  sel ? " > " : "   ",
                  active ? '*' : (unlocked ? ' ' : '?'),
                  unlocked ? td->name : "???");

        wattroff(w, COLOR_PAIR(CP_SELECTED));
        wattroff(w, COLOR_PAIR(CP_CYAN));
        wattroff(w, COLOR_PAIR(CP_GREEN));
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 3, 2, "[Enter] Select");
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void bulk_sell_preview(const Hero *h, int threshold, int *outCount, int *outGold) {
    int count = 0, gold = 0;
    for (int i = 0; i < h->invCount; i++) {
        if (h->inventory[i].rarity < threshold) {
            int sp = h->inventory[i].price / 2;
            if (sp < 1) sp = 1;
            gold += sp;
            count++;
        }
    }
    *outCount = count;
    *outGold = gold;
}

static void render_bulk_sell(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Bulk Sell");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, 2, 2, "Bag: %d/%d items", gs->hero.invCount, MAX_INVENTORY);
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    const char *labels[] = {
        "Sell all Common",
        "Sell all Common+Uncommon",
        "Sell all below Epic",
        "Sell all below Legendary",
    };

    int row = 4;
    for (int i = 0; i < 4; i++) {
        int count, gold;
        bulk_sell_preview(&gs->hero, i + 1, &count, &gold);
        int sel = (i == gs->menuIdx);

        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else if (count > 0) wattron(w, COLOR_PAIR(CP_WHITE));
        else wattron(w, COLOR_PAIR(CP_DEFAULT));

        mvwprintw(w, row, 1, "%s%-24.24s", sel ? " > " : "   ", labels[i]);
        wattroff(w, COLOR_PAIR(CP_SELECTED));
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattroff(w, COLOR_PAIR(CP_DEFAULT));

        if (count > 0) {
            wattron(w, COLOR_PAIR(CP_YELLOW));
            mvwprintw(w, row + 1, 5, "%d item%s for %dg",
                      count, count > 1 ? "s" : "", gold);
            wattroff(w, COLOR_PAIR(CP_YELLOW));
        } else {
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, row + 1, 5, "(nothing to sell)");
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
        row += 3;
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 3, 2, "[Enter] Sell");
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Cancel");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_enemy_panel(GameState *gs) {
    if (gs->screen == SCR_ACHIEVEMENTS) {
        render_achievement_detail(gs);
        return;
    }
    if (gs->screen == SCR_SKILLS) {
        render_skill_detail(gs);
        return;
    }
    if (gs->screen == SCR_CHARACTER) {
        render_stat_detail(gs);
        return;
    }
    if (gs->screen == SCR_EQUIPMENT) {
        const ItemDef *it = equip_highlighted(gs);
        const ItemDef *cmp = NULL;
        if (it && gs->menuIdx >= NUM_SLOTS) {
            int slot = it->slot;
            if (gs->hero.hasEquip[slot])
                cmp = &gs->hero.equipment[slot];
        }
        render_item_detail(gs, it, cmp);
        return;
    }
    if (gs->screen == SCR_SHOP) {
        ItemDef it;
        shop_get_item(gs, gs->menuIdx, &it);
        const ItemDef *cmp = NULL;
        if (it.name[0] != '\0') {
            int slot = it.slot;
            if (gs->hero.hasEquip[slot])
                cmp = &gs->hero.equipment[slot];
            render_item_detail(gs, &it, cmp);
        } else {
            WINDOW *w = gs->wEnemy;
            werase(w);
            wattron(w, COLOR_PAIR(CP_BORDER));
            box(w, 0, 0);
            wattroff(w, COLOR_PAIR(CP_BORDER));
            wnoutrefresh(w);
        }
        return;
    }
    if (gs->screen == SCR_CLASS_SELECT) {
        render_ency_classes_detail(gs);
        return;
    }
    if (gs->screen == SCR_ENCYCLOPEDIA) {
        render_encyclopedia_detail(gs);
        return;
    }
    if (gs->screen == SCR_ENCY_CLASSES) {
        render_ency_classes_detail(gs);
        return;
    }
    if (gs->screen == SCR_ENCY_STATS) {
        render_ency_stats_detail(gs);
        return;
    }
    if (gs->screen == SCR_ENCY_ITEMS) {
        int items[200];
        int cm = (1 << gs->hero.classId);
        int nItems = ency_items_for_slot(gs->encycSlot, cm, items, 200);
        if (gs->menuIdx >= 0 && gs->menuIdx < nItems) {
            const ItemDef *it = data_item(items[gs->menuIdx]);
            render_item_detail(gs, it, NULL);
        } else {
            WINDOW *w = gs->wEnemy;
            werase(w);
            wattron(w, COLOR_PAIR(CP_BORDER));
            box(w, 0, 0);
            wattroff(w, COLOR_PAIR(CP_BORDER));
            wnoutrefresh(w);
        }
        return;
    }
    if (gs->screen == SCR_ENCY_ENEMIES) {
        render_ency_enemies_detail(gs);
        return;
    }
    if (gs->screen == SCR_ENCY_BOSSES) {
        render_ency_bosses_detail(gs);
        return;
    }
    if (gs->screen == SCR_ENCY_SKILLS) {
        render_ency_skills_detail(gs);
        return;
    }
    if (gs->screen == SCR_ENCY_DUNGEONS) {
        render_ency_dungeons_detail(gs);
        return;
    }
    if (gs->screen == SCR_ENCY_COMBAT) {
        render_ency_combat_detail(gs);
        return;
    }

    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    if (!gs->hasEnemy || !gs->inDungeon) {
        if (gs->bossTimer > 0) {
            wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
            mvwprintw(w, 2, RIGHT_W / 2 - 9, "BOSS DEFEATED!");
            wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, 4, RIGHT_W / 2 - 10, "Restarting...");
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        } else if (gs->deathTimer > 0) {
            wattron(w, COLOR_PAIR(CP_RED) | A_BOLD);
            mvwprintw(w, 3, RIGHT_W / 2 - 8, "Reviving...");
            wattroff(w, COLOR_PAIR(CP_RED) | A_BOLD);
        } else if (!gs->inDungeon && gs->heroCreated) {
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, 3, RIGHT_W / 2 - 10, "Select a dungeon...");
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
        wnoutrefresh(w);
        return;
    }

    Enemy *e = &gs->enemy;
    const EnemyTemplate *et = data_enemy(e->templateIdx);

    if (et) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        for (int i = 0; i < ART_LINES; i++) {
            int xoff = (RIGHT_W - ART_COLS) / 2 - 2;
            if (xoff < 1) xoff = 1;
            mvwprintw(w, 1 + i, xoff, "%s", et->art[i]);
        }
        wattroff(w, COLOR_PAIR(CP_WHITE));
    }

    int barW = RIGHT_W - 6;
    int nameColor = gs->isElite ? CP_YELLOW : (gs->bossActive ? CP_MAGENTA : CP_WHITE);
    wattron(w, COLOR_PAIR(nameColor) | A_BOLD);
    mvwprintw(w, 6, 2, "%-20s", e->name);
    wattroff(w, COLOR_PAIR(nameColor) | A_BOLD);
    draw_bar(w, 6, 22, barW - 22, e->hp, e->maxHp, CP_RED, CP_DEFAULT);
    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, 6, barW - 1, "%d/%d", e->hp, e->maxHp);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    if (gs->combatTicks > 0) {
        EStats ces = hero_effective_stats(&gs->hero);
        float secs = gs->combatTicks * ces.tickRate / 1000.0f;
        if (secs > 0.1f) {
            int dps = (int)(gs->combatDmgDealt / secs);
            int dtps = (int)(gs->combatDmgTaken / secs);
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, 7, 2, "DPS:%d  DTPS:%d", dps, dtps);
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
    }

    wnoutrefresh(w);
}

static void render_log_panel(GameState *gs) {
    WINDOW *w = gs->wLog;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    int maxLines = LOG_H - 2;
    int start = gs->logCount - maxLines;
    if (start < 0) start = 0;

    for (int i = start; i < gs->logCount; i++) {
        int row = 1 + (i - start);
        if (row >= LOG_H - 1) break;
        LogLine *l = &gs->log[i];
        wattron(w, COLOR_PAIR(l->color));
        mvwprintw(w, row, 1, " %-*.*s", LOG_LINE_W - 1, LOG_LINE_W - 1, l->text);
        wattroff(w, COLOR_PAIR(l->color));
    }

    wnoutrefresh(w);
}

void ui_render(GameState *gs) {
    render_header(gs);

    switch (gs->screen) {
    case SCR_SAVE_SELECT:  render_save_select(gs);  break;
    case SCR_NEW_SLOT:     render_new_slot(gs);      break;
    case SCR_CLASS_SELECT: render_class_select(gs);  break;
    case SCR_NAME_INPUT:   render_name_input(gs);    break;
    case SCR_MAIN:         render_main_menu(gs);     break;
    case SCR_DUNGEON:      render_dungeon_select(gs);break;
    case SCR_EQUIPMENT:    render_equipment(gs);     break;
    case SCR_SHOP:         render_shop(gs);          break;
    case SCR_CHARACTER:    render_character(gs);     break;
    case SCR_SKILLS:       render_skills(gs);        break;
    case SCR_CONFIRM_QUIT: render_confirm_quit(gs);  break;
    case SCR_ENCYCLOPEDIA:  render_encyclopedia(gs);   break;
    case SCR_ENCY_CLASSES:  render_ency_classes(gs);  break;
    case SCR_ENCY_STATS:    render_ency_stats(gs);    break;
    case SCR_ENCY_ITEMS:    render_ency_items(gs);    break;
    case SCR_ENCY_ENEMIES:  render_ency_enemies(gs);  break;
    case SCR_ENCY_BOSSES:   render_ency_bosses(gs);   break;
    case SCR_ENCY_SKILLS:   render_ency_skills(gs);   break;
    case SCR_ENCY_DUNGEONS: render_ency_dungeons(gs); break;
    case SCR_ENCY_COMBAT:   render_ency_combat(gs);   break;
    case SCR_ACHIEVEMENTS:  render_achievements(gs);   break;
    case SCR_TITLES:        render_titles(gs);         break;
    case SCR_BULK_SELL:     render_bulk_sell(gs);      break;
    }

    render_enemy_panel(gs);
    render_log_panel(gs);
    doupdate();
}

/*
 * Main input dispatcher. Each screen has its own case block handling navigation
 * (up/down/left/right) and actions (enter/esc/hotkeys).
 *
 * Notable complex cases:
 *   SCR_SHOP enter: swap-sell when inventory is full — temporarily clears the equip slot,
 *     equips the new item, and sells the old one. Restores on failure.
 *   SCR_EQUIPMENT '1'-'4': bulk sell all items below a rarity threshold.
 *     Iterates in reverse to avoid index shifting bugs during removal.
 */
void ui_handle_key(GameState *gs, int ch) {
    if (ch == ERR) return;

    if (ch == 'p' || ch == 'P') {
        if (gs->inDungeon && gs->heroCreated && gs->screen == SCR_MAIN) {
            gs->paused = !gs->paused;
            return;
        }
    }

    switch (gs->screen) {
    case SCR_SAVE_SELECT: {
        int maxIdx = NUM_SAVE_SLOTS;
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = maxIdx; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx > maxIdx) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            if (gs->menuIdx < NUM_SAVE_SLOTS && gs->slotInfo[gs->menuIdx].exists) {
                gs->saveSlot = gs->menuIdx;
                load_game(gs);
                ui_log(gs, "Save loaded.", CP_GREEN);
            } else if (gs->menuIdx < NUM_SAVE_SLOTS && !gs->slotInfo[gs->menuIdx].exists) {
                gs->saveSlot = gs->menuIdx;
                gs->screen = SCR_CLASS_SELECT;
                gs->menuIdx = 0;
            } else if (gs->menuIdx == NUM_SAVE_SLOTS) {
                gs->screen = SCR_NEW_SLOT;
                gs->menuIdx = 0;
            }
        }
        break;
    }

    case SCR_NEW_SLOT:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = NUM_SAVE_SLOTS - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= NUM_SAVE_SLOTS) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            if (gs->slotInfo[gs->menuIdx].exists) {
                save_delete_slot(gs->menuIdx);
                save_refresh_slots(gs);
            }
            gs->screen = SCR_SAVE_SELECT;
            gs->menuIdx = NUM_SAVE_SLOTS;
        }
        if (ch == 27) { gs->screen = SCR_SAVE_SELECT; gs->menuIdx = NUM_SAVE_SLOTS; }
        break;

    case SCR_CLASS_SELECT:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = NUM_CLASSES - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= NUM_CLASSES) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            gs->hero = hero_create(gs->menuIdx, "");
            gs->screen = SCR_NAME_INPUT;
            gs->nameBuf[0] = '\0';
            gs->nameLen = 0;
        }
        break;

    case SCR_NAME_INPUT:
        if (ch == '\n' || ch == KEY_ENTER) {
            if (gs->nameLen == 0) strncpy(gs->nameBuf, "Aldric", MAX_NAME - 1);
            strncpy(gs->hero.name, gs->nameBuf, MAX_NAME - 1);
            gs->heroCreated = 1;
            gs->screen = SCR_MAIN;
            gs->menuIdx = 0;
            ui_log(gs, "Welcome, adventurer.", CP_YELLOW);
            save_game(gs);
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            if (gs->nameLen > 0) gs->nameBuf[--gs->nameLen] = '\0';
        } else if (ch >= 32 && ch < 127 && gs->nameLen < MAX_NAME - 2) {
            gs->nameBuf[gs->nameLen++] = (char)ch;
            gs->nameBuf[gs->nameLen] = '\0';
        }
        break;

    case SCR_MAIN:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = MAIN_MENU_N - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= MAIN_MENU_N) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            switch (gs->menuIdx) {
            case 0: gs->screen = SCR_DUNGEON;      gs->menuIdx = 0; break;
            case 1: gs->screen = SCR_CHARACTER;     gs->menuIdx = 0; break;
            case 2: gs->screen = SCR_SKILLS;        gs->menuIdx = 0; break;
            case 3: gs->screen = SCR_EQUIPMENT;     gs->menuIdx = 0; break;
            case 4: gs->screen = SCR_SHOP;          gs->menuIdx = 0; gs->shopSlot = 0; break;
            case 5: gs->screen = SCR_ENCYCLOPEDIA;  gs->menuIdx = 0; break;
            case 6: gs->screen = SCR_CONFIRM_QUIT;  gs->menuIdx = 0; break;
            }
        }
        if (ch == 'q' || ch == 'Q') { gs->screen = SCR_CONFIRM_QUIT; gs->menuIdx = 0; }
        break;

    case SCR_DUNGEON: {
        int maxIdx = gs->inDungeon ? NUM_DUNGEONS : NUM_DUNGEONS - 1;
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = maxIdx; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx > maxIdx) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_MAIN; gs->menuIdx = 0; }
        if (ch == 'h' || ch == 'H') {
            if (gs->menuIdx < NUM_DUNGEONS && gs->hero.hardMode[gs->menuIdx])
                gs->wantHardMode = !gs->wantHardMode;
        }
        if (ch == '\n' || ch == KEY_ENTER) {
            if (gs->inDungeon && gs->menuIdx == NUM_DUNGEONS) {
                gs->inDungeon = 0;
                gs->hasEnemy = 0;
                gs->paused = 0;
                gs->dungeonKills = 0;
                gs->bossActive = 0;
                gs->bossTimer = 0;
                gs->hardModeActive = 0;
                EStats es2 = hero_effective_stats(&gs->hero);
                gs->hero.hp = es2.maxHp;
                gs->hero.maxHp = es2.maxHp;
                gs->hero.numBuffs = 0;
                ui_log(gs, "You leave the dungeon. HP restored.", CP_GREEN);
                gs->screen = SCR_MAIN; gs->menuIdx = 0;
            } else if (gs->menuIdx < NUM_DUNGEONS) {
                const DungeonDef *dg = data_dungeon(gs->menuIdx);
                if (gs->hero.level >= dg->levelReq) {
                    gs->currentDungeon = gs->menuIdx;
                    gs->inDungeon = 1;
                    gs->paused = 0;
                    gs->deathTimer = 0;
                    gs->dungeonKills = 0;
                    gs->bossActive = 0;
                    gs->bossTimer = 0;
                    gs->hero.numBuffs = 0;
                    gs->combatDmgDealt = 0;
                    gs->combatDmgTaken = 0;
                    gs->combatHealed = 0;
                    gs->combatTicks = 0;
                    gs->hardModeActive = (gs->wantHardMode && gs->hero.hardMode[gs->menuIdx]);
                    if (gs->hardModeActive) {
                        gs->activeAffixes[0] = rand() % NUM_AFFIXES;
                        do { gs->activeAffixes[1] = rand() % NUM_AFFIXES; }
                        while (gs->activeAffixes[1] == gs->activeAffixes[0]);
                        char b[LOG_LINE_W + 1];
                        snprintf(b, sizeof(b), "HARD MODE: %s + %s",
                                 data_affix_name(gs->activeAffixes[0]),
                                 data_affix_name(gs->activeAffixes[1]));
                        ui_log(gs, b, CP_MAGENTA);
                    }
                    EStats es2 = hero_effective_stats(&gs->hero);
                    gs->hero.hp = es2.maxHp;
                    gs->hero.maxHp = es2.maxHp;
                    char b[LOG_LINE_W + 1];
                    snprintf(b, sizeof(b), "Entering %s...", dg->name);
                    ui_log(gs, b, dg->colorPair);
                    combat_spawn(gs);
                    gs->screen = SCR_MAIN; gs->menuIdx = 0;
                }
            }
        }
        break;
    }

    case SCR_EQUIPMENT: {
        int filter = gs->equipFilter > 0 ? gs->equipFilter - 1 : -1;
        int viewIdx[MAX_INVENTORY];
        int viewN = inv_build_view(&gs->hero, filter, gs->equipSort, viewIdx, MAX_INVENTORY);
        int totalItems = NUM_SLOTS + viewN;
        if (totalItems < NUM_SLOTS) totalItems = NUM_SLOTS;

        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = totalItems - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= totalItems) gs->menuIdx = 0; }

        if (ch == KEY_LEFT) {
            gs->equipFilter--;
            if (gs->equipFilter < 0) gs->equipFilter = NUM_SLOTS;
            if (gs->menuIdx >= NUM_SLOTS) gs->menuIdx = NUM_SLOTS;
            filter = gs->equipFilter > 0 ? gs->equipFilter - 1 : -1;
            viewN = inv_build_view(&gs->hero, filter, gs->equipSort, viewIdx, MAX_INVENTORY);
            if (gs->menuIdx >= NUM_SLOTS && viewN == 0)
                gs->menuIdx = NUM_SLOTS - 1;
        }
        if (ch == KEY_RIGHT) {
            gs->equipFilter++;
            if (gs->equipFilter > NUM_SLOTS) gs->equipFilter = 0;
            if (gs->menuIdx >= NUM_SLOTS) gs->menuIdx = NUM_SLOTS;
            filter = gs->equipFilter > 0 ? gs->equipFilter - 1 : -1;
            viewN = inv_build_view(&gs->hero, filter, gs->equipSort, viewIdx, MAX_INVENTORY);
            if (gs->menuIdx >= NUM_SLOTS && viewN == 0)
                gs->menuIdx = NUM_SLOTS - 1;
        }
        if (ch == 's' || ch == 'S') {
            gs->equipSort = (gs->equipSort + 1) % NUM_EQUIP_SORTS;
        }

        if (ch == '\n' || ch == KEY_ENTER) {
            if (gs->menuIdx < NUM_SLOTS) {
                if (gs->hero.hasEquip[gs->menuIdx]) {
                    hero_unequip(&gs->hero, gs->menuIdx);
                    char b[LOG_LINE_W + 1];
                    snprintf(b, sizeof(b), "Unequipped %s.", data_slot_name(gs->menuIdx));
                    ui_log(gs, b, CP_YELLOW);
                }
            } else {
                int bagIdx = gs->menuIdx - NUM_SLOTS;
                if (bagIdx >= 0 && bagIdx < viewN) {
                    int realIdx = viewIdx[bagIdx];
                    ItemDef tmp = gs->hero.inventory[realIdx];
                    if (hero_equip(&gs->hero, &tmp)) {
                        for (int i = realIdx; i < gs->hero.invCount - 1; i++)
                            gs->hero.inventory[i] = gs->hero.inventory[i + 1];
                        gs->hero.invCount--;
                        char b[LOG_LINE_W + 1];
                        snprintf(b, sizeof(b), "Equipped %s.", tmp.name);
                        ui_log(gs, b, CP_GREEN);
                        check_achievements(gs);
                        filter = gs->equipFilter > 0 ? gs->equipFilter - 1 : -1;
                        viewN = inv_build_view(&gs->hero, filter, gs->equipSort, viewIdx, MAX_INVENTORY);
                        int newTotal = NUM_SLOTS + viewN;
                        if (gs->menuIdx >= newTotal && newTotal > NUM_SLOTS)
                            gs->menuIdx = newTotal - 1;
                        else if (viewN == 0)
                            gs->menuIdx = NUM_SLOTS - 1;
                    } else {
                        ui_log(gs, "Can't equip that.", CP_RED);
                    }
                }
            }
        }
        if (ch == 'x' || ch == 'X') {
            int bagIdx = gs->menuIdx - NUM_SLOTS;
            if (bagIdx >= 0 && bagIdx < viewN) {
                int realIdx = viewIdx[bagIdx];
                ItemDef sold = gs->hero.inventory[realIdx];
                int sellPrice = sold.price / 2;
                if (sellPrice < 1) sellPrice = 1;
                gs->hero.gold += sellPrice;
                for (int i = realIdx; i < gs->hero.invCount - 1; i++)
                    gs->hero.inventory[i] = gs->hero.inventory[i + 1];
                gs->hero.invCount--;
                char b[LOG_LINE_W + 1];
                snprintf(b, sizeof(b), "Sold %s for %d gold.", sold.name, sellPrice);
                ui_log(gs, b, CP_YELLOW);
                filter = gs->equipFilter > 0 ? gs->equipFilter - 1 : -1;
                viewN = inv_build_view(&gs->hero, filter, gs->equipSort, viewIdx, MAX_INVENTORY);
                int newTotal = NUM_SLOTS + viewN;
                if (gs->menuIdx >= newTotal && newTotal > NUM_SLOTS)
                    gs->menuIdx = newTotal - 1;
                else if (viewN == 0)
                    gs->menuIdx = NUM_SLOTS - 1;
            } else if (gs->menuIdx < NUM_SLOTS) {
                ui_log(gs, "Unequip first to sell.", CP_RED);
            }
        }
        if (ch == 'z' || ch == 'Z') {
            gs->screen = SCR_BULK_SELL;
            gs->menuIdx = 0;
        }
        if (ch == 27) { gs->screen = SCR_MAIN; gs->menuIdx = 3; }
        break;
    }

    case SCR_SHOP: {
        int n = shop_item_count(gs);
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = n > 0 ? n - 1 : 0; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= n) gs->menuIdx = 0; }
        if (ch == KEY_LEFT ) { gs->shopSlot--; if (gs->shopSlot < 0) gs->shopSlot = NUM_SLOTS - 1; gs->menuIdx = 0; }
        if (ch == KEY_RIGHT) { gs->shopSlot++; if (gs->shopSlot >= NUM_SLOTS) gs->shopSlot = 0; gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            ItemDef it;
            shop_get_item(gs, gs->menuIdx, &it);
            if (it.name[0] != '\0' && gs->hero.gold >= it.price && gs->hero.level >= it.levelReq) {
                if (hero_equip(&gs->hero, &it)) {
                    gs->hero.gold -= it.price;
                    char b[LOG_LINE_W + 1];
                    snprintf(b, sizeof(b), "Bought %s!", it.name);
                    ui_log(gs, b, data_rarity_color(it.rarity));
                } else if (gs->hero.invCount >= MAX_INVENTORY &&
                           gs->hero.hasEquip[it.slot]) {
                    ItemDef old = gs->hero.equipment[it.slot];
                    int sellBack = old.price / 2;
                    if (sellBack < 1) sellBack = 1;
                    gs->hero.hasEquip[it.slot] = 0;
                    if (hero_equip(&gs->hero, &it)) {
                        gs->hero.gold -= it.price;
                        gs->hero.gold += sellBack;
                        char b[LOG_LINE_W + 1];
                        snprintf(b, sizeof(b), "Sold %s (%dg), bought %s!",
                                 old.name, sellBack, it.name);
                        ui_log(gs, b, data_rarity_color(it.rarity));
                    } else {
                        gs->hero.equipment[it.slot] = old;
                        gs->hero.hasEquip[it.slot] = 1;
                        ui_log(gs, "Can't equip that.", CP_RED);
                    }
                } else {
                    ui_log(gs, "Inventory full!", CP_RED);
                }
            }
        }
        if (ch == 27) { gs->screen = SCR_MAIN; gs->menuIdx = 4; }
        break;
    }

    case SCR_CHARACTER:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = NUM_STATS - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= NUM_STATS) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            if (gs->hero.talentPoints <= 0) {
                ui_log(gs, "No talent points available.", CP_RED);
            } else if (hero_alloc_talent(&gs->hero, gs->menuIdx)) {
                char b[LOG_LINE_W + 1];
                snprintf(b, sizeof(b), "+1 %s! (%d pts left)",
                         data_stat_name(gs->menuIdx), gs->hero.talentPoints);
                ui_log(gs, b, CP_GREEN);
                check_achievements(gs);
            }
        }
        if (ch == 't' || ch == 'T') { gs->screen = SCR_TITLES; gs->menuIdx = gs->hero.activeTitle; }
        if (ch == 27) { gs->screen = SCR_MAIN; gs->menuIdx = 1; }
        break;

    case SCR_SKILLS: {
        int total = MAX_SKILL_TIERS * 2;
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = total - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= total) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            int tier = gs->menuIdx / 2;
            int option = gs->menuIdx % 2;
            if (gs->hero.level >= data_skill_level(tier) && gs->hero.skillChoices[tier] < 0) {
                gs->hero.skillChoices[tier] = option;
                const SkillDef *sk = data_skill(gs->hero.classId, tier, option);
                if (sk) {
                    char b[LOG_LINE_W + 1];
                    snprintf(b, sizeof(b), "Learned %s!", sk->name);
                    ui_log(gs, b, CP_GREEN);
                }
            }
        }
        if (ch == 'r' || ch == 'R') {
            int hasAny = 0;
            for (int t = 0; t < MAX_SKILL_TIERS; t++)
                if (gs->hero.skillChoices[t] >= 0) { hasAny = 1; break; }
            int resetCost = gs->hero.level * gs->hero.level * 2;
            if (hasAny && gs->hero.gold >= resetCost) {
                gs->hero.gold -= resetCost;
                for (int t = 0; t < MAX_SKILL_TIERS; t++)
                    gs->hero.skillChoices[t] = -1;
                memset(gs->hero.skillCooldowns, 0, sizeof(gs->hero.skillCooldowns));
                char rbuf[LOG_LINE_W + 1];
                snprintf(rbuf, sizeof(rbuf), "Skills reset! (-%dG)", resetCost);
                ui_log(gs, rbuf, CP_YELLOW);
            }
        }
        if (ch == 27) { gs->screen = SCR_MAIN; gs->menuIdx = 2; }
        break;
    }

    case SCR_CONFIRM_QUIT:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx > 1) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            if (gs->menuIdx == 1) {
                save_game(gs);
                gs->running = 0;
            } else {
                gs->screen = SCR_MAIN;
                gs->menuIdx = 0;
            }
        }
        if (ch == 27) { gs->screen = SCR_MAIN; gs->menuIdx = 0; }
        break;

    case SCR_ENCYCLOPEDIA:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = 8; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx > 8) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            switch (gs->menuIdx) {
            case 0: gs->screen = SCR_ENCY_CLASSES;  gs->menuIdx = 0; break;
            case 1: gs->screen = SCR_ENCY_SKILLS;   gs->menuIdx = 0; break;
            case 2: gs->screen = SCR_ENCY_STATS;    gs->menuIdx = 0; break;
            case 3: gs->screen = SCR_ENCY_ITEMS;    gs->menuIdx = 0; gs->encycSlot = 0; break;
            case 4: gs->screen = SCR_ENCY_DUNGEONS; gs->menuIdx = 0; break;
            case 5: gs->screen = SCR_ENCY_ENEMIES;  gs->menuIdx = 0; break;
            case 6: gs->screen = SCR_ENCY_BOSSES;   gs->menuIdx = 0; break;
            case 7: gs->screen = SCR_ENCY_COMBAT;   gs->menuIdx = 0; break;
            case 8: gs->screen = SCR_ACHIEVEMENTS;  gs->menuIdx = 0; break;
            }
        }
        if (ch == 27) { gs->screen = SCR_MAIN; gs->menuIdx = 5; }
        break;

    case SCR_ENCY_CLASSES:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = NUM_CLASSES - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= NUM_CLASSES) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 0; }
        break;

    case SCR_ENCY_STATS:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = NUM_STATS - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= NUM_STATS) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 2; }
        break;

    case SCR_ENCY_ITEMS: {
        int items[200];
        int cm = (1 << gs->hero.classId);
        int nItems = ency_items_for_slot(gs->encycSlot, cm, items, 200);
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = nItems > 0 ? nItems - 1 : 0; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= nItems) gs->menuIdx = 0; }
        if (ch == KEY_LEFT ) { gs->encycSlot--; if (gs->encycSlot < 0) gs->encycSlot = NUM_SLOTS - 1; gs->menuIdx = 0; }
        if (ch == KEY_RIGHT) { gs->encycSlot++; if (gs->encycSlot >= NUM_SLOTS) gs->encycSlot = 0; gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 3; }
        break;
    }

    case SCR_ENCY_DUNGEONS:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = NUM_DUNGEONS - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= NUM_DUNGEONS) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 4; }
        break;

    case SCR_ENCY_ENEMIES: {
        int enemies[50];
        int nEnemies = ency_normal_enemies(enemies, 50);
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = nEnemies > 0 ? nEnemies - 1 : 0; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= nEnemies) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 5; }
        break;
    }

    case SCR_ENCY_BOSSES: {
        int bosses[10];
        int nBosses = ency_bosses(bosses, 10);
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = nBosses > 0 ? nBosses - 1 : 0; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= nBosses) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 6; }
        break;
    }

    case SCR_ENCY_SKILLS: {
        int total = MAX_SKILL_TIERS * 2;
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = total - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= total) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 1; }
        break;
    }

    case SCR_ENCY_COMBAT: {
        int nTopics = 14;
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = nTopics - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= nTopics) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 7; }
        break;
    }

    case SCR_ACHIEVEMENTS:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = NUM_ACHIEVEMENTS - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= NUM_ACHIEVEMENTS) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 8; }
        break;

    case SCR_TITLES: {
        int maxTitle = NUM_TITLES;
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = maxTitle; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx > maxTitle) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            if (gs->menuIdx == 0) {
                gs->hero.activeTitle = 0;
                ui_log(gs, "Title removed.", CP_YELLOW);
            } else {
                int tIdx = gs->menuIdx - 1;
                const TitleDef *td = data_title(tIdx);
                if (td && td->achievementIdx >= 0 && ACH_HAS(&gs->hero, td->achievementIdx)) {
                    gs->hero.activeTitle = gs->menuIdx;
                    char b[LOG_LINE_W + 1];
                    snprintf(b, sizeof(b), "Title set: %s", td->name);
                    ui_log(gs, b, CP_CYAN);
                }
            }
        }
        if (ch == 27) { gs->screen = SCR_CHARACTER; gs->menuIdx = 0; }
        break;
    }

    case SCR_BULK_SELL: {
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = 3; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx > 3) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            int threshold = gs->menuIdx + 1;
            int totalGold = 0, count = 0;
            for (int i = gs->hero.invCount - 1; i >= 0; i--) {
                if (gs->hero.inventory[i].rarity < threshold) {
                    int sp = gs->hero.inventory[i].price / 2;
                    if (sp < 1) sp = 1;
                    totalGold += sp;
                    count++;
                    for (int j = i; j < gs->hero.invCount - 1; j++)
                        gs->hero.inventory[j] = gs->hero.inventory[j + 1];
                    gs->hero.invCount--;
                }
            }
            if (count > 0) {
                gs->hero.gold += totalGold;
                char b[LOG_LINE_W + 1];
                snprintf(b, sizeof(b), "Sold %d item%s for %dg.",
                         count, count > 1 ? "s" : "", totalGold);
                ui_log(gs, b, CP_YELLOW);
            }
            gs->screen = SCR_EQUIPMENT;
            gs->menuIdx = 0;
        }
        if (ch == 27) { gs->screen = SCR_EQUIPMENT; gs->menuIdx = 0; }
        break;
    }
    }
}
