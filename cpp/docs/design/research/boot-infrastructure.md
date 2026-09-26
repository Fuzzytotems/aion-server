# boot-infrastructure

## Summary
The game server has no game loop. GameServer.main() (GameServer.java:92-191) is a long, mostly sequential chain of lazy singleton getInstance()/init() calls. It loads config, sets up the DB, thread pools and Quartz cron, then the ID factory, static data, script engines (in parallel), world, spawns, services and schedules, and finally networking. After that, all game logic runs as callbacks on many concurrent threads:
- client packets: a PacketProcessor with 4 threads by default, serial per connection
- ThreadPoolManager: a ScheduledThreadPoolExecutor with max(4, cores) threads, an 'instant' pool with max(4, cores) threads, and an unbounded cached 'long running' pool
- one Quartz thread that hands work to those pools
- the ForkJoin common pool: MoveTaskManager moves every NPC with parallelStream every 200 ms
- a java.util.Timer thread (FloodManager/NetFlusher) and a java.lang.ref.Cleaner thread.
'Ticks' are 14 AbstractPeriodicTaskManager/FIFO subclasses (200 ms-4 s), about 44 scheduleAtFixedRate call sites in src and 82 in handlers, 21 cron schedules, and GameTimeService (1 game minute per 5 s). Known-list/visibility updates run synchronously on whatever thread moves the object.
Timer use is very heavy. There are 854 direct ThreadPoolManager.schedule/scheduleAtFixedRate call sites (171 in src, 683 in data/handlers), plus 15 execute/submit sites, 21 CronService.schedule sites, 14 periodic managers, and about 100 indirect delayed broadcasts through PacketSendUtility (handlers). Almost every site captures a game object, directly or through `this` (an AI, instance handler, effect or controller). Only about 30 of the 171 src sites are global service timers.
Cancellation is always non-blocking Future.cancel(bool): tasks keep running after despawn and guard themselves with isDead()/isSpawned() checks (222/22 occurrences in files that schedule).
Object lifetime is partly driven by the GC. AionObject registers a Cleaner that releases the objectId to IDFactory only when an Npc, Gatherable, StaticObject, Summon, HouseObject or team object becomes unreachable. IDFactory reuses the lowest freed ID immediately, so a bare objectId is not a safe handle. RespawnService already logs this reuse ('ObjectId ... got released and reassigned').
Config is 33 @Property classes (420 @Property + 5 @Properties fields) and is re-bound at runtime by Event.start/stop, //reload config, ChatProcessor.reload and //configure (reflection). Static data is also swapped at runtime: //reload assigns new DataManager.QUEST_DATA/SKILL_DATA/ITEM_DATA/... objects while live objects still reference old templates.
Handlers are loaded by 5 separate ScriptManager+ClassListener setups (AI, quest, instance, zone, chat commands) that register by class type and annotations: @AIName x457, @InstanceID x73, @ZoneNameAnnotation x2, and no @OnClassLoad/@OnClassUnload usage.
For C++, the evidence strongly favours:
1. one game-logic executor (a strand or single thread) that runs packets, timers and cron callbacks, with blocking DB work optionally moved off it
2. a Java-shaped scheduler API returning cancellable futures
3. an ownership model in which a scheduled lambda cannot touch freed memory and objectId release happens at real end of life, not at despawn.

## Inventory
## 1. Startup sequence (GameServer.java:92-191, initUtilityServicesAndConfig 214-231)

