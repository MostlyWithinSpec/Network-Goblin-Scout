# Goblin Sync & leaderboards: design plan

Goal: a Pwnagotchi-style website where goblins upload a **fun summary** (level, hoard counts,
trophies, hats, friends), with public profiles and leaderboards. Nothing here is built yet;
this is the plan to agree on before writing firmware or server code.

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
