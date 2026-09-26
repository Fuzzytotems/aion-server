# services-skills-quests

## Summary
Method: I parsed all 2,310 Java files in game-server/src (237,192 lines, license headers included) with a Python script. It resolves explicit imports, wildcard imports, same-package simple-name references and inline fully qualified names into a class-level dependency graph (13,326 edges). Then I ran Tarjan SCC, fan-in/fan-out, layer-violation counts and a trial chunk partition over that graph, and read the key classes by hand.

The main result: package-level layering does not exist. All 20 top-level packages form one strongly connected component. At class level, one SCC holds 1,708 classes and 193,081 lines (81% of src). Only 42k lines (templates, DialogAction, geoEngine.math, configs, part of utils) can be ported with no forward declarations of the core.

Cutting all edges into network.* (packets used as an interface) shrinks the SCC to 130k lines. Also cutting edges into services.* shrinks it to 95k. The object model itself (model + skillengine + controllers + questEngine + dataholders + world + geo) stays one SCC of about 90k lines even with those cuts. So the port has to be header-first. A small serial "spine" step delivers the headers of about 40 hub classes (Player fan-in 754, AionConnection 441, PacketSendUtility 323, DataManager 238, Creature 238, Effect 219, ...). After that, parallel chunks implement bodies against those headers.

services (169 files, 30,316 lines) is 61 singleton classes plus 75 static-only classes. Inside services, 52 classes (14.6k lines) form their own SCC (Legion, Siege, Quest, Broker, Housing, Teleport, Drop, Mail, Event, PlayerService, ...).

The skill engine (292 files, 16,349 lines) is mostly JAXB-mapped polymorphic templates with behaviour: 173 effect classes (8.3k lines) selected by 100 @XmlElement names in Effects.java, plus conditions, properties, actions and modifiers. It forms one 254-class, 17.9k-line SCC together with model.stats, controllers.attack (AttackUtil, AggroList), controllers.effect.EffectController and utils.stats.StatFunctions. These must be ported together.

The quest engine (79 files, 7,262 lines) is one 40-class SCC. Its 17 template handlers drive about 4,174 data-driven quests from quest_script_data. 1,035 hand-written handlers depend on AbstractQuestHandler, QuestEnv, QuestState and QuestStatus, and handler lookup is by quest ID.

Stats (38 files, 4.2k lines) are containers of IStatFunction lists keyed by StatEnum. Functions are removed by StatOwner identity (Effect, Item, ...), and the functions themselves are JAXB objects with skillengine Conditions attached.

Static state couples everything:
- 94 getInstance() singletons, called 1,322 times in src and 1,344 times in handlers (ThreadPoolManager 874, SkillEngine 312, World 288).
- 91 public static DataManager fields (517 references in src, 104 in handlers).
- Static DAO methods.
- An explicit start-up order in GameServer.java:96-177.

About 35k lines can be generated instead of ported by hand: SM_SYSTEM_MESSAGE is 28,959 lines with 4,120 factory methods, and DialogAction is 6,247 lines with 6,205 int constants. About 45k lines are JAXB-annotated (779 files), of which roughly 17.6k are pure data.

I propose splitting phases 4-6 into about 34 src chunks and about 25 handler chunks of 3-8k lines each, with measured cross-chunk edge counts (see inventory).

## Inventory
## 1. Size by top-level package (src, Java lines / files)
| package | lines | files | notes |
|---|---|---|---|
| model | 69,972 | 798 | templates 23,787/401; gameobjects 13,241/89; model (direct) 8,319/21 (DialogAction 6,247, TribeClass 749); team 5,076/72; stats 4,212/38; instance 2,513; geometry 2,068; items 1,882; autogroup 1,082; siege 869; house 818; skill 788; account 774; base 734; broker 528; drop 508; others <400 |
| network | 59,470 | 515 | aion.serverpackets 42,212/240 (SM_SYSTEM_MESSAGE 28,959); aion.clientpackets 10,879/190; aion (direct) 1,362; iteminfo 997; instanceinfo 652; loginserver 1,942; chatserver 639 |
| services | 30,316 | 169 | see section 2 |
| skillengine | 16,349 | 292 | effect 8,514/184; model 4,209/43; condition 1,193/28; properties 920/13; task 525/4; action 295/6; effect.modifier 286/7; SkillEngine 228; periodicaction 112/4; change 67/2 |
| geoEngine | 10,059 | 29 | math 5,923/6 (Matrix4f 1,830, Matrix3f 1,243, Vector3f 1,008); scene 1,367; collision 982; bounding 819; models 523 |
| controllers | 8,053 | 61 | direct 3,411 (PlayerController 766, CreatureController 586, NpcController 344); observer 1,343/21; attack 1,294/11; movement 1,004/8; effect 1,001/4 (EffectController 775) |
| dataholders | 7,624 | 100 | 93 holders + DataManager 259 + StaticData 435; loadingutils 698 (XmlMerger 485) |
| questEngine | 7,262 | 79 | handlers.template 2,084/17; handlers.models 2,057/44; handlers 1,376/3 (AbstractQuestHandler 1,291); QuestEngine 946; model 440; task 233 |
| dao | 7,199 | 56 | static DAO classes (PlayerDAO 504) |
| utils | 4,691 | 41 | stats 996 (StatFunctions 699); chathandlers 602; PositionUtil 398; PacketSendUtility 290; ThreadPoolManager 181; IDFactory 192 |
| world | 3,756 | 41 | direct 1,951; zone 1,061; knownlist 415; geo.GeoService 205 |
| ai | 3,489 | 39 | AbstractAI 466, NpcAI 199, AIEngine 152; handler 1,021; manager 821 |
| configs 2,481; custom 2,317; spawnengine 1,492; taskmanager 986; instance 542; root 488; restrictions 391; cache 255 | | | |