| # | Step (line) | What it does / loads | Threads / async |
|---|---|---|---|
| 0 | static `Logging.init()` (73) | logback | |
| 1 | `JAXBUtil.preLoadContextAsync(StaticData.class)` (93) | Builds the JAXB context early | ForkJoin common pool (JAXBUtil.java:31) |
| 2 | `Thread.setDefaultUncaughtExceptionHandler` (216) | | |
| 3 | `PropertyTransformers.register(CronExpressionTransformer)`; `Config.load()` (218-219) | ./config/{administration,main,network}/* as defaults, then ./config/mygs.properties, then **EventService.getInstance().getActiveEventConfigProperties()** (Config.java:48) into 35 classes (33 GS + CommonsConfig + DatabaseConfig); local IP discovery for CLIENT_CONNECT_ADDRESS | Constructs the EventService singleton before DataManager exists (its constructor is empty) |
| 4 | `DatabaseFactory.init()`, `PlayerDAO.setAllPlayersOffline()`, optional `DatabaseCleaningService.deletePlayersOnInactiveAccounts()` (221-224) | DB | |
| 5 | `ThreadPoolManager.getInstance()` (227) | Starts the pools and DeadLockDetector (1 min timeout, then exit RESTART) | |
| 6 | `CronService.initSingleton(ThreadPoolManagerRunnableRunner, TZ)` (230) | Quartz, 1 thread | |
| 7 | `IDFactory.getInstance()` (96) | BitSet locked with the used IDs of 8 DAOs: Player, Inventory, PlayerRegisteredItems, Legion, Mail, Guide, Houses, PlayerPets (IDFactory.java:133-142) | |
| 8 | `DataManager.getInstance()` (98) | XmlMerger merges static_data into a cache XML, then one JAXB unmarshal into StaticData and assigns about 90 static DataManager fields. XSD validation runs in the background (submitLongRunning) and is awaited at step 30 | Long-running pool; afterUnmarshal tasks go through submitLongRunning (StaticDataListener.java:32) |
| 9 | `Stream.of(QuestEngine, AIEngine, InstanceEngine, ChatProcessor, ZoneService, GeoService).parallel().forEach(GameEngine::init)` (100-101) | Script compilation of data/handlers/{quest,ai,instance,admincommands+playercommands+consolecommands,zone} and registration; GeoService loads 159 MB of geo (GeoWorldLoader parallelStreams, collision data built on executeLongRunning) | ForkJoin common pool; QuestEngine also runs QuestSpawnAnalyzer on executeLongRunning (QuestEngine.java:108) |
| 10 | `World.getInstance()` (104) | One WorldMap per template, created with forEachParalllel (World.java:61) | ForkJoin |
| 11 | `GameTimeService.getInstance()` (105) | Loads the game time | |
| 12 | `DropRegistrationService` (107) | | |
| 13 | BaseService, SiegeService, WorldRaidService.initWorldRaidLocations, VortexService.initVortexLocations, RiftService.initRiftLocations, LegionDominionService.initLocations (110-116) | Location objects only (no spawns) | |
| 14 | HousingService (HousesDAO.loadHouses), HousingBidService (HouseBidsDAO), AuctionEndTask / AuctionAutoFillTask / MaintenanceTask (AbstractCronTask: run-on-start check + cron), ChallengeTaskService (118-123) | DB + cron | AbstractCronTask uses a semaphore + executeLongRunning |
| 15 | `SpawnEngine.spawnAll()` (125) | Spawns every non-instance map with forEachParalllel (SpawnEngine.java:120) | ForkJoin |
| 16 | TownService (TownDAO), FlyRingService (spawns rings), RiftService.initRifts (126-128) | | |
| 17 | Race ratio counts from PlayerDAO if ENABLE_RATIO_LIMITATION (130-134) | | |
| 18 | LimitedItemTradeService.start (cron per limited item), PlayerLimitService.scheduleUpdate (cron) (135-137) | | |
| 19 | SiegeService.initSieges (cron), BaseService.initBases, WorldRaidService.initWorldRaids (cron), ConquerorAndProtectorService.init (fixed rate) (141-147) | | |
| 20 | AnnouncementService, DebugService (fixed rate), WeatherService, BrokerService (DB + FIFO manager + fixed rate), Influence, ExchangeService, PeriodicSaveService, AtreianPassportService (cron), CronJobService (cron + schedule) (149-157) | | |
| 21 | CuringZoneService if !GEO_MATERIALS_ENABLE, RoadService (spawns per instance), HTMLCache (Java-serialized cache file), AbyssRankingCache, AbyssRankUpdateService.scheduleUpdate (2 crons), PeriodicInstanceManager (crons), EventService.start (cron every 5 min) (159-166) | | |
| 22 | AdminService, CommandsAccessService.loadAccesses, PlayerTransferService (168-171) | | |
| 23 | `GameTimeService.startClock()` (173) | 2 fixed-rate tasks: +1 game minute every 5 s; broadcast + save every 3 min | |
| 24 | PvpMapService.init (creates an instance with `PvpMapHandler::new`), CustomInstanceService (175-176) | | |
| 25 | `DataManager.waitForValidationToFinishAndShutdownOnFail()` (177) | | |
| 26 | `System.gc()`, version/system info logging (179-182) | | |
| 27 | `initNioServer()` (184, 196-203) | Throws if nio.threads > 1 unless unsafe.allow ("the game server is not thread-safe"); `nioServer.connect(ThreadPoolManager)` | 1 acceptor + 1 read/write thread |
| 28 | `Runtime.addShutdownHook(ShutdownHook)` (185) | ShutdownHook constructor schedules the RESTART_SCHEDULE cron with CurrentThreadRunnableRunner | |
| 29 | LoginServer.connect, ChatServer.connect if enabled (188-190) | Reconnects via schedule(...) (LoginServer.java:82,105; ChatServer.java:64,82) | |

Shutdown (ShutdownHook.run, 45-83): countdown with SM_SYSTEM_MESSAGE broadcasts (skipped when no players), then `GameServer.shutdownNioServer()` (disconnects LS/CS/players; AionConnection.onServerClose calls safeLogout → PlayerLeaveWorldService.leaveWorld), RunnableStatsManager dump, PeriodicSaveService.onShutdown (legion WH + server run time), GameTimeService.saveGameTime, CronService.shutdown, ThreadPoolManager.shutdown (5 s await), logback stop. `initShutdown(exitCode, delay)` calls System.exit on a virtual thread. dist/start.bat restarts on exit code 2 (ExitCode.RESTART).

## 2. Threads in the Java game server

| Thread / pool | Type, size | Runs | Evidence |
|---|---|---|---|
| NIO acceptor + read/write | commons NioServer, `gameserver.network.nio.threads=1` (>1 forbidden) | accept, read, **pck.read()** (packet decode), write | GameServer.java:197; AionConnection.java:188 |
| PacketProcessor | min=max=4 by default, serial per connection | CM_* runImpl = most player-driven game logic | AionConnection.java:46; NetworkConfig PACKET_PROCESSOR_* |
| instantPool | ThreadPoolExecutor with max(4, cores) threads (or base_pool_size), queue 100000, AionRejectedExecutionHandler | execute(): LS/CS packets (LoginServerConnection.java:80, ChatServerConnection.java:72), cron jobs, MapRegion activation, NIO disconnect tasks | ThreadPoolManager.java:35 |
| scheduledPool | ScheduledThreadPoolExecutor with max(4, cores) threads, delayed tasks dropped on shutdown | all schedule/scheduleAtFixedRate calls (854 sites) | ThreadPoolManager.java:40-43 |
| longRunningPool | Executors.newCachedThreadPool (unbounded) | executeLongRunning/submitLongRunning: static data validation, geo collision data, AbstractCronTask init, long cron jobs, neural network training | ThreadPoolManager.java:45 |
| Quartz scheduler | threadCount=1; hands off to instant/long pool | 21 cron call sites | CronService.java:40 |
| ForkJoin common pool | cores-1 | parallel startup steps, MoveTaskManager.run every 200 ms (parallelStream), WorldMap3DInstance region creation | MoveTaskManager.java:40 |
| java.util.Timer "NetFlusher" | 1 daemon thread | FloodManager flush | NetFlusher.java:11, FloodManager.java:137 |
| Cleaner | 1 | objectId auto-release on GC | AionObject.java:14-31 |
| DeadLockDetector | 1 | exit(RESTART) after a 1 min deadlock | ThreadPoolManager.java:34 |
| ShutdownHook | 1 | countdown + save | |

Every task is wrapped in RunnableWrapper(r, ThreadConfig.MAXIMUM_RUNTIME_IN_MILLISEC_WITHOUT_WARNING=5000, true), which catches and logs exceptions and warns on slow tasks. ThreadConfig: base_pool_size=0, scheduled_pool_size=0 (0 means cores), runtime=5000, usepriority=false.

## 3. Scheduling call sites (perl multi-line regex over all .java files)

| Call | src | data/handlers | total |
|---|---|---|---|
| ThreadPoolManager...schedule( | 127 | 601 | 728 |
| ...scheduleAtFixedRate( | 44 | 82 | 126 |
| ...execute( | 6 | 1 | 7 |
| ...executeLongRunning( | 5 | 0 | 5 |
| ...submit / submitLongRunning | 1 / 2 | 0 | 3 |
| CronService.getInstance().schedule | 21 | 0 | 21 |
| Periodic manager subclasses (base schedules at fixed rate) | 14 | 0 | 14 |
| Delayed PacketSendUtility.broadcastMessage(npc,id,delay) / broadcastToMap(...,delay) (regex, approximate) | 1 / 0 | 50 / 51 | about 100 |

- Files using ThreadPoolManager: 139 in src, 306 in handlers.
- Schedule sites by area: handlers/ai 393, handlers/instance 245, handlers/quest 39, src/skillengine 24, src/model/templates (item actions) 18, src/custom 11, src/ai 11, src/services about 60, other src remainder.
- Future bookkeeping (src/handlers):
  - `Future<?>` field declarations: 46/120
  - collections of Futures: 6/13
  - `.cancel(true)`: 15/139
  - `.cancel(false)`: 30/21
  - `isDone()`: 7/65
  - `isCancelled()`: 10/64
  - `controller.addTask(TaskId, future)`: 51/42
  - `cancelTask(TaskId)`: 42/9
- CreatureController keeps `ConcurrentHashMap<Integer, Future<?>> tasks` keyed by the 22-value TaskId enum. addTask cancels any previous task, and onDelete → cancelAllTasks uses cancel(false), which does not wait for a running task (CreatureController.java:368-427).

Capture classification (heuristic parse of 807 of the 854 sites' argument text):
- 521 name a game-object identifier (player/npc/creature/effected/owner/instance/...).
- Most of the rest capture one implicitly through `this`: AI methods, instance-handler helpers like `sp(...)`, AggregatedList, an Effect, `new MaterialSkillTask()` inner classes.
- Manual review of all 171 src sites: about 30 capture no game object (LS/CS reconnect x4, GameTimeService x2, SiegeService x2, VortexService, WeatherService, SurveyService, PlayerGroup/AllianceService offline checkers, BrokerService, DebugService, PeriodicSaveService, CronJobService x2, ConquerorAndProtector, PeriodicInstanceManager, LegionDominion/RiftOpenRunnable, WorldRaid stop, AbstractPeriodicTaskManager base, AuctionEndTask (houseObjectId)).
- Only 3 src sites deliberately capture an **ID and look the object up again**: GeneralUpdateTask(player.getObjectId()) and ItemUpdateTask (PlayerEnterWorldService.java:369-371, lookup via World.getPlayer at 507), and DecayTask(objectId) (RespawnService.java:59).
- In data/handlers, 121 of the 219 AI files that schedule never call cancel( (fire-and-forget, e.g. ai/classNpc/EnemyServantAI.java:27-37, DeliveryManAI.java:32).
- 17 of the 44 instance handler files that schedule have no onInstanceDestroy.

## 4. Periodic "tick" managers (taskmanager + others)

| Class | Base | Period | Collection / work |
|---|---|---|---|
| MoveTaskManager | Periodic | 200 ms | ConcurrentHashMap<Integer, Creature>; **parallelStream** moveToDestination + AI MOVE_ARRIVED/VALIDATE events |
| PlayerMoveTaskManager | Periodic | 200 ms | players moveToDestination |
| MovementNotifyTask | FIFO<Creature> | 500 ms | CREATURE_MOVED AI event to known NPCs (Reshanta limit 200) |
| ZoneUpdateService | FIFO<Creature> | 500 ms | zone revalidation |
| TeamStatUpdater | FIFO<Player> | 500 ms | group/alliance MOVEMENT updates |
| TeamMoveUpdater | FIFO<Player> | 2000 ms | |
| ExpireTimerTask | Periodic | 1000 ms | Map<Expirable, Player>, 30/15/10/5/1 min warnings |
| TemporaryTradeTimeTask | Periodic | 1000 ms | |
| LegionDominionIntruderUpdateTask | Periodic | 4000 ms | |
| BrokerService.BrokerPeriodicTaskManager | FIFO<BrokerOpSaveTask> | DELAY_BROKER_SAVE | DB saves |
| AuctionEndTask, AuctionAutoFillTask, MaintenanceTask | AbstractCronTask | cron from HousingConfig | run-on-start if missed (ServerVariablesDAO "serverLastRun") |
| GameTimeService | 2 × fixed rate | 5 s / 3 min | |
| LifeStatsRestoreService | fixed rate per creature | 1-3 s | HP/MP/FP regen tasks per creature |
| AbstractOverTimeEffect / AuraEffect / Confuse / Fear | fixed rate per effect | checktime / 6.5 s / 1 s | |
| AggroList hate reduction, AbstractMaterialSkillActor, ZoneLevelService drowning, CuringZoneService | fixed rate | 1-10 s | |
| PlayerEnterWorldService General/Item/Pet update | fixed rate per player | PeriodicSaveConfig | DB |

The periodic managers are lazy singletons: AbstractPeriodicTaskManager's constructor schedules on the first getInstance() (AbstractPeriodicTaskManager.java:17-19, first run after Rnd 500-550 ms).

## 5. ID factory (utils/idfactory/IDFactory.java, 191 lines)
- A BitSet under a ReentrantLock. `nextId()` returns the next clear bit ≥ nextMinId and skips the client-invisible pattern (mask 0b0010011111100110101111111111100 == 6484 pattern).
- `release` resets nextMinId to the released ID, so **the lowest freed ID is reused next**.
- Initialized from 8 DAO getUsedIDs.
- Call sites: nextId 28 (1 in a static initializer: CustomInstanceService LEADERBOARD_WINDOW_OBJECT_ID), releaseId 9, releaseObjectIds 3 (InventoryDAO, PlayerRegisteredItemsDAO).
- Automatic release: `AionObject(objId, autoReleaseObjectId=true)` registers a `Cleaner` whose action is RespawnService.setAutoReleaseId(objId) or releaseId. Used by Npc.java:57, Gatherable.java:17, StaticObject.java:15, Summon.java:41, HouseObject (flag), GeneralTeam/TemporaryPlayerTeam (flag).

## 6. Global singletons / registries
- 93 src classes define `getInstance()` (61 in services); 66 use the SingletonHolder idiom.
- DataManager has 94 `public static` fields.
- 56 DAO classes, all static methods.
- getInstance() call count: 1322 in src, 1344 in handlers. Top handler targets: ThreadPoolManager 685, SkillEngine 281, World 73, CraftSkillUpdateService 44, SiegeService 36, QuestEngine 23.
- Static mutable state in GameServer (ELYOS/ASMOS counts behind a ReentrantLock).
- Handler registries: AIEngine.aiHandlers (Map name → Class, @AIName), InstanceEngine (@InstanceID classes), ZoneService (@ZoneNameAnnotation classes), QuestEngine (instantiated handler objects), ChatProcessor.commandHandlers (Map alias → instance).
- Handler file counts: quest 1035, ai 461, admincommands 101, instance 78, consolecommands 35, playercommands 16, zone 3.

## 7. Config (configs/**, 1.9k lines)
- 33 classes with 420 @Property and 5 @Properties.
- Field types: int 146, boolean 123, byte 33, float[] 30, float 18, CronExpression 12, long 10, String 9, Pattern 6, Set<Integer> 5, CronExpression[] 5, InetSocketAddress 4, File 4, Set<String>/String[]/List<String> 2 each, Map (AbyssRankEnum, String→Byte, Integer→Integer, HouseType→Integer), ZoneId, and 4 enums.
- Largest classes: CustomConfig 62, LegionConfig 40, RatesConfig 30, SecurityConfig 26, NetworkConfig 25, AdminConfig 23.
- Reads: 638 in src, 95 in handlers.
- JAXB-based configs (not properties): config/schedule/{rift,siege,world_raid}_schedule.xml (RiftSchedule, SiegeSchedules, WorldRaidSchedules), config/ingameshop/in_game_shop.xml (InGameShopProperty).

Runtime writers:
1. `Config.load()` from Event.start/stop when the event has config properties (Event.java:82,134); event properties are layered over mygs.properties.
2. //reload config (Reload.java:81).
3. `Config.load(CommandsConfig.class)` in ChatProcessor.reload (ChatProcessor.java:40).
4. //configure sets any static field by reflected name via ConfigurableProcessor.transform (Configure.java:39-81).
5. //ai toggles AIConfig.ONCREATE_DEBUG/EVENT_DEBUG/MOVE_DEBUG (Ai.java:46-52).
6. Config.load assigns NetworkConfig.CLIENT_CONNECT_ADDRESS (Config.java:64).

## 8. Static data runtime reload (//reload, Reload.java)
Reassigns DataManager.QUEST_DATA (+ XML_QUESTS.setData, QuestEngine.reload), SKILL_DATA, NPC_SKILL_DATA.setNpcSkillTemplates, ITEM_DATA, CUSTOM_NPC_DROP, EVENT_DATA.setEvents (EventService stop/start), UPGRADE_ARCADE_DATA, DECOMPOSABLE_ITEMS_DATA. It also runs AIEngine.reload and ChatProcessor.reload.

## 9. Cron (services/cron, 314 lines; Quartz)
- `schedule(Runnable, String|CronExpression, longRunning)`, `cancel(JobDetail|Runnable)`, `findJobDetails`, `findJobs(Class)`, `findNextFireTimes(Class)` (used by SiegeService.java:323).
- CronExpression.getTimeAfter is used 3 times (AbstractCronTask).
- Expression features seen: seconds field, `?`, day-of-week names and lists (`TUE,THU,SAT`), hour lists, `0/5` increments, day-of-month lists (`1,16`).
- Time zone: GSConfig.TIME_ZONE_ID.
- A Java test exists: test/com/aionemu/gameserver/services/cron/CronServiceTest.java.

## 10. utils / cache / restrictions / custom
- utils (about 5.8k lines):
  - PacketSendUtility (289 lines; broadcasts via KnownList/World/MapInstance/Zone, optional delay through scheduleOrRun)
  - PositionUtil 397, ChatUtil 403, stats/StatFunctions 698 (pure formulas)
  - collections: SplitList family used at 24 sites for packet splitting; CollectionUtil; Predicates
  - chathandlers: ChatCommand/AdminCommand/PlayerCommand/ConsoleCommand base classes, ChatProcessor, ChatCommandsLoader
  - time: ServerTime (ZonedDateTime in the server TZ; 46 uses in src), GameTime
  - xml: JAXBUtil, XmlUtil (XSD validation), CompressUtil (Deflater/Inflater)
  - captcha: CAPTCHAUtil + DDSConverter use java.awt text rendering into a DDS image, used by GatherableController.java:80
  - audit: AuditLogger, AutoBan, GMService
  - cron/ThreadPoolManagerRunnableRunner
- cache/HTMLCache (254 lines): loads data/static_data HTML, compacts it, and caches it with **Java ObjectOutputStream serialization**.
- restrictions/PlayerRestrictions (390 lines): static canX(Player, ...) checks; 24 call sites in src, 2 in handlers.
- custom (2.4k lines):
  - pvpmap: PvpMapService + PvpMapHandler (858 lines), an instance handler living in src and created with `PvpMapHandler::new`
  - instance: CustomInstanceService + RoahCustomInstanceHandler (431 lines) + a small neural network (PlayerModel*, trained on executeLongRunning)

## 11. Concurrency primitives
- src: 254 synchronized methods/blocks in total (incl. handlers), of which 41 are `synchronized (this)`; others lock on equipment, observers, restoreLock, tasks, queuedSkills and similar.
- src: about 137 lines with ConcurrentHashMap/CopyOnWrite/ConcurrentLinkedQueue and 47 with Atomic*.
- handlers: 32 synchronized, 167 Atomic*, 9 concurrent collections.
- KnownList.update() is `synchronized` and runs on the moving thread: World.updatePosition (World.java:235), World.spawn (297), WalkManager, PlayerController.

## Key findings
- **There is no single game loop. Game logic runs at the same time on at least 5 kinds of threads: 4 packet processor threads, max(4, cores) scheduled, max(4, cores) instant, the ForkJoin common pool and the cached long-running pool, plus the NIO thread for packet decoding.**
  - Evidence: GameServer.java:173-203; ThreadPoolManager.java:30-45; AionConnection.java:46-48 (PacketProcessor min/max 4); MoveTaskManager.java:40 (`movingCreatures.values().parallelStream()` every 200 ms); MapRegion.java:100 (execute); no `while(true)`/tick loop in src except StigmaService/CompressUtil local loops; NetworkConfig.java:70-74 and network.properties:41 say 'the game server is not thread-safe'
  - C++: Copying this model means porting thousands of unsynchronized cross-object accesses that are memory-safe in Java but undefined behaviour in C++. A single game-logic executor (one thread or Asio strand) running packet runImpl, timer callbacks, cron callbacks and periodic managers removes most data races, and with them the need for atomic handles. MoveTaskManager's parallelStream becomes a plain loop. DB-heavy work (GeneralUpdateTask, leaveWorld saves, BrokerPeriodicTaskManager) can stay synchronous for a local server or later move to a DB worker that posts results back. Record this as a deviation.
- **Scheduling is everywhere: 854 direct schedule/scheduleAtFixedRate sites, 683 of them (80%) in data/handlers.**
  - Evidence: Multi-line regex over all .java files: src schedule 127 + fixedRate 44; handlers schedule 601 + fixedRate 82; plus execute 7, executeLongRunning 5, submit* 3, CronService.schedule 21, 14 periodic manager classes, about 100 delayed PacketSendUtility broadcasts in handlers (approximate regex). Files: 139 in src, 306 in handlers. By area: handlers/ai 393, handlers/instance 245, handlers/quest 39, src/skillengine 24.
  - C++: Keep the Java API shape so handler porting stays mechanical: `ThreadPoolManager::getInstance().schedule(std::function<void()>, delayMs)` and `scheduleAtFixedRate(fn, delay, period)`, returning `std::shared_ptr<Future>` with cancel(bool mayInterrupt), isDone(), isCancelled(). Also overloads with std::chrono durations for the TimeUnit variants. Put this in the phase-4 foundation because nearly every handler depends on it. The login server's single-thread ScheduledExecutor (cpp/login-server/src/aion/loginserver/utils/ScheduledExecutor.h) is a starting point but has no one-shot schedule and no isDone.
- **Almost every scheduled task captures a live game object, directly or through `this` (an AI, instance handler, Effect or controller), and many are never cancelled. Cancellation does not wait for a task that is already running.**
  - Evidence: Heuristic over 807 parsed sites: 521 name player/npc/creature/effected/owner/instance identifiers, most of the rest use implicit `this` (e.g. ShugoImperialTombInstance.java:262 `sp(...)`, AggroList.java:206-212, AbstractOverTimeEffect.java:55). Manual review of the 171 src sites: about 30 capture no game object. 121 of 219 scheduling AI handler files contain no `cancel(`. CreatureController.cancelAllTasks uses cancel(false) (CreatureController.java:415-421). Guard checks in scheduling files: isDead() 222, isSpawned() 22. Only 3 src sites use an ID plus a lookup (PlayerEnterWorldService.java:369-371,507; RespawnService.java:59).
  - C++: Decision (A) must guarantee that a timer lambda never dereferences freed memory, even after despawn, delete or instance destroy, without editing all 854 sites by hand. Options: (1) capture shared_ptr (keeps Java semantics: the object stays alive and valid until the task drops it); (2) capture a weak_ptr/handle and have the scheduler skip or lock it (needs a wrapper such as `schedule(owner, fn)` that ties the task to the owner's lifetime, like addTask). Option 1 matches the Java code's assumption that despawned objects stay valid and are checked with isDead/isSpawned. Option 2 requires an owner argument at each site.
- **Object ID release depends on the GC. Npc, Gatherable, StaticObject, Summon, HouseObject and teams release their objectId through java.lang.ref.Cleaner only when unreachable, and IDFactory hands out the lowest freed ID next.**
  - Evidence: AionObject.java:14-31 (CLEANER.register → RespawnService.setAutoReleaseId or IDFactory.releaseId); Npc.java:57, Gatherable.java:17, StaticObject.java:15, Summon.java:41 pass autoReleaseObjectId=true; HouseObject.java:42, GeneralTeam.java:33, TemporaryPlayerTeam.java:26 take the flag; IDFactory.release sets nextMinId=id (IDFactory.java:178-185); RespawnService.java:79-88 warns 'ObjectId ... got released and reassigned while there was a still active respawn task'.
  - C++: objectId alone cannot be a safe weak handle, because of reuse (ABA). If objects are shared_ptr, release the ID in the destructor or custom deleter, which maps the Cleaner exactly, and keep RespawnService.setAutoReleaseId. If handles are used, they need a generation counter, or ID release must be deferred until no references remain. Releasing at despawn would change ID reuse timing and could hand an ID to a new object while old tasks or known lists still point at it.
- **Config is re-bound at runtime from several paths, including reflection by field name in //configure.**
  - Evidence: Config.load() at GameServer.java:219, Event.java:82 and 134 (event start/stop with config properties, layered via Config.java:48), Reload.java:81; Config.load(CommandsConfig.class) at ChatProcessor.java:40; Configure.java:39-81 (field.set by name via ConfigurableProcessor.transform); Ai.java:46-52 toggles AIConfig booleans directly. 33 classes, 420 @Property + 5 @Properties; 638 field reads in src, 95 in handlers.
  - C++: Under the existing CONVENTIONS rule, every field rebound at runtime must be ConfigValue<T> or std::atomic<T>, and events can rebind any key, so that is effectively all 425. If all game logic runs on one executor and reloads are posted there, plain fields are safe except for readers on network/DB threads (e.g. NetworkConfig, PffConfig in processData, ThreadConfig). //configure needs named field enumeration: extend `bind` to record name, getter-as-string and setter. A small generator from the @Property annotations could emit the 33 bind() functions and the field table together.
- **//reload swaps static data holders at runtime while live objects still point into the old data.**
  - Evidence: Reload.java: DataManager.QUEST_DATA, SKILL_DATA, ITEM_DATA (+cleanup), CUSTOM_NPC_DROP, UPGRADE_ARCADE_DATA, DECOMPOSABLE_ITEMS_DATA reassigned; NPC_SKILL_DATA.setNpcSkillTemplates, XML_QUESTS.setData, EVENT_DATA.setEvents mutate in place; AIEngine.reload/QuestEngine.reload/ChatProcessor.reload re-register handlers.
  - C++: For decision (B): either make the reloadable holders shared_ptr<const XData> and make templates referenced by live objects keep their data alive (e.g. items hold a shared_ptr, or old data is never freed), or drop or limit //reload for items/skills/quests and list it in DEVIATIONS.md. Plain `static XData*` with delete-on-reload would dangle.
