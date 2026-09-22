# M5a work plan (wave 5a)

> **Status:** plan rev 2, 2026-09-15. It comes from five static traces (login, create, enter, world, quit) over HEAD `phase-4`, revised after a
> completeness review (§9). Nothing was compiled or run.
> It refines [handlers-and-porting-plan.md](handlers-and-porting-plan.md) §2.8, §2.9 and amendments §10/§11. Ownership follows
> `tools/porting/chunks.py`. Header changes follow [hub-headers.md](hub-headers.md) §14.

## 1. Summary

Everything below the client packets is ported: AionConnection, crypt, the factory, the LS link, the DAOs, world, knownlists, SpawnEngine and the
239 SM packets. Three things block M5a:

1. **No client packet classes.** The registry has 0 markers, and `main.cpp` stops after World (no NioServer, no LS link).
2. **Player and NPC construction throws.** `PlayerGameStats` and `NpcLifeStats` reach unported stat bodies (P5-01), and `AIEngine.newAI` has
   no warn fallback (P5-05). So `spawnAll`, character creation and enter world all fail on their first object.
3. **About 150 small bodies in chunks planned for 5b** sit on the startup, login, creation, enter-world, spawn and logout paths. Many of them
   are singleton constructors that `getInstance()` runs even where config or level makes the Java call return early.

In stage 1, six lanes port the **M5a subset** of their chunks: disabled-config branches, empty-registry paths, and the paths of a fresh level-1
character. Stage 2 finishes the startup wiring and makes the scenario gate pass. Six lanes own every chunk a scenario failure can point to.
Stage 3 adds the stress nightly and the fixes found with the real client. Revision 2 adds these items:
- the event profile
- a final census and live-instance counters
- LogoutBreakers on enter-world rejection paths
- independent decoders for the packets the client parses strictly
- shutdown with a player online
- in-world CMs that need no service
- real dependencies for the GameServer wiring
- owners for fixes in stage 2 and stage 3

## 2. Decisions

| # | Decision | Why |
|---|---|---|
| D1 | **M5a profile.** The scenario and the real client use the same `-D` set: `gameserver.dev.missing_ai_handlers=warn`, `gameserver.siege.enable=false`, `gameserver.autogroup.enable=false`, `gameserver.rift.enable=false`, `gameserver.vortex.enable=false`, `gameserver.worldraid.enable=false`, `gameserver.cp.enable=false`, `gameserver.limits.enable=false`, **`gameserver.event.service.disabled_events=*`**. The scenario adds `gameserver.geodata.enable=false`, `gameserver.character.reentry.time=1`, `gameserver.shutdown.delay=2`, test ports and test schemas. | The profile takes Influence/SM_INFLUENCE_RATIO, SiegeService.onPlayerLogin, SM_AUTO_GROUP, rift spawns and the CP tasks off the path. `custom_events.xml` "Beyond Aion Server Buffs" has no dates, so it is always active; its ENTER_MAP buffs would add effect and stat packets, and one buff depends on the date. With `*`, `EventService.collectActiveEvents` returns an empty set (EventService.java:133). The expected Java order is taken **under the same profile**, so parity holds. The disabled branches sit inside unported bodies and still have to be ported. |
| D2 | The 43 root AI handlers are **not registered** at M5a. Every NPC gets DummyAI through `missing_ai_handlers=warn`. The handlers move to 5b. | An NpcAI reacts to ACTIVATE, SEE and MOVED, which needs walking and aggro (M5b). |
| D3 | **Stub-with-warning** uses the additive `AION_PARTIAL("reason")`: log once, count in `partial_trace.txt`, return normally. `AION_UNPORTED` still throws. | The gate requires 0 `AION_UNPORTED` hits and partial sites ⊆ `tests/scenario/m5a_partial_allowlist.txt`. |
| D4 | The LS and the GS run as **child processes**. DB rows are checked with direct `commons::database::Connection::open`. The LS is stopped with CTRL_BREAK to its own process group, so the LS source does not change. | `DatabaseFactory` is process-wide, network-5 (LS linked into GS tests) was rejected, and `RuntimeLifecycle` runs once per process. The C++ LS console handler already handles `CTRL_BREAK_EVENT` (LoginServer.cpp:113-158). |
| D5 | **Two accounts:** Elyos Warrior on account A, Asmodian Mage on account B. A second race on A expects RESPONSE_OTHER_RACE (12). | `character.creation.mode=0` stays at the Java default. |
| D6 | Persistence is proven by `CM_QUIT(0)` plus a new LS and GS login. The shutdown save is proven by stopping the GS with B still in the world (Q7). | A reload from the DB happens only on a new login; ShutdownHook takes its fast exit when no player is online (ShutdownHook.java:50). |
| D7 | P5-15/P5-16 are not a 5a lane. **Stage 2 leases the seven in-world CMs that need no service** (C-01). CMs with services (targeting, chat, emotes, CM_CHARACTER_EDIT, CM_PLAYER_STATUS_INFO) log "not ported yet". The set of classes seen is written to `m5a_summary.txt`. | CM_SUBZONE_CHANGE (revalidateZones) and CM_CUSTOM_SETTINGS (settings) never stall the client, so their absence would go unnoticed. |
| D8 | **Lifetime gates cannot pass vacuously.** Check-output mode runs a final census after the ShutdownHook logout (censusAfter 0, drain, `reclaimNow`) and compares debug live-instance counters for Player/Item/PlayerCommonData/Account/storages with the baseline. | The census thresholds are 10 min and 30 min (RuntimeConfig.cpp:13-14), and objects outside World (items, parts, transient Players) are never tracked. |
| D9 | **Wire bodies are checked by independent decoders** written from the Java `writeImpl` (not from the C++ packets) for the packets the 4.8 client parses strictly. Each decoder must consume the body exactly. | FakeGameClient decodes with the server's own code, so a symmetric width or order error would stay invisible and show up only as an endless loading screen. |

### Changes to the design's wave 5a grouping

| Design §10 | Plan | Why (trace evidence) |
|---|---|---|
| P5-00 alone | P5-00 + P4-15 | The slice edits `AionConnection.cpp` (`CLIENT_PING_INTERVAL`, shutdown stand-ins). |
| P5-01+P5-02 one agent | Same, plus the P4-05 and P4-12 leftovers | `createStatsTemplate`, the equipment/title listener calls and `GameTime::onHourChange` have no other owner. |
| P5-05 AI, P5-06 quest (two agents) | **One lane**: P5-05 + P5-06 + P5-13 + P4-01/P4-10/P4-11a/P4-11b + the spawn oracle | At M5a each engine needs only dispatch or empty-registry hooks, called from the spawn, zone and knownlist code this lane also owns. The spawn and border oracle belongs to the visibility work. |
| P5-07/08/10/12a/12b in 5b | **Pulled forward** as one M5a-subset lane | Login (HDDBan), creation (ItemFactory, SkillLearn), enter (Warehouse, Kisk, bind point, AbyssRankingCache, Siege/Vortex/Rift/CP/Panesterra, Event) and logout (FindGroup, Recall, Duel, team services) all throw today. |
| P5-09/P5-11 in 5b-2 | **Pulled forward** as one M5a-subset lane | AtreianPassport breaks the first SM_VERSION_CHECK. Broker and Legion break SM_CHARACTER_LIST. Housing breaks spawnAll and storePlayer. Town breaks SM_NPC_INFO. |
| P5-14 misc | P5-14 + P4-17 + new test chunk **P5-SC** (`tests/scenario`) | The process harness needs no F-01/F-03 file, so it starts on day 1. The GameServer wiring is split: the skeleton is in stage 1, the completion in stage 2 (F-01 depends on every service lane). |
| P5-15/P5-16 in 5b | Seven no-service CMs leased in stage 2 (C-01) | See D7. |

## 3. Work items (deduplicated; flows L=login, C=create, E=enter, W=world, Q=quit)

Effort: S < 1 agent-day, M 1-2, L 2-4, XL > 4. Need: **R** required, **W** stub-with-warning (`AION_PARTIAL`) allowed, **O** optional.

### Integrator

| Id | What | Deps | Flows | Need | Eff |
|---|---|---|---|---|---|
| I-01 | Header request batch (§6) and manifest rows: `game-server/tests/scenario` → new chunk P5-SC (`aion_gs_scenario_tests`); `game-server/cmake/RunStartupSmoke.cmake` → P5-14; include path to `login-server/tests/support`; the M5a profile file `config/m5a.properties.example`. | – | all | R | S |
| I-02 | Full verification of the phase-4 tree (carried open issue: wave 3b-2 was not verified as a whole), if not already done for tag `phase-4`. | – | all | R | S |
| I-03 | P4-02b additive runtime support: debug live-instance counters by dynamic type (incremented in `create<T>`, decremented in the Reclaimer destroy observer), `liveCounts()` snapshot. If `LeakCensus::configure` + `Reclaimer::reclaimNow` + `getLeaks` cannot report on demand: `LeakCensus::censusNow()`. | – | Q | R | S |
| I-04 | Stages 2-3, on demand: fixes in `cpp/login-server` and `cpp/commons` (NioServer, DatabaseFactory, LS link) through the freeze-exception process (spine-status.md); file leases for R-01. | – | L,Q | O | S |

### P5-00 slice (+P4-15)