Handlers (data/handlers): quest 95,539/1,035 (reshanta 7,202, inggison 5,911, heiron 4,935, beluslan 4,690, eltnen 4,394, crafting 4,381, beshmundir 4,016, gelkmaros 3,954, morheim 3,899, pandaemonium 3,653, daevanion 3,530, theobomos 3,370, sanctum 3,320, altgard 3,195, ...); ai 33,314/461 (ai/instance 23,466/308, worlds 2,929, siege 1,522, portals 996, events 891); instance 17,155/78; admincommands 8,789/101; consolecommands 1,824/35; playercommands 1,210/16; zone 135/3.

## 2. services (169 files, 30,316 lines), grouped by domain
- **services (direct), 60 files, 13,480 lines:** LegionService 1165, QuestService 949, SiegeService 717, BrokerService 708, EnchantService 592, StigmaService 446, HousingBidService 406, TradeService 389, DialogService 379, AutoGroupService 363, ExchangeService 353, HousingService 327, AtreianPassportService 319, DuelService 288, PvpService 285, ChallengeTaskService 256, RiftService 245, PrivateStoreService 233, RespawnService 230, WeatherService 214, RecallService 201, TribeRelationService 200, LegionDominionService 195, UpgradeArcadeService 192, VortexService 177, ClassChangeService 166, BaseService 164, PunishmentService 163, HTMLService 159, WarehouseService 156, DatabaseCleaningService 143, ArmsfusionService 139, SurveyService 132, LifeStatsRestoreService 131, SocialService 126, CubeExpandService 125, WorldRaidService 124, ShieldService 123, NpcShoutsService 122, SkillLearnService 114, TownService 112, PeriodicSaveService 106, CronJobService 102, AccountService 102, AnnouncementService 95, KiskService 94, StaticDoorService 93, CommandsAccessService 82, RepurchaseService 81, LimitedItemTradeService 81, AdminService 76, RecipeService 75, GameTimeService 75, FactionPackService 74, CuringZoneService 70, BonusPackService 64, DebugService 57, NameRestrictionService 53, RoadService 38, FlyRingService 34
- **siege, 1,873 lines:** FortressSiege 415, Siege 223, FortressAssault 179, SiegeRaceCounter 172, AgentSiege 155, BalaurAssaultService 147, MercenaryLocation 139, ArtifactSiege 107, OutpostSiege 100, Assault 78, ArtifactAssault 56, SiegeCounter 54, SiegeStartRunnable 26, SiegeException 22
- **item, 1,654 lines:** ItemPacketService 236, ItemService 210, ItemSocketService 209, ItemChargeService 183, ItemSplitService 148, ItemPurificationService 139, ItemMoveService 128, ItemRemodelService 125, HouseObjectFactory 84, ItemRestrictionService 81, ItemActionService 70, ItemFactory 41
- **player, 1,619 lines:** PlayerEnterWorldService 545, PlayerService 360, PlayerReviveService 262, PlayerLeaveWorldService 155, MultiClientingService 120, PlayerChatService 77, PlayerLimitService 65, SecurityTokenService 23, PlayerMailboxState 12
- **drop, 1,167 lines:** DropService 535, DropRegistrationService 482, DropDistributionService 150
- **teleport, 1,058 lines:** TeleportService 540, PortalService 369, BindPointTeleportService 149
- **reward, 1,031 lines:** VeteranRewardService 499, WebRewardService 221, AdventService 151, BonusService 83, StarterKitService 77
- **toypet, 861 lines:** PetService 269, PetFeedCalculator 201, PetFeedProgress 100, PetAdoptionService 99, PetMoodService 83, PetSpawnService 63, PetHungryLevel 46
- **panesterra, 849 lines:** PanesterraService 350, ahserion.AhserionRaid 309, PanesterraTeam 132, PanesterraFaction 58
- **event, 821 lines:** EventBuffHandler 292, Event 290, EventService 239
- **transfers, 714 lines:** CMT_CHARACTER_INFORMATION 400, PlayerTransferService 190, PlayerTransfer 101, TransferablePlayer 23
- **mail, 686 lines:** MailService 284, MailFormatter 172, SystemMailService 140, AuctionResult 34, AbyssSiegeLevel 32, SiegeResult 24
- **instance, 613 lines:** InstanceService 242, InstanceScaler 146, PeriodicInstanceManager 141, PvPArenaService 84
- **rift, 592 lines:** RiftInformer 247, RiftEnum 190, RiftManager 126, RiftOpenRunnable 29
- **craft, 564 lines:** CraftService 261, CraftSkillUpdateService 182, RelinquishCraftStatus 121
- **abyss, 496 lines:** AbyssRankUpdateService 142, AbyssRankingCache 133, AbyssSkillService 80, AbyssPointsService 66, AbyssService 38, GloryPointsService 37
- **conquerorAndProtectorSystem, 359 lines:** ConquerorAndProtectorService 269, CPInfo 61, CPBuff 29
- **cron, 341 lines:** CronService 223, RunnableRunner 32, CronExpressions 26, CronExpressionTransformer 23, CronServiceException 22, CurrentThreadRunnableRunner 15
- **vortex, 283 lines:** Invasion 179, DimensionalVortex 104
- **summons, 261 lines:** SummonsService 221, TrapService 40
- **worldraid, 252 lines:** WorldRaid 192, WorldRaidRunnable 60
- **findgroup 212; antihack 165; ban 151 (ChatBanService 77, HDDBanService 53, BanAction 21); trade.PricesService 108; autogroup.AutoGroupUtility 106**

