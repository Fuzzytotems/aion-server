# M5b-2 and M5b-3 real-client session, 2026-09-24

The first session against abilities and loot with a real 4.8 client. The server was built from commit 4867fbc44 (M5b-3 stages 0 and 1) in a
separate worktree (`D:/aion-server-wt/session`), so the session ran while the M5b-3 stage-2 gate pipeline kept using the shared build tree; the
gates use ephemeral ports and their own schemas, so the two did not meet. Profile: `m5b3.properties.example` with Java's default drop rates,
soul sickness on (`gameserver.soulsickness.disable = 10`) and events disabled. The user played the Warrior `Iziseki`, the Mage `Fuzzytotem`
and a new Scout `Flight`.

## What the session proves

| | |
|---|---|
| Startup | "Game server started in 143 seconds" with geodata on, connected to the login server, **0 ERROR lines for the whole session** |
| Abilities (M5b-2 §11) | Ferocious Strike with its chain indicator and the passives in the stats window; Flame Bolt's 2 s cast bar and MP, cancelled by moving with no MP spent; Root's 20 s debuff; a Scout's Stealth ending on its timer, on attack and on damage; striped kerubs casting Brandish on the character; death, bind revive and soul sickness - **all confirmed by the user** |
| Loot and items (M5b-3 §11) | Corpses sparkle and vanish once emptied; kinah from kerubs; unequip and re-equip restoring the attack exactly; split, swap and destroy; the Minor Life Potion's heal and its cooldown; the Administrator's Boon; the Poeta camp fire's damage every 5 s - **all confirmed** |
| Godstone | A test earth godstone (168000116) inserted into the Mage's cube while she was logged out, socketed into her spellbook (2 s bar), and **"triggering for 2 damage"** on her auto-attacks |
| E-13 in live play | The Mage's expired 3-day pass was deleted at her login: `Deleted 164002039 Administrator's Boon – 3-Day Pass ... (deletion type: DEFAULT)`, no ERROR (the ItemPacketService fix of 706dc55c1) |

## Findings

**S-1. One Npc leaked.** `LeakCensus` warned at 20:15:13 that `Npc (object id 25582)` was still alive 10 minutes after its removal from the world
(refcount 1, no task pinning it), and the shutdown hook reported it again with no player online: `refCount 1, removed 1385 s ago`. It left the world
at about 20:05:13, while the Mage was fighting and looting striped kerubs; nothing was logged at that moment, which fits a kill with no drop whose
corpse decays about 2 s later. The gates kill npcs the same way with a clean census, so the leaking path is one only live play took. This is the
same class of find as the M5b-1 session's leaked Player.

*Diagnosis (2026-09-25, docs/deviations/P4-10.md "M5b-3 client session leak S-1").* The log does not say which npc 25582 was, so the cause is
the likeliest one, not a proven one. Ruled out: the skill, effect, task and player paths of a kill (22 lifetime tests), and the zone and region
races a first change closed (every spot of the hunting area lies inside AKARIOS_PLAINS, and a decaying corpse does not move) - that change stays
as hardening plus a diagnostic. Reproduced: the lone walker of the pool-2 route 3805A343, a juvenile sparkie at 1134.95/1012.56 inside the
hunting area, kept its first npc in `InstanceWalkerFormations.groupedSpawnObjects` for good, so its first ordinary kill left the corpse at
refcount 1 with no task until shutdown; Java keeps the same reference but never reads it after the startup `organizeAndSpawn`, which now drops
the list (`WalkerGroupLifetimeTest`; 125 such lone walker spots exist across the spawn files). The sparkie never respawns after its first kill -
Java's own bug, kept. Group members (F912DD8B, 6C27EAF1: a sparkie and a striped kerub) stay referenced while their group reads them, as in Java.
A holder probe (`world::WorldLeakProbe`) that names the npc (template id, position) and every structure still holding it, or what it searched
and could not, runs from the census through header request m5b3-leak-h01, approved and applied with the leak change. To check in the
next session: the sparkie at 1134.95/1012.56 stays away after its first kill without a census report for its corpse, and any report that still
comes carries the probe's holder lines.

**S-2. Two client packets the user sent are not ported** - both expected: `CM_CHAT_MESSAGE_PUBLIC` (typing in chat; in-game chat is
m5j-plan.md stage 0, which that plan recommends pulling forward) and `CM_SHOW_DIALOG` (talking to an npc; npc dialogs are M5c).

## Not exercised

Group play, a second race, gliding, merchants and quests (M5c, M5d), skills above level 10 and class change (M5e), and GM commands (M5j stage 0) -
the godstone had to be seeded through the database because chat commands are unported.
