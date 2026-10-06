#!/usr/bin/env python3
"""Generate src/core/LootData.inc: which brand made a Wi-Fi router (from the first half of its MAC
address, the OUI, assigned to manufacturers by the IEEE) or a Bluetooth gadget (from the company ID in
its advert, assigned by the Bluetooth SIG). The brand decides what kind of loot a find is and how
rare (core/Loot.h).

    python3 tools/gen_oui.py oui.txt companies.py > src/core/LootData.inc

oui.txt is the IEEE MA-L registry in its text form (https://standards-oui.ieee.org/oui/oui.txt; the
`netaddr` Python package ships a copy at netaddr/eui/oui.txt). companies.py is the Bluetooth SIG
company list as shipped by the `bluetooth-numbers` Python package (bluetooth_numbers/_companies.py);
ids newer than that copy are in EXTRA_COMPANIES.

BRANDS is the game design: edit freely, but never change a key (they are saved on the SD card)
and only append kinds in core/Loot.h. First matching rule wins, so specific names go first.
"""
import re
import sys

# key, display name (<= 14 chars, it goes on the small screen), kind, rarity override or None,
# regex matched against the lower-cased IEEE organisation name (Wi-Fi), and optionally a regex
# matched against the lower-cased Bluetooth SIG company name.
BRANDS = [
    # pseudo-brands (no OUIs): what we can tell without a manufacturer
    ("mystery", "Mystery box", "MYSTERY", None, None),     # random (locally administered) address
    ("odd", "Odd box", "ODD", None, None),                 # a maker we don't list
    ("hotspot", "Phone hotspot", "PHONE", None, None),     # random address + a phone-ish name
    ("direct", "Wi-Fi Direct", "PRINTER", None, None),     # "DIRECT-..." printers, TVs, cameras
    ("smarttv", "Smart TV", "TV", None, None),             # "[TV] ..."
    ("fastpair", "Fast Pair kit", "AUDIO", None, None),    # Bluetooth: Google Fast Pair, no company id
    # hacker gear (core/Loot.h Hacker): Wi-Fi ones are recognised by name/address in Loot.cpp
    ("flipper", "Flipper Zero", "HACKER", None, None, r"^flipper devices"),
    ("pwnagotchi", "Pwnagotchi", "HACKER", None, None),
    ("pineapple", "Pineapple", "HACKER", None, None),
    ("deauther", "Deauther", "HACKER", None, None),

    # ISP gateways
    ("linksys", "Linksys", "HOME", None, r"linksys"),
    ("ciscospvtg", "Cisco cable", "ISP", None, r"^cisco spvtg"),
    ("arris", "ARRIS", "ISP", None, r"^arris group|^arris$"),
    ("commscope", "CommScope", "ISP", None, r"^commscope"),
    ("sagemcom", "Sagemcom", "ISP", None, r"^sagemcom"),
    ("technicolor", "Technicolor", "ISP", None, r"^technicolor"),
    ("vantiva", "Vantiva", "ISP", None, r"^vantiva"),
    ("hitron", "Hitron", "ISP", None, r"^hitron"),
    ("askey", "Askey", "ISP", None, r"^askey"),
    ("sercomm", "Sercomm", "ISP", None, r"^sercomm"),
    ("actiontec", "Actiontec", "ISP", None, r"^actiontec"),
    ("calix", "Calix", "ISP", None, r"^calix"),
    ("nokia", "Nokia", "ISP", None, r"^nokia"),
    ("zte", "ZTE", "ISP", None, r"^zte corporation"),
    ("huawei", "Huawei", "ISP", None, r"^huawei technologies"),
    ("ubee", "Ubee", "ISP", None, r"^ubee interactive"),
    ("compal", "Compal", "ISP", None, r"^compal broadband"),
    ("humax", "Humax", "ISP", None, r"^humax"),
    ("skyuk", "Sky", "ISP", None, r"^sky uk"),
    ("skyworth", "Skyworth", "ISP", None, r"skyworth"),
    ("wnc", "Wistron NeWeb", "ISP", None, r"^wistron neweb"),
    ("pegatron", "Pegatron", "ISP", None, r"^pegatron"),
    ("airties", "AirTies", "ISP", None, r"^airties"),
    ("avm", "FRITZ!Box", "ISP", None, r"^avm (audiovisuelles|gmbh)"),

    # home routers
    ("netgear", "Netgear", "HOME", None, r"^netgear"),
    ("tplink", "TP-Link", "HOME", None, r"^tp-link"),
    ("belkin", "Belkin", "HOME", None, r"^belkin"),
    ("asus", "ASUS", "HOME", None, r"^asustek|^asus network"),
    ("dlink", "D-Link", "HOME", None, r"^d-link"),
    ("tenda", "Tenda", "HOME", None, r"^tenda"),
    ("mercusys", "Mercusys", "HOME", None, r"^mercusys"),
    ("xiaomi", "Xiaomi", "HOME", None, r"xiaomi"),
    ("cudy", "Cudy", "HOME", None, r"cudy technology"),
    ("glinet", "GL.iNet", "HOME", None, r"^gl technologies"),
    ("buffalo", "Buffalo", "HOME", None, r"^buffalo\.?\s?inc"),
    ("synology", "Synology", "HOME", None, r"^synology"),
    ("zyxel", "Zyxel", "HOME", None, r"^zyxel"),
    ("mikrotik", "MikroTik", "HOME", "UNCOMMON", r"^routerboard"),

    # mesh systems
    ("eero", "eero", "MESH", None, r"^eero"),
    ("google", "Google", "MESH", None, r"^google,? inc"),
    ("plume", "Plume", "MESH", None, r"^plume design"),

    # business access points
    ("meraki", "Cisco Meraki", "BIZ", None, r"^cisco meraki"),
    ("cisco", "Cisco", "BIZ", None, r"^cisco systems"),
    ("aruba", "Aruba", "BIZ", None, r"^aruba"),
    ("hpe", "HPE", "BIZ", None, r"^hewlett packard enterprise"),
    ("ruckus", "Ruckus", "BIZ", None, r"^ruckus"),
    ("juniper", "Juniper", "BIZ", None, r"^juniper networks"),
    ("mist", "Juniper Mist", "BIZ", None, r"^mist systems"),
    ("fortinet", "Fortinet", "BIZ", None, r"^fortinet"),
    ("extreme", "Extreme", "BIZ", None, r"^extreme networks"),
    ("arista", "Arista", "BIZ", None, r"^arista network"),
    ("mojo", "Mojo", "BIZ", None, r"^mojo networks"),
    ("cambium", "Cambium", "BIZ", None, r"^cambium"),
    ("ubiquiti", "Ubiquiti", "BIZ", None, r"^ubiquiti"),
    ("engenius", "EnGenius", "BIZ", None, r"^engenius"),
    ("sophos", "Sophos", "BIZ", None, r"^sophos"),
    ("sonicwall", "SonicWall", "BIZ", None, r"^sonicwall"),
    ("watchguard", "WatchGuard", "BIZ", None, r"^watchguard technologies"),
    ("ruijie", "Ruijie", "BIZ", None, r"ruijie"),
    ("h3c", "H3C", "BIZ", None, r"h3c technologies"),
    ("grandstream", "Grandstream", "BIZ", None, r"^grandstream"),

    # printers
    ("hp", "HP", "PRINTER", None, r"^hp inc|^hewlett packard$|^hewlett-packard"),
    ("epson", "Epson", "PRINTER", None, r"^seiko epson"),
    ("canon", "Canon", "PRINTER", None, r"^canon inc"),
    ("brother", "Brother", "PRINTER", None, r"^brother industries"),
    ("lexmark", "Lexmark", "PRINTER", None, r"^lexmark"),
    ("xerox", "Xerox", "PRINTER", None, r"^xerox corp"),
    ("kyocera", "Kyocera", "PRINTER", None, r"^kyocera document"),
    ("ricoh", "Ricoh", "PRINTER", None, r"^ricoh company"),

    # phones and laptops (a real maker address on a hotspot is unusual these days)
    ("apple", "Apple", "PHONE", "RARE", r"^apple,? inc"),
    ("samsung", "Samsung", "PHONE", None, r"^samsung electronics"),
    ("motorola", "Motorola", "PHONE", None, r"^motorola mobility"),
    ("oneplus", "OnePlus", "PHONE", None, r"^oneplus"),
    ("oppo", "OPPO", "PHONE", None, r"^guangdong oppo"),
    ("vivo", "vivo", "PHONE", None, r"^vivo mobile"),
    ("huaweidev", "Huawei phone", "PHONE", None, r"^huawei device"),
    ("lgmobile", "LG phone", "PHONE", None, r"^lg electronics \(mobile"),

    # smart home
    ("amazon", "Amazon", "IOT", None, r"^amazon technologies|^amazon\.com"),
    ("sonos", "Sonos", "IOT", None, r"^sonos,? (inc|co)"),
    ("signify", "Philips Hue", "IOT", None, r"^signify|^philips lighting"),
    ("ecobee", "ecobee", "IOT", None, r"^ecobee"),
    ("nest", "Nest", "IOT", None, r"^nest labs"),
    ("chamberlain", "Chamberlain", "IOT", None, r"^the chamberlain group"),
    ("tuya", "Tuya", "IOT", None, r"^tuya smart"),
    ("lutron", "Lutron", "IOT", None, r"^lutron electronics"),
    ("irobot", "iRobot", "IOT", None, r"^irobot"),
    ("ikea", "IKEA", "IOT", None, r"^ikea of sweden"),
    ("whirlpool", "Whirlpool", "IOT", "RARE", r"^whirlpool"),
    ("miele", "Miele", "IOT", "RARE", r"^miele"),
    ("electrolux", "Electrolux", "IOT", "RARE", r"^electrolux"),
    ("bose", "Bose", "IOT", None, r"^bose corporation"),
    ("peloton", "Peloton", "IOT", "RARE", r"^peloton"),

    # TVs and streamers
    ("roku", "Roku", "TV", None, r"^roku"),
    ("vizio", "Vizio", "TV", None, r"^vizio"),
    ("tcl", "TCL", "TV", None, r"^tcl |^shenzhen tcl"),
    ("lg", "LG", "TV", None, r"^lg electronics( inc)?$"),
    ("hisense", "Hisense", "TV", None, r"hisense"),
    ("sony", "Sony", "TV", None, r"^sony (corporation|visual|home)"),
    ("nvidia", "NVIDIA", "TV", "RARE", r"^nvidia"),

    # game consoles
    ("nintendo", "Nintendo", "CONSOLE", None, r"^nintendo"),
    ("playstation", "PlayStation", "CONSOLE", None, r"^sony (interactive|computer) entertainment"),
    ("microsoft", "Microsoft", "CONSOLE", None, r"^microsoft( corporation)?$"),
    ("valve", "Valve", "CONSOLE", "EPIC", r"^valve corporation"),

    # cars
    ("tesla", "Tesla", "CAR", "EPIC", r"^tesla,? ?inc"),
    ("harman", "Harman car", "CAR", None, r"^harman/becker"),
    ("continental", "Continental", "CAR", None, r"^continental automotive"),
    ("alpsalpine", "Alps Alpine", "CAR", None, r"^alps alpine"),
    ("visteon", "Visteon", "CAR", None, r"visteon"),
    ("denso", "Denso", "CAR", None, r"^denso"),
    ("panasonicauto", "Panasonic car", "CAR", None, r"^panasonic automotive"),
    ("vw", "Volkswagen", "CAR", None, r"^volkswagen"),
    ("bmw", "BMW", "CAR", None, r"^bmw ag"),
    ("toyota", "Toyota", "CAR", None, r"toyota"),
    ("polestar", "Polestar", "CAR", "EPIC", r"^polestar"),
    ("lucid", "Lucid", "CAR", "EPIC", r"^lucid motors"),
    ("ford", "Ford", "CAR", None, r"^ford motor company|^ford$"),

    # cameras and drones
    ("dji", "DJI drone", "CAMERA", "EPIC", r"dji technology"),
    ("gopro", "GoPro", "CAMERA", None, r"^gopro"),
    ("parrot", "Parrot drone", "CAMERA", "EPIC", r"^parrot sa"),
    ("hikvision", "Hikvision", "CAMERA", None, r"hikvision"),
    ("dahua", "Dahua", "CAMERA", None, r"dahua technology"),
    ("verkada", "Verkada", "CAMERA", None, r"^verkada"),
    ("axis", "Axis", "CAMERA", None, r"^axis communications"),
    ("ring", "Ring", "CAMERA", None, r"^ring llc"),
    ("blink", "Blink", "CAMERA", None, r"^blink by amazon"),
    ("wyze", "Wyze", "CAMERA", None, r"^wyze"),
    ("arlo", "Arlo", "CAMERA", None, r"^arlo technology"),
    ("ezviz", "EZVIZ", "CAMERA", None, r"ezviz"),
    ("reolink", "Reolink", "CAMERA", None, r"^reolink"),

    # satellite
    ("starlink", "Starlink", "SAT", None, r"^space exploration technologies"),

    # mobile routers (buses, trains, police cars, food trucks)
    ("cradlepoint", "Cradlepoint", "MOBILE", None, r"^cradlepoint"),
    ("sierra", "Sierra", "MOBILE", None, r"^sierra wireless"),
    ("peplink", "Peplink", "MOBILE", None, r"^peplink"),
    ("teltonika", "Teltonika", "MOBILE", None, r"teltonika"),
    ("novatel", "Novatel MiFi", "MOBILE", None, r"^novatel wireless"),

    # shops: card terminals, scanners, tills
    ("verifone", "Verifone", "RETAIL", None, r"^verifone"),
    ("ingenico", "Ingenico", "RETAIL", None, r"ingenico"),
    ("pax", "PAX", "RETAIL", None, r"^pax computer"),
    ("clover", "Clover", "RETAIL", None, r"^clover network"),
    ("toast", "Toast", "RETAIL", None, r"^toast, inc"),
    ("zebra", "Zebra", "RETAIL", None, r"^zebra technologies"),
    ("honeywell", "Honeywell", "RETAIL", None, r"^honeywell$"),

    # tinkerers' boards
    ("espressif", "Espressif", "DIY", None, r"^espressif"),
    ("raspberrypi", "Raspberry Pi", "DIY", "RARE", r"^raspberry pi"),
    ("arduino", "Arduino", "DIY", "RARE", r"^arduino"),

    # factories and buildings
    ("siemens", "Siemens", "INDUSTRIAL", None, r"^siemens ag"),
    ("rockwell", "Rockwell", "INDUSTRIAL", None, r"^rockwell automation"),
    ("schneider", "Schneider", "INDUSTRIAL", None, r"^schneider electric"),
    ("moxa", "Moxa", "INDUSTRIAL", None, r"^moxa"),
]