Singleton style in services: 61 classes use getInstance() (most via SingletonHolder), 75 are static-only, and the rest are value/helper classes.

## 3. Skill engine / stats / quest engine structure
- **Skill (1,096 lines):** cast pipeline canUseSkill → useSkill → startCast → endCast → applyEffect. It schedules endCast/cancel (Skill.java:312-314) and applyEffect after hitTime (:663). Fields include effector, firstTarget and effectedList (:62-64).
- **Effect (1,213 lines):** the runtime instance. It holds effector/effected (Effect.java:39-40), skill (:42), endTask/periodicTasks/periodicActionsTask futures (:47-49), observerRemoveTasks (:75), subEffect (:77), designatedDispelEffect (:119) and successEffects (:92). The end task lambda captures `this` (:682). Cancellation uses cancel(false) (:249, :748, :755, :879).
- **EffectTemplate (575 lines):** abstract JAXB base with applyEffect/startEffect/endEffect/onPeriodicAction/calculate. 173 JAXB subclasses; polymorphism via @XmlElements in Effects.java (100 element names).
- **Other JAXB-polymorphic containers:** Conditions (28 condition classes), Properties (FirstTarget/TargetRange/TargetRelation/...), Actions, PeriodicActions, ActionModifiers, ItemActions (36 item action classes, 3,619 lines), ModifiersTemplate (stat functions), QuestConditions/QuestOperations, XMLQuests, HousingObjectData, MailTemplate. That is 12 files with XmlElements/XmlElementRef. afterUnmarshal appears in 111 files.
- **EffectController (controllers.effect, 775 lines):** StampedLock, passiveEffectMap (volatile) and abnormalEffectMap (LinkedHashMap keyed by stack name).
- **Stats:** CreatureGameStats (404 lines) has `ConcurrentHashMap<StatEnum, List<IStatFunction>>`. addEffect wraps foreign functions in StatFunctionProxy(owner, fn), and endEffect removes by `statOwner.equals(fn.getOwner())`. Subclasses: PlayerGameStats 430, NpcGameStats 230, SummonGameStats 152, Trap/Homing/Servant/SummonedObject. LifeStats: CreatureLifeStats 387, PlayerLifeStats 281. Calc: Stat2/AdditionStat/ReverseStat, StatCapUtil 159, NpcStatCalculation 142, PlayerStatFunctions 332. Listeners: ItemEquipmentListener 222. Combat formulas live in utils.stats.StatFunctions (699) and controllers.attack.AttackUtil (549).
- **Quest engine:** QuestEngine (946 lines) holds 29 registration maps/lists keyed by int IDs, ZoneName, Race, etc. (QuestEngine.java:52-80), with `Map<Integer, AbstractQuestHandler>` (:52). AbstractQuestHandler (1,291 lines) has 33 `on*` event methods and abstract register(). 17 template handlers are instantiated from quest_script_data (item_collecting 1681, monster_hunt 1163, work_order 574, report_to 468, report_to_many 73, kill_in_world 50, crafting_rewards 42, item_order 32, skill_use 30, relic_rewards 30, kill_spawned 13, kill_in_zone 12, fountain_rewards 5, xml_quest 1). QuestEnv/QuestState/QuestStatus are imported by about 1,040 handler files.
- **Static data scale:** skill_templates.xml has 13,570 skill_template elements (13,509 with effects).

## 4. Most central classes (src only; fan-in = referencing classes)
| class | lines | fan-in | fan-out | pkgs referencing |
|---|---|---|---|---|
| model.gameobjects.player.Player | 1649 | 754 | 96 | 16 |
| network.aion.AionConnection | 422 | 441 | 17 | 3 |
| utils.PacketSendUtility | 290 | 323 | 15 | 15 |
| network.aion.AionServerPacket | 122 | 245 | 3 | 4 |
| SM_SYSTEM_MESSAGE | 28959 | 239 | 6 | 14 |
| dataholders.DataManager | 259 | 238 | 97 | 17 |
| model.gameobjects.Creature | 535 | 238 | 33 | 14 |
| skillengine.model.Effect | 1213 | 219 | 43 | 9 |
| AionClientPacket | 78 | 187 | 2 | 2 |
| model.gameobjects.Item | 962 | 150 | 29 | 11 |
| model.Race | 126 | 144 | 1 | 17 |
| utils.ThreadPoolManager | 181 | 138 | 1 | 15 |
| model.gameobjects.Npc | 409 | 138 | 37 | 14 |
| world.World | 351 | 119 | 19 | 14 |
| VisibleObject | 257 | 110 | 11 | 15 |
| utils.PositionUtil | 398 | 95 | 9 | 12 |
| EffectTemplate | 575 | 87 | 26 | 5 |
| StatEnum | 265 | 80 | 0 | 8 |
| ItemTemplate | 578 | 69 | 32 | 6 |
| QuestEnv | 106 | 67 | 3 | 7 |
| Skill | 1096 | 63 | 52 | 9 |
| QuestEngine | 946 | 52 | 37 | 11 |
| ItemPacketService | 236 | 47 | 13 | 6 |
| GeoService | 205 | 45 | 20 | 10 |
| SkillTemplate | 424 | 42 | 26 | 10 |

