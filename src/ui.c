/*
 * ui.c — All ncurses rendering and input handling.
 *
 * Layout: 80×24 terminal. Left panel (28 cols) for menus, right panel
 * split into enemy display (top 8 rows) and combat log (bottom).
 * The right "enemy" panel doubles as a context-sensitive detail view:
 * item stats in equipment/shop, encyclopedia details, etc.
 * render_enemy_panel() dispatches based on gs->screen.
 *
 * The talent tree screen (SCR_TALENTS) uses a temporary full-screen
 * WINDOW that bypasses the normal left/right panel layout.
 */
#include "game.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

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
    init_pair(CP_SEL_GREEN,   COLOR_BLACK, COLOR_GREEN);
    init_pair(CP_SEL_BLUE,    COLOR_BLACK, COLOR_BLUE);
    init_pair(CP_SEL_MAGENTA, COLOR_BLACK, COLOR_MAGENTA);
    init_pair(CP_SEL_YELLOW,  COLOR_BLACK, COLOR_YELLOW);
    init_pair(CP_SEL_CYAN,    COLOR_BLACK, COLOR_CYAN);
    init_pair(CP_SEL_RED,     COLOR_BLACK, COLOR_RED);

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
        int dCol = SCREEN_W - (int)strlen(dname) - 3;
        if (dCol < 35) dCol = 35;
        mvwprintw(w, 0, dCol, " %s ", dname);
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
            mvwprintw(w, row, 1, "%s%d. %s",
                      sel ? " > " : "   ", i + 1, info->name);
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
            mvwprintw(w, row, 1, "%s%d. %s %-11.11s Lv.%d",
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

static const char *MAIN_MENU[] = { "DUNGEON", "CHARACTER", "TALENTS", "EQUIPMENT", "SHOP", "ENCYCLOPEDIA", "QUIT" };
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

    const ClassDef *cd = data_class(h->classId);
    int resColor = (h->classId == CLASS_WARRIOR) ? CP_RED :
                   (h->classId == CLASS_ROGUE)   ? CP_YELLOW : CP_BLUE;
    wattron(w, COLOR_PAIR(resColor));
    mvwprintw(w, 3, 2, "%-3.3s", cd ? cd->resourceName : "Res");
    wattroff(w, COLOR_PAIR(resColor));
    draw_bar(w, 3, 5, 10, h->resource, h->maxResource, resColor, CP_DEFAULT);

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

static int sel_color(int cp) {
    switch (cp) {
    case CP_GREEN:   return CP_SEL_GREEN;
    case CP_BLUE:    return CP_SEL_BLUE;
    case CP_MAGENTA: return CP_SEL_MAGENTA;
    case CP_YELLOW:  return CP_SEL_YELLOW;
    case CP_CYAN:    return CP_SEL_CYAN;
    case CP_RED:     return CP_SEL_RED;
    default:         return CP_SELECTED;
    }
}

static int rarity_sel_color(int rarity) {
    return sel_color(data_rarity_color(rarity));
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
        if (gs->hardModeActive) {
            wattron(w, COLOR_PAIR(CP_MAGENTA));
            mvwprintw(w, 2, 16, "[HARD]");
            wattroff(w, COLOR_PAIR(CP_MAGENTA));
        }
    }

    int row = 4;
    for (int i = 0; i < NUM_DUNGEONS; i++) {
        const DungeonDef *dg = data_dungeon(i);
        int unlocked = gs->hero.level >= dg->levelReq;
        int cur = (gs->inDungeon && gs->currentDungeon == i);
        int hm = gs->hero.hardMode[i];

        if (i == gs->menuIdx) {
            int sc = unlocked ? sel_color(dg->colorPair) : CP_SELECTED;
            wattron(w, COLOR_PAIR(sc));
            mvwprintw(w, row, 1, " %c %-23.23s", cur ? '*' : '>', dg->name);
            wattroff(w, COLOR_PAIR(sc));
        } else if (!unlocked) {
            wattron(w, COLOR_PAIR(CP_RED));
            mvwprintw(w, row, 1, "   Lv.%d ???", dg->levelReq);
            wattroff(w, COLOR_PAIR(CP_RED));
        } else {
            wattron(w, COLOR_PAIR(dg->colorPair));
            mvwprintw(w, row, 1, " %c %-23.23s", cur ? '*' : ' ', dg->name);
            wattroff(w, COLOR_PAIR(dg->colorPair));
        }
        if (hm >= 2 && unlocked) {
            wattron(w, COLOR_PAIR(CP_MAGENTA) | A_BOLD);
            mvwprintw(w, row, LEFT_W - 4, "H");
            wattroff(w, COLOR_PAIR(CP_MAGENTA) | A_BOLD);
        } else if (hm >= 1 && unlocked) {
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, row, LEFT_W - 4, "H");
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
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
    mvwprintw(w, PANEL_H - 3, 2, "[H] Toggle Hard Mode");
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
            int sc = rarity_sel_color(gs->hero.equipment[s].rarity);
            if (sel) wattron(w, COLOR_PAIR(sc));
            else     wattron(w, COLOR_PAIR(rc));
            mvwprintw(w, row, 1, "%s%-4s%-18.18s",
                      sel ? " > " : "   ", SLOT_ABBR[s], gs->hero.equipment[s].name);
            wattroff(w, COLOR_PAIR(sel ? sc : rc));
        } else {
            if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
            else     wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, row, 1, "%s%-4s--",
                      sel ? " > " : "   ", SLOT_ABBR[s]);
            wattroff(w, COLOR_PAIR(sel ? CP_SELECTED : CP_DEFAULT));
        }
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
        int sc = rarity_sel_color(gs->hero.inventory[invI].rarity);

        if (sel) wattron(w, COLOR_PAIR(sc));
        else     wattron(w, COLOR_PAIR(rc));

        if (gs->equipFilter > 0)
            mvwprintw(w, row, 1, "%s%-23.23s",
                      sel ? " > " : "   ", gs->hero.inventory[invI].name);
        else
            mvwprintw(w, row, 1, "%s%c %-21.21s",
                      sel ? " > " : "   ", SLOT_CHAR[gs->hero.inventory[invI].slot],
                      gs->hero.inventory[invI].name);

        wattroff(w, COLOR_PAIR(sel ? sc : rc));
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
        mvwprintw(w, PANEL_H - 4, 2, "[Enter] Unequip");
    else if (!inSlots && bagSel >= 0 && bagSel < viewN)
        mvwprintw(w, PANEL_H - 4, 2, "[Enter] Equip");
    mvwprintw(w, PANEL_H - 3, 2, "[X]Sell [Z]Bulk [S]ort");
    mvwprintw(w, PANEL_H - 2, 2, "[L/R]Filter [Esc]Back");
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

        int sc = rarity_sel_color(it.rarity);
        if (sel) wattron(w, COLOR_PAIR(canBuy ? sc : CP_SELECTED));
        else     wattron(w, COLOR_PAIR(canBuy ? rc : CP_RED));

        mvwprintw(w, row, 1, "%s%-16.16s %4dg",
                  sel ? ">" : " ", it.name, it.price);

        if (sel) wattroff(w, COLOR_PAIR(canBuy ? sc : CP_SELECTED));
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
    mvwprintw(w, row, 2, "XP  %d / %d", h->xp, hero_xp_needed(h->level));
    row++;

    if (h->talentPoints > 0) {
        wattron(w, COLOR_PAIR(CP_GREEN) | A_BOLD);
        mvwprintw(w, row, 2, "Talent Points: %d", h->talentPoints);
        wattroff(w, COLOR_PAIR(CP_GREEN) | A_BOLD);
    }
    row++;

    int eqBonus[NUM_STATS] = {0};
    for (int s = 0; s < NUM_SLOTS; s++)
        if (h->hasEquip[s])
            for (int j = 0; j < NUM_STATS; j++)
                eqBonus[j] += h->equipment[s].stats[j];

    int talPct[NUM_STATS] = {0};
    for (int t = 0; t < NUM_TALENT_TREES; t++) {
        const TalentTreeDef *td = data_talent_tree(h->classId, t);
        if (!td) continue;
        for (int n = 0; n < td->nodeCount; n++) {
            int ranks = h->talentRanks[t][n];
            if (ranks == 0) continue;
            for (int b = 0; b < MAX_TALENT_BONUSES; b++) {
                int val = td->nodes[n].bonus[b].perRank * ranks;
                switch (td->nodes[n].bonus[b].type) {
                case TB_PCT_STR: talPct[STR] += val; break;
                case TB_PCT_AGI: talPct[AGI] += val; break;
                case TB_PCT_INT: talPct[INT_]+= val; break;
                case TB_PCT_WIS: talPct[WIS] += val; break;
                case TB_PCT_VIT: talPct[VIT] += val; break;
                case TB_PCT_DEF: talPct[DEF] += val; break;
                default: break;
                }
            }
        }
    }

    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, row, 9, "%3s", "Tot");
    mvwprintw(w, row, 13, "%4s", "%T");
    mvwprintw(w, row, 19, "%4s", "Eq");
    wattroff(w, COLOR_PAIR(CP_DEFAULT));
    row++;

    for (int i = 0; i < NUM_STATS; i++) {
        int pct = talPct[i];
        int eq = eqBonus[i];

        wattron(w, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(w, row, 1, "   %-4s %3d", data_stat_short(i), es.stats[i]);
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
        if (pct > 0) {
            char pBuf[8];
            snprintf(pBuf, sizeof(pBuf), "+%d%%", pct);
            wattron(w, COLOR_PAIR(CP_GREEN));
            mvwprintw(w, row, 13, "%5s", pBuf);
            wattroff(w, COLOR_PAIR(CP_GREEN));
        }
        if (eq > 0) {
            wattron(w, COLOR_PAIR(CP_CYAN));
            mvwprintw(w, row, 19, "%+5d", eq);
            wattroff(w, COLOR_PAIR(CP_CYAN));
        }
        row++;
    }

    row++;
    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, row++, 2, "DMG %-4d  Crit %.1f%%", es.damage, es.critChance * 100);
    mvwprintw(w, row++, 2, "CritD %d%% DR %.0f%%", (int)(es.critMult * 100), es.dmgReduction * 100);
    mvwprintw(w, row++, 2, "Dodge %.1f%% Atk %.1f/s", es.dodgeChance * 100, 1000.0f / es.tickRate);
    if (es.blockChance > 0)
        mvwprintw(w, row++, 2, "Block %.1f%%", es.blockChance * 100);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    row++;
    wattron(w, COLOR_PAIR(CP_GREEN));
    mvwprintw(w, row++, 2, "K:%d D:%d G:%d", h->totalKills, h->deaths, h->totalGoldEarned);
    wattroff(w, COLOR_PAIR(CP_GREEN));

    wattron(w, COLOR_PAIR(CP_CYAN));
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
static int cmp_enemy_name(const void *a, const void *b) {
    return strcmp(data_enemy(*(const int *)a)->name,
                  data_enemy(*(const int *)b)->name);
}

