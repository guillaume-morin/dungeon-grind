#!/usr/bin/env python3
"""
Item rebalance computation script.
Generates C code for:
  1. Existing Epics scaled by 1.25x
  2. New Lv90 Epic tier
  3. Rewritten Legendaries with proper budgets + distribution
"""
import math, re, sys

# ── Constants ──────────────────────────────────────────────────────
SLOT_MUL = [120, 90, 110, 85, 60, 50, 50]  # Weapon,Helmet,Chest,Legs,Boots,Ring,Amulet
SLOT_NAMES = ["SLOT_WEAPON","SLOT_HELMET","SLOT_CHEST","SLOT_LEGS","SLOT_BOOTS","SLOT_RING","SLOT_AMULET"]
SLOT_SHORT = ["Weapon","Helmet","Chest","Legs","Boots","Ring","Amulet"]
SLOT_PLURAL = ["Weapons","Helmets","Chests","Legs","Boots","Rings","Amulets"]
CLASS_NAMES = ["Warrior","Rogue","Mage","Priest"]
CLASS_MASKS = ["CM_WAR","CM_ROG","CM_MAG","CM_PRI"]
# stat indices: STR=0, AGI=1, INT=2, WIS=3, VIT=4, DEF=5, SPD=6

# Primary stat index per class
PRI = [0, 1, 2, 3]  # STR, AGI, INT, WIS

# ── Part 1: Scale existing Epics by 1.25x ─────────────────────────
# We read the actual C file and do a regex replacement on RARITY_EPIC lines.
# Multiply each stat by 1.25 (round), price by 1.25 (round to nearest 10).

def scale_epic_line(line, factor=1.25):
    """Scale stats and price of an Epic item line by factor."""
    # Match: { "Name", SLOT_X, RARITY_EPIC, level, price, {s,s,s,s,s,s,s}, CM_X },
    m = re.match(
        r'(\s*\{\s*"[^"]+",\s*\w+,\s*RARITY_EPIC,\s*\d+,\s*)(\d+)(\s*,\s*\{)'
        r'(\s*\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*)'
        r'(\}\s*,\s*\w+\s*\}.*)', line)
    if not m:
        return line
    prefix = m.group(1)
    price = int(m.group(2))
    mid = m.group(3)
    stats_str = m.group(4)
    suffix = m.group(5)

    # Scale stats
    stats = [int(s.strip()) for s in stats_str.split(',')]
    new_stats = [round(s * factor) for s in stats]
    # Scale price (round to nearest 10, min 10)
    new_price = max(10, round(price * factor / 10) * 10)

    new_stats_str = ','.join(f'{s:>3}' if i > 0 else str(s) for i, s in enumerate(new_stats))
    # Reconstruct — keep original formatting as much as possible
    return f'{prefix}{new_price}{mid}{new_stats_str}{suffix}'


# ── Part 2: Generate Lv90 Epic tier ────────────────────────────────
# Budget: (2 + 90*1.1) * slot_mul/100 * 1.42/1.0 → we use RAR_MUL=142 for Epic
# (1.35x over Rare's 105)
# Theme: "Storm" — Stormforge, Thunder, Tempest, Cyclone, Gale(taken→Squall)

RAR_EPIC = 142  # 1.35x over Rare (105)

def epic_budget(level, slot):
    return round((2 + level * 1.1) * SLOT_MUL[slot] / 100.0 * RAR_EPIC / 100.0)

# Variant patterns (fraction of budget per stat):
# A (Might):    65% primary, 22% VIT, 13% DEF  (3 stats)
# B (Guardian): 40% primary, 10% secondary, 25% VIT, 25% DEF (4 stats)
# C (Swift):    45% primary, 15% secondary, 10% VIT, 30% SPD, budget*0.93 (4 stats)
#
# For boots: A gets SPD instead of DEF, B same, C same
# For jewelry (CM_ALL): special pattern (see below)

# Secondary stat per class: AGI for WAR, STR for ROG, WIS for MAG, INT for PRI
SEC = [1, 0, 3, 2]