Top fan-out (integration-last classes): ServerPacketsOpcodes 238, AionClientPacketFactory 190, Effects 172, PlayerEnterWorldService 127, PlayerController 109, DataManager 97, Player 96, StaticData 94, GameServer 70, QuestService 63, PlayerService 60, TeleportService 60, LegionService 56.

## 5. Proposed logical layering (bottom to top) and upward-edge counts
L1 base (model enums, utils, configs, geo) → L2 static data (model.templates, dataholders) → L3 object model (gameobjects, stats, team, items, world, controllers, ai, spawnengine) → L4 engines (skillengine, questEngine, dao, taskmanager) → L5 services (+custom, instance, restrictions) → L6 server packets → L7 network core + client packets.

Of 13,326 edges, 2,011 point upward:
- L1→higher: 149
- L2→L3: 213; L2→L4: 61; L2→L5: 28; L2→L6: 64
- L3→L4: 194; L3→L5: 133; L3→L6: 210
- L4→L5: 61; L4→L6: 87
- L5→L6: 286
- L6→L7: 491 (writeImpl(AionConnection))

Most-referenced upward targets: AionConnection 244, AionServerPacket 239, SM_SYSTEM_MESSAGE 191, Player 59, SM_ATTACK_STATUS 36, Item 35, SM_ITEM_USAGE_ANIMATION 34, Effect 26, SkillTemplate 22, QuestService 18, ItemUseObserver 17. Classes with the most upward references: PlayerController 57, PlayerEnterWorldService 34, StatFunctions 28, NpcController 19, LegionService 19, EffectController 18, CreatureController 17, XMLQuests 17, RideAction 16, SkillUseAction 16.

## 6. SCC results
- Package level (depth 1 and depth 2): one SCC covering all 20 top-level packages, and 121 depth-2 packages, 235.5k lines.
- Class level: the biggest SCC is 1,708 classes / 193,081 lines. Per package: network 58.4k, model 45.1k, services 29.5k, skillengine 14.7k, controllers 7.4k, questEngine 6.9k, dao 6.4k, dataholders 5.4k, geoEngine 4.4k, utils 3.2k, world 3.2k, ai 2.8k.
- Below the SCC (portable with no core headers): 42,430 lines. model.templates 10.0k, DialogAction 6.2k, geoEngine.math 5.5k, configs 2.3k, dataholders 2.2k, gameobjects 2.1k, utils 1.5k, skillengine.model 1.2k, model.geometry 1.1k.
- Edge-cut experiments: without edges into network.*, the SCC is 130.4k lines; also without edges into services.*, 95.3k; also without model/templates/world/controllers → skill/quest/ai, 94.4k; also without dao, 90.6k (model 38.4k, skillengine 14.0k, controllers 7.3k, questEngine 6.9k, dataholders 5.3k, geo 4.4k, world 3.2k).
- Sub-SCCs within a package: services 52 classes / 14.6k; skill+stats+attack+effect-controller 254 classes / 17.9k; questEngine 40 / 5.5k; model 150 / 22.7k.

## 7. Proposed chunk partition (Java lines; ext = dependency edges leaving the chunk, with the share not going to spine or generated code in parentheses)
**Phase 4 serial prerequisites**
- G1 generators: SM_SYSTEM_MESSAGE + DialogAction (35,206 lines, 2 classes, ext 6). JAXB generator for about 327 data-only classes (~17.6k lines, ext 155).
- S0 spine headers: 41 hub classes (15,778 Java lines, but headers only). Player, Creature, Npc, VisibleObject, AionObject, Item, Summon, World, WorldPosition, WorldMapInstance, ZoneInstance, KnownList, VisibleObject/Creature/Observe/EffectController, ActionObserver, CreatureGameStats/LifeStats, StatEnum, Skill, Effect, EffectTemplate, SkillTemplate, AbnormalState, QuestEngine, QuestEnv, QuestState, AbstractAI, AIEventType, AionConnection, AionServerPacket, AionClientPacket, PacketSendUtility, ThreadPoolManager, DataManager, SpawnEngine, GeoService, Race, PlayerClass, TaskId. Bodies go to the owning domain chunk.

**Phase 4 parallel**
| chunk | lines | ext (non-spine) |
|---|---|---|
| P4-01 configs + utils + model.geometry | 6,728 | 60 (33) |
| P4-02 geoEngine.math | 5,923 | 5 |
| P4-03 geoEngine rest (+ world.geo body) | 4,136 | 53 (50, of which 38 go to geo-math) |
| P4-04 dataholders (after generator) | 7,239 | 257 (67) |
| P4-05 templates with logic (non-generated) | 4,096 | 118 (43) |
| P4-06 world + zone + knownlist + spawnengine | 3,385 | 202 (98) |
| P4-07 gameobjects core + controllers (movement, observer, direct) | 7,841 | 646 (327) |
| P4-08 model.gameobjects.player | 5,908 | 177 (92) |
| P4-09 model misc (items/storage, house, skill lists, account, broker, trade, drop, enchants, town, ...); split in 2 | 8,183 | 301 (135) |
| P4-10 dao | 7,199 | 170 (114) |
| P4-11 network core (opcodes, packet factories, login/chat links, iteminfo) | 5,757 | 562 (515; mostly the two opcode/factory tables) |
| P4-12/13 server packets (excluding SM_SYSTEM_MESSAGE); split in 2-3 | 13,253 | 932 (298) |