static int ency_normal_enemies(int *out, int max) {
    int n = 0;
    for (int d = 0; d < NUM_DUNGEONS && n < max; d++) {
        const DungeonDef *dg = data_dungeon(d);
        for (int e = 0; e < dg->numEnemies && n < max; e++)
            out[n++] = dg->enemyIdx[e];
    }
    qsort(out, (size_t)n, sizeof(int), cmp_enemy_name);
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

    const char *categories[] = { "Classes", "Stats", "Skills", "Dungeons", "Enemies", "Bosses", "Items", "Combat", "Achievements" };
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
        "Learn what each stat does.",
        "View all skill choices for your class.",
        "Explore dungeons and their inhabitants.",
        "Study the creatures in each dungeon.",
        "Face the guardians that rule each dungeon.",
        "View equipment organized by slot.",
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
    const char **art = data_class_art(gs->menuIdx);

    if (art) {
        wattron(w, COLOR_PAIR(CP_WHITE));
        for (int i = 0; i < ART_LINES; i++) {
            int xoff = (RIGHT_W - ART_COLS) / 2 - 2;
            if (xoff < 1) xoff = 1;
            mvwprintw(w, 1 + i, xoff, "%s", art[i]);
        }
        wattroff(w, COLOR_PAIR(CP_WHITE));
    }

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 6, 2, "%s", cd->name);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    wattron(w, COLOR_PAIR(CP_WHITE));
    const char *primary = data_stat_short(cd->primaryStat);
    mvwprintw(w, 6, 12, "%s:%d HP:%d+%d/Lv",
              primary, cd->baseStats[cd->primaryStat], cd->baseHp, cd->hpPerLevel);
    wattroff(w, COLOR_PAIR(CP_WHITE));

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

        int sc = rarity_sel_color(it->rarity);
        if (sel) wattron(w, COLOR_PAIR(sc));
        else     wattron(w, COLOR_PAIR(rc));

        mvwprintw(w, row, 1, "%s%-21.21s", sel ? " > " : "   ", it->name);

        wattroff(w, COLOR_PAIR(sel ? sc : rc));
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
    for (int i = 0; i < ART_LINES; i++)
        mvwprintw(w, 1 + i, 2, "%s", et->art[i]);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    int tc = ART_COLS + 4;
    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, tc, "%s", et->name);
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, 2, tc, "HP %d  ATK %d", et->hp, et->attack);
    mvwprintw(w, 3, tc, "DEF %d  SPD %d", et->defense, et->speed);
    mvwprintw(w, 4, tc, "XP %d  Gold %d", et->xpReward, et->goldReward);
    mvwprintw(w, 5, tc, "%s", ency_enemy_dungeon(enemies[gs->menuIdx]));
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

        if (sel) wattron(w, COLOR_PAIR(CP_SEL_MAGENTA));
        else     wattron(w, COLOR_PAIR(CP_MAGENTA));

        mvwprintw(w, 3 + i, 1, "%s%-21.21s", sel ? " > " : "   ", et->name);

        wattroff(w, COLOR_PAIR(sel ? CP_SEL_MAGENTA : CP_MAGENTA));
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
    for (int i = 0; i < ART_LINES; i++)
        mvwprintw(w, 1 + i, 2, "%s", et->art[i]);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    int tc = ART_COLS + 4;
    wattron(w, COLOR_PAIR(CP_MAGENTA) | A_BOLD);
    mvwprintw(w, 1, tc, "%s", et->name);
    wattroff(w, COLOR_PAIR(CP_MAGENTA) | A_BOLD);
    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, 2, tc, "HP %d  ATK %d", et->hp, et->attack);
    mvwprintw(w, 3, tc, "DEF %d  SPD %d", et->defense, et->speed);
    mvwprintw(w, 4, tc, "XP %d  Gold %d", et->xpReward, et->goldReward);
    mvwprintw(w, 5, tc, "%s", ency_enemy_dungeon(bosses[gs->menuIdx]));
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

        int sc = sel_color(dg->colorPair);
        if (sel) wattron(w, COLOR_PAIR(sc));
        else     wattron(w, COLOR_PAIR(dg->colorPair));

        mvwprintw(w, 3 + i, 1, "%sLv%-2d %-18.18s",
                  sel ? " > " : "   ", dg->levelReq, dg->name);

        wattroff(w, COLOR_PAIR(sel ? sc : dg->colorPair));
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

    /* Comma-separated enemy list with line wrapping */
    int row = 3, col = 2;
    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, row, col, "Enemies: ");
    col += 9;
    wattroff(w, COLOR_PAIR(CP_WHITE));

    wattron(w, COLOR_PAIR(CP_DEFAULT));
    for (int e = 0; e < dg->numEnemies; e++) {
        const EnemyTemplate *et = data_enemy(dg->enemyIdx[e]);
        if (!et) continue;
        const char *sep = (e < dg->numEnemies - 1) ? ", " : "";
        int len = (int)strlen(et->name) + (int)strlen(sep);
        if (col + len > RIGHT_W - 2) { row++; col = 4; }
        if (row >= ENEMY_DISPLAY_H - 2) break;
        mvwprintw(w, row, col, "%s%s", et->name, sep);
        col += len;
    }
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    const EnemyTemplate *boss = data_enemy(dg->bossIdx);
    if (boss) {
        wattron(w, COLOR_PAIR(CP_MAGENTA));
        mvwprintw(w, row + 1, 2, "Boss: %s", boss->name);
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
        "Tick System", "Hero Damage", "Critical Hits",
        "Enemy Armor", "Damage Reduction",
        "Dodge & Evasion", "Block (Warrior)", "Shields & Ward",
        "Buffs & Effects", "Stun & Slow",
        "Resource & Regen", "Death & Revive", "Boss Encounters",
        "Loot System", "Hard Mode", "Offline Progress"
    };
    int nTopics = (int)(sizeof(topics) / sizeof(topics[0]));

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
        "Tick System", "Hero Damage", "Critical Hits",
        "Enemy Armor", "Damage Reduction",
        "Dodge & Evasion", "Block (Warrior)", "Shields & Ward",
        "Buffs & Effects", "Stun & Slow",
        "Resource & Regen", "Death & Revive", "Boss Encounters",
        "Loot System", "Hard Mode", "Offline Progress"
    };
    int nTopics = (int)(sizeof(topics) / sizeof(topics[0]));

    if (gs->menuIdx < 0 || gs->menuIdx >= nTopics) {
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
    case 1: /* Hero Damage */
        mvwprintw(w, row++, 2, "War STR*1.5  Rog AGI*1.4+STR*0.4");
        mvwprintw(w, row++, 2, "Mage INT*1.5+WIS*0.5  Pri WIS*1.2");
        mvwprintw(w, row++, 2, "Rogue gets 1.8x crit multiplier.");
        mvwprintw(w, row++, 2, "+/-10%% random variance on every hit.");
        break;
    case 2: /* Critical Hits */
        mvwprintw(w, row++, 2, "Crit chance: AGI / 300 (max 50%%)");
        mvwprintw(w, row++, 2, "Crit multiplier: 1.5x (Rogue: 1.8x)");
        mvwprintw(w, row++, 2, "Buff critDmgBonus stacks additively.");
        mvwprintw(w, row++, 2, "Crit check happens after dmg variance.");
        break;
    case 3: /* Enemy Armor */
        mvwprintw(w, row++, 2, "Enemy damage reduction:");
        mvwprintw(w, row++, 2, "  armor = DEF^2 / (DEF + 100)");
        mvwprintw(w, row++, 2, "Your hit = baseDmg - armor (min 1)");
        mvwprintw(w, row++, 2, "Some skills ignore armor entirely.");
        break;
    case 4: /* Damage Reduction */
        mvwprintw(w, row++, 2, "%% DR: DEF / (DEF + 100) (max 50%%)");
        mvwprintw(w, row++, 2, "Flat DR: WIS * 0.15");
        mvwprintw(w, row++, 2, "Applied before block, after variance.");
        mvwprintw(w, row++, 2, "Both stack (%% first, flat second).");
        break;
    case 5: /* Dodge & Evasion */
        mvwprintw(w, row++, 2, "Dodge chance: AGI / 300 (max 35%%)");
        mvwprintw(w, row++, 2, "Buff dodgeBonus stacks additively.");
        mvwprintw(w, row++, 2, "dodgeNext buff: guaranteed next dodge.");
        mvwprintw(w, row++, 2, "Dodged attacks deal zero damage.");
        break;
    case 6: /* Block (Warrior) */
        mvwprintw(w, row++, 2, "Warrior only. DEF / 250 (max 30%%)");
        mvwprintw(w, row++, 2, "Blocks reduce damage by 30%%.");
        mvwprintw(w, row++, 2, "Block check is after dodge check.");
        mvwprintw(w, row++, 2, "Other classes cannot block.");
        break;
    case 7: /* Shields & Ward */
        mvwprintw(w, row++, 2, "buffShieldPct: absorb shield (HP%%).");
        mvwprintw(w, row++, 2, "Shields drain before HP damage.");
        mvwprintw(w, row++, 2, "Multiple shields stack, drain in order.");
        mvwprintw(w, row++, 2, "Mana Shield: damage costs mana instead.");
        break;
    case 8: /* Buffs & Effects */
        mvwprintw(w, row++, 2, "Skills create timed buffs (ticksLeft).");
        mvwprintw(w, row++, 2, "damageMul, HoT, DoT, dodge, crit...");
        mvwprintw(w, row++, 2, "Buffs tick each combat step.");
        mvwprintw(w, row++, 2, "Max 10 active buffs at once.");
        break;
    case 9: /* Stun & Slow */
        mvwprintw(w, row++, 2, "Stunned enemies skip their attack.");
        mvwprintw(w, row++, 2, "Slowed enemies also skip their turn.");
        mvwprintw(w, row++, 2, "Duration in ticks, decrements each tick.");
        mvwprintw(w, row++, 2, "Hero still attacks stunned enemies.");
        break;
    case 10: /* Resource & Regen */
        mvwprintw(w, row++, 2, "Each class has a resource (Rage, etc).");
        mvwprintw(w, row++, 2, "Regen per tick: class resourceRegen.");
        mvwprintw(w, row++, 2, "VIT heals VIT*2 HP on each kill.");
        mvwprintw(w, row++, 2, "Level-up fully restores HP and resource.");
        break;
    case 11: /* Death & Revive */
        mvwprintw(w, row++, 2, "On death: lose 10%% gold, clear buffs.");
        mvwprintw(w, row++, 2, "Revive after 4 ticks at full HP/mana.");
        mvwprintw(w, row++, 2, "Dungeon kill count resets to 0.");
        mvwprintw(w, row++, 2, "Boss progress is lost on death.");
        break;
    case 12: /* Boss Encounters */
        mvwprintw(w, row++, 2, "Boss spawns after 50 normal kills.");
        mvwprintw(w, row++, 2, "6 tick delay after boss kill.");
        mvwprintw(w, row++, 2, "Kill count resets after boss phase.");
        mvwprintw(w, row++, 2, "Bosses are always the dungeon guardian.");
        break;
    case 13: /* Loot System */
        mvwprintw(w, row++, 2, "Mob drops: randomized level per dungeon.");
        mvwprintw(w, row++, 2, "D4-D8 mobs: 2%% Epic from static table.");
        mvwprintw(w, row++, 2, "Boss: tiered weights (higher = more Epic).");
        mvwprintw(w, row++, 2, "Legendary: D8 boss only (10%% chance).");
        break;
    case 14: /* Hard Mode */
        mvwprintw(w, row++, 2, "[H] in dungeon select. Beat boss first.");
        mvwprintw(w, row++, 2, "2 random affixes. +50%% gold & XP.");
        mvwprintw(w, row++, 2, "'H' on unlocked dungeons = available.");
        wattron(w, COLOR_PAIR(CP_CYAN));
        mvwprintw(w, row++, 2, "[Enter] View affix details");
        wattroff(w, COLOR_PAIR(CP_CYAN));
        wattron(w, COLOR_PAIR(CP_WHITE));
        break;
    case 15: /* Offline Progress */
        mvwprintw(w, row++, 2, "Quit while in a dungeon to earn");
        mvwprintw(w, row++, 2, "XP & gold while offline (~half rate).");
        mvwprintw(w, row++, 2, "Rewards based on dungeon enemies.");
        mvwprintw(w, row++, 2, "Capped at 8 hours of offline time.");
        break;
    }
    wattroff(w, COLOR_PAIR(CP_WHITE));

    wnoutrefresh(w);
}

