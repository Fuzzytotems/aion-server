# M5a real-client session, 2026-09-21

The first session of a real Aion 4.8 client against the C++ server (m5a-plan.md §8). Recorded here because it is the only evidence that covers
the paths the automated gate cannot reach, and because two of its findings define stage 3 work.

## Setup

| Piece | Value |
|---|---|
| Server build | Debug, commit `7ce0f3b3f` (tag `m5a-gate`) |
| Config | `game-server/config/mygs.properties`, copied from `m5a.properties.example`; **geo enabled** |
| Database | local MariaDB 11.8, `aion_ls` / `aion_gs`, account `Fuzzytotems` (id 2, access level 0) |
| Client | `C:\Aion`, `start.bat` (`-ip:127.0.0.1 -port:2106 -loginex`) |
| Startup | 144 s, 83,872 npc spawns, 64 startup steps |

## What the session proved

| Checklist step | Result |
|---|---|
| 1-3 servers up, no ERROR at startup | 13 npc spawns failed, see F-1 below; everything else clean |
| 4 login, right and wrong password | passed: wrong password denied, right password authed |
| 4 server list | passed: game server 1 online and selectable |
| 5 character creation | passed: Elyos mage `Fuzzytotem` created |
| 6 enter world | passed: Poeta (`world_id` 210010000), plausible stats |
| 7-8 look around, walk | passed by eye: npcs visible, movement clean, no rubber-banding, no ERROR |
| 9 idle | passed: 17 minutes connected, game time ticked, no ping disconnect |
| 10-11 relog and full client restart | passed: same character, position persisted (1130.97 / 1056.35 / 134.59) |
| 12 second race | not exercised |
| 13 Ctrl+C with a character online | not exercised (the gate covers the synthetic case) |

Server-side evidence over 25 minutes of play: **0 ERROR lines and 0 AION_UNPORTED hits after startup**, idle CPU 1 % of one core, 3.2 GB resident
in a Debug build, 5 connect/disconnect cycles without a stale session.

## Findings

**F-1 (fixed in this wave). 13 npc spawns failed because `SiegeShield::onEnterZone` was unported.** 7 in Verteron (210050000) and 6 in Reshanta
(400010000): each npc that spawns inside a fortress shield runs the zone handler, whose body threw `UnportedException`, and `VisibleObjectSpawner`
logged "Error during spawn" followed by "did not leave world cleanly". Java does nothing at all for a non-Player creature there
(SiegeShield.java:37-45), so the fix is the faithful body plus the `DespawnableNode` id assignment that `setSiegeLocationId` had also dropped.
A geo-enabled start after the fix logs 0 spawn errors, 0 ERROR lines and 0 unported hits.

The class of bug matters more than the bug: `gs.scenario.m5a` and `gs.smoke.startup` both run with `gameserver.geodata.enable=false`, so **no zone
handler ever fires in an automated run**. m5a-plan.md §"Geodata" predicted exactly this ("A geo-only crash, hang or wrong spawn z leaves
gs.scenario.m5a green and hits the user on his first walk"). Stage 3 owes a geo-enabled startup gate.

**F-2. Nine client packets are not ported yet**, each logged once by `AionClientPacketFactory` while playing: `CM_TARGET_SELECT`, `CM_EMOTION`,
`CM_USE_ITEM`, `CM_MOVE_ITEM`, `CM_FRIEND_STATUS`, `CM_SHOW_BLOCKLIST`, `CM_PLAYER_LISTENER`, `CM_INSTANCE_INFO`, `CM_CHECK_PAK`. Tab targeting and
clicking a mob therefore look right in the client and are invisible to the server: the target frame is drawn client-side and no server state
changes. This is the M5b entry list, ordered by what a player touches first.

**F-3. One AION_PARTIAL was reached in play**: `SkillEngine::applyEffectDirectly` (passive skill effects are not applied, M5a O-09).

## Not covered by this session

Second race and account, gliding and jumping, gather nodes, a shutdown with a character online, and everything the milestone excludes anyway
(combat, quests, dialogs, chat server, reacting monsters).