def make_epic_A(budget, cls, slot):
    """Might variant: 65% primary, 22% VIT, 13% DEF (boots: 13% SPD)"""
    stats = [0]*7
    pri = PRI[cls]
    stats[pri] = round(budget * 0.65)
    stats[4] = round(budget * 0.22)  # VIT
    if slot == 4:  # boots
        stats[6] = budget - stats[pri] - stats[4]  # SPD
    else:
        stats[5] = budget - stats[pri] - stats[4]  # DEF
    return stats

def make_epic_B(budget, cls, slot):
    """Guardian variant: 40% primary, 10% secondary, 25% VIT, 25% DEF"""
    stats = [0]*7
    pri = PRI[cls]
    sec = SEC[cls]
    stats[pri] = round(budget * 0.40)
    stats[sec] = round(budget * 0.10)
    stats[4] = round(budget * 0.25)  # VIT
    stats[5] = budget - stats[pri] - stats[sec] - stats[4]  # DEF
    return stats

def make_epic_C(budget_full, cls, slot):
    """Swift variant: budget*0.93, 45% primary, 15% secondary, 10% VIT, 30% SPD"""
    budget = round(budget_full * 0.93)
    stats = [0]*7
    pri = PRI[cls]
    sec = SEC[cls]
    stats[pri] = round(budget * 0.45)
    stats[sec] = round(budget * 0.15)
    stats[4] = round(budget * 0.10)  # VIT
    stats[6] = budget - stats[pri] - stats[sec] - stats[4]  # SPD
    return stats

LV90_PRICES = [160000, 110000, 140000, 100000, 75000, 65000, 65000]

def epic_price(level, slot, budget):
    if level == 90:
        return LV90_PRICES[slot]
    base = {46: 1.0, 55: 1.6, 66: 3.0, 78: 5.6}
    multiplier = base.get(level, 5.6)
    raw = round(budget * multiplier * 100)
    return max(100, round(raw / 100) * 100)

# Lv90 Epic names per class per slot
# Theme: Storm/Thunder/Tempest/Lightning
LV90_NAMES = {
    # Weapons
    (0, 0): ("Stormreaver",      "Stormwall Blade",  "Stormwind Sword"),   # WAR
    (0, 1): ("Thunderstrike",    "Thunderguard Fang", "Thunderwind Shiv"),  # ROG
    (0, 2): ("Tempest Staff",    "Tempest Aegis Rod", "Tempest Gale Wand"), # MAG
    (0, 3): ("Storm Scepter",    "Stormward Mace",   "Stormgale Scepter"), # PRI
    # Helmets
    (1, 0): ("Stormforged Helm", "Stormwall Helm",   "Stormwind Visor"),
    (1, 1): ("Thunderstalker Hood","Thunderguard Hood","Thunderwind Cowl"),
    (1, 2): ("Tempest Crown",    "Tempest Ward Crown","Tempest Gale Tiara"),
    (1, 3): ("Storm Halo",       "Stormward Coif",   "Stormgale Mitre"),
    # Chests
    (2, 0): ("Stormplate Armor", "Stormwall Plate",  "Stormwind Mail"),
    (2, 1): ("Thunderweave Vest","Thunderguard Vest", "Thunderwind Tunic"),
    (2, 2): ("Tempest Robe",     "Tempest Ward Robe","Tempest Gale Mantle"),
    (2, 3): ("Storm Vestment",   "Stormward Raiment","Stormgale Cassock"),
    # Legs
    (3, 0): ("Stormforged Greaves","Stormwall Tassets","Stormwind Guards"),
    (3, 1): ("Thunderstalker Pants","Thunderguard Pants","Thunderwind Chaps"),
    (3, 2): ("Tempest Leggings", "Tempest Ward Kilt","Tempest Gale Skirt"),
    (3, 3): ("Storm Cuisses",    "Stormward Robes",  "Stormgale Breeches"),
    # Boots
    (4, 0): ("Stormforged Boots","Stormwall Treads",  "Stormwind Stompers"),
    (4, 1): ("Thunderstalker Boots","Thunderguard Boots","Thunderwind Shoes"),
    (4, 2): ("Tempest Walkers",  "Tempest Ward Shoes","Tempest Gale Steps"),
    (4, 3): ("Storm Striders",   "Stormward Sandals","Stormgale Treads"),
}
# Jewelry (CM_ALL)
LV90_RING_NAMES = ("Storm Signet",    "Storm Aegis Band",  "Storm Gale Loop")
LV90_AMUL_NAMES = ("Storm Heart",     "Storm Ward Charm",  "Storm Gale Locket")