- **Handler registration uses five separate ScriptManager + ClassListener setups. Selection is by base type, public non-abstract class and annotation; nothing uses @OnClassLoad/@OnClassUnload.**
  - Evidence: AIEngine.java:39-45 (AIHandlerClassListener → registerAI by @AIName, duplicate check), InstanceEngine.java:29-34 (@InstanceID), ZoneService.java:43-48 (@ZoneNameAnnotation), QuestEngine.java:99-103 (QuestHandlerLoader instantiates each AbstractQuestHandler; preUnload clears), ChatProcessor.java:31-33 (ChatCommandsLoader instantiates ChatCommand; duplicate alias check). Counts: @AIName 457, @InstanceID 73, @ZoneNameAnnotation 2, @OnClassLoad/@OnClassUnload 0. Handler files: quest 1035, ai 461, admincommands 101, instance 78, consolecommands 35, playercommands 16, zone 3. The engines are initialized in parallel (GameServer.java:100-101).
  - C++: Five registries with factories: AI name → factory(Npc), instanceId → factory, zone name → factory, quest handler instances, command instances. Prefer a generated registration translation unit (a script scans the ported handler dirs for REGISTER_* markers or class names) over self-registering static objects, which the linker can silently drop from static libraries and which have unspecified init order. Keep the Java duplicate checks. Reload becomes clear-and-re-register from the same binary.