# Bluetooth company names for brands above (by key), and Bluetooth-only brands below.
BT = {
    "apple": r"^apple, inc", "samsung": r"^samsung electronics", "google": r"^google$", "microsoft": r"^microsoft$",
    "amazon": r"^amazon\.com services", "sonos": r"^sonos inc", "bose": r"^bose corporation", "tesla": r"^tesla, inc",
    "xiaomi": r"^xiaomi inc", "signify": r"^signify netherlands|^koninklijke philips", "nest": r"^nest labs",
    "ecobee": r"^ecobee", "tuya": r"tuya information", "wyze": r"^wyze", "lg": r"^lg electronics",
    "sony": r"^sony corporation", "nintendo": r"^nintendo", "valve": r"^valve corporation", "dji": r"dji technology",
    "gopro": r"^gopro", "espressif": r"^espressif", "oneplus": r"^oneplus", "oppo": r"oppo mobile",
    "huawei": r"^huawei technologies", "motorola": r"^motorola$", "peloton": r"^peloton", "bmw": r"^bayerische motoren",
    "ford": r"^ford motor", "epson": r"^seiko epson", "canon": r"^canon inc", "brother": r"^brother industries",
    "zebra": r"^zebra technologies", "verifone": r"^verifone", "clover": r"^clover network",
}
BRANDS += [
    # Bluetooth-only brands
    ("garmin", "Garmin", "WEARABLE", None, None, r"^garmin international"),
    ("fitbit", "Fitbit", "WEARABLE", None, None, r"^fitbit"),
    ("huami", "Amazfit", "WEARABLE", None, None, r"huami"),
    ("polar", "Polar", "WEARABLE", None, None, r"^polar electro"),
    ("suunto", "Suunto", "WEARABLE", "RARE", None, r"^suunto"),
    ("oura", "Oura ring", "WEARABLE", "RARE", None, r"^oura health"),
    ("withings", "Withings", "WEARABLE", None, None, r"^withings"),
    ("meta", "Meta", "WEARABLE", "RARE", None, r"^meta platforms"),
    ("beats", "Beats", "AUDIO", None, None, r"^beats electronics"),
    ("harmanbt", "JBL / Harman", "AUDIO", None, None, r"^harman international|^harman co"),
    ("gnaudio", "Jabra", "AUDIO", None, None, r"^gn audio"),
    ("bangolufsen", "B&O", "AUDIO", "RARE", None, r"^bang & olufsen"),
    ("sennheiser", "Sennheiser", "AUDIO", None, None, r"^sennheiser"),
    ("audiotechnica", "Audio-Technica", "AUDIO", None, None, r"^audio-technica"),
    ("shure", "Shure", "AUDIO", "RARE", None, r"^shure"),
    ("skullcandy", "Skullcandy", "AUDIO", None, None, r"^skullcandy"),
    ("anker", "Anker", "AUDIO", None, None, r"^anker innovations"),
    ("nothing", "Nothing", "AUDIO", None, None, r"^nothing technology"),
    ("teenage", "Teenage Eng.", "AUDIO", "EPIC", None, r"^teenage engineering"),
    ("logitech", "Logitech", "CONSOLE", None, None, r"^logitech"),
    ("razer", "Razer", "CONSOLE", None, None, r"^razer"),
    ("tilebt", "Tile", "TRACKER", None, None, r"^tile, inc"),
    ("chipolo", "Chipolo", "TRACKER", None, None, r"^chipolo"),
    ("eve", "Eve", "IOT", None, None, r"^eve systems"),
    ("nanoleaf", "Nanoleaf", "IOT", None, None, r"^nanoleaf"),
    ("august", "August lock", "IOT", "RARE", None, r"^august home"),
    ("yale", "Yale", "IOT", "RARE", None, r"^yale$|^assa abloy"),
    ("nespresso", "Nespresso", "IOT", "RARE", None, r"nespresso"),
    ("rivian", "Rivian", "CAR", "EPIC", None, r"^rivian"),
    ("bird", "Bird scooter", "CAR", None, None, r"^bird rides"),
    ("nordic", "Nordic chip", "DIY", None, None, r"^nordic semiconductor"),
    ("adafruit", "Adafruit", "DIY", "RARE", None, r"^adafruit"),
    ("ruuvi", "Ruuvi", "DIY", "RARE", None, r"^ruuvi"),
    ("square", "Square", "RETAIL", None, None, r"^square, inc"),
]
# Company ids newer than the bluetooth-numbers copy (checked against the Bluetooth SIG list).
EXTRA_COMPANIES = {0x0E29: "Flipper Devices Inc."}


