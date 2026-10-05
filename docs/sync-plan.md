# Goblin Sync & leaderboards: design plan

Goal: a Pwnagotchi-style website where goblins upload a **fun summary** (level, hoard counts,
trophies, hats, friends), with public profiles and leaderboards.

**Status (v0.5.0):** sections 1, 2, 4 and 5 are built (firmware `src/social/Sync.cpp`, server
`api/` in the network-goblin-labs repo, pages at scout.networkgoblin.dev/leaderboard). Identity
changed from the plan below: instead of QR pairing to an account, each Scout makes its own secret
key on first boot (see "Identity as built"). Account pairing (section 3) and the encounter
cross-check are still to do.

### Identity as built

- The Scout makes a random 32-byte secret (NVS, mirrored to `/scout/sync.key` on the SD card so a
  factory flash doesn't lose it). Its public **key** is the first 8 bytes of SHA-256(secret).
- Every upload is signed: `X-Goblin-Sig: HMAC-SHA256(secret, raw body)`. The first upload also
  carries the secret (`claim`), over HTTPS, so the server can register the key; after that it never
  leaves the device.
- The beacon's goblin id is broadcast to anyone nearby, so it is **not** used as an identity: someone
  could otherwise claim your goblin on the site before you do.
- A sequence number that only goes up stops replays; one sync per minute per goblin.
- Implausible numbers (level vs XP curve, impossible ratios, XP per hour) quietly hide the goblin
  from the boards.

## 1. Privacy contract (what is and isn't uploaded)

**Uploaded** (and shown publicly on the goblin's profile):

| Field | Example |
|---|---|
| goblin id (random, from the beacon) | `5a1d2b3c` |
| goblin name, hat, colour | `Grub`, `Crown`, hue 200 |
| level, XP, title | 23, 34 810, "Packet Hunter" |
| counts: Wi-Fi, 5 GHz, BLE, mesh, PANs, goblins met, encounters, areas | 1 342, 420, ... |
| achievement ids, hats unlocked, quests done | `["first_scan", ...]` |
| firmware version | `0.4.0` |

**Never uploaded:** MAC addresses, SSIDs, any salted id from the hoard files, GPS coordinates,
exploration-cell ids, timestamps of individual sightings. The device never sends what it heard,
only how much.

The design rule "the only transmission is the goblin beacon" becomes: *scanning stays passive;
the only other traffic is a sync the owner starts, to a network the owner chose.*

## 2. Getting online

- **Setup > Sync > Wi-Fi:** pick your network from a list, type its password on the on-screen
  keyboard (already built for naming). Credentials go in NVS only.
- **Sync** connects, uploads, disconnects, and goes back to passive scanning. Manual first;
  maybe "auto-sync at home" later.

## 3. Pairing a goblin to an account (QR, no typing)

1. On the website: log in, then "Add a goblin".
2. On the device: Setup > Sync > Pair. The goblin shows a QR code (the share-card code
   already does this) for `https://<site>/pair?g=<goblinId>&c=<6-digit code>`.
3. Scan it with your phone while logged in: the site links the goblin to your account.
4. On its next sync the device receives a **per-goblin secret** and stores it in NVS. Every
   later upload is signed with **HMAC-SHA256(secret, body + timestamp)**.

## 4. API sketch (v1)

```
POST /api/v1/pair/start      { goblinId, code }                       -> 201
POST /api/v1/sync            { goblinId, ts, stats{...}, sig }        -> 200 { rank, motd, secret? }
GET  /api/v1/goblin/:id      public profile
GET  /api/v1/leaderboard?board=xp|wifi|mesh|goblins|trophies&period=week|all
```

The sync body is about 1–2 KB of JSON, built from the fields in section 1.

## 5. Anti-cheat (you can't stop it, but you can make it boring)

The firmware is open source, so a determined person *can* forge uploads. Aim for "not worth it":

- **Signed uploads** (per-goblin secret): stops casual curl scripts and impersonation.
- **Plausibility checks** on the server:
  - counters only go up;
  - XP per hour since the last sync is capped;
  - ratios make sense (5 GHz ≤ Wi-Fi total; unique ≤ sightings);
  - the level matches the XP curve.
- **Encounters are witnessed twice.** When two goblins meet, *both* report it, so the server can
  cross-check them. The "goblins met" board only counts encounters both sides agree on. Hard to
  fake without a real second device.
- **Rate limits** per goblin and per IP. Implausible goblins are quietly hidden from the
  leaderboards, not banned, so cheaters get no signal to tune against.
- **Weekly boards** reset often, so one inflated profile doesn't sit at the top forever.

## 6. Website ideas

- Goblin profile pages with the share-card look, hats, trophy shelf and friends list.
- Leaderboards: XP, Wi-Fi, mesh, goblins met, trophies, hat collectors, longest streak.
- A "goblin of the week", firmware download and changelog, and links to your other projects.

## 7. Suggested order

1. Firmware: Wi-Fi credentials screen and a "Sync now" button that POSTs to a test endpoint.
2. Server: one endpoint plus a leaderboard page (small: e.g. Cloudflare Workers + D1, or a FastAPI
   + Postgres box).
3. QR pairing and signed uploads.
4. Encounter cross-checks and plausibility rules.

## 8. Hosting and cost (cheapest workable setup)

Prices as last checked; confirm on the providers' pages before you commit.

| Piece | Suggested | Cost |
|---|---|---|
| Domain | a subdomain of a domain you already own (e.g. `scout.networkgoblin.<tld>`), or a new one | $0, or ~$10-15/year |
| Website + web flasher | Cloudflare Pages (static: profile pages, leaderboards, flasher) | free tier |
| API (`/sync`, `/pair`, leaderboards) | Cloudflare Workers | free tier: 100k requests/day |
| Database | Cloudflare D1 (SQLite) | free tier: 5 GB, 100k row writes/day |
| Accounts / login | GitHub or Discord OAuth (no passwords to store) | free |

**Expected cost: $0/month plus the domain.** Rough sizing: 1,000 goblins syncing 10 times a day is
10,000 requests and writes a day, well inside the free tiers. Leaderboards can be computed every few
minutes and cached, so page views barely touch the database. The first upgrade, if it ever
gets popular, is Workers Paid at about $5/month.

Alternatives: GitHub Pages is free for the static site but has no backend, so the API would still
need Workers or a small VPS (~$4-6/month, more to look after). Supabase/Firebase free tiers also
work; Cloudflare keeps everything (site, API, DB, domain, TLS) in one place.

**Web flasher:** [ESP Web Tools](https://esphome.github.io/esp-web-tools/) puts an "Install" button on
a page. It needs HTTPS (Pages gives you that) and Chrome or Edge (Web Serial), and it uses esptool-js
under the hood: check that the version you embed supports the ESP32-C5 before relying on it. The
manifest points at `firmware.factory.bin` at offset 0, which CI already builds; a GitHub Release per
version is a free place to host the binaries.