| Id | What | Java refs | Deps | Flows | Need | Eff |
|---|---|---|---|---|---|---|
| S-01 | Login CM classes with `AION_CLIENT_PACKET`: CM_VERSION_CHECK, CM_TIME_CHECK, CM_L2AUTH_LOGIN_CHECK, CM_MAC_ADDRESS (fixHddSerial with UTF-16 semantics), CM_CHARACTER_LIST, CM_MAY_LOGIN_INTO_GAME, CM_PING (`CLIENT_PING_INTERVAL` replaces the TODO; null-safe audit), CM_GAMEGUARD, CM_SECURITY_TOKEN, CM_RECONNECT_AUTH, CM_CHARACTER_PASSKEY (keeps NUL padding), CM_DISCONNECT, CM_UI_SETTINGS | clientpackets/CM_*.java | I-01 | L,Q | R (PASSKEY/RECONNECT: O) | M |
| S-02 | AccountService: getAccount, loadAccount, loadPlayerAccountData(Account&, id), loadAccountWarehouse, removeDeletedCharacters | AccountService.java:33-95 | I-01 | L,C,Q | R | S |
| S-03 | AbstractCharacterEditPacket, CM_CHECK_NICKNAME, CM_CREATE_CHARACTER (validateBasicInfo with the off-by-one kept), CM_DELETE_CHARACTER, CM_RESTORE_CHARACTER. The transient Player from newPlayer is released on every exit (success, validation error, DB error). | CM_CREATE_CHARACTER.java:33-79 | S-04 | C | R (DELETE/RESTORE: O) | M |
| S-04 | PlayerService: isNameUsedOrReserved x2, newPlayer (`create<T>`), storeNewPlayer, storeCreationTime, deletePlayer, cancelPlayerDeletion, storeDeletionTime, deletePlayerFromDB x2, getPlayerName | PlayerService.java:54-317 | B-02, B-05, E1-01, E1-03, F-03 | C | R | M |
| S-05 | CM_ENTER_WORLD; enterWorld(client, id) with Java's catch rollback; PlayerService.getPlayer. **C++ lifetime additions:** inside getPlayer, a scope guard after `accountWarehouse.setOwner(player)` runs `LogoutBreakers::run` if a DAO load throws. In enterWorld, a guard opened right after getPlayer returns runs `LogoutBreakers::run(player)` unless enterWorld(client, player) completed. This covers the multi-client reject (SM_ENTER_WORLD_CHECK), the duplicate enter (no packet, PlayerEnterWorldService.java:159) and the catch. | PlayerEnterWorldService.java:89-179; PlayerService.java:102-175; LogoutBreakers.h "Callers" | B-02, E2-04, S-02 | E | R | M |
| S-06 | enterWorld(client, player) plus updateEnergyOfRepose, activatePassiveSkillEffects (effects `AION_PARTIAL`), validateFortressZone, validateVortexZone, sendItemInfos, sendWarehouseItemInfos, sendMacroList; GeneralUpdateTask/ItemUpdateTask | PlayerEnterWorldService.java:181-489 | S-05, E1-*, E2-*, W-02, W-03 | E | R (tasks: O) | L |
| S-07 | CM_LEVEL_READY | CM_LEVEL_READY.java:64-115 | S-06, W-01, B-04, B-06 | E,W | R | S |
| S-08 | CM_MOVE (readImpl, runImpl, handleBogusPacket, notifyControllers) | CM_MOVE.java:77-188 | B-03, B-06, F-03 | W | R | S |
| S-09 | CM_QUIT; PlayerLeaveWorldService.leaveWorld (the LogoutBreakers guard comes first) and leaveWorldDelayed; PlayerService.storePlayer | PlayerLeaveWorldService.java:52-63; PlayerService.java:80 | B-03, B-05, B-06, W-02, W-03, E1-02, E1-04, E1-05, E2-02, E2-03, E2-04, E2-05, F-03 | Q | R | M |
| S-10 | `tests/login_slice`: CM readImpl byte vectors; fixHddSerial vectors; AccountService and storeNewPlayer/storePlayer on the test schema; breakers still run after a DAO exception during logout; logout during item use. **Destroy-observer tests** (weak ref or a destroy observer): the Player is destroyed after (a) a multi-client reject, (b) a duplicate enter, (c) a DAO exception injected into getPlayer after setOwner, (d) CM_CREATE_CHARACTER success and DB error. CM flow tests in-process over `tests/network/support/GameServerTestServer.h`, so the slice need not wait for GameServer::main. | – | S-02..S-09 | L,C,E,Q | R | M |
| S-11 | Stage 2: the scenario flows of §5 | – | F-01b, F-04, F-05, F-08, all of stage 1 | all | R | L |
| S-12 | Stage 2: replace the stand-ins `isGameServerShuttingDownSoon` / `isShutdownScheduled` (AionConnection.cpp:51, LoginServerConnection.cpp:25, ChatServerConnection.cpp:25) with GameServer.h calls | GameServer.java:244 | F-01a | Q | R (Q7 needs them) | S |

### P5-01/P5-02 stats and skills (+P4-05, P4-12)

| Id | What | Deps | Flows | Need | Eff |
|---|---|---|---|---|---|
| B-01 | Stat core: Stat2 getters, AdditionStat (new), StatCapUtil (new, rule table); CreatureGameStats getStat x2, applyStatFunctions, getStatsSorted, getMaxHp/Mp/Dp, getPower..getWill, getMovementSpeedFloat, getAttackSpeed; NpcGameStats getStatsTemplate, applyStatFunctions, getMovementSpeed | – | C,E,W | R | L |
| B-02 | Player stats: PlayerStatCalculator (new), PlayerStatsTemplate, P4-05 PlayerClassInfo::createStatsTemplate; PlayerGameStats ctor, updateStatsTemplate, getFlyTime, updateStatsVisually, updateStatInfo, speed; PlayerStatFunctions.addPredefinedStatFunctions (new); StatFunctions adjustSpeedByMovementModifier/calculateFallDamage | B-01 | C,E,W | R | L |
| B-03 | Life stats: CreatureLifeStats getMaxHp/Mp, isDead, getHpPercentage, setCurrentHp/Mp/HpPercent, onHpChanged, synchronizeWithMaxStats, updateCurrentStats, reduceHp, cancelAllTasks, cancelRestoreTask; PlayerLifeStats setCurrentFp, cancelFp*, updateCurrentStats, **triggerRestoreTask/triggerFpRestore (R, so Q3 creates a real pin)** | B-01, E1-04 | C,E,W,Q | R | M |
| B-04 | ItemEquipmentListener.onItemEquipment (+ the P4-12 call at Equipment.cpp:82), TitleChangeListener (+P4-12 TitleList), AttackUtil cancelCastOn/removeTargetFrom, AggroList remove x2/clear | B-01 | E,W,Q | R (Title: O) | M |
| B-05 | PlayerSkillList getAllSkills/getDeletedSkills/addSkill x2/getSkillEntry/isSkillPresent; PlayerSkillEntry (Player ctor, is* predicates, setPersistentState) | – | C,E,Q | R | M |
| B-06 | EffectController isEmpty, isAbnormalSet, isInAnyAbnormalState, isUnderFear, isConfused, getAllEffects, getAbnormalEffects, broadCastEffects, removeAllEffects; PlayerEffectController updatePlayerEffectIcons, removeNonStorableEffectsForLogout, addSavedEffect; SkillEngine.applyEffectDirectly `AION_PARTIAL` | – | E,W,Q | R | M |
| B-07 | P4-05 GameTime::onHourChange | – | W | R | S |

### World and engines: P5-05, P5-06, P5-13 (+P4-01, P4-10, P4-11a, P4-11b, tools/oracle)

| Id | What | Deps | Flows | Need | Eff |
|---|---|---|---|---|---|
| W-01 | AIEngine init/validateScripts/registerAI/newAI with the warn fallback (P4-01 key); AbstractAI onGeneralEvent/onCreatureEvent/canHandleEvent/handle*/logEvent; AIState::canHandle companion with Java's EnumSets | I-01 | W,E,Q | R | M |
| W-02 | QuestEngine init (empty registry), getQuestNpc, sendCompletedQuests, onLevelChanged, onEnterWorld, onEnterZone/onLeaveZone, onAtDistance, onLogOut, getQuestHandlerByQuestId; QuestService checkStartConditions/getLevelRequirementDiff over empty handlers | – | E,W,Q | R | M |
| W-03 | InstanceScaler onBeforeSpawn/canScale/onEnterInstance; InstanceService onPlayerLogin, getRegisteredInstance, onEnterInstance, onEnterZone/onLeaveZone, onLogout (personal instance and moveToExitPoint `AION_PARTIAL`); GeneralInstanceHandler::onDespawn; PeriodicInstanceManager ctor; PlayerTransferService ctor; **PvpMapService header + init (`AION_PARTIAL`) + onLogin (null handler → return, PlayerEnterWorldService.java:384)**; CustomInstanceService header | – | E,W,Q | R | M |
| W-04 | P4-11a StaticDoor/StaticObject/Gatherable/FlyRing/Road ctors; P4-11b GatherableController ctor; P4-10 StaticDoorSpawnManager/StaticObjectSpawnManager spawnTemplate, VisibleObjectSpawner.spawnGatherable | – | W | R | M |
| W-05 | P4-10 World storeObject/removeObject feed LeakCensus onAddedToWorld/onRemovedFromWorld, plus a test | – | Q | R | S |
| F-05 | `tools/oracle`: `m5a-spawns` (spots with npcId, x/y/z/h, level and pool/temporary/walker/handler/gatherable/flag flags; **temporary spawns evaluated for a given game hour**), `m5a-border-target`, `m5a-creation` (starting items with equip slots, level-1 autolearn skills, spawn point, **base max HP/MP from the PlayerStatCalculator formula and the PlayerClass multipliers**), plus tools.oracle tests | – | C,W | R | M |
| W-06 | Stage 2: P4-11b ControllerStandIns replaced by the providers (AttackUtil, StatFunctions, gameServerUpdateRatio); the stopFalling path | B-02, B-04, F-01a | E,W | R | S |
| W-07 | Stage 2: NPC visibility and region-move assertions green. Each notifySee/notSee/notKnow catch counts as a failure. | S-11 | W | R | M |

### Player services, items, team, siege and world events: P5-07, P5-08, P5-10, P5-12a, P5-12b

| Id | What | Deps | Flows | Need | Eff |
|---|---|---|---|---|---|
| E1-01 | ItemFactory newItem x2/calculateCount; WarehouseService.sendWarehouseInfo; StigmaService.onPlayerLogin; RepurchaseService.removeRepurchaseItems; LimitedItemTradeService.start (`AION_PARTIAL`) | – | C,E,Q | R | M |
| E1-02 | HDDBanService isBanned/loadBan/addBan/removeBan; SecurityTokenService.generateToken; MultiClientingService tryEnterWorld/onLeaveWorld (checkForFactionSwitchCooldownTime: O) | – | L,E,Q | R | S |
| E1-03 | SkillLearnService learnNewSkills/autoLearnSkills/onLearnSkill (creation branch and the unspawned no-packet branch) | B-05 | C,E | R | M |
| E1-04 | KiskService onLogin/onLogout; TeleportService sendObeliskBindPoint/sendKiskBindPoint; BindPointTeleportService.onLogin; PunishmentService.updatePrisonStatus; PetService.onPlayerLogin; AbyssRankingCache ctor/getRankingListPosition; AbyssSkillService.updateSkills; RecallService cancel/remove; DuelService isDueling/getOpponentId; **LifeStatsRestoreService HP/MP/FP restore tasks**; PlayerLimitService/AbyssRankUpdateService schedules (`AION_PARTIAL`) | – | E,Q | R | M |
| E1-05 | FindGroupService.onLogout; AutoGroupService getInstance/onPlayerLogin/onLogout/onEnterInstance (safe when disabled); new PlayerGroupService/PlayerAllianceService headers with onPlayerLogin/onPlayerLogout (no-team path) | – | E,Q | R | M |
| E1-06 | SiegeService ctor (disabled branch plus location maps), findFortress, getSiege, initSieges (disabled), onPlayerLogin guard; Influence (new) | – | E,Q | R | M |
| E1-07 | VortexService getLocationByWorld/initVortexLocations; RiftService initRiftLocations/initRifts; RiftManager.addRiftSpawnTemplate (`AION_PARTIAL`); RiftInformer.sendRiftsInfo(Player) + getSpawnedRifts; PanesterraService onEnterPanesterra/getSiegeId; ConquerorAndProtectorService init/onEnterMap/onEnterZone/onLeaveZone/onLeaveMap/getCPInfoForCurrentMap; BaseService ctor/initBases (`AION_PARTIAL`); WorldRaidService (disabled); **EventService start/validateConfiguredEventNames/checkActiveEvents/onTimeChanged cron/isAllEvents/collectActiveEvents/startOrStopEvents, onPlayerLogin/onEnterMap, all exact for `disabled_events=*`** (the `-D` provider is kept; Event and EventBuffHandler bodies are O-11) | – | E,W,Q | R | L |