def main(oui_path, bt_path):
    keys = set()
    for b in BRANDS:
        assert b[0] not in keys, b[0]
        assert len(b[1]) <= 14, b[1]
        keys.add(b[0])
    for k in BT:
        assert k in keys, k
    rows = [(b[0], b[1], b[2], b[3], b[4], b[5] if len(b) > 5 else BT.get(b[0])) for b in BRANDS]

    # Wi-Fi: IEEE OUIs
    rules = [(i, re.compile(r[4])) for i, r in enumerate(rows) if r[4]]
    table = {}
    used = [0] * len(rows)
    for line in open(oui_path, encoding="utf-8", errors="replace"):
        m = re.match(r"([0-9A-F]{2})-([0-9A-F]{2})-([0-9A-F]{2})\s+\(hex\)\s+(.*)", line)
        if not m:
            continue
        org = m[4].strip().lower()
        for i, rx in rules:
            if rx.search(org):
                table[int(m[1] + m[2] + m[3], 16)] = i
                used[i] += 1
                break
    for i, r in enumerate(rows):
        if r[4] and not used[i]:
            sys.exit(f"brand {r[0]} matched nothing in the IEEE registry")

    # Bluetooth: SIG company ids
    companies = {int(m[1], 16): m[2] for m in re.finditer(r'(0x[0-9A-Fa-f]{4}):\s*"([^"]*)"', open(bt_path).read())}
    companies.update(EXTRA_COMPANIES)
    brules = [(i, re.compile(r[5])) for i, r in enumerate(rows) if r[5]]
    bt = {}
    bused = [0] * len(rows)
    for cid, name in companies.items():
        for i, rx in brules:
            if rx.search(name.lower()):
                bt[cid] = i
                bused[i] += 1
                break
    for i, r in enumerate(rows):
        if r[5] and not bused[i]:
            sys.exit(f"brand {r[0]} matched no Bluetooth company")

    out = ["// Generated by tools/gen_oui.py from the IEEE OUI registry and the Bluetooth SIG company list.",
           f"// Don't edit by hand. {len(table)} OUIs, {len(bt)} Bluetooth companies, {len(rows)} brands.",
           "const Brand kBrands[] = {"]
    for key, name, kind, rarity, _, _ in rows:
        r = "R_" + rarity if rarity else "R_KIND"
        out.append(f'    {{"{key}", "{name}", K_{kind}, {r}}},')
    out.append("};")

    def emit(name, entries, comment):
        out.append(comment)
        out.append(f"const uint32_t {name}[] = {{")
        row = []
        for code in sorted(entries):
            row.append(f"0x{(code << 8) | entries[code]:08x}")
            if len(row) == 8:
                out.append("    " + ", ".join(row) + ",")
                row = []
        if row:
            out.append("    " + ", ".join(row) + ",")
        out.append("};")

    emit("kOui", table, "// (OUI << 8) | brand index, sorted: binary search.")
    emit("kCompany", bt, "// (Bluetooth company id << 8) | brand index, sorted.")
    print("\n".join(out))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