static void render_ency_affixes(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(w, 1, 2, "Hard Mode Affixes");
    wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

    for (int i = 0; i < NUM_AFFIXES; i++) {
        if (i == gs->menuIdx) {
            wattron(w, COLOR_PAIR(CP_SEL_MAGENTA));
            mvwprintw(w, 3 + i, 1, " > %-23s", data_affix_name(i));
            wattroff(w, COLOR_PAIR(CP_SEL_MAGENTA));
        } else {
            wattron(w, COLOR_PAIR(CP_MAGENTA));
            mvwprintw(w, 3 + i, 1, "   %-23s", data_affix_name(i));
            wattroff(w, COLOR_PAIR(CP_MAGENTA));
        }
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_ency_affixes_detail(GameState *gs) {
    WINDOW *w = gs->wEnemy;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    if (gs->menuIdx < 0 || gs->menuIdx >= NUM_AFFIXES) {
        wnoutrefresh(w);
        return;
    }

    wattron(w, COLOR_PAIR(CP_MAGENTA) | A_BOLD);
    mvwprintw(w, 1, 2, "%s", data_affix_name(gs->menuIdx));
    wattroff(w, COLOR_PAIR(CP_MAGENTA) | A_BOLD);

    wattron(w, COLOR_PAIR(CP_WHITE));
    int row = 3;
    switch (gs->menuIdx) {
    case 0: /* Fortified */
        mvwprintw(w, row++, 2, "Enemies gain +30%% max HP.");
        mvwprintw(w, row++, 2, "Makes fights longer and more");
        mvwprintw(w, row++, 2, "resource-intensive.");
        break;
    case 1: /* Raging */
        mvwprintw(w, row++, 2, "Enemies deal +20%% damage.");
        mvwprintw(w, row++, 2, "Increases pressure on healing");
        mvwprintw(w, row++, 2, "and defensive cooldowns.");
        break;
    case 2: /* Cursed */
        mvwprintw(w, row++, 2, "All healing and regen reduced");
        mvwprintw(w, row++, 2, "by 20%%. Affects HP regen,");
        mvwprintw(w, row++, 2, "HoT, and resource regen.");
        break;
    case 3: /* Thorny */
        mvwprintw(w, row++, 2, "Enemies reflect 10%% of the");
        mvwprintw(w, row++, 2, "damage they deal back to");
        mvwprintw(w, row++, 2, "themselves as thorn damage.");
        break;
    case 4: /* Frenzied */
        mvwprintw(w, row++, 2, "Enemies are immune to stun.");
        mvwprintw(w, row++, 2, "Stun effects still apply but");
        mvwprintw(w, row++, 2, "enemies will not skip turns.");
        break;
    case 5: /* Draining */
        mvwprintw(w, row++, 2, "Hero loses 1%% max HP each");
        mvwprintw(w, row++, 2, "combat tick. Constant pressure");
        mvwprintw(w, row++, 2, "that bypasses all defenses.");
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
    int infoCol = 2 + (int)strlen(it->name) + 1;
    if (infoCol < RIGHT_W - 22)
        mvwprintw(w, 1, infoCol, "(%s %s)",
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
        wattron(w, COLOR_PAIR(CP_DEFAULT));
        wprintw(w, " (%d)", cmp->levelReq);
        wattroff(w, COLOR_PAIR(CP_DEFAULT));
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

        if (cmp) {
            int dc = diff > 0 ? CP_GREEN : (diff < 0 ? CP_RED : CP_DEFAULT);
            wattron(w, COLOR_PAIR(dc));
            wprintw(w, "(%+d)", diff);
            wattroff(w, COLOR_PAIR(dc));
        }

        col += 16;
        if (col + 14 > RIGHT_W - 2) { col = 2; row++; }
        if (row >= ENEMY_DISPLAY_H - 1) break;
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

#define BULK_MENU_N 6

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

    int raw = gs->hero.autoSellThreshold;
    int asOn = raw > 0;
    int asTh = raw > 0 ? raw : (raw < 0 ? -raw : 1);
    if (asTh < 1) asTh = 1;
    if (asTh > 4) asTh = 4;

    const char *sellLabels[] = { "Sell below Uncommon", "Sell below Rare",
                                  "Sell below Epic", "Sell below Legendary" };
    const char *thLabels[] = { "< Uncommon", "< Rare", "< Epic", "< Legendary" };

    int row = 4;
    for (int i = 0; i < BULK_MENU_N; i++) {
        int sel = (i == gs->menuIdx);

        if (i < 4) {
            int count, gold;
            bulk_sell_preview(&gs->hero, i + 1, &count, &gold);
            if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
            else if (count > 0) wattron(w, COLOR_PAIR(CP_WHITE));
            else wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, row, 1, "%s%-23.23s", sel ? " > " : "   ", sellLabels[i]);
            wattroff(w, COLOR_PAIR(CP_SELECTED));
            wattroff(w, COLOR_PAIR(CP_WHITE));
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
            if (count > 0) {
                wattron(w, COLOR_PAIR(CP_YELLOW));
                mvwprintw(w, row + 1, 4, "%d item%s for %dg",
                          count, count > 1 ? "s" : "", gold);
                wattroff(w, COLOR_PAIR(CP_YELLOW));
            } else {
                wattron(w, COLOR_PAIR(CP_DEFAULT));
                mvwprintw(w, row + 1, 4, "(nothing to sell)");
                wattroff(w, COLOR_PAIR(CP_DEFAULT));
            }
        } else if (i == 4) {
            if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
            else if (asOn) wattron(w, COLOR_PAIR(CP_GREEN));
            else wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, row, 1, "%sAuto-sell %s",
                      sel ? " > " : "   ", asOn ? "ON" : "OFF");
            wattroff(w, COLOR_PAIR(CP_SELECTED));
            wattroff(w, COLOR_PAIR(CP_GREEN));
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
            row += 1;
            continue;
        } else {
            if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
            else wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, row, 1, "%sThreshold: %s",
                      sel ? " > " : "   ", thLabels[asTh - 1]);
            wattroff(w, COLOR_PAIR(CP_SELECTED));
            wattroff(w, COLOR_PAIR(CP_DEFAULT));
        }
        row += 3;
    }

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 2, 2, "[Enter] Apply  [Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

static void render_talents_fullscreen(GameState *gs) {
    WINDOW *w = gs->wLeft;
    WINDOW *we = gs->wEnemy;
    WINDOW *wl = gs->wLog;
    werase(w); werase(we); werase(wl);
    wnoutrefresh(w); wnoutrefresh(we); wnoutrefresh(wl);

    Hero *h = &gs->hero;
    int tree = gs->talentTreeIdx;
    const TalentTreeDef *td = data_talent_tree(h->classId, tree);
    if (!td) return;

    WINDOW *fw = stdscr;
    werase(fw);

    static const int TIER_GATE[TALENT_TIERS] = { 0, 5, 10, 15, 20 };
    static const int COL_X[3] = { 2, 28, 54 };
    static const int COL_CENTER[3] = { 14, 40, 66 };
    #define NODE_W 24

    /* Row 0: point summary — distribution left, available right */
    {
        wattron(fw, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(fw, 0, 1, "(%d/%d/%d)",
                  h->talentTreePoints[0], h->talentTreePoints[1], h->talentTreePoints[2]);
        wattroff(fw, COLOR_PAIR(CP_DEFAULT));

        wattron(fw, COLOR_PAIR(CP_GREEN) | A_BOLD);
        char pts[16];
        int plen = snprintf(pts, sizeof(pts), "Pts: %d", h->talentPoints);
        mvwprintw(fw, 0, SCREEN_W - plen - 1, "%s", pts);
        wattroff(fw, COLOR_PAIR(CP_GREEN) | A_BOLD);
    }

    /* Row 1: tab bar — [1/2/3] tree names, current highlighted */
    for (int t = 0; t < NUM_TALENT_TREES; t++) {
        const TalentTreeDef *tt = data_talent_tree(h->classId, t);
        const char *tn = tt ? tt->name : "???";
        int tc = tt ? tt->color : CP_DEFAULT;
        if (t == tree) {
            wattron(fw, COLOR_PAIR(tc) | A_BOLD);
            mvwprintw(fw, 1, COL_X[t], "[%d] %s", t + 1, tn);
            wattroff(fw, COLOR_PAIR(tc) | A_BOLD);
        } else {
            wattron(fw, COLOR_PAIR(tc));
            mvwprintw(fw, 1, COL_X[t], " %d  %s", t + 1, tn);
            wattroff(fw, COLOR_PAIR(tc));
        }
    }

    /* Build tier→col→nodeIdx lookup */
    int nodeAt[TALENT_TIERS][3];
    memset(nodeAt, -1, sizeof(nodeAt));
    for (int i = 0; i < td->nodeCount; i++) {
        int ti = td->nodes[i].tier, co = td->nodes[i].col;
        if (ti < TALENT_TIERS && co < 3) nodeAt[ti][co] = i;
    }

    int selNode = gs->talentNodeIdx;

    /*
     * Tree layout: 5 tiers with 3 rows each (node + connector + gate).
     * Tier T occupies:
     *   nodeRow  = 2 + T * 3       (the actual node cells)
     *   connRow  = 2 + T * 3 + 1   (prereq vertical lines)
     *   gateRow  = 2 + T * 3 + 2   (horizontal gate line "--- N ---")
     * Tier 4 (last) has no connector/gate below it.
     * Rows used: 2..16 (15 rows for 5 tiers).
     */

    /* Draw gate lines between tiers (rows 4, 7, 10, 13) */
    for (int tier = 0; tier < TALENT_TIERS - 1; tier++) {
        int gateRow = 2 + tier * 3 + 2;
        int gatePts = TIER_GATE[tier + 1];
        int met = (h->talentTreePoints[tree] >= gatePts);
        int gateColor = met ? CP_DEFAULT : CP_RED;

        char label[8];
        int llen = snprintf(label, sizeof(label), " %d ", gatePts);
        int lx = (SCREEN_W - llen) / 2;

        wattron(fw, COLOR_PAIR(gateColor));
        for (int x = 4; x < SCREEN_W - 4; x++) {
            if (x >= lx && x < lx + llen)
                mvwaddch(fw, gateRow, x, (chtype)label[x - lx]);
            else
                mvwaddch(fw, gateRow, x, ACS_HLINE);
        }
        wattroff(fw, COLOR_PAIR(gateColor));
    }

    /* Draw prereq connectors on connector rows (rows 3, 6, 9, 12) */
    for (int i = 0; i < td->nodeCount; i++) {
        const TalentNodeDef *nd = &td->nodes[i];
        if (nd->prereqNode < 0) continue;
        const TalentNodeDef *pre = &td->nodes[nd->prereqNode];
        int srcCol = pre->col, dstCol = nd->col;
        int connRow = 2 + pre->tier * 3 + 1;
        int preRanks = h->talentRanks[tree][nd->prereqNode];
        int lineColor = (preRanks >= pre->maxRank) ? CP_GREEN : CP_DEFAULT;

        wattron(fw, COLOR_PAIR(lineColor));
        if (srcCol == dstCol) {
            mvwaddch(fw, connRow, COL_CENTER[srcCol], ACS_VLINE);
        } else {
            int x1 = COL_CENTER[srcCol], x2 = COL_CENTER[dstCol];
            int lo = x1 < x2 ? x1 : x2, hi = x1 > x2 ? x1 : x2;
            mvwaddch(fw, connRow, x1, (x1 < x2) ? ACS_LLCORNER : ACS_LRCORNER);
            for (int x = lo + 1; x < hi; x++)
                mvwaddch(fw, connRow, x, ACS_HLINE);
            mvwaddch(fw, connRow, x2, (x2 > x1) ? ACS_URCORNER : ACS_ULCORNER);
        }
        wattroff(fw, COLOR_PAIR(lineColor));
    }

    /* Draw node cells */
    for (int tier = 0; tier < TALENT_TIERS; tier++) {
        int nodeRow = 2 + tier * 3;
        for (int c = 0; c < 3; c++) {
            int idx = nodeAt[tier][c];
            if (idx < 0) continue;
            const TalentNodeDef *nd = &td->nodes[idx];
            int ranks = h->talentRanks[tree][idx];
            int canInvest = hero_can_invest_talent(h, tree, idx);
            int isSel = (idx == selNode);

            int nodeColor;
            if (ranks >= nd->maxRank) nodeColor = CP_CYAN;
            else if (ranks > 0)       nodeColor = CP_GREEN;
            else if (canInvest)        nodeColor = CP_WHITE;
            else                       nodeColor = CP_DEFAULT;

            if (isSel) wattron(fw, COLOR_PAIR(sel_color(nodeColor)));
            else        wattron(fw, COLOR_PAIR(nodeColor));

            char cell[NODE_W + 1];
            snprintf(cell, sizeof(cell), " %c%-16.16s %d/%d",
                     nd->isActive ? '*' : ' ', nd->name, ranks, nd->maxRank);
            mvwprintw(fw, nodeRow, COL_X[c], "%-*.*s", NODE_W, NODE_W, cell);

            if (isSel) wattroff(fw, COLOR_PAIR(sel_color(nodeColor)));
            else        wattroff(fw, COLOR_PAIR(nodeColor));
        }
    }

    /* Detail panel (rows 16-19) */
    for (int x = 1; x < SCREEN_W - 1; x++)
        mvwaddch(fw, 16, x, ACS_HLINE | COLOR_PAIR(CP_DEFAULT));

    if (selNode >= 0 && selNode < td->nodeCount) {
        const TalentNodeDef *nd = &td->nodes[selNode];
        int ranks = h->talentRanks[tree][selNode];

        /* Row 17: name + type */
        wattron(fw, COLOR_PAIR(CP_WHITE) | A_BOLD);
        mvwprintw(fw, 17, 2, "%s", nd->name);
        wattroff(fw, COLOR_PAIR(CP_WHITE) | A_BOLD);
        if (nd->isActive) {
            wattron(fw, COLOR_PAIR(CP_YELLOW));
            wprintw(fw, " (Active)");
            wattroff(fw, COLOR_PAIR(CP_YELLOW));
        }

        /* Row 18: description + skill info */
        wattron(fw, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(fw, 18, 2, "%s", nd->desc);
        wattroff(fw, COLOR_PAIR(CP_DEFAULT));
        if (nd->isActive) {
            const SkillDef *sk = data_talent_skill(h->classId, tree, selNode);
            if (sk) {
                const ClassDef *cd = data_class(h->classId);
                wattron(fw, COLOR_PAIR(CP_YELLOW));
                mvwprintw(fw, 18, 54, "CD:%d %s:%d",
                          sk->cooldown, cd ? cd->resourceName : "Res", sk->resourceCost);
                wattroff(fw, COLOR_PAIR(CP_YELLOW));
            }
        }

        /* Row 19: rank progress + bonus values */
        wattron(fw, COLOR_PAIR(CP_WHITE));
        mvwprintw(fw, 19, 2, "Rank: %d/%d", ranks, nd->maxRank);
        wattroff(fw, COLOR_PAIR(CP_WHITE));
        if (!nd->isActive) {
            int bx = 16;
            for (int b = 0; b < MAX_TALENT_BONUSES; b++) {
                if (nd->bonus[b].type == TB_NONE) continue;
                int val = nd->bonus[b].perRank;
                int cur = val * ranks;
                int total = val * nd->maxRank;
                wattron(fw, COLOR_PAIR(CP_GREEN));
                bx += snprintf(NULL, 0, "  %+d / %+d", cur, total);
                mvwprintw(fw, 19, bx - snprintf(NULL, 0, "  %+d / %+d", cur, total),
                          "  %+d / %+d", cur, total);
                wattroff(fw, COLOR_PAIR(CP_GREEN));
            }
        }
        if (nd->prereqNode >= 0) {
            const TalentNodeDef *pre = &td->nodes[nd->prereqNode];
            int preRanks = h->talentRanks[tree][nd->prereqNode];
            int preMet = (preRanks >= pre->maxRank);
            wattron(fw, COLOR_PAIR(preMet ? CP_GREEN : CP_RED));
            mvwprintw(fw, 19, 48, "Req: %s %s",
                      pre->name, preMet ? "(OK)" : "(!)");
            wattroff(fw, COLOR_PAIR(preMet ? CP_GREEN : CP_RED));
        }
    }

    /* Row 21: controls */
    wattron(fw, COLOR_PAIR(CP_CYAN));
    mvwprintw(fw, 21, 2, "[Arrows]Move [Enter]+1 [Bksp]-1 [R]Reset [Tab]Tree [Esc]Back");
    wattroff(fw, COLOR_PAIR(CP_CYAN));

    wnoutrefresh(fw);
    #undef NODE_W
}

static void render_enemy_panel(GameState *gs) {
    if (gs->offlineShowUntil > (int64_t)time(NULL) && gs->screen == SCR_MAIN) {
        WINDOW *w = gs->wEnemy;
        werase(w);
        wattron(w, COLOR_PAIR(CP_BORDER));
        box(w, 0, 0);
        wattroff(w, COLOR_PAIR(CP_BORDER));
        wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
        mvwprintw(w, 2, RIGHT_W / 2 - 7, "Welcome back!");
        wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
        wattron(w, COLOR_PAIR(CP_CYAN) | A_BOLD);
        if (gs->offlineMin >= 60)
            mvwprintw(w, 4, 4, "Offline for %dh%dm",
                      gs->offlineMin / 60, gs->offlineMin % 60);
        else if (gs->offlineMin >= 1)
            mvwprintw(w, 4, 4, "Offline for %dm", gs->offlineMin);
        else
            mvwprintw(w, 4, 4, "Offline for <1m");
        wattroff(w, COLOR_PAIR(CP_CYAN) | A_BOLD);
        wattron(w, COLOR_PAIR(CP_GREEN) | A_BOLD);
        mvwprintw(w, 5, 4, "+%d XP  +%d Gold", gs->offlineXp, gs->offlineGold);
        wattroff(w, COLOR_PAIR(CP_GREEN) | A_BOLD);
        wnoutrefresh(w);
        return;
    }
    if (gs->screen == SCR_SAVE_SELECT) {
        WINDOW *w = gs->wEnemy;
        werase(w);
        wattron(w, COLOR_PAIR(CP_BORDER));
        box(w, 0, 0);
        wattroff(w, COLOR_PAIR(CP_BORDER));
        if (gs->menuIdx < NUM_SAVE_SLOTS) {
            SaveSlotInfo *info = &gs->slotInfo[gs->menuIdx];
            if (info->exists) {
                const char **art = data_class_art(info->classId);
                if (art) {
                    wattron(w, COLOR_PAIR(CP_WHITE));
                    for (int i = 0; i < ART_LINES; i++) {
                        int xoff = (RIGHT_W - ART_COLS) / 2 - 2;
                        if (xoff < 1) xoff = 1;
                        mvwprintw(w, 1 + i, xoff, "%s", art[i]);
                    }
                    wattroff(w, COLOR_PAIR(CP_WHITE));
                }
            }
        }
        wnoutrefresh(w);
        return;
    }
    if (gs->screen == SCR_ACHIEVEMENTS) {
        render_achievement_detail(gs);
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
    if (gs->screen == SCR_ENCY_AFFIXES) {
        render_ency_affixes_detail(gs);
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

    char hpBuf[24];
    int hpLen = snprintf(hpBuf, sizeof(hpBuf), "%d/%d", e->hp, e->maxHp);
    int hpCol = RIGHT_W - 2 - hpLen;
    if (hpCol < 23) hpCol = 23;
    int barLen = hpCol - 23;
    if (barLen < 1) barLen = 1;

    int nameColor = gs->isElite ? CP_YELLOW : (gs->bossActive ? CP_MAGENTA : CP_WHITE);
    wattron(w, COLOR_PAIR(nameColor) | A_BOLD);
    mvwprintw(w, 6, 2, "%-20s", e->name);
    wattroff(w, COLOR_PAIR(nameColor) | A_BOLD);
    draw_bar(w, 6, 22, barLen, e->hp, e->maxHp, CP_RED, CP_DEFAULT);
    wattron(w, COLOR_PAIR(CP_WHITE));
    mvwprintw(w, 6, hpCol, "%s", hpBuf);
    wattroff(w, COLOR_PAIR(CP_WHITE));

    if (gs->hardModeActive) {
        char aff[48];
        int affLen = snprintf(aff, sizeof(aff), "%s + %s",
                  data_affix_name(gs->activeAffixes[0]),
                  data_affix_name(gs->activeAffixes[1]));
        int affCol = RIGHT_W - 2 - affLen;
        if (affCol < 2) affCol = 2;
        wattron(w, COLOR_PAIR(CP_MAGENTA));
        mvwprintw(w, 5, affCol, "%s", aff);
        wattroff(w, COLOR_PAIR(CP_MAGENTA));
    }

    if (gs->combatTicks > 0) {
        EStats ces = hero_effective_stats(&gs->hero);
        float secs = gs->combatTicks * ces.tickRate / 1000.0f;
        if (secs > 0.1f) {
            int dps = (int)(gs->combatDmgDealt / secs);
            int dtps = (int)(gs->combatDmgTaken / secs);
            char buf[32];
            int len = snprintf(buf, sizeof(buf), "DPS:%d DTPS:%d", dps, dtps);
            wattron(w, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(w, 1, RIGHT_W - len - 2, "%s", buf);
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

    /* Save-select: show full hero detail instead of combat log */
    if (gs->screen == SCR_SAVE_SELECT) {
        if (gs->menuIdx < NUM_SAVE_SLOTS) {
            SaveSlotInfo *info = &gs->slotInfo[gs->menuIdx];
            if (info->exists) {
                Hero *h = &info->hero;
                const ClassDef *cd = data_class(h->classId);
                EStats es = hero_effective_stats(h);

                wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
                mvwprintw(w, 1, 2, "Lv.%d %s", h->level, cd->name);
                wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);

                wattron(w, COLOR_PAIR(CP_YELLOW));
                mvwprintw(w, 1, 30, "Gold: %d", h->gold);
                wattroff(w, COLOR_PAIR(CP_YELLOW));

                wattron(w, COLOR_PAIR(CP_WHITE));
                mvwprintw(w, 2, 2, "HP:%-5d DMG:%-4d DR:%d%%",
                          es.maxHp, es.damage, (int)(es.dmgReduction * 100));
                mvwprintw(w, 3, 2, "SPD:%dms  Crit:%.0f%%  Dodge:%.0f%%",
                          es.tickRate, es.critChance * 100, es.dodgeChance * 100);
                wattroff(w, COLOR_PAIR(CP_WHITE));

                mvwhline(w, 4, 1, ACS_HLINE, RIGHT_W - 2);

                for (int s = 0; s < NUM_SLOTS; s++) {
                    int row = 5 + s;
                    if (row >= LOG_H - 2) break;
                    wattron(w, COLOR_PAIR(CP_WHITE));
                    mvwprintw(w, row, 2, "%-7s", data_slot_name(s));
                    wattroff(w, COLOR_PAIR(CP_WHITE));
                    if (h->hasEquip[s]) {
                        int rc = data_rarity_color(h->equipment[s].rarity);
                        wattron(w, COLOR_PAIR(rc));
                        mvwprintw(w, row, 10, "%-28.28s", h->equipment[s].name);
                        wattroff(w, COLOR_PAIR(rc));
                        wattron(w, COLOR_PAIR(CP_WHITE));
                        mvwprintw(w, row, 39, "Lv.%d", h->equipment[s].levelReq);
                        wattroff(w, COLOR_PAIR(CP_WHITE));
                    } else {
                        wattron(w, COLOR_PAIR(CP_DEFAULT));
                        mvwprintw(w, row, 10, "---");
                        wattroff(w, COLOR_PAIR(CP_DEFAULT));
                    }
                }

                mvwhline(w, 12, 1, ACS_HLINE, RIGHT_W - 2);

                wattron(w, COLOR_PAIR(CP_WHITE));
                mvwprintw(w, 13, 2, "Kills:%-5d Deaths:%-3d", h->totalKills, h->deaths);
                wattroff(w, COLOR_PAIR(CP_WHITE));
                if (info->wasInDungeon) {
                    const DungeonDef *dg = data_dungeon(info->dungeon);
                    if (dg) {
                        wattron(w, COLOR_PAIR(CP_CYAN));
                        mvwprintw(w, 13, 28, "In: %s", dg->name);
                        wattroff(w, COLOR_PAIR(CP_CYAN));
                    }
                }
            }
        }
        wnoutrefresh(w);
        return;
    }

    int logStartRow = 1;
    int maxLogLines = LOG_H - 2;

    if (gs->heroCreated && gs->inDungeon) {
        Hero *h = &gs->hero;
        const ClassDef *cd = data_class(h->classId);
        EStats es = hero_effective_stats(h);

        wattron(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
        mvwprintw(w, 1, 2, "%-20.20s", h->name);
        wattroff(w, COLOR_PAIR(CP_YELLOW) | A_BOLD);
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, 1, 24, "Lv.%d %s", h->level, cd->name);
        wattroff(w, COLOR_PAIR(CP_WHITE));

        wattron(w, COLOR_PAIR(CP_RED));
        mvwprintw(w, 2, 2, "HP");
        wattroff(w, COLOR_PAIR(CP_RED));
        draw_bar(w, 2, 5, 20, h->hp, es.maxHp, CP_RED, CP_DEFAULT);
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, 2, 26, "%d/%d", h->hp, es.maxHp);
        wattroff(w, COLOR_PAIR(CP_WHITE));

        int resColor = (h->classId == CLASS_WARRIOR) ? CP_RED :
                       (h->classId == CLASS_ROGUE)   ? CP_YELLOW : CP_BLUE;
        wattron(w, COLOR_PAIR(resColor));
        mvwprintw(w, 3, 2, "%-2.2s", cd->resourceName);
        wattroff(w, COLOR_PAIR(resColor));
        draw_bar(w, 3, 5, 20, h->resource, h->maxResource, resColor, CP_DEFAULT);
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, 3, 26, "%d/%d", h->resource, h->maxResource);
        wattroff(w, COLOR_PAIR(CP_WHITE));

        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, 4, 2, "DMG:%d DR:%d%% SPD:%dms Crit:%.0f%%",
                  es.damage, (int)(es.dmgReduction * 100), es.tickRate,
                  es.critChance * 100);
        wattroff(w, COLOR_PAIR(CP_WHITE));

        wattron(w, COLOR_PAIR(CP_GREEN));
        mvwprintw(w, 5, 2, "XP");
        wattroff(w, COLOR_PAIR(CP_GREEN));
        float xpPct = hero_xp_pct(h);
        draw_bar(w, 5, 5, 15, (int)(xpPct * 100), 100, CP_GREEN, CP_DEFAULT);
        wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, 5, 21, "%.0f%%", xpPct * 100);
        wattroff(w, COLOR_PAIR(CP_WHITE));
        wattron(w, COLOR_PAIR(CP_YELLOW));
        mvwprintw(w, 5, 28, "Gold:%d", h->gold);
        wattroff(w, COLOR_PAIR(CP_YELLOW));

        mvwhline(w, 6, 1, ACS_HLINE, RIGHT_W - 2);

        logStartRow = 7;
        maxLogLines = LOG_H - 8;
    }

    int start = gs->logCount - maxLogLines;
    if (start < 0) start = 0;

    for (int i = start; i < gs->logCount; i++) {
        int row = logStartRow + (i - start);
        if (row >= LOG_H - 1) break;
        LogLine *l = &gs->log[i];
        wattron(w, COLOR_PAIR(l->color));
        mvwprintw(w, row, 1, " %-*.*s", LOG_LINE_W - 1, LOG_LINE_W - 1, l->text);
        wattroff(w, COLOR_PAIR(l->color));
    }

    wnoutrefresh(w);
}

#define DEBUG_MENU_N 3

static void render_debug(GameState *gs) {
    WINDOW *w = gs->wLeft;
    werase(w);
    wattron(w, COLOR_PAIR(CP_BORDER));
    box(w, 0, 0);
    wattroff(w, COLOR_PAIR(CP_BORDER));

    wattron(w, COLOR_PAIR(CP_RED) | A_BOLD);
    mvwprintw(w, 1, 2, "DEBUG");
    wattroff(w, COLOR_PAIR(CP_RED) | A_BOLD);

    Hero *h = &gs->hero;
    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, 2, 2, "Lv.%d  Gold: %d", h->level, h->gold);
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    const char *labels[] = { "Reset Character", "Set Level", "Set Gold (+/-10k)" };
    int row = 4;
    for (int i = 0; i < DEBUG_MENU_N; i++) {
        int sel = (i == gs->menuIdx);
        if (sel) wattron(w, COLOR_PAIR(CP_SELECTED));
        else wattron(w, COLOR_PAIR(CP_WHITE));
        mvwprintw(w, row, 1, "%s%-23.23s", sel ? " > " : "   ", labels[i]);
        wattroff(w, COLOR_PAIR(CP_SELECTED));
        wattroff(w, COLOR_PAIR(CP_WHITE));
        row += 2;
    }

    wattron(w, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(w, row + 1, 2, "[L/R] adjust value");
    wattroff(w, COLOR_PAIR(CP_DEFAULT));

    wattron(w, COLOR_PAIR(CP_CYAN));
    mvwprintw(w, PANEL_H - 3, 2, "[Enter] Apply");
    mvwprintw(w, PANEL_H - 2, 2, "[Esc] Back");
    wattroff(w, COLOR_PAIR(CP_CYAN));
    wnoutrefresh(w);
}

void ui_render(GameState *gs) {
    if (gs->screen == SCR_TALENTS) {
        render_talents_fullscreen(gs);
        doupdate();
        return;
    }

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
    case SCR_ENCY_AFFIXES:  render_ency_affixes(gs);  break;
    case SCR_ACHIEVEMENTS:  render_achievements(gs);   break;
    case SCR_TITLES:        render_titles(gs);         break;
    case SCR_BULK_SELL:     render_bulk_sell(gs);      break;
    case SCR_DEBUG:         render_debug(gs);          break;
    default: break;
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
            gs->offlineShowUntil = 0;
            switch (gs->menuIdx) {
            case 0: gs->screen = SCR_DUNGEON;      gs->menuIdx = 0; break;
            case 1: gs->screen = SCR_CHARACTER;     gs->menuIdx = 0; break;
            case 2: gs->screen = SCR_TALENTS;       gs->talentTreeIdx = 0; gs->talentNodeIdx = 0; break;
            case 3: gs->screen = SCR_EQUIPMENT;     gs->menuIdx = 0; break;
            case 4: gs->screen = SCR_SHOP;          gs->menuIdx = 0; gs->shopSlot = 0; break;
            case 5: gs->screen = SCR_ENCYCLOPEDIA;  gs->menuIdx = 0; break;
            case 6: gs->screen = SCR_CONFIRM_QUIT;  gs->menuIdx = 0; break;
            }
        }
        if (ch == 'q' || ch == 'Q') { gs->offlineShowUntil = 0; gs->screen = SCR_CONFIRM_QUIT; gs->menuIdx = 0; }
        if (gs->debugMode && (ch == 'd' || ch == 'D')) { gs->screen = SCR_DEBUG; gs->menuIdx = 0; }
        break;

    case SCR_DUNGEON: {
        int maxIdx = gs->inDungeon ? NUM_DUNGEONS : NUM_DUNGEONS - 1;
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = maxIdx; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx > maxIdx) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_MAIN; gs->menuIdx = 0; }
        if (ch == 'h' || ch == 'H') {
            if (gs->menuIdx >= 0 && gs->menuIdx < NUM_DUNGEONS && gs->hero.hardMode[gs->menuIdx] >= 1)
                gs->hero.hardMode[gs->menuIdx] = gs->hero.hardMode[gs->menuIdx] == 2 ? 1 : 2;
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
                    gs->hardModeActive = (gs->hero.hardMode[gs->menuIdx] == 2);
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
        if (ch == 't' || ch == 'T') { gs->screen = SCR_TITLES; gs->menuIdx = gs->hero.activeTitle; }
        if (ch == 27) { gs->screen = SCR_MAIN; gs->menuIdx = 1; }
        break;

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
            case 1: gs->screen = SCR_ENCY_STATS;    gs->menuIdx = 0; break;
            case 2: gs->screen = SCR_ENCY_SKILLS;   gs->menuIdx = 0; break;
            case 3: gs->screen = SCR_ENCY_DUNGEONS; gs->menuIdx = 0; break;
            case 4: gs->screen = SCR_ENCY_ENEMIES;  gs->menuIdx = 0; break;
            case 5: gs->screen = SCR_ENCY_BOSSES;   gs->menuIdx = 0; break;
            case 6: gs->screen = SCR_ENCY_ITEMS;    gs->menuIdx = 0; gs->encycSlot = 0; break;
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
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 1; }
        break;

    case SCR_ENCY_ITEMS: {
        int items[200];
        int cm = (1 << gs->hero.classId);
        int nItems = ency_items_for_slot(gs->encycSlot, cm, items, 200);
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = nItems > 0 ? nItems - 1 : 0; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= nItems) gs->menuIdx = 0; }
        if (ch == KEY_LEFT ) { gs->encycSlot--; if (gs->encycSlot < 0) gs->encycSlot = NUM_SLOTS - 1; gs->menuIdx = 0; }
        if (ch == KEY_RIGHT) { gs->encycSlot++; if (gs->encycSlot >= NUM_SLOTS) gs->encycSlot = 0; gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 6; }
        break;
    }

    case SCR_ENCY_DUNGEONS:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = NUM_DUNGEONS - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= NUM_DUNGEONS) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 3; }
        break;

    case SCR_ENCY_ENEMIES: {
        int enemies[50];
        int nEnemies = ency_normal_enemies(enemies, 50);
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = nEnemies > 0 ? nEnemies - 1 : 0; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= nEnemies) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 4; }
        break;
    }

    case SCR_ENCY_BOSSES: {
        int bosses[10];
        int nBosses = ency_bosses(bosses, 10);
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = nBosses > 0 ? nBosses - 1 : 0; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= nBosses) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 5; }
        break;
    }

    case SCR_ENCY_SKILLS: {
        int total = MAX_SKILL_TIERS * 2;
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = total - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= total) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 2; }
        break;
    }

    case SCR_ENCY_COMBAT: {
    int nTopics = 16;
    if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = nTopics - 1; }
    if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= nTopics) gs->menuIdx = 0; }
        if ((ch == '\n' || ch == KEY_ENTER) && gs->menuIdx == 14)
            { gs->screen = SCR_ENCY_AFFIXES; gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCYCLOPEDIA; gs->menuIdx = 7; }
        break;
    }

    case SCR_ENCY_AFFIXES:
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = NUM_AFFIXES - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= NUM_AFFIXES) gs->menuIdx = 0; }
        if (ch == 27) { gs->screen = SCR_ENCY_COMBAT; gs->menuIdx = 14; }
        break;

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

    case SCR_TALENTS: {
        int tree = gs->talentTreeIdx;
        const TalentTreeDef *td = data_talent_tree(gs->hero.classId, tree);
        if (!td) break;
        int cur = gs->talentNodeIdx;
        const TalentNodeDef *cnd = (cur >= 0 && cur < td->nodeCount) ? &td->nodes[cur] : NULL;
        int curTier = cnd ? cnd->tier : 0;
        int curCol = cnd ? cnd->col : 0;

        /* L/R: move between columns in the same tier */
        if (ch == KEY_LEFT) {
            for (int nc = curCol - 1; nc >= 0; nc--) {
                for (int i = 0; i < td->nodeCount; i++)
                    if (td->nodes[i].tier == curTier && td->nodes[i].col == nc)
                        { gs->talentNodeIdx = i; goto nav_done; }
            }
        }
        if (ch == KEY_RIGHT) {
            for (int nc = curCol + 1; nc < 3; nc++) {
                for (int i = 0; i < td->nodeCount; i++)
                    if (td->nodes[i].tier == curTier && td->nodes[i].col == nc)
                        { gs->talentNodeIdx = i; goto nav_done; }
            }
        }
        /* Up/Down: move to closest node in adjacent tier, preferring same column */
        if (ch == KEY_UP) {
            int bestNode = -1, bestDist = 999;
            for (int i = 0; i < td->nodeCount; i++) {
                if (td->nodes[i].tier == curTier - 1) {
                    int dist = abs(td->nodes[i].col - curCol);
                    if (dist < bestDist) { bestDist = dist; bestNode = i; }
                }
            }
            if (bestNode >= 0) gs->talentNodeIdx = bestNode;
        }
        if (ch == KEY_DOWN) {
            int bestNode = -1, bestDist = 999;
            for (int i = 0; i < td->nodeCount; i++) {
                if (td->nodes[i].tier == curTier + 1) {
                    int dist = abs(td->nodes[i].col - curCol);
                    if (dist < bestDist) { bestDist = dist; bestNode = i; }
                }
            }
            if (bestNode >= 0) gs->talentNodeIdx = bestNode;
        }
        nav_done:
        /* Tab: cycle to next tree */
        if (ch == '\t') {
            gs->talentTreeIdx = (gs->talentTreeIdx + 1) % NUM_TALENT_TREES;
            gs->talentNodeIdx = 0;
        }
        /* 1/2/3: jump directly to a tree */
        if (ch >= '1' && ch <= '3') {
            int t = ch - '1';
            if (t != gs->talentTreeIdx) {
                gs->talentTreeIdx = t;
                gs->talentNodeIdx = 0;
            }
        }
        if (ch == '\n' || ch == KEY_ENTER) {
            tree = gs->talentTreeIdx;
            if (hero_invest_talent(&gs->hero, tree, gs->talentNodeIdx)) {
                const TalentNodeDef *nd = data_talent_node(gs->hero.classId, tree, gs->talentNodeIdx);
                char b[LOG_LINE_W + 1];
                snprintf(b, sizeof(b), "+1 %s (%d pts left)", nd ? nd->name : "?", gs->hero.talentPoints);
                ui_log(gs, b, CP_GREEN);
            }
        }
        if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
            tree = gs->talentTreeIdx;
            if (hero_uninvest_talent(&gs->hero, tree, gs->talentNodeIdx)) {
                const TalentNodeDef *nd = data_talent_node(gs->hero.classId, tree, gs->talentNodeIdx);
                char b[LOG_LINE_W + 1];
                snprintf(b, sizeof(b), "-1 %s (%d pts left)", nd ? nd->name : "?", gs->hero.talentPoints);
                ui_log(gs, b, CP_YELLOW);
            }
        }
        if (ch == 'r' || ch == 'R') {
            int spent = hero_total_talent_points_spent(&gs->hero);
            if (spent > 0) {
                int resetCost = gs->hero.level * gs->hero.level * 2;
                if (gs->hero.gold >= resetCost) {
                    gs->hero.gold -= resetCost;
                    hero_reset_talents(&gs->hero);
                    char rbuf[LOG_LINE_W + 1];
                    snprintf(rbuf, sizeof(rbuf), "Talents reset! (-%dG)", resetCost);
                    ui_log(gs, rbuf, CP_YELLOW);
                }
            }
        }
        if (ch == 27) {
            gs->screen = SCR_MAIN;
            gs->menuIdx = 2;
            gs->needsRender = 1;
        }
        break;
    }

    case SCR_BULK_SELL: {
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = BULK_MENU_N - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= BULK_MENU_N) gs->menuIdx = 0; }
        if (ch == '\n' || ch == KEY_ENTER) {
            if (gs->menuIdx == 4) {
                int raw = gs->hero.autoSellThreshold;
                int th = raw > 0 ? raw : (raw < 0 ? -raw : 1);
                gs->hero.autoSellThreshold = raw > 0 ? -th : th;
            } else if (gs->menuIdx == 5) {
                int raw = gs->hero.autoSellThreshold;
                int on = raw > 0;
                int th = raw > 0 ? raw : (raw < 0 ? -raw : 1);
                th = th % 4 + 1;
                gs->hero.autoSellThreshold = on ? th : -th;
            } else {
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
        }
        if (ch == 27) { gs->screen = SCR_EQUIPMENT; gs->menuIdx = 0; }
        break;
    }

    case SCR_DEBUG: {
        if (ch == KEY_UP) { gs->menuIdx--; if (gs->menuIdx < 0) gs->menuIdx = DEBUG_MENU_N - 1; }
        if (ch == KEY_DOWN) { gs->menuIdx++; if (gs->menuIdx >= DEBUG_MENU_N) gs->menuIdx = 0; }

        Hero *h = &gs->hero;
        int step = 1;
        if (gs->menuIdx == 1) step = 1;
        if (gs->menuIdx == 2) step = 10000;

        if (ch == KEY_RIGHT) {
            if (gs->menuIdx == 1 && h->level < MAX_LEVEL) {
                h->xp = 0;
                h->level++;
                h->talentPoints++;
                if (h->level > h->highestLevel) h->highestLevel = h->level;
                EStats es = hero_effective_stats(h);
                h->maxHp = es.maxHp;
                h->hp = h->maxHp;
                h->resource = h->maxResource;
            }
            if (gs->menuIdx == 2) h->gold += step;
        }
        if (ch == KEY_LEFT) {
            if (gs->menuIdx == 1 && h->level > 1) {
                h->xp = 0;
                h->level--;
                EStats es = hero_effective_stats(h);
                h->maxHp = es.maxHp;
                h->hp = h->maxHp;
                h->resource = h->maxResource;
            }
            if (gs->menuIdx == 2 && h->gold >= step) h->gold -= step;
        }

        if (ch == '\n' || ch == KEY_ENTER) {
            if (gs->menuIdx == 0) {
                int cls = h->classId;
                char name[MAX_NAME];
                strncpy(name, h->name, MAX_NAME - 1);
                name[MAX_NAME - 1] = '\0';
                *h = hero_create(cls, name);
                strncpy(h->name, name, MAX_NAME - 1);
                EStats es = hero_effective_stats(h);
                h->maxHp = es.maxHp;
                h->hp = h->maxHp;
                gs->inDungeon = 0;
                gs->hasEnemy = 0;
                gs->dungeonKills = 0;
                gs->bossActive = 0;
                gs->bossTimer = 0;
                gs->deathTimer = 0;
                ui_log(gs, "[DEBUG] Character reset.", CP_RED);
            }
        }

        if (ch == 27) { gs->screen = SCR_MAIN; gs->menuIdx = 0; }
        break;
    }
    }
}