**Phase 5**
| chunk | lines | ext (non-spine) |
|---|---|---|
| P5-01 stats + combat (model.stats, utils.stats, controllers.attack) | 5,446 | 202 (76) |
| P5-02 skill core (skillengine.model/properties/condition/action/periodic/change/modifier/task, controllers.effect, Effects) | 5,719 | 429 (236; 170 of them to the effects chunk) |
| P5-03/04 effects (180 classes); split in 2 | 7,484 | 793 (279) |
| P5-05 AI framework | 2,966 | 195 (69) |
| P5-06 quest engine (+ QuestService, XMLQuests, QuestsData) | 7,101 | 334 (90) |
| P5-07 item actions + item services + Enchant/Armsfusion/Stigma/Warehouse/CubeExpand/Repurchase/LimitedItemTrade/UpgradeArcade | 7,085 | 562 (238) |
| P5-08 player lifecycle (player.*, teleport, revive, recall, kisk, duel, pvp, skill learn, class change, dialog, social, summons, abyss, toypet) | 6,309 | 661 (441) |
| P5-09 economy (broker, mail, drop, craft, reward, trade/exchange/private store, recipe, passport, bonus/faction packs) | 5,771 | 374 (240) |
| P5-10 legion/housing/team (LegionService, Housing*, Town, LegionDominion, AutoGroup, FindGroup, ChallengeTask, model.team, model.autogroup); split model.team 5.1k / services 4.2k | 9,300 | 412 (205) |
| P5-11 world events (siege, SiegeService, BaseService, rift, vortex, worldraid, panesterra, CP system, event, model.siege/base/vortex/rift); split siege ~4k / rest ~4.3k | 8,334 | 443 (186) |
| P5-12 instance + custom + transfers + model.instance + restrictions | 7,090 | 311 (163) |
| P5-13 misc/admin (remaining services, cron, taskmanager, GameServer, ShutdownHook, chathandlers, audit, cache) | 5,432 | 345 (203) |
| P5-14/15 client packets; split in 2 | 10,879 | 1,250 (505) |

**Phase 6 handlers (158k)**
- Quests: ~13 chunks by region directory (e.g. reshanta 7.2k; inggison 5.9k; heiron 4.9k + ...).
- ai/instance: 4 chunks (23.5k). Other AI: 2 chunks (9.8k).
- instance: 3 chunks (17.2k).
- Commands: 2 chunks (11.8k).

## Key findings
- **No package-level layering exists; 81% of src is one class-level strongly connected component, so a strict bottom-up port order is impossible.**
  - Evidence: Tarjan on 2,310 classes / 13,326 edges: all 20 top packages in one SCC; biggest class SCC = 1,708 classes, 193,081 of 237,192 lines. Only 42,430 lines sit below it (model.templates 10.0k, DialogAction 6.2k, geoEngine.math 5.5k, configs 2.3k). Cutting edges into network.* and services.* still leaves 95.3k lines; the object-model core (model+skillengine+controllers+questEngine+dataholders+world+geo) stays about 90k.
  - C++: Port header-first. A serial 'spine headers' step (about 40 hub classes, with the ownership model from decision A applied) must come before any parallel chunk; chunks then implement .cpp bodies against forward declarations. C++ resolves the cycles through headers/.cpp separation, but only if headers avoid including each other's full definitions (pImpl or forward declarations for Player/Creature/Effect/Skill members).
- **A handful of hub classes carry most of the coupling: Player, the packet send path, DataManager, Creature, Effect.**
  - Evidence: src fan-in: Player 754 (16 packages), AionConnection 441, PacketSendUtility 323, AionServerPacket 245, SM_SYSTEM_MESSAGE 239, DataManager 238, Creature 238, Effect 219, AionClientPacket 187, Item 150, Race 144, ThreadPoolManager 138, Npc 138, World 119, VisibleObject 110. Handlers add, e.g., QuestEnv/QuestState/QuestStatus/AbstractQuestHandler in about 1,040 files and SkillEngine in 165.
  - C++: Freeze these headers early and keep them stable. Changing Player.h or Creature.h later forces a rebuild and re-review of almost every chunk. Consider splitting Player (1,649 lines, 96 outgoing deps) into a lean header plus component headers.
- **About 15% of src is mechanically generatable message and enum code, not logic.**
  - Evidence: SM_SYSTEM_MESSAGE.java: 28,959 lines, 4,120 `public static SM_SYSTEM_MESSAGE STR_...(args)` factories with doc comments. model/DialogAction.java: 6,247 lines, 6,205 `public static final int` constants (converted from an enum because of the JVM 64 kB limit, per its own comment).
  - C++: Write a small generator (Python or CMake step) that emits SM_SYSTEM_MESSAGE factories and DialogAction constants from the Java source. It removes 35k lines from manual porting and from the review load; the generated header should be split or use out-of-line definitions to limit compile times.
- **The skill engine is mostly JAXB-mapped polymorphic template classes with behaviour, so a pure 'generated structs' approach for static data cannot cover it.**
  - Evidence: 779 JAXB-annotated files (44.9k lines). skillengine.effect: 173 JAXB files (8.3k lines) with applyEffect/startEffect/endEffect virtuals (EffectTemplate.java:374-492), selected by 100 @XmlElement names in Effects.java. Item actions: 36 classes (3.6k lines) that call services and send packets (e.g. SkillUseAction → SkillEngine and 8 effect classes). XML-quest conditions/operations, stat functions with Conditions. 12 files use @XmlElements/@XmlElementRef; afterUnmarshal appears in 111 files. skill_templates.xml has 13,570 skill_template entries.
  - C++: For decision B the generator must support: (1) generated data members plus parsing for a class whose virtual behaviour is hand-written in a separate .cpp (e.g. generated `EffectTemplate_gen.h` base plus a hand-written derived class, or generated declarations of `applyEffect` with hand-written definitions); (2) XmlElements name→subclass factories; (3) afterUnmarshal hooks. Templates are immutable and immortal after load, so the runtime can hold `const SkillTemplate*` safely.