# Jewelry patterns (CM_ALL): balanced across off stats
# A: primary offense spread, B: VIT+DEF, C: balanced+SPD
def make_jewel_A(budget):
    """Offense ring: split across 4 offense stats + some VIT."""
    s = [0]*7
    each = round(budget * 0.20)
    s[0] = each; s[1] = each; s[4] = budget - 4*each; s[2] = each; s[3] = each
    # Reorder: put VIT in slot 4
    return s

def make_jewel_B(budget):
    """Tank ring: VIT+DEF focused."""
    s = [0]*7
    s[0] = round(budget * 0.08)
    s[1] = round(budget * 0.08)
    s[2] = round(budget * 0.08)
    s[3] = round(budget * 0.08)
    s[4] = round(budget * 0.34)  # VIT
    s[5] = budget - s[0]-s[1]-s[2]-s[3]-s[4]  # DEF
    return s

def make_jewel_C(budget_full):
    """Speed ring: balanced + SPD."""
    budget = round(budget_full * 0.95)
    s = [0]*7
    each = round(budget * 0.12)
    s[0] = each; s[1] = each; s[2] = each; s[3] = each
    s[4] = round(budget * 0.12)
    s[6] = budget - 5*each  # SPD
    return s


# ── Part 3: Legendary items ───────────────────────────────────────
# Budget: (2 + 100*1.1) * slot_mul/100 * RAR_LEG/100
RAR_LEG = 192  # 1.35x over Epic (142)

def leg_budget(slot):
    return round((2 + 100 * 1.1) * SLOT_MUL[slot] / 100.0 * RAR_LEG / 100.0)

def leg_price(slot, budget):
    return max(10000, round(budget * 1000 / 10000) * 10000)

# Legendary stat distribution per class per slot
# Philosophy: "Supreme" — best at everything, primary-stat loaded, class-specific
# Weapon: 55% primary, 20% VIT, 10% DEF, 15% class-secondary
# Armor (H/C/L): 38% primary, 28% VIT, 12% DEF, 22% class-secondary
# Boots: 22% primary, 20% VIT, 10% DEF, 48% SPD (boots = speed)
# Ring: class-specific (40% primary, 25% secondary, 20% VIT, 15% SPD)
# Amulet: class-specific (35% primary, 25% secondary, 25% VIT, 15% DEF)

# Legendary secondary stats per class (for weapon + armor, NOT boots):
# WAR: AGI (crit chance via AGI/300, 0.2× dmg coeff — moderate scaling)
# ROG: STR (0.2× dmg coeff — moderate scaling)
# MAG: WIS (0.4× dmg coeff — strong secondary)
# PRI: INT (0.2× dmg coeff — moderate secondary)
# Boots always use SPD. Jewelry uses secondary + SPD split.
LEG_SEC = [1, 0, 3, 2]  # AGI, STR, WIS, INT

def make_leg_weapon(budget, cls):
    s = [0]*7
    pri = PRI[cls]
    sec = LEG_SEC[cls]
    s[pri] = round(budget * 0.55)
    s[4] = round(budget * 0.20)
    s[5] = round(budget * 0.10)
    s[sec] = budget - s[pri] - s[4] - s[5]
    return s

def make_leg_armor(budget, cls):
    s = [0]*7
    pri = PRI[cls]
    sec = LEG_SEC[cls]
    s[pri] = round(budget * 0.38)
    s[4] = round(budget * 0.28)
    s[5] = round(budget * 0.12)
    s[sec] = budget - s[pri] - s[4] - s[5]
    return s

def make_leg_boots(budget, cls):
    s = [0]*7
    pri = PRI[cls]
    s[pri] = round(budget * 0.22)
    s[4] = round(budget * 0.20)
    s[5] = round(budget * 0.10)
    s[6] = budget - s[pri] - s[4] - s[5]
    return s

