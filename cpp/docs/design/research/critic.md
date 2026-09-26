# Completeness critic

## Gaps (verified answers)
### Can one game-logic executor work at all? Nobody checked whether game code blocks while waiting for work on another thread (Future.get, sleep, latches, join, wait/notify) or relies on thread interruption. This was the main unverified assumption behind the single-executor recommendation, and behind treating cancel(true) differently from cancel(false).
Why it matters: If game logic waits synchronously for tasks that run on other pool threads, a single strand or thread deadlocks. If tasks check for interrupts, cancel(true) needs real interruption semantics. The threading model constrains decision (A): whether refcounts must be atomic, whether containers need locks, and whether snapshots are needed.

Answer: I grepped src and data/handlers for .get(), .get(n, TimeUnit), join(), CountDownLatch, Thread.sleep, await(, acquire(, wait()/notify, isInterrupted, Thread.interrupted and InterruptedException. Game logic has only two blocking sites.

(1) CM_TELEPORT_ANIMATION_DONE.java:36-41 takes the TELEPORT controller task, calls `spawnTask.run()` and then `spawnTask.get()`. This runs on the calling thread, so it is not a cross-thread wait. But the Future abstraction must support 'run now if not started', including on a task that was never put on the scheduler: TeleportService.java:191 stores `new FutureTask<Void>(spawnTask, null)` under TaskId.TELEPORT. PvPZone.java:45 and PlayerReviveService.java:251 store real scheduled futures under the same TaskId, so run() can also hit a scheduled task.

(2) admincommands/FixPath.java:85 runs on the instant pool. At :143-152 it blocks up to 5 s (`waitTask[0].get(5, SECONDS)`) on a scheduleAtFixedRate task that waits for the admin's Z to be changed by incoming movement packets. On a single executor this stalls the world for 5 s and never succeeds, so it must become a continuation.

No game code checks for interruption. InterruptedException appears only in DataManager/StaticData/XmlMerger/JAXBUtil startup waits, ShutdownHook, ThreadPoolManager.shutdown, and the two sites above. So cancel(true) behaves exactly like cancel(false) at all 154 call sites: a cancel flag is enough.

ThreadLocal use:
- AbstractAI.java:31 DEPTH, a recursion guard reset in finally. Fine on any executor.
- DatabaseCleaningService.java:36 requires the main thread (threadId()==1).

The real cost of a single executor is blocking DB I/O inline in game logic:
- services: 255 DAO calls in 43 files
- model: 33 calls in 16 files (Item, Equipment, PetList, MotionList, EmotionList, TitleList, RecipeList, PlayerScripts, House, HouseRegistry, IdianStone, LegionDominionLocation, CosmeticItemAction, PolishAction)
- controllers: PetController, 5 calls
- client packets: 8 calls in 5 files (CM_APPEARANCE, CM_CHARACTER_EDIT, CM_CHARACTER_PASSKEY, CM_DELETE_CHARACTER, CM_QUIT)
- taskmanager: 1 call
- data/handlers: 11 files

DB calls inside model mutators cannot move to a DB worker without restructuring. Not verifiable without a JVM: per-tick CPU load, e.g. how many NPCs MoveTaskManager moves per 200 ms in active regions.

### Scheduler/Future API surface actually used, beyond schedule/scheduleAtFixedRate/cancel/isDone/isCancelled
Why it matters: The phase-4 scheduler must be complete before 854 call sites are ported. The ownership decision also affects what a cancelled or self-cancelling callable may do; for example, it must not be destroyed while it is running.

Answer: ThreadPoolManager.java:51-80 returns ScheduledFuture from schedule/scheduleAtFixedRate and Future from submit/submitLongRunning. Every task is wrapped in RunnableWrapper, which catches exceptions so periodic tasks keep running. That is the opposite of the login server's C++ ScheduledExecutor behaviour noted in DEVIATIONS.md:150.

Methods used beyond cancel/isDone/isCancelled:
- getDelay: DropService.java:119 `decayTask.getDelay(TimeUnit.MILLISECONDS)` extends corpse decay.
- run-now through RunnableFuture: CM_TELEPORT_ANIMATION_DONE.java:40.
- Plain unscheduled FutureTask objects stored as controller tasks: TeleportService.java:191.
- get(timeout): FixPath.java:152.

Tasks cancel themselves from inside their own run(), e.g. FixPath.java:147. There are 6 holders of the form `Future<?>[]` or `AtomicReference<Future>` (Effect.java:48, AbstractMaterialSkillActor.java:28, HarlequinLordReshkaAI.java:20, TheHexwayInstance.java:51). CreatureController.addTask (CreatureController.java:400-403) cancels the previous task atomically inside compute. 49 sites call `addTask(TaskId.X, ThreadPoolManager...)` directly.

C++ requirements that follow: a Future with cancel/isDone/isCancelled/getDelay/runNowIfPending; a 'deferred' future type not bound to the scheduler; safe self-cancel (destroy the callable after it returns, not during run); and exceptions logged without stopping fixed-rate tasks.

### Are 'templates' really immutable and immortal after load? The services and object-model maps assume `const SkillTemplate*`/`const T*` is safe; static-data flags only spawn/walker/event paths without quantifying them.
Why it matters: Decision (B) (generated const structs owned by immortal holders) and decision (A) (raw template pointers in live objects) both depend on this. If some template types are created per spawn or mutated at runtime, they need shared ownership and synchronization, and generated classes need ordinary constructors and setters.

Answer: Not immutable for the spawn family, and not immortal.

(1) Every handler spawn creates a new SpawnGroup and SpawnTemplate at runtime. SpawnEngine.java:86 `new SpawnTemplate(new SpawnGroup(...))` and :94 for siege spawns, called from AbstractAI.spawn (AbstractAI.java:403-405, which also calls template.setStaticId) and GeneralInstanceHandler.spawn (:92-99). There are 71 direct SpawnEngine.new* call sites in src plus handlers, behind about 2,586 `spawn(` calls in handlers. The only owner is the final field VisibleObject.spawnTemplate (VisibleObject.java:46), plus RespawnTask (RespawnService.java:161), so the template is GC-owned.

(2) Instance spawns share the static SPAWNS_DATA SpawnTemplate objects across all instances of a map: SpawnEngine.spawnInstance iterates DataManager.SPAWNS_DATA groups (SpawnEngine.java:137-176). 34 files make 47 setter calls on getSpawnTemplate()/getSpawn(), e.g. AlarmAI.java:47, ArminosDrakyAI.java:31,42, CaptainXastaAI.java:75,89 setWalkerId, and NidalberBalaurAI.java:29 setX. These mutate shared data across instances, which is a Java race and quirk.

(3) SpawnGroup.poolUsedTemplates holds per-instance runtime state under synchronized (SpawnGroup.java:163-189) and is never cleared when an instance is destroyed.

(4) Event start/stop runs on the cron thread and mutates JAXB Spawn.setEventTemplate (Event.java:88), SpawnsData.addRegularSpawns (Event.java:94, SpawnsData.java:82-99) and removeEventSpawnObjects (Event.java:149, SpawnsData.java:438-444). That removes SpawnGroups that live NPCs still reference. GuideTemplate.setActivated is toggled at Event.java:116,175.

(5) HostileUpEffect.tempHate (HostileUpEffect.java:32,61) is per-cast state on a shared effect template.

(6) About 29 runtime constructions of *Template classes outside dataholders: FlyRingTemplate 13, QueuedNpcSkillTemplate 3, SpawnTemplate 2, SiegeSpawnTemplate 2, and 1 each of WalkerTemplate (FixPath.java:121-125 uses setters), MaterialZoneTemplate (geo loader), PlayerStatsTemplate (PlayerClass.java:66-78 uses setters), KiskStatsTemplate, WorldZoneTemplate, Rift/Vortex/Base/Town/AhserionsFlight spawn templates.

Implication: split loaded read-only templates (item, npc, skill, ...), which can be immortal `const T*`, from SpawnTemplate/SpawnGroup/Spawn, which need refcounting, mutability and a lock or executor confinement. The generator must also emit constructible, settable classes for the roughly 15 template types built at runtime.

### Scope of the XML generator: which classes carry JAXB annotations, compared with what PORTING_PLAN.md says
Why it matters: PORTING_PLAN.md:36 says the generator 'reads the JAXB annotations of game-server/src/**/templates'. Porting chunks and the generator input list are planned from that sentence.

Answer: Counting files that import javax.xml.bind:
- model/templates: 360
- skillengine: 255
- dataholders: 97
- questEngine: 47
- model/enchants: 6
- utils: 4
- configs: 4 (XML schedule and shop roots)
- model/drop: 3
- model/stats: 2
- model/items, model/autogroup and world: 1 each
- enums in model/: Race, PlayerClass, Gender, TribeClass, EventTheme, AttendType

So about 427 of 787 JAXB files (54%) are outside model/templates, and the plan's scope sentence is wrong. It must cover skillengine (effects, conditions, properties, actions, modifiers), dataholders, questEngine models/XMLQuests, model.stats functions, enums and the 4 config XML roots.

The services map says there are '100 @XmlElement names in Effects.java'. That is a line count (100 matching lines). The file actually has 169-170 `name =`/`type =` mappings in the @XmlElements block, consistent with the static-data map's 170.

### Pointer identity vs objectId equality between game objects, and object-keyed collections. object-model left this as an open question.
Why it matters: This decides whether (id, generation) handles, weak_ptr, or intrusive Ref<T> preserve semantics. It matters most for relogin, where a new Player instance gets the same objectId while old instances are still held by teams, effects and tasks.

Answer: A heuristic regex scan (script in scratchpad/critic/ident.py) found:
- Identity comparisons (`==`/`!=`) between game-object expressions: about 12 in src and 2 in handlers. Examples: AttackUtil.java:510 and :528 (`getTarget() == target/object`), VisibleObject.java:207, AntiHackService.java:69, EffectTemplate.java:537, ProvokerEffect.java:69, TargetRangeProperty.java:50 (`trap.getCreator() == creature`), OrissansSummonAI.java:30, PvPArenaInstance.java:133, plus AionObject.equals itself and the deliberate World.removeObject identity check.
- `.equals(` on object variables: about 72 lines (41 src, 31 handlers).
- Object-keyed collections are rare: Map/Set<Player> 3, <Creature> 2, <VisibleObject> 1, <Item> 5. Everything else is keyed by Integer objectId (AggroList, KnownList, teams, RepurchaseService, Kisk, Invasion).

Conclusion: equality is overwhelmingly objectId-based, and the few pointer comparisons can be audited. A handle or Ref design must keep equals as objectId. Pointer identity survives only in World.removeObject and the ~14 sites above. Regex-based, so the count is not exhaustive (it misses comparisons through arbitrary local names).

### ID release timing differs by object kind. The object-model map says 'IDs are released only when the object is GC'd', which covers only part of the objects.
Why it matters: Decision (A) proposes 'release the ID in the last destructor' as the Cleaner analogue, and ID reuse (ABA) drives the handle/generation question. Objects whose IDs are released explicitly, or never, need different rules.

Answer: There are three lifecycles.

(a) Cleaner, i.e. on GC: autoRelease=true for Npc (Npc.java:57), Gatherable (Gatherable.java:17), StaticObject (StaticObject.java:15), Summon, HouseObject, PlayerAlliance, League, and new PlayerGroups (PlayerGroup.java:16). Respawns can defer release (RespawnService.java:203-214). The object-model map omits Gatherable and StaticObject.

(b) Explicit, driven by the DB or a service, while Java objects may still be referenced:
- InventoryDAO.java:240 releases deleted item IDs only after a successful DB store, i.e. at logout or the 900 s periodic save. Items are marked UPDATED even when the store fails.
- PlayerRegisteredItemsDAO.java:164,167 (house objects and decor).
- ExchangeService.java:274,301; ItemSplitService.java:90; PetAdoptionService.java:95; HTMLService.java:144; CM_CREATE_CHARACTER.java:68; CMT_CHARACTER_INFORMATION.java:153.
- Sold Items stay in RepurchaseService `Map<Integer, Set<Item>>` (RepurchaseService.java:20) and are matched by objectId (:58) after their ID may already have been released.

(c) Never released: FlyRing (FlyRing.java:22), Road (Road.java:25), CuringObject (CuringObject.java:19), AssembledNpcPart (BalaurAssaultService.java:125, SpawnAssembledNpc.java:46), HTML message IDs (HTMLService.java:50,55,82), and the static CustomInstanceService.java:39. Player IDs are DB-owned and stable across relogin.

Implication: 'release in destructor' fits only group (a). Items and HouseObjects/decor are released at DB delete, independent of in-memory lifetime. A generation-counter handle table would have to cover these objects too, or Items must never be looked up by bare ID after deletion.

### Does readImpl, which runs on the NIO thread, touch game state? This was the boot map's open question about why nio.threads > 1 is forbidden.
Why it matters: It decides whether C++ can keep N Asio IO threads for the game connection, and whether packet decoding must also move onto the game executor.

Answer: I scanned all 190 readImpl bodies with a brace-matching script (scratchpad/critic/readimpl.py) for getActivePlayer, other getConnection() calls, World., getInstance(), DataManager and *Service. Only two hits, both for logging:
- CM_BUY_ITEM.java:48 reads getActivePlayer() to pass to AuditLogger.log at :55,71.
- CM_ATREIAN_PASSPORT.java:33 builds a warning message.

AionConnection.processData (AionConnection.java:150-195) does decrypt, factory, PFF check, pck.read(), then packetProcessor.executePacket. readImpl is effectively pure. The single-thread restriction comes from lazy SM writeImpl and the shared BaseServerPacket.buf, as the network map says. With eager serialization, N IO threads are safe once the two AuditLogger/log calls move to runImpl, or AuditLogger is made thread-safe.

### Quartz features actually used (L, W, #, year field), an open question in the boot map
Why it matters: It sets the scope of the C++ CronExpression parser that sieges, rifts, world raids, housing and daily resets depend on.

Answer: Every cron string in src, config properties and config/schedule/*.xml was grepped. None uses L, W or #.

Features in use:
- the seconds field, always 0
- `?`
- day-of-week names and lists (`TUE,THU,SAT`, `MON,WED,SAT`)
- day-of-month lists (`6,21`, `1,16`, `12,27`)
- hour lists (`0,12,20`, `14,18,21`)
- `0/5` minutes (EventService.java:51)
- exactly one 7-field expression with a year `*`: CronJobService.java:70 `0 0 9 ? * WED *`

Sources: 16 @Property CronExpression defaults (AutoGroupConfig, CustomConfig, HousingConfig, RankingConfig, SiegeConfig), config/main/custom.properties:84-85, siege/rift/world_raid_schedule.xml, QuestEngine.java:934, AtreianPassportService.java:39 and SiegeService.java:50. config/main/shutdown.properties:12 restart_schedule is empty by default. The parser must accept the optional year field; L/W/# can be rejected with an error. Event XML and user-edited mygs.properties could not be checked for other expressions.

### How many XML quest handlers and template classes there are. Maps disagree: 4,184 vs ~4,174 vs ~4,040, and 16 vs 17 templates.
Why it matters: Phase 4/5 'counts match Java' acceptance criteria and the quest-engine milestone use this number.

Answer: grep over data/static_data/quest_script_data: the 16 element types (item_collecting ... mentor_monster_hunt) occur 4,184 times with 4,184 unique id attributes, so 4,184 is correct. src/questEngine/handlers/template has 17 files: 16 concrete handlers plus AbstractTemplateQuestHandler. The services map's '~4,174 via 17 template handlers' and static-data's '~4,040 XMLQuest entries' are wrong. The expected total is 1,035 script quests + 4,184 XML quests = 5,219 handlers. Whether ItemCollecting or other templates register more than one handler per element was not checked, so this is not verified at runtime.

## Contradictions between maps
- The threading recommendations conflict:
- boot-infrastructure: one game-logic executor.
- services-skills-quests: keep Java's multithreaded pools with recursive_mutex, citing CONVENTIONS.
- object-model-world: stay free-threaded with atomic intrusive refcounts and concurrent maps.
- dao-geo: a per-player strand or snapshot-then-write.
- network: eager serialization with N IO threads.
My checks found no cross-thread blocking dependency except FixPath.java:152, no interrupt reliance, and readImpl effectively pure. So a single executor is technically viable; blocking inline DB I/O (model mutators included) is its main cost. This must be decided before decision (A).
- Ownership recommendations conflict. object-model recommends intrusive Ref<T> with ID release in the destructor. dao-geo (the playerId re-resolve precedent), handlers-registration (the getNpc id lookups, 511 in ai+instance) and services (maps already keyed by objectId) favor handles, IDs or weak_ptr. boot lists both options. The evidence splits: fire-and-forget tasks and effects that outlive their effector favor strong refs; objectId-keyed maps and rare identity comparisons are compatible with handles.
- Packets: object-model says server packets should hold Ref<T> and serialize lazily, as Java does. network says to serialize eagerly so queued packets never own game objects. These cannot both be adopted.
- Generated code shape for behaviour-bearing JAXB classes: static-data proposes a generated member block and bindXml inside hand-written classes, and calls a generated base struct awkward. services proposes a generated `EffectTemplate_gen.h` base with a hand-written derived class. PORTING_PLAN.md:36 says 'C++ structs plus pugixml loaders'.
- PORTING_PLAN.md:36 limits the generator to src/**/templates, but about 427 of 787 JAXB files are elsewhere: skillengine 255, dataholders 97, questEngine 47, and others.
- Effect XML mappings: services says '100 @XmlElement names in Effects.java'. The actual count is 169-170 mappings (100 is the number of matching lines), consistent with static-data's 170. The effect class count also varies across maps: 170, 173, 177, 178, 180, 184 files. Effects.java maps about 170; the effect directory has 178 *Effect.java files, 9 of them abstract.
- XML quest count: handlers-registration says 4,184 (verified), services says ~4,174 with '17 template handlers', static-data says ~4,040 XMLQuest entries. The correct figures are 4,184 quests and 16 concrete templates plus 1 abstract base.
- Template immutability: services says 'Templates are immutable and immortal after load, so the runtime can hold const SkillTemplate* safely', and object-model suggests raw template pointers. In fact SpawnTemplate/SpawnGroup are created per runtime spawn (SpawnEngine.java:86), mutated by 47 handler setter calls on shared static spawn templates, and removed from SpawnsData by events. Only non-spawn templates (with exceptions like HostileUpEffect.tempHate and GuideTemplate.activated) are effectively immutable.
- ID release: object-model says IDs are released only after GC and lists Npc, Summon, HouseObject and teams. boot adds Gatherable and StaticObject, which is correct (Gatherable.java:17, StaticObject.java:15). Neither map mentions that Item and house-object IDs are released explicitly after a DB delete (InventoryDAO.java:240, PlayerRegisteredItemsDAO.java:164-167), or that FlyRing, Road, CuringObject and AssembledNpcPart IDs are never released.
- Reason for nio.threads=1: boot leaves open that readImpl might touch game state. network attributes it to lazy writes. The readImpl scan supports network; only CM_BUY_ITEM and CM_ATREIAN_PASSPORT read the active player, both for logging.
- network's server packet categories do not add up. 140 value + 63 lazy + 25 connection-dependent + 10 global + 2 abstract = 240, against 237 mapped packets, and the summary also says 'only about 120 of 237 are plain value packets'. The categories overlap and are heuristic.
- The schedule call-site counts differ slightly across maps (boot 728/126, object-model 724/125, handlers 685 ThreadPoolManager uses in handlers vs boot's 683). These are regex variations and do not affect the decisions.