### Economy, legion and housing: P5-09, P5-11

| Id | What | Deps | Flows | Need | Eff |
|---|---|---|---|---|---|
| E2-01 | AtreianPassportService ctor, isAtreianPassportDisabled x2, findLastRewardTime, calculatePassportExpireDate, onLogin and helpers; the cron lambda `AION_PARTIAL` | – | L,E | R | M |
| E2-02 | BrokerService ctor, initBrokerService, getRaceBrokerSettledItems, getSettledItemsForPlayer, extractEarnedKinahForSoldItems, getEarnedKinahFromSoldItems x2, onPlayerLogin, removePlayerCache, onPlayerDeleted; checkExpiredItems ported, or `AION_PARTIAL` that never throws inside the periodic task | – | L,C,E,Q | R | M |
| E2-03 | MailService.onPlayerLogin; PricesService getters; ExchangeService cancelExchange/getCurrentParter/returnItems/cleanUpExchanges; DropService see/unregisterDrop/closeDropList; BonusPack/FactionPack ctors + addPlayerCustomReward; VeteranRewardService.tryReward; AdventService ctor/onLogin; RelinquishCraftStatus | E1-06 | E,W,Q | R | M |
| E2-04 | LegionService getLegionMember x2 (no-member path), checkDisband, LegionWhUpdate (onLogin/onLogout: O); LegionDominionService initLocations/getLegionDominions | – | L,C,E,Q | R | M |
| E2-05 | HousingService ctor, spawnHouses, onPlayerLogin, canOwnHouse, findActiveHouse, findPlayerHouses, onPlayerDeleted; HousingBidService ctor/onPlayerLogin; TownService ctor/getTownIdByPosition/onEnterWorld (Town ctor after I-01); AuctionEndTask/AuctionAutoFillTask/MaintenanceTask (`AION_PARTIAL`) | I-01, F-03 | C,E,W,Q | R | L |

### P5-14 app, misc (+P4-17) and harness (P5-SC)

| Id | What | Deps | Flows | Need | Eff |
|---|---|---|---|---|---|
| F-01a | GameServer.h/.cpp **skeleton** in Java order, with a log line before each startup step ("startup step N: name"). Engine stream as in GameServer.java:100: QuestEngine, AIEngine, InstanceEngine, ChatProcessor, ZoneService, GeoService `init`. Then GameTimeService, DropRegistration, the location inits, housing, ChallengeTask, spawnAll, Town, FlyRing, rifts, limits, the siege/base/raid/CP inits, Announcement/Debug/Weather/Broker/Influence/Exchange/PeriodicSave/AtreianPassport/CronJob, CuringZone/Road/HTMLCache/AbyssRanking, PeriodicInstanceManager, EventService.start, Admin/CommandsAccess/PlayerTransfer/PvpMap/CustomInstance, startClock, initNioServer (DisconnectExecutor on the instant pool), LoginServer.connect. Also START_TIME_SECONDS, the ratios, initShutdown, isShutdownScheduled, isShuttingDownSoon, shutdownNioServer, and the P4-17 SM_VERSION_CHECK stand-ins. Stage-1 acceptance: `gs.smoke.startup_progress` reaches the last merged step or stops at a step whose owner has not merged yet. | F-03 | L,W,Q | R | M |
| F-01b | Stage 2: startup reaches "Game server started" and "LoginServer connected" with 0 `AION_UNPORTED` hits; the fixes are in P5-14 | B-01..B-03, W-01, W-03, W-04, E1-01, E1-04, E1-06, E1-07, E2-01..E2-05, F-01a, F-03 | L,W,Q | R | S |
| F-02 | main.cpp run mode: wait loop, `--stop-file=<path>` (200 ms poll), `--check-output` reports at shutdown (unported, partial, census, live counts, lockdep, watchdog, `m5a_summary.txt` including the "not ported yet" CM classes seen). **`--check-static-data` and `--check-id-factory` stay early exits after World** (gs.m4.check_static_data stays a regression test). RunStartupSmoke gets `--stop-file` and writes it after "Game server started"; its expected line and TIMEOUT are updated. ShutdownHook: countdown with fast exit, shutdownNioServer, PeriodicSaveService.onShutdown, saveGameTime, RuntimeLifecycle::shutdown, Ctrl+C/close handler, quick_exit. | F-01a | Q | R | M |
| F-03 | NameRestrictionService (4); TribeRelationService (6); AntiHackService canMove/punish/moveBack/checkAionBin; ExpireTimerTask (new) + the 4 P4-12 registrations; PeriodicSaveService (+3 tasks, onShutdown); AbstractCronTask (new); **ChatProcessor::init with an empty registry (R)**; SurveyService ctor/showAvailable; HTMLService; Announcement/Debug/CronJob/Admin/CommandsAccess/FlyRing/Road/CuringZone (port or `AION_PARTIAL`) | W-04 | C,E,W,Q | R | L |
| F-04 | `tests/scenario` harness (P5-SC). **ScenarioServers:** schemas; DB URLs built from the configured ones, keeping `serverTimezone` and `characterEncoding`; `-Ddatabase.user/-Ddatabase.password` from AION_TEST_GS_DATABASE_USER/PASSWORD (GS) and AION_TEST_DATABASE_USER/PASSWORD (LS); free ports; LS child started with CREATE_NEW_PROCESS_GROUP and stopped with `GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT)`, waiting for exit before its log is scanned; GS child with the stop file; report reading. **FakeLoginClient** over `AionLoginClientCrypto.h`. **GameSession** over FakeGameClient: CM builders, a fixed MAC `0A-1B-2C-3D-4E-5F` and HDD serial, SM recorder by opcode name. **PacketSequence** matcher with the async-allowed set (§5.9). DB assert helpers. | I-01 | all | R | L |
| F-07 | Final census in check-output mode (D8): after the ShutdownHook logout, drain the pools, configure censusAfter/checkInterval 0, run `Reclaimer::reclaimNow` twice, write `census.txt` (class, id, refcount, `ThreadPoolManager::tasksPinning` sites) and `live_counts.txt` | I-03, W-05, F-02 | Q | R | S |
| F-08 | Stage 2 (scenario lane): `tests/scenario/decoders` written from Java writeImpl for SM_CHARACTER_LIST/SM_CREATE_CHARACTER player info, SM_PLAYER_INFO, SM_PLAYER_SPAWN, SM_INVENTORY_INFO, SM_SKILL_LIST, SM_STATS_INFO, SM_QUEST_COMPLETED_LIST, SM_QUEST_LIST, SM_WAREHOUSE_INFO, SM_MACRO_LIST, SM_NPC_INFO. Each asserts that the body is consumed exactly; no `serverpackets/` include is allowed. | F-04 | C,E,W | R | M |
| F-06 | Stage 2: CTest `gs.scenario.m5a` (§5.10), checked RelWithDebInfo gate | F-04, S-11 | all | R | S |
| C-01 | Stage 2, leased from P5-15/P5-16: CM_MAY_QUIT, CM_CHECK_MAIL_UNK (no-ops), CM_PING_REQUEST (SM_PING_RESPONSE), CM_SHOW_FRIENDLIST (SM_FRIEND_LIST), CM_CUSTOM_SETTINGS (settings + broadcast), CM_SUBZONE_CHANGE (revalidateZones), CM_CHAT_AUTH (no-op while the chat server is off). Byte-vector tests. | S-01 | E,W,Q | R | S |
| X-01a | Stage 2: bodies named by the scenario traces (unported, partial beyond the allow-list, ERROR lines) in P5-01, P5-02, P4-05, P4-12, P5-07, P5-08, P5-10 | S-11 | all | R | M |
| X-01b | Stage 2: the same for P5-09, P5-11, P5-12a, P5-12b | S-11 | all | R | M |
| X-02 | Stage 2: decoder, value and DB mismatches in P4-13 (storage), P4-14 (DAOs), P4-16/P4-17 (SM packets) | F-08, S-11 | all | R | M |
| X-03 | Stage 2: P5-14 misc service fixes and the ShutdownHook fixes found by Q7 | S-11 | Q | R | S |
| G-01 | Stage 3: nightly `gs.scenario.m5a_stress`: 20 FakeGameClients for 30 min, ASan + checked, injected DAO exceptions during logout, logouts with HP below max, `-Dgameserver.debug.leak_census_seconds=60 -Dgameserver.runtime.zombie_break_seconds=120` (header request). Asserts: no reused-id warnings, zombie cut count 0, final census empty. The SummonerAI fight moves to M5b. | F-06, F-07 | Q | R | M |
| R-01 | Stage 3: real-client fixes. Edits outside the lane's chunks need an integrator file lease: one active lease per chunk, released at merge, and lease rows recorded in the manifest. | F-06 | all | O | M |

### Optional and deferred