- **Skill engine, stats, attack calculation and EffectController are one 254-class SCC and must be ported together, header-first.**
  - Evidence: SCC over skillengine + model.stats + controllers.effect + controllers.attack + utils.stats: 254 classes, 17,873 lines. It includes Skill, Effect, EffectTemplate, all 173 effects, conditions, properties, CreatureGameStats, CreatureLifeStats, PlayerGameStats, StatFunction*, AggroList, AttackUtil, EffectController and StatFunctions. Effects reference controllers.attack/observer 75 times; controllers reference skillengine 81 times; model.stats → Effect/SkillTemplate/Conditions 9 times.
  - C++: Sequence: P5-01 (stats + combat) and P5-02 (skill core headers + bodies) by closely coordinating agents, then the 180 effect classes in 2 parallel mechanical chunks. Put the Effect/EffectTemplate/Skill headers in the spine.
- **Effect and Skill are heap objects referenced from many places, including scheduled tasks that may already be running when cancelled.**
  - Evidence: Effect.java:39-49 (effector, effected, skill, endTask, periodicTasks, periodicActionsTask); :75 observerRemoveTasks; :77 subEffect; :119 designatedDispelEffect. End-task lambda captures `this` (:682-685); scheduleAtFixedRate (:871); cancel(false) (:249, :748, :755, :879; whole src: 30 cancel(false), 15 cancel(true)). Skill schedules this::endCast/cancel (Skill.java:312-314) and applyEffect(effects) (:663). EffectController uses a StampedLock and a LinkedHashMap abnormalEffectMap. CreatureGameStats removes stat functions by StatOwner identity. Inner observer classes capture `Creature effected` (FearEffect.java:60-61, StaggerEffect.java:61-62, HideEffect.java:46,57).
  - C++: Input for decision A: tasks must keep their target alive or check liveness. cancel(false) does not stop a task that already started, so raw `this` captures are unsafe. Effect needs shared ownership (shared_ptr or intrusive refcount, with tasks capturing a strong or weak reference). StatOwner can stay a raw identity pointer as long as endEffect runs before the owner is destroyed.
- **Long-lived strong references to game objects exist in collections keyed by objectId, in services and controllers.**
  - Evidence: AggroList: ConcurrentHashMap<Integer, AggroInfo> where AggroInfo holds `final Creature attacker`. KnownList: Map<Integer, KnownObject> where KnownObject holds `final VisibleObject object`. Invasion.java:29-30 Map<Integer, Player> invaders/defenders; SiegeService.java:69 Set<Player> rvrEventPlayers; RiftManager.java:30 Map<Integer, List<Npc>>; WorldRaid.java:40-41 Npc boss/flag/vortex and List<Npc>; KiskService.java:22-23 Map<Integer, Kisk>; RepurchaseService.java:20 Map<Integer, Set<Item>>; TrapService.java:17 Map<Integer, Queue<Trap>>; Creature.java:43-55 holds AI, stats, EffectController, castingSkill, ObserveController, AggroList.
  - C++: Since the maps already key by objectId, a handle/ID-based or weak_ptr-valued design is a natural fit. Ported code must handle a despawned or logged-out object behind those references; Java quietly keeps such objects alive through the GC.
- **The Java game server is genuinely multithreaded with fine-grained locking, not a single game-loop thread.**
  - Evidence: ThreadPoolManager: ScheduledThreadPoolExecutor + instant ThreadPoolExecutor + cached long-running pool (ThreadPoolManager.java:25-45). Whole src: 223 `synchronized`, 245 Concurrent* collections / CopyOnWriteArrayList, 112 Atomic*, 14 explicit locks. schedule/execute calls: services 63, skillengine 24, ai 11, questEngine 8, controllers 8; handlers 680.
  - C++: Keep Java's concurrency semantics (see CONVENTIONS: recursive_mutex for reentrant monitors). Shared-ownership refcounts must be thread-safe (std::shared_ptr is; a custom intrusive count needs atomics). ConcurrentHashMap iteration semantics (weakly consistent, no exceptions on concurrent modification) need a C++ equivalent, e.g. locked snapshots.
- **Static singletons and static tables couple every chunk, and start-up order is explicit and significant.**
  - Evidence: 94 getInstance() definitions (61 in services, 11 taskmanager, 4 world, 4 utils, 3 network); 68 SingletonHolder uses. getInstance() calls: 1,322 in src, 1,344 in handlers (ThreadPoolManager 874, SkillEngine 312, World 288, QuestEngine 83, SiegeService 80, GeoService 73, LegionService 70, CraftSkillUpdateService 52, HousingService 51). 75 services classes are static-only; DAOs are static (PlayerDAO.storePlayer static, PlayerDAO.java:47). DataManager has 91 public static fields, referenced 517 times in src and 104 in handlers (ITEM_DATA 86, QUEST_DATA 78, SKILL_DATA 58). GameServer.java:96-177 initializes in order: IDFactory → DataManager → parallel QuestEngine/AIEngine/InstanceEngine/ChatProcessor/ZoneService/GeoService init → World → GameTimeService → DropRegistrationService → Base/Siege/WorldRaid/Vortex/Rift/LegionDominion → Housing → ... → EventService.start → GameTimeService.startClock.
  - C++: Keep function-local-static singletons (as CONVENTIONS says) but reproduce the explicit init sequence in GameServer::main. Accessor calls should not start heavy work from a lazy first call on an arbitrary thread. DataManager tables should be `const` after load. Because singletons are global, chunks can be ported against a header of the singleton before its body exists; link-time stubs let chunks compile independently.