def make_leg_ring(budget, cls):
    s = [0]*7
    pri = PRI[cls]
    sec = LEG_SEC[cls]
    s[pri] = round(budget * 0.40)
    s[sec] = round(budget * 0.25)
    s[4] = round(budget * 0.20)
    s[6] = budget - s[pri] - s[sec] - s[4]
    return s

def make_leg_amulet(budget, cls):
    s = [0]*7
    pri = PRI[cls]
    sec = LEG_SEC[cls]
    s[pri] = round(budget * 0.35)
    s[sec] = round(budget * 0.25)
    s[4] = round(budget * 0.25)
    s[5] = budget - s[pri] - s[sec] - s[4]
    return s

# Legendary names (keep existing names, they're good)
LEG_NAMES = {
    (0, 0): "Eternity's Edge",   (0, 1): "Eternal Whisper",
    (0, 2): "Staff of Ages",     (0, 3): "Eternal Grace",
    (1, 0): "Helm of Ages",      (1, 1): "Timeless Hood",
    (1, 2): "Crown of Aeons",    (1, 3): "Eternal Aureole",
    (2, 0): "Armor of Eternity", (2, 1): "Timeless Garb",
    (2, 2): "Robe of Aeons",     (2, 3): "Eternal Vestment",
    (3, 0): "Greaves of Ages",   (3, 1): "Timeless Legwraps",
    (3, 2): "Leggings of Aeons", (3, 3): "Eternal Greaves",
    (4, 0): "Boots of Eternity", (4, 1): "Timeless Stride",
    (4, 2): "Steps of Aeons",    (4, 3): "Eternal Striders",
}
# Ring and Amulet legendaries — NOW class-specific (4 each)
LEG_RING_NAMES = {0: "Eternal Signet", 1: "Timeless Loop", 2: "Band of Aeons", 3: "Eternal Seal"}
LEG_AMUL_NAMES = {0: "Charm of Ages", 1: "Timeless Pendant", 2: "Aeon Locket", 3: "Eternal Talisman"}


# ── Output generation ──────────────────────────────────────────────

def fmt_stats(s):
    """Format stats as C initializer {s,s,s,s,s,s,s}"""
    return '{' + ','.join(f'{v:>3}' if i > 0 else f'{v}' for i, v in enumerate(s)) + '}'

def fmt_item(name, slot, rarity, level, price, stats, cm):
    """Format a single item as C struct initializer."""
    rar_str = "RARITY_EPIC" if rarity == 3 else "RARITY_LEGENDARY"
    pad_name = f'"{name}",'
    # Pad to align columns
    return f'    {{ {pad_name:<24}{SLOT_NAMES[slot]},  {rar_str},{level:>8},{price:>6}, {fmt_stats(stats)}, {cm} }},'


def generate_lv90_epics():
    """Generate all Lv90 Epic items."""
    lines = []
    level = 90

    # Armor + Weapons (class-specific)
    for slot in range(5):  # 0-4
        lines.append(f'    /* ── {SLOT_PLURAL[slot]} Lv{level} ── */')
        for cls in range(4):
            budget = epic_budget(level, slot)
            price = epic_price(level, slot, budget)
            names = LV90_NAMES[(slot, cls)]
            cm = CLASS_MASKS[cls]

            stats_a = make_epic_A(budget, cls, slot)
            stats_b = make_epic_B(budget, cls, slot)
            stats_c = make_epic_C(budget, cls, slot)

            lines.append(fmt_item(names[0], slot, 3, level, price, stats_a, cm))
            lines.append(fmt_item(names[1], slot, 3, level, price, stats_b, cm))
            lines.append(fmt_item(names[2], slot, 3, level, price, stats_c, cm))

    # Rings
    lines.append(f'    /* ── Rings Lv{level} (CM_ALL) ── */')
    budget = epic_budget(level, 5)
    price = epic_price(level, 5, budget)
    lines.append(fmt_item(LV90_RING_NAMES[0], 5, 3, level, price, make_jewel_A(budget), "CM_ALL"))
    lines.append(fmt_item(LV90_RING_NAMES[1], 5, 3, level, price, make_jewel_B(budget), "CM_ALL"))
    lines.append(fmt_item(LV90_RING_NAMES[2], 5, 3, level, price, make_jewel_C(budget), "CM_ALL"))

    # Amulets
    lines.append(f'    /* ── Amulets Lv{level} (CM_ALL) ── */')
    budget = epic_budget(level, 6)
    price = epic_price(level, 6, budget)
    lines.append(fmt_item(LV90_AMUL_NAMES[0], 6, 3, level, price, make_jewel_A(budget), "CM_ALL"))
    lines.append(fmt_item(LV90_AMUL_NAMES[1], 6, 3, level, price, make_jewel_B(budget), "CM_ALL"))
    lines.append(fmt_item(LV90_AMUL_NAMES[2], 6, 3, level, price, make_jewel_C(budget), "CM_ALL"))

    return '\n'.join(lines)