| Id | What | Need |
|---|---|---|
| O-01 | CM_CHARACTER_PASSKEY enter/delete branches and the CM_RECONNECT_AUTH back-to-server-list flow tested | O |
| O-02 | SM_HOUSE_SCRIPTS parity: PlayerScript.LUA_SANDBOX_FIX as a compressed script | W |
| O-03 | Staff logins: ChatProcessor command execution (LOGIN_EXECUTE_COMMANDS), VERSION_INFO for access ≥ 9 | O |
| O-04 | MultiClientingService SAME_FACTION/FULL, ClassChangeService dialog, PlayerReviveService on logout, SummonsService.release | O |
| O-05 | Siege enabled: SiegeService.onPlayerLogin packets, SM_INFLUENCE_RATIO writeImpl, LinkedHashMap locations | O |
| O-06 | AutoGroup enabled: checkAndSendOpenRegistrations, SM_AUTO_GROUP | O |
| O-07 | DatabaseCleaningService; GeneralUpdateTask/ItemUpdateTask 15-min stores tested | O |
| O-08 | Ratio block and SM_VERSION_CHECK ratios/counts from real data | O |
| O-09 | Passive skill effects at enter world (applyEffectDirectly in full) | W |
| O-10 | Spawn oracle cross-check against the "Loaded N npc spawns" counts | O |
| O-11 | Events on (M5b): Event, EventBuffHandler (ctor, buff data DAO, onEnterMap, tryBuff, onTimeChanged), login_message events, and the date-dependent pattern entries | O |
| O-12 | Generated wire schemas from Java write sequences via `tools/gen/javasrc.py` (replaces the hand decoders later) | O |
| O-13 | **M5b: the derived half of `SM_STATS_INFO` (V9).** The gate compares base max HP/MP against `m5a-creation` and asserts everything else that is checkable without a stat oracle (identity fields, the oracle values in *every* `SM_STATS_INFO` of the burst, full HP/MP and 0 DP for a character that never fought, level 1 exp, the packet's game time against the `SM_GAME_TIME` of the same burst, the attack speed against `SM_PLAYER_INFO`, and "a class stats template was applied at all" for the six base attributes and the base attack values). What stays unchecked is every **value** of a derived stat: attack, accuracy, evasion, parry, block, crit, magic boost, resistances, the current maxima and the equipment contribution. It cannot be closed at M5a for two reasons, and O-13 is the item that closes both: `tools/oracle` needs an `m5a-stats` command that computes the full stat set from the Java stat functions and the starting equipment, and **O-09** must land first, because passive skill effects are the allow-listed `AION_PARTIAL` (`SkillEngine::applyEffectDirectly`, 8 hits per enter world) and a real client's stat window shows them. Until then a derived stat may be wrong by any amount and `gs.scenario.m5a` stays green. | O |

## 4. Lanes

At most 6 lanes per stage. Chunks are disjoint within a stage. `tools/oracle` and `game-server/cmake` are outside the chunk manifest and are
assigned here.

| Stage | Lane | Chunks | Items | Tests |
|---|---|---|---|---|
| 1 | **slice** | P5-00, P4-15 | S-01..S-10 | tests/login_slice, tests/network |
| 1 | **stats-skills** | P5-01, P5-02, P4-05, P4-12 | B-01..B-07 | tests/stats, skills, base, player (4 sm_ak stat tests unskipped) |
| 1 | **world-engines** | P5-05, P5-06, P5-13, P4-01, P4-10, P4-11a, P4-11b, tools/oracle | W-01..W-05, F-05 | tests/ai, quest, instance, world, objects; tools.oracle |
| 1 | **player-events** | P5-07, P5-08, P5-10, P5-12a, P5-12b | E1-01..E1-07 | tests/itemsvc, playersvc, team, siege, worldevents |
| 1 | **economy-legion** | P5-09, P5-11 | E2-01..E2-05 | tests/economy, legionhouse |
| 1 | **app-harness** | P5-14, P4-17, P5-SC, game-server/cmake | F-01a, F-02, F-03, F-04, F-07 | tests/misc, tests/app, gs.smoke.startup_progress, gs.m4.check_static_data, harness self-tests with a stub GS |
| 2 | **scenario** | P5-00, P4-15, P5-SC | S-11, S-12, F-06, F-08 | gs.scenario.m5a |
| 2 | **world-visibility** | P4-01, P4-10, P4-11a, P4-11b, P5-05, P5-06, P5-13, tools/oracle | W-06, W-07 | visibility and move cases |
| 2 | **app-gate** | P5-14, game-server/cmake | F-01b, X-03 | gs.smoke.startup, Q7 |
| 2 | **svc-fixups-a** | P5-01, P5-02, P4-05, P4-12, P5-07, P5-08, P5-10 | X-01a | owning chunk tests + scenario rerun |
| 2 | **svc-fixups-b** | P5-09, P5-11, P5-12a, P5-12b | X-01b | owning chunk tests + scenario rerun |
| 2 | **packets-dao** | P4-13, P4-14, P4-16, P4-17, P5-15 (lease), P5-16 (lease) | C-01, X-02 | tests/sm_ak, sm_lz, dao, items; C-01 vectors |
| 3 | **stress** | P5-14, P5-SC | G-01 | gs.scenario.m5a_stress (nightly) |
| 3 | **client-fixes** | P5-00, P4-15, P5-15, P5-16 + leases (R-01 rule) | R-01 | real-client checklist |
| all | integrator | runtime (P4-02a/b), commons, login-server, manifest | I-01..I-04 | full verification |

**Merge order in stage 1.** B-01..B-03 go first: Player and NPC construction. Then W-01 and W-04 (spawnAll completes), then F-03 and F-01a/F-02,
then the rest. F-04 and F-05 have no body dependencies and start on day 1. Stage 2 starts when every stage-1 lane is merged; the stage-2 gate
adds the 0-hit startup check (F-01b).

## 5. Scenario gate (`ctest -L scenario`)

### 5.1 Processes and databases

| Piece | Setup |
|---|---|
| Schemas | `aion_ls_test_m5a_<hash>` and `aion_gs_test_m5a_<hash>`, recreated from `login-server/sql/aion_ls.sql` and `game-server/sql/aion_gs.sql`. The LS gets `gameservers(id=1, mask='127.0.0.1', password='1234')`. |
| Environment | AION_TEST_GS_DATABASE_URL/_USER/_PASSWORD and AION_TEST_LS_DATABASE_URL + AION_TEST_DATABASE_USER/_PASSWORD. Without the URLs the test prints "skipped". |
| URLs | Taken from the respective `config/network/database.properties` with the database name replaced. `serverTimezone` (GS: `${gameserver.timezone}`) and `characterEncoding=UTF-8` are kept, and user/password are passed as `-D`. |
| Ports | Free ports: p1 LS client, p2 LS GS-link, p3 GS client. |
| LS child | `aion_login_server` in cwd `D:/aion-server/login-server` with `-Ddatabase.*`, `-Dloginserver.network.client.socket_address=127.0.0.1:p1`, `-Dloginserver.network.gameserver.socket_address=127.0.0.1:p2`, `-Dloginserver.accounts.autocreate=true`. Started with CREATE_NEW_PROCESS_GROUP; stopped with CTRL_BREAK; the harness waits for exit. |
| GS child | `aion_game_server` in cwd `D:/aion-server/game-server` with `-Ddatabase.*`, D1 + scenario keys, `-Dgameserver.network.client.socket_address=127.0.0.1:p3`, `-Dgameserver.network.login.address=127.0.0.1:p2`, `--stop-file=<bin>/scenario/stop`, `--check-output=<bin>/scenario/<cfg>`. |
| Readiness | GS log "Game server started" and the authed LS link line, then p3 accepts connections. |
| Run model | One GS per run, one run per output directory. Cases run in fixed order. Case 7 writes the stop file with account B online. Both children live in a Windows job object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`, so a CTest TIMEOUT or a crash of the gate cannot orphan a server holding its schemas. **Both schemas also carry an in-use marker** (`SchemaLease`, a MariaDB session lock that dies with the process), and `createSchemas()` sweeps the schemas of runs that were killed before they could drop their own — the only place that ever reclaims them, because a TIMEOUT runs no destructor. A run drops its two schemas at the end, a **failed** one as well unless `AION_SCENARIO_KEEP_SCHEMAS` is set for a post mortem; what a post mortem reads first (the reports and both server logs) is kept and named either way. Since stage 3 wave B there are **two** gates in this model — `gs.scenario.m5a` and `gs.scenario.m5a_geo` — each with its own output directory, schema pair and logs, sharing the CTest `RESOURCE_LOCK` so that two geo-sized servers never run at once. |
| GS log directory | The GS child gets `--log-folder=<outputDir>/gs_log`. It must never write the shared `game-server/log`: `Logging::init` archives and **deletes** what it finds there, so two trees running the gate — or a gate run next to the user's own play server — destroy each other's logs. Q8 reads `gs_log/server_errors.log` as well as the captured console stream. |
| LS log directory | The login server has **no** `--log-folder` and resolves `./config` and `./log` against its working directory, so the harness gives it one of its own: `startLoginServer()` copies `login-server/config` into `<outputDir>/ls_run` and starts the child there (`ScenarioServers::loginServerWorkingDirectory`). Started in the Java module directory it would archive and delete the shared `login-server/log` of every other tree and of the user's own login server, exactly as the GS row describes. |
| Geodata | **Paid (2026-09-21, stage 3 waves A and B).** `gs.scenario.m5a` itself keeps `gameserver.geodata.enable=false`; the geo data costs a startup (measured over six runs of this tree, checked RelWithDebInfo: the case that brings both servers up takes 6–9 s with it against 4 s without, and a Debug build pays the 144 s and 3.2 GB of m5a-client-session.md), so it is a second CTest and not a flag on the first — 18–21 s for the geo gate against 32 s for `gs.scenario.m5a` and its twelve cases. Two CTests now cover the configuration the user plays in: `gs.smoke.startup_geo` (wave A) starts the server with the geo data and asserts 0 spawn failures, which is where the real-client session lost 13 npc spawns (m5a-client-session.md F-1), and **`gs.scenario.m5a_geo` (wave B) walks a character through it**: enter world, the §5.8 packet order, V6/V9, V1-V4 against the spawn oracle, `CM_SUBZONE_CHANGE` (the client-driven `Creature::revalidateZones`, which with geo on asks the player's position against the material zones the geo loader built), one region move, quit, and the Q8 report bar (0 `AION_UNPORTED`, `partial_trace` ⊆ the allow-list, empty census, no ERROR, no unported client packet, `knownListNotifyFailures 0`, exit code 0). **Its geo-specific rows are live-instance counts of classes a geo-off run never creates once** (`live_counts.txt`, measured): `Terrain` 89 (GEO1), `TerrainZoneCollisionMaterialActor` 6598 (GEO2, one per Creature that has a move controller and whose world has terrain materials) and `MaterialZoneHandler` 7901 (GEO3, the player-side zone handlers themselves). **GEO4 is the row that keeps the other three honest:** all three were already at their final value in `live_counts_baseline.txt`, which the server writes after the spawns and before the first client connects, so they are facts about the world the character walked and *not* about the enter-world path, and GEO4 asserts that difference (the client half created no `TerrainZoneCollisionMaterialActor`, no `ZoneCollisionMaterialActor` and no material zone). Why the player creates none: `worldHasTerrainMaterials(210010000)` is false, because only 10 of the 78 terrain images of the 4.8 tree are 8-bit material images (`*_materials.png`; the nearest map that has one is 220020000) while `210010000.png` is a 16-bit heightmap, so `CreatureController::onAfterSpawn` takes its other arm for the character exactly as it does for every npc of Poeta; and nothing in the run entered a material zone, which `MaterialZoneHandler::onEnterZone` would have shown as a `ZoneCollisionMaterialActor`. What the gate therefore buys is the F-1 class of bug on the path a player walks — an exception, an ERROR, an `AION_UNPORTED` site, a lost spawn, a wrong position or a hang that only a geo-built world reaches — plus one assertion that a geo-off run cannot make: V1-V4 hold *in a geo-built world*, to the 1 cm of `onSpot`, which is what a terrain-z snap added to the spawn path breaks (proven by mutation: that snap fails `gs.scenario.m5a_geo` and leaves `gs.scenario.m5a` green). **What is still not covered, because it does not exist in 4.8:** no npc spawn z is corrected against the terrain (`SpawnEngine`/`VisibleObjectSpawner` call the geo engine only for the postman, the functional npc and a summon — VisibleObjectSpawner.java:172/188/238 — and the walker height correction is commented out in Java at WalkerGroup.java:273-278), no player move is geo-corrected (neither `CM_MOVE` nor `AntiHackService` contains a geo call), and `canSee` never decides what a client is told about — its call sites are the gather dialog (GatherableController.java:56), the attack path (PlayerController.java:411), the siege weapon and npc movement, none of which an M5a client packet reaches. The gather obstacle check and `getClosestCollision` therefore stay open until the M5b client packet set (CM_TARGET_SELECT and the gather flow, m5a-client-session.md F-2); a player-side terrain-material path needs a character on a map that has a material image. The `if (!GeoDataConfig.GEO_MATERIALS_ENABLE) CuringZoneService` startup branch takes its geo arm in `gs.smoke.startup_geo` and `gs.scenario.m5a_geo` and its other arm in `gs.scenario.m5a`. |

### 5.2 Case 1: login (account A; B repeats it before case 2b)

1. FakeLoginClient: SM_INIT, CM_AUTH_GG, CM_LOGIN (autocreate), CM_SERVER_LIST (GS 1 listed), CM_PLAY (loginOk, playOk1, playOk2).
2. GameSession connects and gets `SM_KEY`.
3. CM_VERSION_CHECK → `SM_VERSION_CHECK`.
4. CM_L2AUTH_LOGIN_CHECK, then CM_MAC_ADDRESS (pins FIFO) → `SM_L2AUTH_LOGIN_CHECK(ok)`.
5. CM_TIME_CHECK → `SM_AFTER_TIME_CHECK_4_7_5`, `SM_TIME_CHECK`.
6. CM_CHARACTER_LIST → `SM_ACCOUNT_PROPERTIES`, `SM_CHARACTER_LIST(0)`.
7. CM_PING → `SM_PONG`. CM_GAMEGUARD → nothing within 500 ms. CM_SECURITY_TOKEN → `SM_SECURITY_TOKEN`.

### 5.3 Case 2: create

1. CM_CREATE_CHARACTER(type 1) → `SM_CREATE_CHARACTER(22)`.
2. CM_CHECK_NICKNAME → `SM_NICKNAME_CHECK_RESPONSE(0)`.
3. CM_CREATE_CHARACTER (Elyos Warrior) → `SM_CREATE_CHARACTER(0)`; the player info block decodes exactly (F-08).
4. The same name → 10. An Asmodian on A → 12.
5. DB checks against `m5a-creation`:
   - `players`: race, class, level 1, world 210010000 and the spawn x/y/z/heading, online 0.
   - `player_appearance` bytes equal what was sent.
   - `inventory` ids and counts; weapon and armor equipped with slot masks.
   - `player_skills` = the autolearn set.
6. 2b, account B: Asmodian Mage, same checks on 220010000.

### 5.4 Case 3: enter world (A, first enter)

- CM_MAY_LOGIN_INTO_GAME → `SM_MAY_LOGIN_INTO_GAME`.
- CM_ENTER_WORLD: the types match §5.8 (with the first-enter prefix); there is no `SM_ENTER_WORLD_CHECK(CONNECTION_ERROR)`.

**Value assertions** (F-08 decoders; every body consumed exactly):

| # | Assertion |
|---|---|
| V6 | `SM_PLAYER_SPAWN` world and position equal the spawn point. |
| V7 | `SM_INVENTORY_INFO` item ids, counts and equip slots equal the `inventory` rows. |
| V8 | `SM_SKILL_LIST` ids and levels equal the `m5a-creation` autolearn set. |
| V9 | `SM_STATS_INFO`: base max HP/MP equal the oracle's base — the PlayerStatCalculator value **plus** the health/will dependent addition of `PlayerStatFunctions.MaxHpFunction`/`MaxMpFunction`, which is what `getMaxHp().getBase()` returns; current max ≥ base. `SM_PLAYER_INFO` (§5.8 #33, its own 230-line `writeImpl`, not the shared `writePlayerInfo` block) is decoded in case 4 and case 7: object id, name, class id, level, HP%, x/y/z and the **exact** equipment block — count, item ids in ascending slot order and the full slot mask, all derived from the same `m5a-creation` rows through `ItemSlot.isVisible` (ItemSlot.java:41-56, mask 589055) and the `mask \|= slot` loop of AbstractPlayerInfoPacket.java:148-160, because a subset check plus "not empty" is satisfied by a server that announces one of the three starter pieces or ORs one slot bit instead of three; decoding it consumes the body exactly, so its framing is checked too. **Scope:** this proves only the base half of the stats. Passive skill effects are the allow-listed `AION_PARTIAL` O-09 (`SkillEngine::applyEffectDirectly`, 8 hits per enter world), so every derived stat a real client shows — attack, accuracy, evasion, crit, magic boost and current max HP/MP — is unchecked at M5a. A stage-3 item must compare a full `SM_STATS_INFO` against an oracle once O-09 lands. |
| V10 | `SM_QUEST_LIST` and `SM_QUEST_COMPLETED_LIST` are empty; `SM_WAREHOUSE_INFO` x43 and `SM_MACRO_LIST` decode as empty. |

### 5.5 Case 4: level ready and NPC visibility

CM_LEVEL_READY, then collect until 1 s passes with no packet. The types match the level-ready part of §5.8. The harness takes the game hour from
the enter-world `SM_GAME_TIME` and runs `oracle.py m5a-spawns --map 210010000 --x 1212.94 --y 1044.85 --z 140.76 --game-hour H`.

| # | Assertion |
|---|---|
| V1 | Every SM_NPC_INFO npcId is a spot within 95 m (+5 m slack, +10 m for walkers) or a flag NPC of the map. |
| V2 | For non-walker, non-pool NPCs: x/y/z equal a spot of that id (±0.01); heading and template level equal; HP% 100; the body decodes exactly. An NPC whose id the oracle knows **only** as fixed spots (the id has no pool, walker or randomWalk spot at all) and that stands on none of them fails here — it must not fall through to V1, which matches by id alone. An id with both fixed and moving spots, such as 210115 on the Elyos start map (4 fixed, 3 walker, 1 randomWalk), may legitimately stand anywhere and is still only covered by V1. The oracle's level is an **optional**: `"level": null` means "this spot has no npc template" (a gatherable spot), while `0` is a level like any other and is compared. Collapsing the two into a plain 0 made the check assert `level > 0` on a genuine level-0 npc instead of comparing it, which is a gate that fails on correct behaviour (`OracleSpot::level`, `OracleTest`). |
| V3 | count(SM_NPC_INFO) ≥ N, the deterministic spots within 90 m (no pool, no walker; temporary spots included if in time for H), each matched by id and position. |
| V4 | SM_GATHERABLE_INFO ids ⊆ the gather spots within 100 m, **and** every deterministic gather spot within 90 m has an SM_GATHERABLE_INFO of that id at that position. The completeness half mirrors V3 and is what makes V4 fail on a world without gatherables: a subset assertion alone is satisfied by zero SM_GATHERABLE_INFO, and the level-ready `(SM_NPC_INFO \| SM_GATHERABLE_INFO)+` is satisfied by the NPCs on their own. |
| V5 | The same for the Mage (account B) on 220010000 (571.04, 2787.34, 299.875), in case 7. |

### 5.6 Case 5: region move

`m5a-border-target` gives T. CM_MOVE(POSITION|MANUAL) every 5 m, then a stop move, then 1 s of waiting.

| # | Assertion |
|---|---|
| M0 | `SM_PLAYER_STATE` (self) arrives after the first CM_MOVE (protection stops, CM_MOVE.java:141), unless the 60 s protection timer ran out first. |
| M1 | New SM_NPC_INFO ids are within 100 m of some path point, and include the deterministic spots within 90 m of T that were not within 100 m of the start. |
| M2 | SM_DELETE ids were sent before, and include the deterministic start objects farther than 100 m from T. |
| M3 | No SM_FORCED_MOVE and no SM_MOVE to self. |

### 5.7 Case 6: quit, persistence, relogin; case 7: shutdown with a player online; case 8: reports

| # | Step and assertions |
|---|---|
| Q1 | CM_QUIT(1) → `SM_QUIT_RESPONSE`. CM_CHARACTER_LIST → `SM_ACCOUNT_PROPERTIES`, `SM_CHARACTER_LIST(1)`. |
| Q2 | DB: `players` online=0, world and x/y/z/heading = T, last_online ≥ quit time, old_level=1. `abyss_rank` and `player_life_stat` rows exist. `inventory` and `player_skills` are unchanged. |
| Q3 | The harness seeds an idian stone on the equipped weapon and sets `player_life_stat.hp` to half. After 1.5 s: CM_ENTER_WORLD without the first-enter prefix, SM_PLAYER_SPAWN at T, SM_STATS_INFO current HP = half (the restore task pins the Player), then CM_LEVEL_READY. |
| Q4 | CM_QUIT(0): `SM_QUIT_RESPONSE` is the last frame and the socket closes. |
| Q5 | New LS and GS login without a kick. SM_CHARACTER_LIST: 1 entry, level 1, map 210010000, position T, appearance bytes equal those of creation, visible equipment = the equipped items. After waiting out `gameserver.character.reentry.time` (as Q3 does — `PlayerEnterWorldService.java:152-156` answers `SM_ENTER_WORLD_CHECK(REENTRY_TIME)` and returns while `now - lastOnline` is below it, and Q4 logged the character out seconds earlier), the enter gives `SM_ENTER_WORLD_CHECK` 0 and SM_PLAYER_SPAWN at T. CM_QUIT(0). |
| Q7 | Account B enters (V5), sends 3 CM_MOVE to P, and stays online. The harness writes the stop file (shutdown delay 2). B receives `SM_SYSTEM_MESSAGE` STR_SERVER_SHUTDOWN, then the socket closes. GS exit 0. **Before any restart:** `players` for B has online=0, position = P, last_online set. |
| Q8 | Reports (the LS is stopped with CTRL_BREAK and has exited): `unported_trace.txt` empty; `partial_trace.txt` ⊆ the allow-list (an entry with a line number matches the whole site, so a 3-digit row cannot cover 4-digit lines; rows the run did not hit are printed); `census.txt` empty; lockdep empty; no watchdog dump; no ERROR line in either server log **and** an empty `server_errors.log` in the gate's own `--log-folder` (logback's `additivity="false"` loggers never reach the console the harness captures); `m5a_summary.txt` has `started true`, `exitCode 0`, `knownListNotifyFailures 0` (W-07) and no "not ported yet" CM from the scripted path. <br>`live_counts.txt`: **0** for Player, Item, AbyssRank and the interaction tasks, and **≤ the number of client connections still open when the stop file was written** for Account, AccountTime, PlayerAccountData, PlayerCommonData, PlayerAppearance, ConnectionAliveChecker and every `*Storage`. The bound is Java's own behaviour, not a relaxation: `AionConnection.java:239-243` returns from `onDisconnect` right after `safeLogout()` while `GameServer.isShuttingDownSoon()`, i.e. before `LoginServer.onDisconnect`, and `LoginServer.java:119` is the only place that removes the connection from `loggedInAccounts`; the still-registered connection therefore holds its Account, its PlayerAccountData/PlayerCommonData and the account warehouse until the process exits — which is exactly what Q7 arranges. It still catches a leak: account A quits normally in case 6, so if its account-level objects survived, the count would exceed the one open connection. Beyond those six classes the **server itself** checks the live counts: `CheckOutput::zeroLiveClasses()` lists the classes that must be 0 once the runtime shut down (§10.2), `checkLiveCounts()` logs one ERROR per offending class and `m5a_summary.txt` gets `liveLeaks <n>` plus a `liveLeak <class> <live>` row each, so a leak of one of them fails Q8's "no ERROR line" assertion whatever the gate itself reads. <br>Run length (revised in stage 3, §10.3): the "zombie-cut" and "stale pin" rows used to be unreachable — `gameserver.runtime.zombie_break_minutes` is 30 and `LeakCensus::stalePinAfter` 10 minutes against a one-to-three-minute run, `CheckOutput::runFinalCensus` switched the breaker off before the final scan, and no log message contained the string "stale pin" that the gate greps for. `runFinalCensus` now ends with `runBreakerPass()`: after `census.txt` is written it scans once more with `zombieBreakAfter`, `stalePinAfter` and `stalePinCheckInterval` at 0 and the breaker on, waits for the posted breakers and reclaims again, and the warning reads "Leak census: stale pin: periodic task …". Both rows therefore describe what is left at the end of the run. Neither can fire on a clean run: both only look at objects that left the world and are still referenced, i.e. exactly what `census.txt` reports, so they are attribution (which task pins it, which edge was cyclic) on top of the census rather than independent checks. The **timed** machinery (a cut after 30 minutes, a stale pin after 10) is still not proven by the gate; it is proven by `LeakCensusTest` and by the G-01 nightly. `census.txt` was never in that category: `runFinalCensus` sets `censusAfter` to 0. `LeakCensus` is fed only by `World::removeObject`, so it covers VisibleObjects alone; the gate additionally prints the whole `live_counts_baseline.txt` → `live_counts.txt` difference as diagnostics, so the classes it does not assert are at least visible in a run. |

### 5.8 Expected enter-world SM sequence (Java order under D1; new level-1 non-staff character, no legion, friends or house)

Notation: `T` once, `T{n}` n times, `T+` one or more, `[T]` optional, `T*` any number. The §5.9 async set is filtered.

| # | CM_ENTER_WORLD |
|---|---|
| 0 | first enter only (`old_level=0`): `SM_STATS_INFO`, `SM_ACTION_ANIMATION`, `SM_NEARBY_QUESTS` |
| 1-3 | `SM_HOUSE_SCRIPTS`, `SM_UNK_3_5_1`, `SM_ENTER_WORLD_CHECK` |
| 4-6 | `SM_SKILL_LIST+`, `[SM_SKILL_COOLDOWN]`, `[SM_ITEM_COOLDOWN]` |
| 7-10 | `SM_QUEST_COMPLETED_LIST+`, `SM_QUEST_LIST`, `SM_TITLE_INFO{2}`, `SM_MOTION` (corrected in wave 5a stage 2: the second `SM_TITLE_INFO` is `TitleList.setBonusTitle`'s action-6 packet. `PlayerEnterWorldService.java:239` sends `SM_TITLE_INFO(pcd.getTitleId())`, and lines 240-242 then run `if (pcd.getBonusTitleId() != 0) player.getTitleList().setBonusTitle(...)`, whose first statement is `SM_TITLE_INFO(6, bonusTitleId)` (`TitleList.java:88`). A character's bonus title is -1, not 0 — `PlayerCommonData.java:51` and `aion_gs.sql:931` `bonus_title_id int NOT NULL DEFAULT '-1'` — so it is sent on **every** enter world, first enter and relogin alike) |
| 11-12 | `SM_AFTER_TIME_CHECK_4_7_5`, `[SM_UI_SETTINGS]{0..3}` |
| 13 | `SM_INVENTORY_INFO{ceil((1+items)/10)+1}` (count from the oracle) |
| 14-17 | `SM_CHANNEL_INFO`, `SM_BIND_POINT_INFO`, `SM_PLAYER_SPAWN`, `SM_GAME_TIME` |
| 18 | `SM_WAREHOUSE_INFO{43}` |
| 19-23 | `SM_TITLE_INFO`, `SM_EMOTION_LIST`, `SM_PRICES`, `SM_FRIEND_LIST`, `SM_BLOCK_LIST` |
| 24-26 | `SM_INSTANCE_INFO`, `SM_ABYSS_RANK`, `SM_STATS_INFO` |
| 27-28 | `SM_LEGION_DOMINION_LOC_INFO`, `SM_MAIL_SERVICE` |
| 29 | `[SM_SYSTEM_MESSAGE]{0..2}`, `[SM_ATREIAN_PASSPORT]` (present iff the passport is not disabled; read from the GS summary) |
| 30-32 | `SM_MACRO_LIST+`, `SM_RECIPE_LIST`, `SM_HOUSE_OWNER_INFO` |

| # | CM_LEVEL_READY |
|---|---|
| 33-36 | `SM_PLAYER_INFO`, `SM_PLAYER_STATE`, `SM_ACCOUNT_PROPERTIES`, `SM_MOTION` |
| 37 | `SM_WINDSTREAM_ANNOUNCE*` |
| 38 | `(SM_NPC_INFO \| SM_GATHERABLE_INFO)+` (§5.5) |
| 39 | `SM_RIFT_ANNOUNCE` |
| 40-42 | `SM_NEARBY_QUESTS`, `[SM_QUEST_REPEAT]`, `[SM_WEATHER]` |
| 43-44 | `SM_ABNORMAL_STATE`, `SM_CUBE_UPDATE` |

With `disabled_events=*`, EventService.onPlayerLogin and onEnterMap add nothing.

### 5.9 Async-allowed set (PacketSequence)

Packets allowed at any position, each still recorded and checked:
- periodic `SM_GAME_TIME` (180 s)
- `SM_PONG`
- `SM_PLAYER_STATE` to self (the 60 s protection end, PlayerController.java:626)
- `SM_WEATHER` (weather change 20-240 s after an in-game hour)
- `SM_NPC_INFO` / `SM_DELETE` of temporary-spawn ids at an hour change, outside the region-move window
- `SM_SYSTEM_MESSAGE` STR_SERVER_SHUTDOWN (Q7 only)

Case 5 expects `SM_PLAYER_STATE` explicitly (M0).

### 5.10 CTest wiring

- `aion_gs_scenario_tests` (gtest, `tests/scenario`, P5-SC) depends on `aion_game_server` and `aion_login_server`.
- `gs.scenario.m5a`: LABELS `scenario;realdata`; TIMEOUT 900; `RESOURCE_LOCK "aion_game_server_log;aion_login_server_log"`; `SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a: skipped"`. Gate build: checked RelWithDebInfo.
- **A skipped gate is not a passed gate.** The database URLs reach the test through the environment, not through the configure, so without them the gate skips itself, `SKIP_REGULAR_EXPRESSION` turns that into CTest's `***Skipped` and CTest counts a skip as passed: a full `ctest` without `AION_TEST_GS_DATABASE_URL` / `AION_TEST_LS_DATABASE_URL` reports 100% green with M5a never executed, and nothing else in the suite asserts any M5a behaviour end to end. Configure a milestone or CI tree with `-DAION_SCENARIO_REQUIRE=ON`: the gate then fails, instead of skipping, when its inputs are missing. Whoever declares M5a reached must read the gate's own case-by-case report, not the CTest summary line.
- `gs.scenario.m5a_geo` (stage 3 wave B, §5.1 "Geodata"): the same binary and the same scripted path with `-Dgameserver.geodata.enable=true`, `--gtest_filter=M5aScenarioGeo.Run`. LABELS `scenario;realdata;geo`; TIMEOUT 2700 (the geo startup is seconds in a checked RelWithDebInfo tree and minutes in a Debug one); the **same** `RESOURCE_LOCK` as `gs.scenario.m5a`, which is what keeps two geo-sized servers from running at once; `SKIP_REGULAR_EXPRESSION "gs\\.scenario\\.m5a_geo: skipped"`, and it skips itself when the tree has no `game-server/data/geo/*.geo`. A separate CTest and not a flag on the gate above, so that `ctest -R 'gs\.scenario\.m5a$'` still runs the milestone gate alone on a tree that cannot afford the geo run.
- `gs.smoke.startup` (updated by F-02), `gs.smoke.startup_geo` (wave A) and `gs.m4.check_static_data` keep passing.
- `gs.scenario.m5a_stress`: LABELS `scenario;stress;nightly`.

## 6. Header requests expected

| Request | Kind | For |
|---|---|---|
| `AccountService::loadPlayerAccountData(Account&, int32_t)` | signature | S-02, S-05 |
| P4-01 `AIConfig` key `gameserver.dev.missing_ai_handlers = fail\|warn` (+DEVIATIONS) | additive | W-01 |
| P4-02a `runtime/base/Unported.h`: `AION_PARTIAL(reason)`, `partialHits()`, `writePartialTrace()` | additive | D3 |
| P4-05 `AuditLogger::log(Player*, …)` null-safe | additive | S-01 |
| `Town` ctor with `levelUpDate` | signature | E2-05 |
| P4-02b live-instance counters (`liveCounts()`); `LeakCensus::censusNow()` only if needed | additive | I-03, F-07 |
| P4-15 `AionClientPacketFactory`: `unportedPacketClassesSeen()` | additive | F-02 summary, D7 |
| P4-01 `RuntimeConfig`: C++-only test keys `gameserver.debug.leak_census_seconds`, `gameserver.runtime.zombie_break_seconds` (override the minutes when set, +DEVIATIONS) | additive | G-01 |
| Manifest: `tests/scenario` → P5-SC; `game-server/cmake` → P5-14; login-server test support include path; stage-2 leases of the seven C-01 files | build | F-04, C-01 |
| (only if D1 changes) `SiegeService.locations` as `runtime::LinkedHashMap` | layout | O-05 |

## 7. Risks

- **Throwing warn stubs.** In the spawn paths, spawnNpc creates the Npc outside its try, and WalkerGroup, spawnGatherable, RiftManager and spawnHouses have no catch. Java leaveWorld has no try/finally. Any exception there ends the startup or leaves `players.online=1`. `AION_PARTIAL` must return.
- **Swallowed exceptions.** KnownList notifications, `World::removeObject` (which skips onDelete) and PlayerLifeStatsDAO catch `std::exception`. The gate fails on every ERROR line and on "did not leave world cleanly".
- **Lifetime gates.** Without W-05, F-07 and I-03 the census checks pass vacuously. The 30-min stress run needs the second-granularity keys to ever reach the zombie breaker.
- **Dropped Players on enter rejection.** The account warehouse retains the Player (a SelfOrRef actor) after getPlayer. Without the S-05 guards, every reject or duplicate enter leaks a Player and its storages.
- **Singleton constructors.** A function-local static retries a throwing constructor and logs the stack only once.
- **Critical path.** stats-skills (about 7-9 days) gates Player and NPC construction. F-01b cannot pass until every service lane merges; stage-1 startup smoke therefore only checks progress.
- **app-harness load.** About 9 days even after the split. F-04 is independent and can start first. The lane cap of 6 prevents a separate harness lane.
- **Pattern brittleness.** Date and data dependent packets (passport messages, cooldowns, UI settings, windstreams, weather) are optional or counted by the oracle. The AtreianPassport cron at 09:00 can fire during a run; the passport packets are then async (add them to §5.9 if seen).
- **Profile parity.** D1 proves parity only for that profile; siege, autogroup and events bring back O-05, O-06 and O-11.
- **Spawn nondeterminism.** Pools, temporary spawns (game hour) and walker formations (DummyAI) are checked as subsets. A C++ AIState.canHandle that differs from Java's EnumSets makes DummyAI react to events Java drops.
- **Decoder independence.** The F-08 decoders are only independent if written from Java. A review checks that none includes `serverpackets/`. The real client remains the final authority on byte layout.
- **Unported client packets.** CMs outside P5-00 and C-01 get no answer; stage 3 pulls them in one by one (R-01).
- **Server list address.** `connect_address` defaults to 0.0.0.0:7777; the real-client profile sets 127.0.0.1:7777.
- **Staff accounts** throw in GMService.onPlayerLogin (command execution, O-03). Use access level 0.
- **Wall clock.** Reentry time, the 10 s delayed logout and the 60 s protection timer use real time; the harness waits explicitly. A relogin during a pending leaveWorldDelayed races kickOnlineCharacters, as in Java.
- **Windows shutdown.** Ctrl+C runs a 120 s countdown with players online (default `shutdown.delay`). Closing the console window gives about 5 s, so saves can be lost.
- **UTF-16 input.** `readS` turns lone surrogates into U+FFFD, which affects fixHddSerial and the type-1 CM_CREATE_CHARACTER name.
- **Wire order approximations.** SM_CHARACTER_LIST follows PartMap order; SM_HOUSE_SCRIPTS sends 0 scripts; SM_NEARBY_QUESTS and SM_ITEM_COOLDOWN use hash-order approximations.
- **CM_QUIT race.** CM_QUIT's leaveWorld is not synchronized, so a shutdown during CM_QUIT can run it twice. Keep `// java-race` or guard it with a DEVIATIONS row.
- **Tree verification.** Wave 3b-2 was not verified as a whole (I-02).
- **First real parallel spawnAll.** IDFactory, World, RespawnService and the Reclaimer backlog run concurrently for the first time. Watch lockdep and the watchdog in the smoke test.

## 8. Real-client checklist (user)

1. Start MariaDB (`D:\aion-dev` start.bat). Check that `aion_ls.gameservers` has id 1, password 1234 and a mask matching 127.0.0.1.
2. `game-server/config/mygs.properties`: the D1 profile, including `gameserver.event.service.disabled_events=*`, plus
   `gameserver.network.client.connect_address=127.0.0.1:7777`. Keep geo on. Use an account with access level 0.
3. Start `aion_login_server`, then `aion_game_server`. The GS log shows "Loaded N npc spawns", "Game server started" and the LS connected
   line, with no `AION_UNPORTED` warning and no ERROR line.
4. `C:\Aion\start.bat -ip:127.0.0.1 -port:2106 -loginex`. Log in; server 1 is listed online and selectable.
5. Character select is empty. On the creation screen the name check rejects a used name. Create an Elyos Warrior; it appears with its starting gear.
6. Enter the world. Poeta loads at the start point, with no endless loading and no disconnect. The stats window shows plausible HP/MP.
7. Look around: NPC names, titles, levels, positions and headings are right. Monsters stand still (DummyAI). Gather nodes are visible.
8. Walk about 100 m: objects ahead appear and objects behind disappear. Jump and glide: no rubber-banding and no server ERROR.
9. Stay idle for more than 3 minutes: no ping disconnect, and the game time advances. Open the friend list and change a display option: no
   warning in the GS log.
10. Log out to character select: level 1, gear, Poeta. Enter again at the same spot.
11. Quit the client completely and log in again: same position, appearance and equipment.
12. On a second account (creation mode 0) create an Asmodian Mage and enter Ishalgen. Repeat steps 7-8.
13. With a character online, press Ctrl+C in the GS console and wait for the countdown (120 s by default; do not close the window). **Before
    restarting**, check in MariaDB that `players.online=0` for the character and that its x/y/z match where it stood. Then restart both
    servers and log in.
14. Send `game-server/log/` and `m5a_summary.txt` (the "not ported yet" CM classes and the `AION_PARTIAL` sites) for stage 3.

## 9. Review log (rev 2)

| Critic gap | Verdict | Change |
|---|---|---|
| Permanent buff event | Valid (custom_events.xml:3 has no dates; `disabled_events` is empty) | `disabled_events=*` in D1, E1-07 exact for `*`, O-11 |
| Census and zombie breaker vacuous | Valid (RuntimeConfig.cpp:13-14 in minutes; census fed only by World) | D8, I-03, F-07, Q3/Q8, G-01 seconds keys. LSan with an orderly exit **rejected**: quick_exit is the design's exit path, so the final census is used instead. |
| Enter rejection keeps the Player alive | Valid (PlayerService.java:133; PlayerStorage SelfOrRef actor; LogoutBreakers.h Callers). Corrected: a duplicate enter sends no packet. | S-05 guards, S-03 release, S-10 destroy tests |
| No stage-2/3 owner for packets, DAOs, misc, runtime, LS | Valid | Lanes packets-dao, svc-fixups-a/b, app-gate; I-04 for LS/commons; R-01 lease rule |
| F-01 dependencies undeclared; app-harness overloaded | Valid | F-01a/F-01b split with real deps; progress smoke in stage 1; F-05 moved to world-engines; slice tests in-process. A separate harness lane is **rejected**: it would be a 7th lane in stage 1. |
| Opcode types only | Valid | D9, F-08 decoders, V6-V10. Generating schemas from javasrc is **deferred** (O-12): no write-sequence extractor exists, and loops and branches in these packets make it more than a 5a job. |
| Shutdown with a player online untested | Valid (ShutdownHook.java:50 fast exit; delay 120) | Q7, S-12 required, checklist 13 |
| In-world CMs outside P5-00 | Valid (bodies read: 7 need no service) | C-01, D7, the summary accessor |
| Time-driven packets | Valid | §5.9, M0, the game hour passed to the oracle |
| Harness details | Valid (database.properties; env var names; LS CTRL_BREAK handler) | F-04, §5.1. An LS `--stop-file` is **rejected**: CTRL_BREAK needs no LS change. |
| Smoke and M4 tests break | Valid (RunStartupSmoke.cmake:62) | F-02 |
| PvpMapService.onLogin, ChatProcessor init | Valid (PlayerEnterWorldService.java:384; GameServer.java:99) | W-03, F-03, F-01a stream |

## 10. Stage 3: the leak evidence of the gate

Five findings of the stage-3 review of what `gs.scenario.m5a` actually proves about lifetimes, with the measurement each was settled by. The
numbers below come from a checked RelWithDebInfo gate run of this tree (`live_counts.txt`, columns live / created / class); the same rows of the
stage-2 runs of the app-gate, packets, svc-a, svc-b, world and final-gate lanes agree.

### 10.1 Q8's live-instance assertion is kept, not weakened

Two reviews disagreed: one held that Player "can never be 0 while case 7 shuts the server down with a character online", so that a faithful port
must fail the assertion; the other, that the live-count check is the gate's only leak check for anything outside `World`, and that weakening it on
a hypothesis would remove the check. **The measurement settles it for the second.**

Java's shutdown does log the character out. `ShutdownHook.run` (ShutdownHook.java:45-72) counts down, leaves the loop as soon as
`World.getAllPlayers().isEmpty()` (line 50) and then calls `GameServer.shutdownNioServer()` (line 72), which "disconnects cs/ls/all players and
saves them". Per connection that ends in `AionConnection.onServerClose` → `close()` + `safeLogout()` (AionConnection.java:264-267), and
`safeLogout` calls `PlayerLeaveWorldService.leaveWorld(player)`, whose **last statement** is `con.setActivePlayer(null)`
(PlayerLeaveWorldService.java:152; the port does the same at PlayerLeaveWorldService.cpp:178). A connection therefore holds no Player after the
shutdown logout, online at the stop file or not. What it does keep is the account level, because `onDisconnect` returns right after `safeLogout()`
while `isShuttingDownSoon()` (AionConnection.java:240-243, ported at AionConnection.cpp:273-274), i.e. before `LoginServer.onDisconnect`, and
LoginServer.java:119 is the only place that unregisters the connection — which is exactly why Q8 bounds those classes by the number of open
connections instead of demanding 0.

Case 7 shuts the server down with account B in the world, and one client connection was open when the stop file was written:

```
live created class                                 live created class
0      6   model::gameobjects::player::Player          1     3   model::account::Account
0     78   model::gameobjects::Item                    1     3   model::account::AccountTime
0      4   model::gameobjects::player::AbyssRank       1     3   model::gameobjects::player::PlayerCommonData
0    330   world::knownlist::KnownObject               1     6   model::gameobjects::player::PlayerAppearance
0      2   services::…::HpMpRestoreTask                1     4   network::aion::…::ConnectionAliveChecker
0    179   questEngine::model::QuestEnv                1   207   model::items::storage::ItemStorage
                                                   82127 82131   model::gameobjects::Npc
```

Six Players were created and none is alive; `PlayerAccountData` is even at 0 where the per-connection bound would allow 1. **No lane may relax the
strict 0 for Player, AbyssRank or the interaction tasks**: the port passes it as written, and a run in which it failed would be a real leak.

**Item is the one row of that sentence that had to move** (found by the stage-3 review of wave A, which read the login path instead of the table).
`0 78 model::gameobjects::Item` looks as strict as the rest, but the 0 is a property of the scenario's data, not of the port. A login loads the
**account** warehouse — `AccountService::loadAccountWarehouse` → `InventoryDAO::loadStorage` + `ItemStoneListDAO::load`, AccountService.cpp:98-104
(Java AccountService.java:95-100) — and the logout only detaches its owner: `player.getAccount()->getAccountWarehouse().setOwner(nullptr)`,
PlayerLeaveWorldService.cpp:170 (Java PlayerLeaveWorldService.java:146). The `Storage` stays on the `Account`, and an Account whose connection
never reached `LoginServer::onDisconnect` survives the shutdown — the early return above, the same one that bounds the account-level classes. That
is precisely the `1 207 model::items::storage::ItemStorage` row of the table: the account warehouse of the one connection that was still
registered. **Its items are alive for the same faithful reason.** The two scenario accounts have an *empty* account warehouse, so the strict 0
passed by accident; the first real-client run, or an M5b scenario that leaves one item in an account warehouse, would have failed the gate for
correct behaviour, and §10.1 as first written forbade fixing it.

`Item` therefore moved to `CheckOutput::accountBoundedLiveClasses()`, the connection-bounded rule the storages already use, in the only form the
server process itself can apply: it is checked for **0 while no `model::account::Account` survived the shutdown** (no Account means no surviving
warehouse, so every item of the run must be gone) and is otherwise reported as a WARN naming the count and left to the reader of
`live_counts.txt`. The bound itself belongs to whoever knows the warehouses — the gate counts the connections it left open and fills the
warehouses it uses (§5.7 Q8). **Follow-up, not this lane's file:** `M5aScenarioTest.cpp` still lists `Item` in `strictlyZeroLiveClasses()`, where
it has the same accidental pass; it belongs in the `perConnectionLiveClasses()` branch (bounded by the items the harness put into the warehouses
of the accounts still connected, i.e. 0 for today's scenario). The Q8 row of §5.7 still spells the old rule ("**0** for Player, Item, AbyssRank
and the interaction tasks") and needs the same correction.

### 10.2 The classes the leak check covers

The gate asserted 6 classes out of the 148 in `live_counts.txt`, and none of the objects wave 5a added. The strict-zero list now lives in the
server, in `CheckOutput::zeroLiveClasses()`, so a leak fails the run through its own ERROR line and through `m5a_summary.txt` (`liveLeaks`,
`liveLeak <class> <live>`) even where the gate reads nothing: Player, AbyssRank, BlockList, Cooldowns, Macros, PlayerSettings,
QuestStateList, RecipeList, PlayerSkillList, PlayerSkillEntry, StatFunctionProxy, **KnownObject** and **HpMpRestoreTask** (this wave's visibility
entries and the restore task that pins a Player in Q3), ItemInfoBlob, SkillEntryWriter, LoginServer::LoginRequest and QuestEnv — every one of them
created and back at 0 in each gate run kept so far — plus `Item` under the account-bounded rule of §10.1
(`CheckOutput::accountBoundedLiveClasses()`: strict while no Account survived the shutdown, a WARN with the count when one did).

Seven further entries — Summon, Pet, Kisk, AbstractInteractionTask, GatheringTask, GatheringTask_ActionObserver, StanceObserver — are **guards,
not assertions**: the scripted path has no summon, pet, kisk or gathering, so none of them is ever created and **no gate run can fail on those
rows today**. They start to mean something in G-01, with the real client and in M5b, and they are written down here so nobody reads the length of
the list as coverage. The rows that the M5a gate does exercise are the ones above them, and the check itself (both the class list and the ERROR
line) is pinned by `CheckOutputTest.TheLiveCountCheckReadsTheProcessCountersOfAZeroClass` and
`…BoundsAccountWarehouseItemsByTheSurvivingAccounts`.

**The check only exists in a checked build.** `runtime::LIVE_COUNTS_ENABLED` is `AION_CHECKED != 0`: in a release build `makeRef` and the
Reclaimer count nothing, `live_counts.txt` has no rows and `checkLiveCounts()` returns an empty vector whatever the run leaked — a green
`liveLeaks 0` that means "not measured". The check says so itself (one WARN: "nothing is counted in this build"), and `m5a_summary.txt` carries
the row **`liveCountsEnabled true|false`**. A gate that relies on this check must assert that row is `true`; the M5a gate builds the server from
the same checked build tree as its tests, so it is `true` there today (*follow-up for the P5-SC owner: assert it in `M5aScenarioTest.cpp` next to
the `live_counts.txt` rows, so a release build cannot report a vacuous pass*).

Two groups stay out, and neither is an oversight. **Npcs and the rest of the world** (Gatherable, StaticObject, House, …) are still alive at
shutdown — 82,127 of 82,131 Npcs in the run above — because the shutdown does not despawn the world; Java keeps them too, and a leak check that
demanded 0 would be wrong rather than strict. **The account-level classes** (Account, AccountTime, PlayerAccountData, PlayerCommonData,
PlayerAppearance, ConnectionAliveChecker, `*Storage`) are bounded by the number of connections still open, which the process cannot know while it
writes its report; the gate checks them, because it knows how many clients it left open (§5.7 Q8). Creature-attached observers
(AttackCalcObserver, ShieldObserver) are out for the same reason as npcs: their creature stays in the world.

### 10.3 Zombie cuts and stale pins now describe the run

Both rows were decoration, for three reasons: `zombie_break_minutes` is 30 and `LeakCensus::stalePinAfter` 10 minutes against a run of one to three
minutes, `runFinalCensus` switched the zombie breaker **off** before the final scan, and — fatally — no log message in the tree contained the string
`"stale pin"` that the gate greps for, so that assertion could not have failed even with a stale pin in front of it.

`CheckOutput::runFinalCensus` now ends with `runBreakerPass()`: after `census.txt` is written it scans once more with `zombieBreakAfter`,
`stalePinAfter` and `stalePinCheckInterval` at 0 and the breaker on, drains the instant pool so the posted breakers have run, and reclaims again;
`LeakCensus` logs "Leak census: stale pin: periodic task … still pins …". Both rows therefore report what is left at the end of the run. They
remain **attribution rather than independent checks**: only an object that left the world and is still referenced can be in the table, which is
what `census.txt` already reports — the cut names the cyclic edge and the stale pin names the task that holds the leak. The timed behaviour (a cut
after 30 minutes, a stale pin after 10, the periodic machinery of D7) is still not exercised by the gate; `LeakCensusTest` proves it on a
ManualClock, and G-01 keeps the second-granularity keys for a 30-minute run.

The pass itself is now pinned where it is *called*, not only where its effect is reproduced: `LeakCensusTest` configures the zero thresholds by
hand, so deleting the `runBreakerPass()` call from `runFinalCensus` used to keep every test and the whole gate green (the gate's two rows assert
absence). `CheckOutputTest.FinalCensusEndsWithTheZeroThresholdBreakerPass` runs the real `runFinalCensus` over a two-object cycle that only a
breaker can cut, and fails with "cuts 0, expected 2" when the call is gone.

### 10.4 W-07's notify-failure counter

`KnownList::notifyFailureCount()` is wired: each of the three catches counts it (`notifySee`, `notifyNotSee`, `notifyNotKnow`,
KnownList.cpp:224-250), `CheckOutput::writeSummary` writes the `knownListNotifyFailures` row, §5.7 Q8 asserts it is `0` and
`KnownListTest` checks that a throwing controller increments it. The finding that the counter has no caller predates commit 7ce0f3b3f; a gate run
of this tree reports `knownListNotifyFailures 0`.

### 10.5 A throwing periodic body leaves the cycle that `stop()` would have cut

`cycles.toml` resolves `AbstractInteractionTask$1#this` with "finish / abort cancel the periodic task". The cycle is task → Future → body → task,
and `stop()` is its only cut, so a body that throws before reaching `stop()` leaves it: `runInteraction` throws, the exception is logged inside the
body and the task is re-armed with its captures (Future.h:89), which is what Java does as well —
`ThreadPoolManager.scheduleAtFixedRate` wraps the Runnable in `RunnableWrapper(catchAndLogThrowables = true)` (ThreadPoolManager.java:60-62), and
`AbstractInteractionTask.stop()` is likewise only reached from a body that returned (AbstractInteractionTask.java:68-75). The port is faithful, so
no deviation is added and the `stop()` call sites stay as Java has them. It is reproduced in
`LeakCensusTest.AThrowingPeriodicBodyLeavesTheSelfRetainingCycleThatStopWouldHaveCut`: after three throwing runs the Player is still alive, the
census reports it with its pinning task, the stale-pin warning names that task, the zombie breaker cuts nothing (a Pin is not an edge a breaker can
cut), and a run that does reach `stop()` releases both objects. Two consequences for the reader of a run: such a body also writes
"Exception in a Runnable execution: …" as ERROR, which Q8 fails on anyway, and the cycles.toml wording should say that `stop()` is the only cut and
that a body which throws leaves the cycle for the stale-pin report to name.

## 11. Stage 3 wave B (open after wave A)

Wave A closed the startup half of the geo debt, the silent-skip hole, the assertions that could not fail, the leak surface and the vacuous tests
(commit of 2026-09-21). What its five reviewers left open, in the order it should be taken:

| Item | Why it is open |
|---|---|
| ~~**Geo enter-world**~~ | **Done (wave B): `gs.scenario.m5a_geo`.** The gate enters the world, sees the oracle's npcs and gatherables, revalidates its zones through `CM_SUBZONE_CHANGE`, walks a region border and quits, all with `gameserver.geodata.enable=true`, and asserts the live-instance counts a geo-off run cannot produce (`Terrain`, `TerrainZoneCollisionMaterialActor`, `MaterialZoneHandler`), the difference between them and the pre-client baseline (GEO4), and the Q8 report bar. Of the three assertions this row asked for, **none exists in 4.8**: no npc spawn z is geo-corrected, no player move is geo-corrected, and `canSee` decides nothing a client is told about — it is reachable only through the gather dialog, i.e. through an M5b client packet. What replaced them: V1-V4 against the oracle *in a geo-built world* (1 cm tolerance), which is exactly what a terrain-z snap on the spawn path breaks — mutation-proven, and the same mutation leaves `gs.scenario.m5a` green, which is the reason this gate exists. §5.1 "Geodata" carries the evidence, the numbers and the line numbers. |
| **G-01 stress nightly** | `gs.scenario.m5a_stress`: 20 FakeGameClients for 30 minutes, ASan and checked, injected DAO exceptions during logout, logouts below full HP, census and zombie-breaker thresholds in seconds. Asserts no reused-id warnings, zombie cut count 0, an empty final census. |
| **Unported client packets** | The real-client session logged nine: `CM_TARGET_SELECT`, `CM_EMOTION`, `CM_USE_ITEM`, `CM_MOVE_ITEM`, `CM_FRIEND_STATUS`, `CM_SHOW_BLOCKLIST`, `CM_PLAYER_LISTENER`, `CM_INSTANCE_INFO`, `CM_CHECK_PAK` (m5a-client-session.md F-2). Targeting first: it is the one a player notices. |
| **Low findings of wave A** | ~~A failing geo run leaves its schema behind~~ (done: `ScenarioServers::dropSchemas()`, and a failed run drops them too unless `AION_SCENARIO_KEEP_SCHEMAS` is set — the gate prints the way back in its failure message); ~~`stopProblems()` latches on read and its destructor report has no permanent test~~ (done: the reader is pure, `stopProblemsReported()` takes the destructor's word away, and `ScenarioServersTest.TheDestructorReportsStopProblemsNobodyReported` pins the net with `EXPECT_NONFATAL_FAILURE`); ~~V2's rule for pool-only and randomWalk-only npc ids, and a genuine level-0 npc~~ (done: `isPinnedToFixedSpots()`, `OracleSpot::level` is an `optional`, both pinned in `OracleTest`); ~~§5.1's rows for the login server child's log directory and the run model are stale~~ (done, and §5.1 now has an "LS log directory" row of its own). ~~the `cycles.toml` wording for `GatheringTask`~~ (done in the runtime-lows lane: `abort()` already cuts the cycle at logout, so the proposed rewording was the less accurate text); ~~`MaterialZoneHandlerTest` sits in the wrong chunk~~ (struck: `chunks.py owner` puts `MaterialZoneHandler.cpp` and its test both in P4-10, so there was never anything to move - P5-12a owns `SiegeShield`, whose test is in `tests/siege`). **Still open:** the `knownListNotifyFailures` row's VALUE is pinned by nothing - a literal 0 in `writeSummary` keeps every test and both gates green (P5-14). |
| **Second race and gliding** | The real-client checklist steps 12 and 13 were not exercised: no Asmodian character, no gliding or jumping, no shutdown with a character online from a real client. |