- **The quest engine is ID-keyed and largely data-driven; a small template set covers most quests.**
  - Evidence: QuestEngine.java:52-80 has 29 registration maps/lists keyed by questId/npcId/ZoneName/Race; handlers are stored in Map<Integer, AbstractQuestHandler>. AbstractQuestHandler (1,291 lines) exposes 33 on* methods. quest_script_data drives about 4,174 quests via 17 template handlers (item_collecting 1681, monster_hunt 1163, work_order 574, report_to 468, ...). 1,035 Java quest handlers (95.5k lines) are loaded through QuestHandlerLoader reflection (isAssignableFrom + newInstance).
  - C++: Quest handlers are stateless per-questId singletons: registration is a static table of factories (e.g. a `REGISTER_QUEST(QuestXXXX)` macro or a generated list), with no lifetime issues. Porting questEngine (7.1k lines) early makes about 4,174 quests work before any of the 95k handler lines are ported.
- **Static data classes depend upward on services, packets, quests and skills, mostly through item actions and a few holders.**
  - Evidence: model.templates → services 25, controllers 19, network.sm 64, skillengine 16, questEngine 9 edges. Almost all come from model.templates.item.actions (e.g. EnchantItemAction → EnchantService, QuestStartAction → QuestEngine, SkillUseAction → SkillEngine plus 8 effects). Also PetFlavour → toypet, SpawnGroup → PanesterraFaction, TemporarySpawn → GameTimeService. dataholders → QuestEngine (QuestsData), XMLQuests → 17 quest models, MotionData/SkillData/SkillTreeData → skillengine, InstanceCooltimeData → InstanceService, Portal2Data → PortalService.
  - C++: Move item-action behaviour into the item-services chunk (P5-07) and keep the generated data part in phase 4. Holder classes that call services should take those calls through forward-declared service headers, or move the calls out of the holders.
- **The services package has a 52-class internal SCC; god orchestrators should be ported last.**
  - Evidence: services SCC: 52 classes, 14,593 lines (LegionService, QuestService, SiegeService, BrokerService, HousingService, TeleportService, DropService, MailService, EventService, PlayerService, PlayerReviveService, siege.*, ...). Highest fan-out: PlayerEnterWorldService 127, PlayerController 109, QuestService 63, PlayerService 60, TeleportService 60, LegionService 56, DialogService 49, SiegeService 46. Most-used services: ItemPacketService fan-in 47, LegionService 33, PlayerService 33, TeleportService 31, ItemService 29, SiegeService 25, QuestService 24 (plus 252 handler imports).
  - C++: Service chunks can run in parallel once all service headers exist (they are singletons or static functions, so headers are cheap). PlayerEnterWorldService, PlayerController, ServerPacketsOpcodes (238 deps) and AionClientPacketFactory (190 deps) are integration points: port them last or generate them.

## Risks
- 1. Header churn in the spine: Player/Creature/Effect/Skill/DataManager are referenced by hundreds of classes. If the ownership model (decision A) or member layout changes after parallel chunks start, most chunks need rework. Mitigation: finish and review the spine headers before fan-out.
- 2. Effect/Skill lifetime under a multithreaded scheduler: tasks capture `this` and cancel(false) cannot stop running tasks (Effect.java:682, :748). Raw pointers here cause use-after-free that only shows up under load.
- 3. Behaviour-bearing JAXB classes (about 16k lines: 173 effects, 36 item actions, conditions, XML-quest operations, stat functions) straddle decision B and hand-porting. A generator that only emits POD structs would force rework, and one that emits full classes makes hand-written virtuals awkward.
- 4. The skills+stats+attack SCC (17.9k lines) is the gameplay core ('fight a mob'). Formula details (StatFunctions 699, AttackUtil 549, EffectTemplate.calculate, Math.round semantics, float→int saturation, integer overflow in counters such as attackCounter) are easy to get subtly wrong, and with no Java reference run the only checks are review and the client.
- 5. Java concurrency idioms without a direct C++ equivalent: ConcurrentHashMap.compute plus synchronized on list values (CreatureGameStats.addEffectOnly), weakly consistent iteration, StampedLock optimistic reads (EffectController), synchronized reentrancy (223 uses).
- 6. Singleton initialization order: lazy C++ statics would silently change the explicit GameServer start-up order (GameServer.java:96-177), including the parallel GameEngine init of QuestEngine/AIEngine/InstanceEngine/ChatProcessor/ZoneService/GeoService.
- 7. Chunk coupling is highest for client packets (1,250 external edges) and player lifecycle services (661 external edges, 441 outside the spine). These need most of the service headers and should come late.
- 8. Handler volume (158k lines, 680 schedule calls, instance/AI handlers holding Npc/Player fields) will surface every gap in the core API. Handler chunks should start only when AbstractQuestHandler, AbstractAI/NpcAI and the instance handler base are stable.
- 9. Dependency analysis accuracy: same-package references are detected by capitalized token match, nested classes are folded into their outer file, and handler references are not included in fan-in, so counts are approximate (probably ±5%).