def generate_legendaries():
    """Generate all Legendary items with new budgets."""
    lines = []

    # Weapons
    lines.append('    /* ── Legendary weapons (D8 boss exclusive, levelReq 100) ── */')
    for cls in range(4):
        slot = 0
        budget = leg_budget(slot)
        price = leg_price(slot, budget)
        stats = make_leg_weapon(budget, cls)
        name = LEG_NAMES[(slot, cls)]
        lines.append(fmt_item(name, slot, 4, 100, price, stats, CLASS_MASKS[cls]))

    # Armor (Helmet, Chest, Legs, Boots)
    for slot in range(1, 5):
        lines.append(f'    /* ── Legendary {SLOT_PLURAL[slot].lower()} ── */')
        for cls in range(4):
            budget = leg_budget(slot)
            price = leg_price(slot, budget)
            if slot == 4:  # boots
                stats = make_leg_boots(budget, cls)
            else:
                stats = make_leg_armor(budget, cls)
            name = LEG_NAMES[(slot, cls)]
            lines.append(fmt_item(name, slot, 4, 100, price, stats, CLASS_MASKS[cls]))

    # Rings (class-specific now)
    lines.append('    /* ── Legendary rings (class-specific) ── */')
    for cls in range(4):
        slot = 5
        budget = leg_budget(slot)
        price = leg_price(slot, budget)
        stats = make_leg_ring(budget, cls)
        name = LEG_RING_NAMES[cls]
        lines.append(fmt_item(name, slot, 4, 100, price, stats, CLASS_MASKS[cls]))

    # Amulets (class-specific now)
    lines.append('    /* ── Legendary amulets (class-specific) ── */')
    for cls in range(4):
        slot = 6
        budget = leg_budget(slot)
        price = leg_price(slot, budget)
        stats = make_leg_amulet(budget, cls)
        name = LEG_AMUL_NAMES[cls]
        lines.append(fmt_item(name, slot, 4, 100, price, stats, CLASS_MASKS[cls]))

    return '\n'.join(lines)


# ── Power verification ──────────────────────────────────────────────

def get_legendary_totals(cls):
    """Get total stats for a full Legendary set for a given class."""
    total = [0]*7
    for slot in range(7):
        budget = leg_budget(slot)
        if slot == 0:
            s = make_leg_weapon(budget, cls)
        elif slot < 4:
            s = make_leg_armor(budget, cls)
        elif slot == 4:
            s = make_leg_boots(budget, cls)
        elif slot == 5:
            s = make_leg_ring(budget, cls)
        else:
            s = make_leg_amulet(budget, cls)
        for i in range(7):
            total[i] += s[i]
    return total

def get_epic90_might_totals(cls):
    """Get total stats for a full Epic Lv90 'Might' (A-variant) set.
    Jewelry uses jewel_A (offense spread) which is CM_ALL."""
    total = [0]*7
    for slot in range(7):
        budget = epic_budget(90, slot)
        if slot < 5:
            s = make_epic_A(budget, cls, slot)
        else:
            s = make_jewel_A(budget)  # CM_ALL jewelry
        for i in range(7):
            total[i] += s[i]
    return total