- **Cron runs on Quartz with Quartz cron syntax, and the scheduler also exposes lookup APIs (findNextFireTimes by runnable type).**
  - Evidence: CronService.java:38-191 (1 Quartz thread, JobDataMap holds the Runnable, cancel by JobDetail or Runnable identity, findJobs(Class, withSubTypes)); SiegeService.java:323; CronExpression.getTimeAfter 3 uses (AbstractCronTask.java:81-118); 12 CronExpression + 5 CronExpression[] config fields; config/schedule/*.xml expressions like `0 0 17 ? * TUE,THU,SAT`, `0 30 17 1,16 * ?`, `0 0/5 * ? * *`; test/com/aionemu/gameserver/services/cron/CronServiceTest.java exists.
  - C++: Write a Quartz-compatible CronExpression (seconds, ?, L/W/# if present, names, lists, ranges, increments) with getTimeAfter in the server time zone (std::chrono::zoned_time), plus a CronService on top of the timer. Return a job handle, and for findNextFireTimes(SiegeStartRunnable) keep a type tag (e.g. std::type_index) with each job. Port CronServiceTest as test vectors. AbstractCronTask's 'run on start if missed' logic depends on ServerVariablesDAO.
- **Startup ordering relies on lazy class-initialization side effects, and some singletons are created as a side effect of others.**
  - Evidence: Config.load calls EventService.getInstance() before DataManager exists (Config.java:48); AbstractPeriodicTaskManager's constructor schedules on first getInstance (AbstractPeriodicTaskManager.java:17); AbstractCronTask static SERVER_STOP_MILLIS loads from DB at class init (AbstractCronTask.java:21); CustomInstanceService static LEADERBOARD_WINDOW_OBJECT_ID = IDFactory.nextId(); ShutdownHook's constructor schedules a cron (ShutdownHook.java:36-42); HousingService must exist before spawns (GameServer.java:118 comment).
  - C++: Use function-local statics for singletons, as CONVENTIONS already says, and never namespace-scope statics with side effects. Convert Java static initializers that touch DB or IDFactory into lazy accessors. Keep main() in the same explicit order as Java and keep the parallel phases (engine init, World creation, spawnAll per map, geo loading) optional; they can start sequential.
- **Some Java-specific infrastructure has no direct C++ equivalent.**
  - Evidence: HTMLCache uses ObjectOutputStream/ObjectInputStream for its cache file (HTMLCache.java:68,114); CAPTCHAUtil/DDSConverter render text with java.awt (Graphics2D, Font) into DDS (utils/captcha, used at GatherableController.java:80-81); XSD validation via javax.xml.validation (XmlDataLoader.java:59-77); NetFlusher java.util.Timer (FloodManager.java:137); DeadLockDetector; RunnableStatsManager/RunnableWrapper (5 s slow-task warning).
  - C++: HTMLCache: use an own cache format or no cache (deviation). CAPTCHA: an embedded bitmap font or stb_truetype, or disable it (deviation). XSD validation: skip, or use libxml2 as an optional validation tool; the generated loader can be strict instead. NetFlusher: a periodic timer. RunnableWrapper already exists in C++ commons.
- **ShutdownHook depends on the JVM shutdown-hook model (runs on System.exit or Ctrl+C) and on exit code 2 for restarts.**
  - Evidence: ShutdownHook.java:45-92 (countdown with broadcasts, fast exit without players, NIO shutdown saves players via AionConnection.onServerClose → safeLogout at AionConnection.java:264-280, PeriodicSaveService.onShutdown, saveGameTime, Cron/TPM shutdown); dist/start.bat loops on ERRORLEVEL 2; RESTART_SCHEDULE cron runs System.exit on the Quartz thread.
  - C++: Implement an explicit shutdown coordinator: console Ctrl+C handler (SetConsoleCtrlHandler/signal) plus a //shutdown command that posts to it. Run the countdown on a timer, then execute the Java sequence in order and return the exit code from main(). No std::exit from worker threads while objects are live.

## Risks
- 1. Lifetime of objects captured by timers (highest). 854 schedule sites plus about 100 delayed broadcasts capture Npc/Player/AI/InstanceHandler/Effect objects, and most are fire-and-forget. Any ownership model that frees objects at despawn/delete breaks them unless the scheduler ties each task to an owner or the lambdas hold owning references. Errors show up as random use-after-free crashes long after the event.
- 2. Threading model mismatch. The Java code relies on GC memory safety under pervasive concurrent access: 4 packet threads, the scheduled pool, the instant pool, ForkJoin parallelStream every 200 ms, with only 254 synchronized and some concurrent maps. A faithful multi-threaded port will contain countless UB data races. A single-executor design is safer but is a behavioural and performance deviation, and synchronous DB calls on that thread (periodic saves, leaveWorld, broker saves) can stall the world.
- 3. ObjectId reuse (ABA). IDs are released by the GC Cleaner and the lowest free ID is reused immediately. Any ID- or handle-based reference scheme without generations, or any change to release timing, can make stale references (respawn tasks, known lists, aggro lists) resolve to a different object.
- 4. Runtime reload of config and static data. Config.load from events and //reload, //configure by field name, and DataManager holder swaps are memory-safe in Java only because of the GC. In C++ they need ConfigValue/atomics or executor confinement, and shared ownership of template data (or dropping the feature).
- 5. Cron compatibility. Quartz syntax, time zone handling and the missed-run-at-startup logic (AbstractCronTask.findLastPlannedRun) must match exactly, or sieges, rifts, world raids, auctions and daily resets fire at the wrong time. findNextFireTimes needs type-tagged jobs.
- 6. Handler registration at scale (about 1,730 handler classes). Self-registering statics in static libraries can be dropped by the linker or initialized in an unspecified order. Duplicate-name checks and the per-engine validation (AIEngine.validateScripts, QuestSpawnAnalyzer) must be kept.
- 7. Hidden startup dependencies. Lazy class-init side effects (EventService during Config.load, static DB loads in AbstractCronTask, static IDFactory.nextId, the ShutdownHook constructor scheduling a cron) and parallel init phases (engines, World, spawnAll, geo) can deadlock or run in the wrong order if they become eager or parallel C++ statics.
- 8. Smaller Java-only pieces: java.awt CAPTCHA rendering, Java-serialized HTML cache, XSD validation, DeadLockDetector, Timer thread. Each needs a replacement or a documented deviation.

## Dependencies
Depends on (all already ported in cpp/commons unless noted): Logging, ConfigurableProcessor and transformers (needs a CronExpression PropertyTransformer, not ported), DatabaseFactory and 8 DAOs for IDFactory (PlayerDAO, InventoryDAO, PlayerRegisteredItemsDAO, LegionDAO, MailDAO, GuideDAO, HousesDAO, PlayerPetsDAO) plus ServerVariablesDAO for AbstractCronTask and GameTime, NioServer/PacketProcessor (ported), RunnableWrapper/ExecuteWrapper/RunnableStatsManager (ported), Rnd (ported), and a Quartz-compatible cron parser (not ported). AionRejectedExecutionHandler policy is noted in CONVENTIONS.

Depended on by nearly everything:
- ThreadPoolManager: 139 src files and 306 handler files
- IDFactory: every spawned object and item (28 nextId sites)
- Config classes: 638 reads in src, 95 in handlers
- CronService: sieges, rifts, world raids, events, housing auctions, passport, abyss ranking, periodic instances, restart schedule
- PacketSendUtility: all packet broadcasting
- DataManager: everything after step 8

Suggested order inside phase 4:
1. Config classes (33 bind functions, ideally generated) and GSConfig/ThreadConfig/NetworkConfig
2. Game-logic executor + ThreadPoolManager API (schedule, scheduleAtFixedRate, execute, executeLongRunning, submit) + Future handle
3. CronExpression + CronService (+ port CronServiceTest)
4. IDFactory (with the chosen release-on-destruction or generation policy) once the object-model decision is made
5. ShutdownHook/GameServer main skeleton with the startup order table as a stubbed checklist
6. DataManager (decision B)
7. Engines' registries (AI, quest, instance, zone, commands), then World, geo, spawns
8. The periodic managers (MoveTaskManager etc.) come with the movement/AI systems in phase 5

The object-ownership decision (A) must be made before step 4 and before any code that schedules tasks is ported.

## Open questions
- Should the C++ game server run all game logic on one executor (strand/thread), which is safer and simpler for ownership, or keep Java's multi-pool concurrency? This decides whether config fields, known lists and handles need atomics or locks.
- Why does the Java server forbid nio.threads > 1? pck.read() (readImpl) runs on the NIO thread (AionConnection.java:188), so some readImpl may touch game state. Not verified which ones.
- How often do tasks actually run after their owner is deleted, e.g. AI handlers without cancel or instance handlers without onInstanceDestroy (17 of 44)? A shared_ptr capture model would keep such objects (and their IDs) alive until the task finishes. Is that acceptable for long delays like 30 min (AnohasSwordAI.java:66-69)?
- Is runtime //reload of items/skills/quests/AI/commands a requirement for the C++ port, or can it be a documented deviation? It strongly affects decision (B) (shared_ptr<const Data> holders vs plain statics).
- Does //configure need to work for all 425 fields (runtime name → field table), or only the scalar ones?
- Do Quartz features beyond those observed (L, W, #, year field) appear anywhere in configs, event XML or siege/rift/world raid schedules? Only a sample of config/schedule/*.xml and the properties files was inspected.
- The heuristic capture classification covered 807 of 854 sites (47 had unbalanced parentheses in string literals). The '~95% capture a game object' figure is a heuristic plus a manual review of src only; handler sites were only sampled.
- Several startup services (DropRegistrationService, ExchangeService, EventService, CommandsAccessService, ZoneService.init details, GeoService/GeoWorldLoader internals) were not read in depth. Their exact loads and hidden schedules are unverified.
- Should blocking DB work stay on the game thread (as Java does on packet/scheduled threads) or go to a separate DB worker that posts results back? That affects how DAO-heavy services (PlayerLeaveWorldService, PeriodicSaveService, Broker saves) are ported.