## Dependencies
What the analysed subsystems depend on:
- **skillengine:** model.gameobjects (214 edges), utils (87: ThreadPoolManager, PacketSendUtility, PositionUtil), controllers (75: attack/observer/effect), network.sm (67), model.stats (59), model.templates (27), world (27), model.other (26), geoEngine (21), services (16: Teleport/Summons/Recall/PlayerRevive/Trap/Craft/Item), dataholders (15), ai (15: AIEventType, EmoteManager, NpcAI), configs (12), spawnengine (9).
- **model.stats:** gameobjects (33), utils (27), templates (22), skillengine (9: Effect, AbnormalState, SkillTemplate, Conditions), services (6: LifeStatsRestoreService, EnchantService, SkillLearnService).
- **questEngine:** gameobjects (50), templates (44), model.other (31), services (24: mostly QuestService, plus ItemService, RiftService, VortexService, CraftSkillUpdateService, RecipeService, CronService), dataholders (20), network.sm (17), world (10).
- **services:** gameobjects (360), model.other (327), network.sm (277), templates (252), utils (242), dao (109), configs (103), world (100), dataholders (80), model.team (61), questEngine (37), skillengine (26), spawnengine (24), controllers (19).

What depends on them:
- Client packets → services: 140 edges.
- controllers → services: 41 (PlayerController alone uses about 20 services).
- model.gameobjects → services: 18 (Player → Duel/Exchange/Housing; Equipment → Stigma/ItemPacket; Npc → TribeRelation).
- model.templates → services: 25 (item actions).
- Handlers: 680 service imports (QuestService 252, TeleportService 124, CraftSkillUpdateService 44, InstanceService 29, ItemService 27), 317 skillengine imports (SkillEngine 165, SkillTemplate 65, Effect 65), 4,328 questEngine imports.

Porting order that follows from the graph:
1. Generators: JAXB, SM_SYSTEM_MESSAGE/DialogAction.
2. In parallel with generator work: configs/utils, geoEngine.math.
3. Spine headers (serial).
4. Phase 4 parallel chunks: dataholders, templates-with-logic, world/spawn, gameobjects+controllers, player model, model misc, dao, network core, server packets. Server packets depend only on model getters plus AionConnection, so they parallelize well once model headers exist.
5. Phase 5: stats+combat together with skill core, then effects ×2, AI framework, quest engine. Service domain chunks in parallel once all service headers exist. Client packets and the integration classes last (PlayerEnterWorldService, PlayerController, ServerPacketsOpcodes, AionClientPacketFactory, GameServer).
6. Phase 6 handlers: quests after the quest engine and QuestService; AI after the AI framework and skills; instances after InstanceService/custom; commands last, since they touch everything.

Subsystems that must be ported together (same SCC):
- skillengine + model.stats + controllers.attack + controllers.effect + utils.stats (254 classes, 17.9k lines)
- questEngine core + templates + models + XMLQuests (40 classes, 5.5k lines)
- the services SCC (52 classes, 14.6k lines; parallelizable only after headers)
- the model SCC (150 classes, 22.7k lines: Player, Creature, Item, storages, stat containers, skill lists, House, TransformModel, ...)
- geoEngine is pulled into the core SCC only by GeoWorldLoader → ZoneService/DataManager and DespawnableNode → SiegeService/EventService. Those few edges can be forward-declared, so geo can be ported early.

Evidence tooling (read-only, in the session scratchpad): C:\Users\esfis\AppData\Local\Temp\claude\...\scratchpad\svc\ (deps.json dependency graph, an.py, chunks.py, cut.py, scc.py, layer.py, lay2.py).

## Open questions
- Should the spine headers be produced by a Java-to-C++ skeleton generator (class names, fields, method signatures from a Java parser such as tree-sitter-java), or by hand? About 40 hub classes are needed at minimum; skeletons for all 2,310 classes would let chunks compile independently from day one.
- Decision B detail: should behaviour-bearing JAXB classes (EffectTemplate subclasses, AbstractItemAction subclasses, XML-quest conditions/operations, StatFunction) be generated as full classes with hand-written virtual definitions in separate .cpp files, or generated as data structs wrapped by hand-written behaviour classes? I did not check how many effect subclasses keep non-JAXB mutable fields (template-level state shared across all casts).
- Should the C++ port keep the Java multithreaded model (scheduled pool + instant pool + per-object locks), or move world/combat logic to strands per map instance? This choice strongly affects decision A and the porting effort for 223 synchronized blocks and 245 concurrent collections.
- Not verified: how far the geo data format and GeoWorldLoader tie into geoEngine.scene/collision (10k lines), and whether an existing C++ math library could replace geoEngine.math (5.9k lines, a jME port) without changing float results.
- Not verified: whether client packets (P5-14/15) can be split by opcode ranges that line up with service chunks, and whether ServerPacketsOpcodes/AionClientPacketFactory should be generated from the packet classes.
- Not measured: how many handler classes (ai/instance/quest) keep per-instance mutable state or scheduled futures referencing Npc/Player. The single-tab field regex found 36 such fields in 25 files, which is likely an undercount. This matters for handler ownership rules.
- Counts are static-import based; handler references into src were counted by import lines only (not same-package or reflection/string lookups such as AI names in XML). Phase 6 coupling is therefore approximate.
- Process note: the session scratchpad is shared with other parallel agents (my first an.py was overwritten by another agent's script). Later research agents should use unique subfolders.