def calc_damage(cls, total):
    """Calculate damage given class index and gear stat totals."""
    # Base stats: WAR{12,5,3,3,10,8,5} ROG{5,12,3,3,7,4,10} MAG{3,4,14,5,5,2,6} PRI{3,3,6,14,8,5,4}
    bases = [
        [12, 5, 3, 3, 10, 8, 5],   # Warrior
        [5, 12, 3, 3, 7, 4, 10],    # Rogue
        [3, 4, 14, 5, 5, 2, 6],     # Mage
        [3, 3, 6, 14, 8, 5, 4],     # Priest
    ]
    b = bases[cls]
    t = [b[i] + total[i] for i in range(7)]
    if cls == 0:   # WAR: STR*1.5 + AGI*0.2
        return t[0] * 1.5 + t[1] * 0.2
    elif cls == 1:  # ROG: AGI*1.5 + STR*0.2, critMult 1.8
        return (t[1] * 1.5 + t[0] * 0.2)
    elif cls == 2:  # MAG: INT*1.5 + WIS*0.4
        return t[2] * 1.5 + t[3] * 0.4
    else:           # PRI: WIS*1.4 + INT*0.2
        return t[3] * 1.4 + t[2] * 0.2

def calc_hp(cls, total):
    """Calculate maxHP given class index and gear stat totals."""
    base_hp = 120
    bases_vit = [10, 7, 5, 8]
    bases_str = [12, 5, 3, 3]
    t_vit = bases_vit[cls] + total[4]
    t_str = bases_str[cls] + total[0]
    return base_hp + t_vit * 5 + t_str * 2

def verify_all_classes():
    """Compare Legendary vs Epic Lv90 vs Rare Lv91 for all 4 classes."""
    stat_labels = ["STR","AGI","INT","WIS","VIT","DEF","SPD"]

    # Rare Lv91 budgets per slot (budget = (2+91*1.1)*slot_mul/100 * 105/100)
    rare_budgets = [round((2 + 91*1.1) * SLOT_MUL[s] / 100.0 * 105 / 100.0) for s in range(7)]

    # Rare suffix patterns per class (best offensive suffix):
    # WAR Bear: 60% STR / 40% VIT for armor+weapon, Stag for jewelry (40% STR / 60% VIT)
    # ROG Fox:  60% AGI / 40% VIT, Stag for jewelry
    # MAG Owl:  60% INT / 40% WIS for armor+weapon, Sage for jewelry (40% INT / 60% WIS)
    # PRI Sage: 60% WIS / 40% INT, Sage for jewelry

    for cls in range(4):
        print(f"\n=== {CLASS_NAMES[cls].upper()} FULL COMPARISON ===")

        # --- Legendary ---
        leg_tot = get_legendary_totals(cls)
        leg_dmg = calc_damage(cls, leg_tot)
        leg_hp = calc_hp(cls, leg_tot)
        leg_def = leg_tot[5] + [8,4,2,5][cls]  # base DEF
        leg_dr = min(0.50, leg_def / (leg_def + 100))

        # --- Epic Lv90 Might (A-variant) ---
        ep90_tot = get_epic90_might_totals(cls)
        ep90_dmg = calc_damage(cls, ep90_tot)
        ep90_hp = calc_hp(cls, ep90_tot)

        # --- Rare Lv91 (best offensive suffix) ---
        rare_tot = [0]*7
        pri = PRI[cls]
        if cls <= 1:  # WAR/ROG: 60% primary, 40% VIT for armor+weapon
            for slot in range(5):
                rare_tot[pri] += round(rare_budgets[slot] * 0.60)
                rare_tot[4] += rare_budgets[slot] - round(rare_budgets[slot] * 0.60)
            # Jewelry: Stag suffix — 40% primary, 60% VIT
            for slot in [5, 6]:
                rare_tot[pri] += round(rare_budgets[slot] * 0.40)
                rare_tot[4] += rare_budgets[slot] - round(rare_budgets[slot] * 0.40)
        else:  # MAG/PRI: 60% primary, 40% secondary (WIS/INT)
            sec_idx = 3 if cls == 2 else 2  # MAG sec=WIS, PRI sec=INT
            for slot in range(5):
                rare_tot[pri] += round(rare_budgets[slot] * 0.60)
                rare_tot[sec_idx] += rare_budgets[slot] - round(rare_budgets[slot] * 0.60)
            for slot in [5, 6]:
                rare_tot[pri] += round(rare_budgets[slot] * 0.40)
                rare_tot[sec_idx] += rare_budgets[slot] - round(rare_budgets[slot] * 0.40)

        rare_dmg = calc_damage(cls, rare_tot)
        rare_hp = calc_hp(cls, rare_tot)

        # Print per-slot Legendary breakdown
        for slot in range(7):
            budget = leg_budget(slot)
            if slot == 0: s = make_leg_weapon(budget, cls)
            elif slot < 4: s = make_leg_armor(budget, cls)
            elif slot == 4: s = make_leg_boots(budget, cls)
            elif slot == 5: s = make_leg_ring(budget, cls)
            else: s = make_leg_amulet(budget, cls)
            nz = ', '.join(f'{stat_labels[i]}={s[i]}' for i in range(7) if s[i])
            print(f"  Leg {SLOT_SHORT[slot]:>8}: budget={budget:>3}  {nz}  sum={sum(s)}")

        # Print totals
        nz_leg = ', '.join(f'{stat_labels[i]}={leg_tot[i]}' for i in range(7) if leg_tot[i])
        print(f"  Legendary total: {nz_leg} (budget={sum(leg_tot)})")
        nz_ep = ', '.join(f'{stat_labels[i]}={ep90_tot[i]}' for i in range(7) if ep90_tot[i])
        print(f"  Epic90 total:    {nz_ep} (budget={sum(ep90_tot)})")
        nz_rare = ', '.join(f'{stat_labels[i]}={rare_tot[i]}' for i in range(7) if rare_tot[i])
        print(f"  Rare91 total:    {nz_rare} (budget={sum(rare_tot)})")

        # Print damage comparison
        print(f"\n  Damage: Legendary={leg_dmg:.0f}  Epic90={ep90_dmg:.0f}  Rare91={rare_dmg:.0f}")
        print(f"  Leg/Epic90: {leg_dmg/ep90_dmg:.2f}x   Leg/Rare91: {leg_dmg/rare_dmg:.2f}x   Epic90/Rare91: {ep90_dmg/rare_dmg:.2f}x")
        print(f"  HP: Legendary={leg_hp}  Epic90={ep90_hp}  Rare91={rare_hp}")
        print(f"  Leg DR: {leg_dr:.2f} (DEF={leg_def})")


def print_lv90_budgets():
    print("\n=== Lv90 EPIC BUDGETS ===")
    for slot in range(7):
        b = epic_budget(90, slot)
        print(f"  {SLOT_SHORT[slot]:>8}: {b}")
    print(f"  Full set: {sum(epic_budget(90, s) for s in range(7))}")

def print_leg_budgets():
    print("\n=== LEGENDARY BUDGETS (Lv100) ===")
    for slot in range(7):
        b = leg_budget(slot)
        print(f"  {SLOT_SHORT[slot]:>8}: {b}")
    print(f"  Full set: {sum(leg_budget(s) for s in range(7))}")


# ── Main ───────────────────────────────────────────────────────────

if __name__ == '__main__':
    if '--verify' in sys.argv:
        print_lv90_budgets()
        print_leg_budgets()
        verify_all_classes()

    if '--lv90' in sys.argv:
        print("\n/* ══════ Lv90 Epic Tier (Storm Theme) ══════ */")
        print(generate_lv90_epics())

    if '--legendary' in sys.argv:
        print("\n/* ══════ Legendary Items (Rebalanced) ══════ */")
        print(generate_legendaries())

    if '--scale' in sys.argv:
        # Read data_items.c and scale all Epic lines
        fname = sys.argv[sys.argv.index('--scale') + 1] if len(sys.argv) > sys.argv.index('--scale') + 1 else 'src/data_items.c'
        with open(fname) as f:
            lines = f.readlines()
        for line in lines:
            if 'RARITY_EPIC' in line and '{' in line:
                print(scale_epic_line(line.rstrip()))
            else:
                print(line.rstrip())

    if len(sys.argv) == 1:
        print("Usage: python3 rebalance.py [--verify] [--lv90] [--legendary] [--scale <file>]")
        print("  --verify    Show budget tables and power comparisons")
        print("  --lv90      Generate Lv90 Epic tier C code")
        print("  --legendary Generate rebalanced Legendary C code")
        print("  --scale     Scale existing Epics in file by 1.25x")
