# dao-geo

## Summary
I covered the persistence layer (game-server/src/com/aionemu/gameserver/dao, 56 files, 7,143 lines) and the geo engine (geoEngine, 29 files, 10,030 lines), plus their callers and the data files. Everything was read with grep/read plus two small Python scripts in the scratchpad that parse data/geo. Nothing was written to D:\aion-server.

DAOs: all 56 are static utility classes with the SQL written inline. There is no DAOManager or interface/implementation split any more. The 61 tables in sql/aion_gs.sql are all used by DAOs, and every table a DAO uses exists in the schema. The only SQL outside the dao package is DatabaseCleaningService (OPTIMIZE TABLE). The JDBC patterns in use (batches, manual transactions, generated keys, blobs via setBytes/getBytes, scrollable result sets, nullable setObject/getObject, a batch mixing plain SQL with session variables) are already supported by the ported cpp/commons database API. The DAOs themselves can therefore be ported almost line by line.

The real coupling is to the model. DAOs build and change Player, Item, Legion, House and QuestState objects directly, read DataManager, look up World/LegionService/PvpService, and filter items by the Persistable dirty-state enum (73 files). Player data is loaded synchronously by PlayerService.getPlayer (about 30 DAO calls, one pooled connection each, and N+1 queries for item stones). It is saved by PlayerService.storePlayer (17 DAOs) at logout, and every 900 s by per-player periodic tasks. Those tasks capture only the playerId and look the player up in World, which is precedent for ID/handle-based references. The tasks read mutable Player state from pool threads without locks. Java tolerates that race; in C++ it is undefined behaviour and needs a design answer.

Geo: GeoWorldLoader reads a big-endian binary models.mesh (70.8 MB, 18,583 entries, 25,437 meshes, 3.44M vertices, 5.54M triangles) and 151 big-endian <mapId>.geo instance files (419,707 placements, 484,111 geometry instances). Terrain is 78 PNG files, not a binary format: 16-bit grayscale heightmaps, 8-bit grayscale materials, and one 8-bit palette PNG whose raw palette indices are used. The engine is a small jMonkeyEngine subset: a scene graph (Node/Geometry/DespawnableNode), one lazily built BIH tree per shared Mesh, ray/triangle and ray/AABB tests, and a heightmap ray walker. 59% of its lines (5,918) are the generic jME math classes, most of which the geo code does not use. The game queries are canSee (21 call sites), getClosestCollision (18), getZ (13) and findMovementCollision (4), plus runtime door/placeable/town/house-door state per instance. geoEngine.math.Vector3f is also used as the general 3D vector type in 45 files outside geo (skills, AI, zones). The algorithms port easily; the hard parts are memory layout, float-identical results, a lazy-init data race in Java, and small but real dependencies from geo back into game services (EventService, SiegeService, ZoneService, AI).

## Inventory
## Part 1: DAOs (game-server/src/com/aionemu/gameserver/dao, 56 files, 7,143 lines)

Style: `public class XDAO { private static final Logger log; public static ... }`. There are no interfaces or DAOManager (GameServer.java:113 still has a commented-out `DAOManager.getDAO(...)`). The SQL is in string constants or inline. 78 non-DAO source files in src and 11 handler files (data/handlers) call DAOs. 334 distinct `XDAO.method(` call names.

### DAO → tables (checked against sql/aion_gs.sql: 61 CREATE TABLEs, all used, none missing)
| DAO | lines | tables | batch | manual tx | gen. keys | blob | other |
|---|---|---|---|---|---|---|---|
| PlayerDAO | 503 | players, legion_members | | | | | scrollable RS (getUsedIDs), DB.* helpers x13, DataManager exp table |
| InventoryDAO | 414 | inventory, item_stones (join), players, legion_members | yes (insert/update/delete) | yes (commit per batch) | | | setObject(Types.INTEGER), getObject nullable, scrollable RS, IDFactory.releaseObjectIds |
| LegionDAO | 370 | legions, legion_announcement_list, legion_emblems, legion_history | | | yes (:341) | emblem_data longblob | dynamic IN (%s) placeholders, DB.* x11, scrollable RS |
| AbyssRankDAO | 342 | abyss_rank, players, legions, legion_members | yes, mixed `addBatch(sql)` with `SET @a=0` session vars (:298-327) | | | | DataManager exp table |
| PlayerRegisteredItemsDAO | 298 | player_registered_items | yes | yes | | | World.findVisibleObject, HouseObjectFactory, setObject nullable, scrollable RS |
| ItemStoneListDAO | 290 | item_stones | yes | yes | | | N+1 load per item, deletes rows while loading, DataManager.ITEM_DATA |
| MailDAO | 234 | mail, players | | | | | calls InventoryDAO.loadItems and links attachments, scrollable RS |
| PlayerAppearanceDAO | 180 | player_appearance | | | | | REPLACE INTO |
| PlayerPetsDAO | 175 | player_pets | | | | | scrollable RS |
| HousesDAO | 166 | houses | | | | | scrollable RS |
| PlayerQuestListDAO | 162 | player_quests | yes | yes | | | setObject(Types.TIMESTAMP/SMALLINT) |
| AccountPassportsDAO, BrokerDAO | 158 each | account_passports, account_stamps / broker | | | | | ON DUPLICATE KEY; Broker loads InventoryDAO + ItemStoneListDAO |
| LegionDominionDAO | 147 | legion_dominion_locations, legion_dominion_participants | | | | | |
| PlayerSkillListDAO | 139 | player_skills | yes | yes | | | REPLACE INTO |
| LegionMemberDAO | 137 | legion_members | | | | | LegionService lookup |
| PlayerSettingsDAO | 128 | player_settings | | | | settings blob | 5 REPLACE INTO per save |
| PlayerEffectsDAO | 124 | player_effects | yes | yes | | | |
| PlayerPunishmentsDAO | 118 | player_punishments | | | | | |
| EventDAO | 115 | event | yes | | | | |
| FriendListDAO | 104 | friends | yes | | | | |
| GuideDAO | 104 | guides | | | | | scrollable RS |
| HouseBidsDAO | 104 | house_bids, players | | | | | |
| BlockListDAO | 99 | blocks | | | | | |
| ChallengeTasksDAO | 98 | challenge_tasks | | | | | DataManager.CHALLENGE_DATA |
| PlayerPasskeyDAO | 98 | player_passkey | | | | | |
| CustomInstancePlayerModelEntryDAO | 97 | custom_instance_records | yes | yes | | | |
| ItemCooldownsDAO | 97 | item_cooldowns | yes | yes | | | |
| PlayerBindPointDAO | 96 | player_bind_point | | | | | |
| PlayerCooldownsDAO | 92 | player_cooldowns | yes | yes | | | |
| PlayerNpcFactionsDAO | 91 | player_npc_factions | | | | | |
| SiegeDAO | 90 | siege_locations | | | | | |
| MotionDAO | 84 | player_motions | | | | | |
| PortalCooldownsDAO | 83 | portal_cooldowns | | | | | |
| HouseScriptsDAO | 82 | house_scripts | | | | script (UTF-16LE, compressed) | ON DUPLICATE KEY |
| PlayerMacrosDAO | 82 | player_macrosses | | | | | |
| HeadhuntingDAO | 80 | headhunting | | | | | PvpService lookup |
| TownDAO | 79 | towns | | | | | |
| CommandsAccessDAO | 78 | commands_access | | | | | |
| CustomInstanceDAO, PlayerTitleListDAO | 76 each | custom_instance / player_titles | | | | | |
| AnnouncementsDAO | 75 | announcements | | | yes (:48) | | |
| CraftCooldownsDAO, HouseObjectCooldownsDAO | 71 each | craft_cooldowns / house_object_cooldowns | | | | | |
| BookmarkDAO, ServerVariablesDAO, PlayerRecipesDAO, PlayerEmotionListDAO, PlayerLifeStatsDAO, RewardServiceDAO (batch+tx), SurveyControllerDAO, VeteranRewardDAO, OldNamesDAO, FactionPackDAO, BonusPackDAO, AdventDAO | 37-68 | bookmark, server_variables, player_recipes, player_emotions, player_life_stats, player_web_rewards, surveys, player_veteran_rewards, old_names, faction_packs, bonus_packs, advent | | | | | mostly REPLACE INTO |

Pattern totals:
- `addBatch`: 13 DAOs.
- `setAutoCommit(false)`: 10 DAOs.
- `RETURN_GENERATED_KEYS`: 2 (AnnouncementsDAO:48, LegionDAO:341).
- Binary columns: 3 (PlayerSettingsDAO, LegionDAO, HouseScriptsDAO).
- `TYPE_SCROLL_INSENSITIVE` + `rs.last()/getRow()`: 8 (the `getUsedIDs` methods feeding IDFactory.java:134-141).
- Legacy DB helpers with anonymous `IUStH`/`ParamReadStH`/`ReadStH` classes: about 81 call sites in 19 DAOs.
- Foreign keys: 27 constraints with ON DELETE CASCADE. PlayerDAO.deletePlayer depends on the cascade. The inventory table has no FK, so InventoryDAO.deletePlayerOrLegionItems deletes items explicitly.
- Schema drift: update.sql adds `player_effects.magical_criticals`, which aion_gs.sql:662 and PlayerEffectsDAO already contain.

### Load and save flow
- **Account (character list) load:** LoginServer.java:161 → AccountService.getAccount → loadAccount. For each character: PlayerDAO.loadPlayerCommonData, PlayerPunishmentsDAO.getCharBanInfo, PlayerAppearanceDAO.load, InventoryDAO.loadVisibleEquipment and PlayerDAO.setCreationDeletionTime. Then the account warehouse (InventoryDAO.loadStorage + ItemStoneListDAO.load).
- **Enter world:** PlayerEnterWorldService.enterWorld checks PlayerDAO.isOnline (the reentry guard), then calls PlayerService.getPlayer (PlayerService.java:103-176). That runs about 30 DAO calls in sequence (macros, skills, friends, blocks, titles, settings, abyss rank, NPC factions, motions, passport, quests, recipes, inventory/warehouse/pet bags/cabinets plus stones per storage, punishments, effects, cooldowns (player, item, portal, house object, craft), bind point, life stats, emotions). Each call borrows its own pooled connection. The pool has 5 connections (config/network/database.properties:17) with a 5000 ms timeout.
- **Full save:** PlayerService.storePlayer (:80-100) covers PlayerDAO, skills, settings, quests, abyss rank, 2x punishments, InventoryDAO.store, houses, ItemStoneListDAO.save, mailbox, portal/craft/house cooldowns, NPC factions, passport and headhunting. It is called from PlayerLeaveWorldService.leaveWorld:142, CMT_CHARACTER_INFORMATION:395 and the admin command Debug.java:60.
- **Logout extras:** PlayerEffectsDAO, ItemCooldownsDAO and PlayerLifeStatsDAO (:104-106), PlayerCooldownsDAO (:118), then PlayerDAO.storeOldCharacterLevel, storeLastOnlineTime and onlinePlayer(false) (:148-150). The online flag is the "fully saved" marker.
- **Periodic saves:**
  - Per player, scheduled at PlayerEnterWorldService.java:367-371: GeneralUpdateTask (PLAYER_GENERAL=900 s: abyss rank, skills, quests, player, houses) and ItemUpdateTask (PLAYER_ITEMS=900 s: inventory + stones). Both capture a playerId and call `World.getInstance().getPlayer(playerId)`.
  - PeriodicSaveService: legion warehouse every 1200 s, server_variables `serverLastRun` every 120 s.
  - PetSpawnService: PLAYER_PETS every 10 s.
- **Event-driven writes** from services, e.g. LegionService (about 25 DAO calls), MailService, BrokerService, HousingService, CM_APPEARANCE, CM_CHARACTER_EDIT, CM_CHARACTER_PASSKEY.
- **Startup:**
  - IDFactory loads used IDs from 8 tables.
  - GameServer calls PlayerDAO.setAllPlayersOffline (:222) and getCharacterCountForRace (:131-132).
  - HousingService loads houses; BrokerService loads broker + items; LegionService loads legions lazily.
- **Caching:** none in the DAO layer (ChallengeTasksDAO only builds a local ConcurrentHashMap as its return value). Caches live in services: LegionService cached legions, AbyssRankingCache, BrokerService.playerBrokerCache, HousingService maps. PlayerService.getOrLoadPlayerCommonData tries World first, then the DAO.

### DAOs most tied to model classes (model imports / other dependencies)
- ItemStoneListDAO: 10 imports, DataManager.ITEM_DATA, Item mutation.
- InventoryDAO: 9, 27-argument Item constructor, IDFactory.
- PlayerDAO: 8.
- MailDAO: 8.
- PlayerRegisteredItemsDAO: 7, World and HouseObjectFactory.
- HousesDAO: 6.
- AbyssRankDAO: 6.
- PlayerSkillListDAO, PlayerNpcFactionsDAO, ChallengeTasksDAO: 5 each.
- DataManager/Service/World use: AbyssRankDAO, ChallengeTasksDAO, HeadhuntingDAO (PvpService), ItemStoneListDAO, LegionMemberDAO (LegionService), PlayerDAO, PlayerRegisteredItemsDAO.

## Part 2: geoEngine (29 files, 10,030 lines)
| Package | Files (lines) |
|---|---|
| math | FastMath 748, Matrix3f 1242, Matrix4f 1829, Ray 373, Vector2f 718, Vector3f 1007 (5,918 total = 59%) |
| bounding | BoundingVolume 201, BoundingBox 616 |
| collision | Collidable 48, CollisionIntention 63, CollisionResult 94, CollisionResults 185, IgnoreProperties 52, UnsupportedCollisionException 59; bih/BIHTree 238, bih/BIHNode 235 |
| scene | Spatial 257, Node 496, Geometry 167, Mesh 150, DespawnableNode 139, CollisionData 51, mesh/IndexArray 20, IndexByteArray 39, IndexShortArray 39 |
| models | GeoMap 333, Terrain 188 |
| utils / root | TempVars 158, GeoWorldLoader 285 |
Facade: world/geo/GeoService.java (not in geoEngine).

### data/geo (159 MB, 230 files), verified by parsing every file
- **models.mesh** (70,828,016 bytes), big endian, no header, repeated entries:
  - `int16 nameLen, byte[nameLen] name, uint8 modelCount`
  - per model: `uint16 vertexCount, float32[3*vertexCount] vertices, uint16 faceCount, int8 indexSize (1|2), (indexSize)[3*faceCount] indices, int8 materialId, int8 collisionIntentions`
  - 18,583 entries; 1,053 contain `|` and expand to 2,498 aliases.
  - 25,437 meshes: 3,441,846 vertices, 5,539,954 triangles; largest mesh 10,969 vertices / 16,110 triangles.
  - Index size 1 byte: 22,308 meshes; 2 bytes: 3,129. Vertex payload 41.3 MB, index payload 27.9 MB.
  - Models per entry: 1 (16,394 entries) up to 84.
  - Intention byte values: 1 (24,819), 3 (326), 136 (128), 2 (97), 33 (58), 8 (9). Materials: 19 distinct IDs, 423 meshes with a non-zero material.
- **<mapId>.geo**: 151 files (world_maps.xml defines 161 maps), big endian, repeated 64-byte records after the name:
  - `int16 nameLen, name, float32 x,y,z, float32[9] rotation (row-major Matrix3f.set(i,j)), float32 sx,sy,sz, int8 type, int16 staticId (-1 = none), int8 townLevel`
  - Checked: 320070000.geo = 248 bytes = 2 records; `3f800000` = 1.0f; the name length is big endian.
  - 419,707 placements, 0 missing mesh references, 484,111 geometry instances (66.1M instanced triangles), 7,961 MATERIAL geometries that become material zones.
  - Type counts: 0 → 404,123; 1 EVENT 949; 2 PLACEABLE 5,890; 3 HOUSE 5,174; 4 HOUSE_DOOR 1,036; 5 TOWN_OBJECT 819; 6/7 DOOR_STATE1/2 858 each. SHIELD (8) never appears in files; SiegeShield sets it at runtime.
  - Largest maps: 700010000 (22,603 placements), 710010000, 220040000, 210070000 (6.0M instanced triangles), 220080000 (6.2M).
- **PNG terrain** (78 files; the file name is a comma-separated list of map ID prefixes shared by several maps):
  - 68 are 16-bit grayscale heightmaps, 9 are 8-bit grayscale materials, and 1 (301210000_materials.png) is an 8-bit palette image whose raw indices are used (ImageIO DataBufferByte).
  - None interlaced. Sizes: 1536² (23), 1024² (13), 768² (7), 512² (20), 384² (1), 256² (11), 128² (3), plus tiny 1x1 flat images.
  - Upper bound of decoded samples: 149.1 MB. Flat heightmaps collapse to 1 value (Terrain.java:32). Z = uint16 * 2048 / 65536, 0xFFFF = hole; grid step 2 m; perimeter forced to z=0.

### Queries used by the game (GeoService call sites)
| Method | Sites |
|---|---|
| canSee | 21 |
| getClosestCollision | 18 |
| getZ | 13 |
| findMovementCollision | 4 |
| setDoorState | 3 |
| getCollisions, updateTown, setHouseDoorState, despawnPlaceableObject, worldHasTerrainMaterials | 2 each |
| spawnPlaceableObject, getTerrainMaterialAt | 1 each |

43 files outside geoEngine import it:
- math.Vector3f: 45 imports (effects Dash/Fear/Pulled/..., AI, zones, WalkManager, SM_PLAYER_INFO).
- CollisionResults: 17; CollisionIntention: 14; IgnoreProperties: 10.
- scene.Spatial: 10 (MaterialZoneHandler, SiegeShield, ZoneService, AbstractCollisionObserver, CollisionDieActor, ShieldService).

GeoDataConfig switches: GEO_ENABLE, CANSEE_ENABLE, FEAR_ENABLE, GEO_NPC_MOVE, GEO_MATERIALS_ENABLE, GEO_MATERIALS_SHOWDETAILS, GEO_SHIELDS_ENABLE.

## Key findings
- **The ported C++ commons database API already covers every JDBC feature the game DAOs use, so the 56 DAOs are a mechanical port once the model classes exist.**
  - Evidence: cpp/commons/src/aion/commons/database: PreparedStatement.h:165-180 (addBatch(), addBatch(sql), executeBatch, getGeneratedKeys), ResultSet.h:139-151 (last/beforeFirst/absolute/getRow), Connection.h:65-76 (setAutoCommit/commit/rollback/savepoints), setBytes, setObject(value, sqlType), DB.h (select/insertUpdate/prepareStatement/executeUpdateAndClose/close), DatabaseFactory.cpp:28-40 rolls back a dirty transaction and restores auto-commit when a connection goes back to the pool (same as HikariCP). Game-side uses: 13 DAOs with batches, 10 with manual transactions, 2 with generated keys (AnnouncementsDAO:48, LegionDAO:341), 3 with blobs, 8 with scrollable getUsedIDs, and the AbyssRankDAO:298-327 batch mixing plain SQL and session variables.
  - C++: No new database infrastructure is needed. Port the DAOs file by file (7.1k Java lines, probably 6-8k C++ lines). Replace the anonymous IUStH/ParamReadStH classes (about 81 sites) with lambdas, as CONVENTIONS.md already shows. DAO porting is gated by the model classes (Player, PlayerCommonData, Item, Storage, Legion, House, QuestState, ...), not by the database layer.
- **DAO 'transactions' are not atomic and keep Java-specific quirks that must be preserved on purpose or listed in DEVIATIONS.md.**
  - Evidence: InventoryDAO.store:227-243 sets autocommit false, then delete/insert/update each commit on their own, there is no rollback on failure, and all items are marked UPDATED even when a batch failed. PlayerQuestListDAO.store:67-72 and PlayerSkillListDAO, ItemStoneListDAO, PlayerRegisteredItemsDAO follow the same pattern. PlayerSettingsDAO.saveSettings runs 5 separate REPLACE INTO statements. IDs are released (IDFactory.releaseObjectIds) only if the delete succeeded.
  - C++: Keep the behaviour for fidelity (the pool reset already mimics Hikari), or deliberately switch to one Transaction per store and record it in DEVIATIONS.md. Do not silently 'fix' only some DAOs.
- **Player persistence is synchronous and fragmented: about 30 DAO calls to load a player and 17 to save one, each with its own pooled connection, and some N+1 query loops. Load and save run on whatever thread triggered them.**
  - Evidence: PlayerService.getPlayer (PlayerService.java:103-176), called from PlayerEnterWorldService.java:158 and PlayerTransferService.java:86. PlayerService.storePlayer (:80-100), called from PlayerLeaveWorldService.leaveWorld:142 (a delayed ThreadPoolManager task or CM_QUIT), CMT_CHARACTER_INFORMATION:395 and Debug.java:60. ItemStoneListDAO.load:40-104 runs one SELECT per armor/weapon item. AccountService.loadAccount runs 5 queries per character plus the warehouse, called from LoginServer.java:161. Pool size is 5 (config/network/database.properties:17).
  - C++: Load/save cannot run on an Asio I/O thread without stalling it. Use the packet-processor pool or scheduled pool as Java does. Keep the online flag and the reentry guard (PlayerDAO.isOnline / onlinePlayer(false) at the end of leaveWorld) exactly as they are, because they serialize logout-save against re-login.
- **Periodic saves read live, mutable Player state from pool threads without synchronization. The Java code tolerates this race; in C++ it is undefined behaviour.**
  - Evidence: PlayerEnterWorldService.java:367-371 schedules GeneralUpdateTask (:496-521: AbyssRankDAO.storeAbyssRank, PlayerSkillListDAO.storeSkills, PlayerQuestListDAO.store, PlayerDAO.storePlayer, houses) and ItemUpdateTask (:523-544: InventoryDAO.store(player) iterating player.getDirtyItemsToUpdate(), ItemStoneListDAO.save) every 900 s on ThreadPoolManager threads while packet threads change the same inventory and quest lists. PeriodicSaveService.LegionWarehouseSaveTask iterates legion warehouses the same way.
  - C++: Decision (A) has to cover this. Options: run the save on the player's serial executor or strand; or take a snapshot (row structs) under the player's lock and write it off-thread; or give storages and lists their own mutexes. The row-snapshot option also decouples the DAOs from the live object graph.
- **Java already references players by ID in delayed persistence tasks, and DAOs look up live objects through World by ID.**
  - Evidence: GeneralUpdateTask and ItemUpdateTask store `int playerId` and resolve `World.getInstance().getPlayer(playerId)` on each run (PlayerEnterWorldService.java:496-544). PlayerRegisteredItemsDAO.constructObject:80-90 uses `World.getInstance().findVisibleObject(itemUniqueId)`. PlayerService.getOrLoadPlayerCommonData checks World, then the DAO. LegionMemberDAO:89 uses LegionService.getLegion(id), HeadhuntingDAO:61 uses PvpService.getHeadhunter(id).
  - C++: This is evidence for option (A) with ID/handle lookups: delayed tasks and DAO code that touch world objects can take an object ID and re-resolve it, which removes most GC-style captures. Objects loaded by DAOs have a clear single owner (Player owns storages; a storage owns its Items), so they fit unique_ptr/value ownership. Items that move between owners (mail attachments, broker, exchange, legion warehouse) need an explicit transfer.
- **Several DAOs are inseparable from the model and static data. They are not a leaf layer and cannot be ported ahead of the model.**
  - Evidence: ItemStoneListDAO (DataManager.ITEM_DATA :57, mutates Item stones, deletes invalid stones during load). InventoryDAO.constructItem:108-139 (27-argument Item constructor that needs ItemTemplate lookup). MailDAO:71 calls InventoryDAO.loadItems. BrokerDAO:28-29 calls InventoryDAO + ItemStoneListDAO. AbyssRankDAO:199 and PlayerDAO:431 use DataManager.PLAYER_EXPERIENCE_TABLE. ChallengeTasksDAO:41 uses DataManager.CHALLENGE_DATA. Persistable.PersistentState (NEW/UPDATE_REQUIRED/UPDATED/DELETED/NOACTION) appears in 73 files.
  - C++: Porting order: static data (generated loaders) → model value classes with PersistentState → DAOs, in the same phase as the owning service. Port Persistable/PersistentState early as a small shared header.
- **Geo data is all big endian, and the format is simple enough to parse in a few hundred lines. Terrain comes from PNGs, which needs a PNG decoder that returns 16-bit grayscale and raw palette indices.**
  - Evidence: No `order()` call on the MappedByteBuffer (GeoWorldLoader.java:115, 183), so Java's big-endian default applies. The hex dump shows `00 34` name length and `3f 80 00 00` = 1.0f. Parser totals: 18,583 mesh entries / 25,437 meshes; 151 .geo files / 419,707 records; every file parsed to exactly its end. PNGs: 68 x (16-bit, grayscale), 9 x (8-bit, grayscale), 1 x (8-bit, palette) = 301210000_materials.png, which Java reads through DataBufferByte (indices, not RGB). GeoWorldLoader.java:72-76.
  - C++: Read with the commons utils::ByteBuffer set to BIG_ENDIAN_ORDER (already in CONVENTIONS.md) or with std::byteswap on memory-mapped data; copy into native float/uint8/uint16 arrays, as Java does (Mesh.setVertices copies). PNG: lodepng (vcpkg) with color_convert=false or libspng with raw output both give 16-bit grayscale and palette indices. stb_image cannot return raw palette indices, so avoid it or special-case that one file.
- **The geo engine is a small jME subset. The real logic is about 2k lines (loader, GeoMap, Terrain, BIH, Ray/AABB intersection, Node/Geometry/DespawnableNode); 59% of the 10k lines is the generic jME math library.**
  - Evidence: math/*.java = 5,918 of 10,030 lines. The collision path uses Ray.intersects (a Möller-Trumbore style test with FLT_EPSILON, Ray.java), BoundingBox.intersects(Ray)/collideWith/transform/mergeLocal/contains, Matrix4f.invert/mult/multNormal/setRotationMatrix/scale/setTranslation, BIHTree.construct (MAX_TRIS_PER_NODE=21, MAX_TREE_DEPTH=100) and BIHNode.intersectWhere (explicit stack, computes `worldMatrix.invert()` per geometry per ray, :127). Unused jME features include frustum/spherical/Catmull-Rom/half-float helpers (FastMath) and angle-axis builders. Exact per-method usage could not be counted reliably by grep because method names are generic.
  - C++: Port the used subset by hand under the Java names (Vector3f, Matrix3f, Matrix4f, Ray, BoundingBox) in about 1.5-2.5k lines of C++. That keeps greppability and exact float semantics for the 45 non-geo files using Vector3f. glm could replace the internals, but jME's row-major m[row][col] layout and mult conventions make a silent transposition bug likely. Precomputing the inverse world matrix per Geometry at load time (64 bytes x 484k = about 31 MB) or computing it on the fly are both cheap in C++.
- **Geo objects are static after load except for per-instance door/placeable/town state, and zone code holds long-lived references into the scene graph.**
  - Evidence: DespawnableNode.setActive/isActive sync on a BitSet indexed by instanceId (DespawnableNode.java). GeoMap keeps despawnables, despawnableHouseDoors, despawnableTownObjects and despawnableDoors maps. SiegeShield(Spatial) uses geometry.getParent() and changes the type to SHIELD at runtime. MaterialZoneHandler, AbstractCollisionObserver and CollisionDieActor store a `Spatial geometry`. GeoWorldLoader.createZone → ZoneService.createMaterialZoneTemplate(geometry, ...) (synchronized, ZoneService.java:194). Spatial has a parent back-pointer.
  - C++: The geo world can be an immutable arena owned by GeoService for the whole process: std::vector-backed nodes, meshes shared by index, raw `const Spatial*` or indices in zone handlers. Per-instance state needs thread-safe storage that can grow: e.g. a shared_mutex-protected vector<uint64_t> per DespawnableNode, or one global unordered_map<(nodeId, instanceId), bool>. Keep parent links as indices.
- **Java builds BIH trees lazily on first collision, without synchronization, while a background preload builds them concurrently. Tree construction reorders the shared mesh index array in place, so this is a real data race.**
  - Evidence: Mesh.collideWith: `if (collisionTree == null) createCollisionData();` (unsynchronized). BIHTree.sortTriangles calls mesh.swapTriangles → IndexByteArray/IndexShortArray.swap mutates shared index data. GeoWorldLoader.load:46-47 starts a `parallelStream().forEach(Mesh::createCollisionData)` on executeLongRunning. GeoService.init runs in parallel with QuestEngine/AIEngine/ZoneService init (GameServer.java:100-101), and the server continues to spawns and players while preloading runs.
  - C++: Build all 25,437 BIH trees eagerly (parallel, about 5.5M triangles) before the world starts, and make Mesh immutable afterwards. Record this in DEVIATIONS.md as a fix. Lazy construction in C++ would be undefined behaviour.
- **The geo core depends on game services at collision time, and GeoService depends on NPC AI, templates and player state. That fixes its place in the porting order and prevents it from being a standalone library.**
  - Evidence: DespawnableNode.collideWith calls EventService.getInstance().getEventTheme() for EVENT and SiegeService.getSiegeLocation(id) for SHIELD. GeoMap.setDoorState uses WorldMapType and GeoDataConfig. GeoWorldLoader uses DataManager.MATERIAL_DATA (:146), DataManager.WORLD_MAPS_DATA (:176) and ZoneService. GeoService.canSee asks npc.getAi().ask(AIQuestion.CONSIDER_BOUNDS_IN_CAN_SEE_CHECK_WHEN_*) and reads getObjectTemplate().getBoundRadius(). IgnoreProperties uses Race.
  - C++: Split it in two: a geo core library (math, bounding, BIH, scene, Terrain, GeoMap, loader) with callbacks or interfaces for 'is event theme X active' and 'is siege location Y under shield / race' and a material-zone sink; plus a GeoService facade ported with the world/AI layer. The core can then be unit-tested early against the data files (entity counts, getZ at known coordinates).
- **Memory: geo data is dominated by terrain heightmaps and per-instance scene objects, not by mesh vertices.**
  - Evidence: Raw mesh payload: 41.3 MB vertices + 27.9 MB indices. Decoded PNG samples: up to 149.1 MB (23 maps at 1536² x 2 bytes = 108 MB alone; flat maps collapse to one value; several maps share one terrain). Scene: 419,707 Node clones + 484,111 Geometry clones, each Geometry with its own Matrix4f and world BoundingBox; 256 m chunk nodes per map (GeoMap.NODE_CHUNK_SIZE). BIH: about 5.54M triangles / leaves of at most 21 triangles, estimated around 0.5-1M nodes. The Java heap was not measured (no JVM available).
  - C++: Rough C++ estimate: terrain up to about 150 MB, meshes 70 MB, BIH 15-25 MB with compact 16-24 byte nodes, instances 50-80 MB with a compact struct (mesh index, 3x4 or 4x4 matrix, AABB, name index, parent index). Total about 300 MB. Do not store a std::string name per clone (484k path strings ≈ 40+ MB); intern names into a table, as Java shares String references.
- **Keeping movement and line-of-sight decisions identical to Java requires strict float semantics.**
  - Evidence: Collision uses float32 arithmetic throughout (Ray.intersects, Terrain.getZ = uint16*2048/65536f, BIH tMin/tMax). GeoWorldLoader.getVectorHash uses Float.floatToIntBits with long multiply and % 700001 to name material zones. The names become ZoneName keys (`name_regionId_worldId`) that must match static data zone names. GeoService uses Math.toRadians/cos/sin for bound offsets.
  - C++: Compile the geo code without FMA contraction (MSVC default /fp:precise without /fp:contract; GCC/Clang need -ffp-contract=off) on SSE2. Port getVectorHash with std::bit_cast<int32_t>, int64 arithmetic and Java `%` semantics (sign of dividend, like C++). Its output is a lookup key and must be bit-identical, so add a unit test on known zone names.

## Risks
- 1. Data races that Java tolerates become UB in C++. Periodic player, item and legion-warehouse saves iterate live Player/Storage/QuestStateList containers from pool threads (PlayerEnterWorldService.java:496-544, PeriodicSaveService). The geo BIH lazy build races with background preload. This must be solved by the ownership/threading design (A), not ad hoc per DAO.
- 2. Model coupling gates DAO porting. InventoryDAO, ItemStoneListDAO, MailDAO, PlayerRegisteredItemsDAO, BrokerDAO and HousesDAO need Item/ItemTemplate/Storage/HouseObject/World/IDFactory first; the DAO code itself is easy.
- 3. Behavioural fidelity of non-atomic transactions and failure handling: InventoryDAO marks items UPDATED even after failed batches, and quest/skill/stone saves commit per sub-batch. Changing this silently alters the risk of item loss or duplication.
- 4. Geo float and hash fidelity: FMA contraction, float/double promotion and Java `%` in getVectorHash could change material zone names (lookup keys), getZ results and canSee outcomes. The zone-name mismatch fails silently.
- 5. Geo memory layout: a naive port (std::string name per clone, shared_ptr per Spatial, Matrix4f heap objects) of 900k scene objects would waste hundreds of MB and slow the load.
- 6. Circular dependency of geo on services (EventService, SiegeService, ZoneService, AI). Without callbacks the geo core cannot be built or tested before phase 5.
- 7. The palette-indexed materials PNG (301210000_materials.png) and 16-bit PNG byte order: the wrong decoder mode quietly produces wrong terrain materials or heights.
- 8. Blocking DB calls with a 5-connection pool: a C++ design that moves game logic onto fewer threads (e.g. strands on the Asio pool) could stall I/O on login bursts, because enter-world does about 30 sequential queries plus N+1 stone loads.
- 9. Connector differences hidden behind the JDBC-shaped API: REPLACE INTO row counts, getGeneratedKeys after a batch, TIMESTAMP time-zone handling (serverTimezone=${gameserver.timezone} in database.properties:8), and NULL handling in getObject (nullable item_color, color). The commons layer claims Connector/J parity, but it was verified only with login-server DAOs.

## Dependencies
**Persistence (DAO)**
- Depends on:
  - cpp commons database: done.
  - Config: DatabaseConfig; GSConfig, EventsConfig, PeriodicSaveConfig, CleaningConfig used by callers.
  - Static data: DataManager.ITEM_DATA, PLAYER_EXPERIENCE_TABLE, CHALLENGE_DATA, HOUSE_DATA (via HousingService); so it needs the phase-4 XML generator first.
  - Model: Player, PlayerCommonData, PlayerAccountData, Account, Item + ItemStone/ManaStone/GodStone/IdianStone, Storage/StorageType, Mailbox/Letter, Legion/LegionMember/LegionEmblem/LegionHistory, House/HouseRegistry/HouseObject/HouseDecoration, QuestState, SkillList, cooldown maps, Pet data, Persistable.PersistentState.
  - Singletons: World (findVisibleObject, getPlayer), IDFactory (release/lock IDs), LegionService, PvpService, HouseObjectFactory, CompressUtil.
- Depended on by:
  - IDFactory at startup (8 getUsedIDs).
  - GameServer startup (setAllPlayersOffline, race counts).
  - LoginServer/AccountService (character list).
  - PlayerService / PlayerEnterWorldService / PlayerLeaveWorldService.
  - About 78 service/packet files in src and 11 handler files (admin commands Ban/BanChar/Rename/SysMail/PasskeyReset/UnBan/UnBanChar/Headhunting/Bookmark, ResurrectAI, consolecommands Bookmark_add).
- Suggested order: PersistentState + DB helpers → IDFactory DAOs (getUsedIDs only) → account/character-select DAOs (PlayerDAO, PlayerAppearanceDAO, InventoryDAO.loadVisibleEquipment, PlayerPunishmentsDAO) so the C++ server can show the character list early → the rest of the player load/save set together with the Player model → legion/house/mail/broker DAOs together with their services.

**Geo**
- The geo core depends only on: its own math; commons ByteBuffer/logging; a PNG decoder (new vcpkg dependency: lodepng or libspng); DataManager.MATERIAL_DATA and WORLD_MAPS_DATA (templates); ZoneService.createMaterialZoneTemplate (a sink for 7,961 material geometries); EventService theme and SiegeService shield state (collision-time callbacks); WorldMapType, RegionUtil, HouseDoorState, Race; GeoDataConfig.
- GeoService facade also depends on: VisibleObject/Creature/Player/Npc (position, instance, template bound radius, transform model, move controller last client position, AI ask), DataManager.NPC_DATA, PositionUtil.
- Depended on by 43 files: Vector3f everywhere (skills effects, AI, zones Plane3D/Road/FlyRing, SM_PLAYER_INFO); CollisionResults/CollisionIntention/IgnoreProperties (WalkManager, NpcMoveController, VisibleObjectSpawner, Summon*Effect, FirstTargetRangeProperty, observers, admin command Collide); Spatial/BoundingBox (ZoneService, MaterialZoneTemplate, MaterialZoneHandler, SiegeShield, ShieldService); GeoService (spawns, movement, AI aggro and canSee, skills, doors via static door objects, housing, towns).
- Suggested order: geoEngine.math (Vector3f first, since non-geo code needs it) in phase 4 foundation → geo core + loader with unit tests against data/geo (entity counts 419,707 / 18,583 / 25,437 and getZ probes) → GeoService facade when World/VisibleObject/AI exist (phase 4/5).

## Open questions
- Should C++ DAOs keep taking live model objects (Player&, Item&), as a 1:1 port would, or take and return plain row structs, with services doing the mapping? The second makes off-thread saves safe and decouples DAO porting from the model, but departs from 'DAOs port almost line by line'.
- How should periodic and logout saves be serialized against game logic that changes the same Player: a per-player strand or executor, a per-player mutex, or snapshot-then-write? This has to be decided with ownership design (A).
- Keep Java's non-atomic multi-commit store methods and 'mark UPDATED even on failure', or wrap each store in a Transaction (documented deviation)?
- Heap and memory use of the Java geo engine and of the whole game server was not measured, because no JVM could be run. The C++ figures (about 300 MB for geo) are estimates from file statistics, not measurements.
- BIH node count is estimated (about 0.5-1M), not computed. A quick C++ or Python simulation of BIHTree.construct over models.mesh would pin down memory and build time.
- Must geo results be bit-identical to Java (movement end points, canSee edge cases), or only for lookup keys such as material zone names (getVectorHash)? This decides whether a faster BVH (Embree, tinybvh, madmann91/bvh) is acceptable instead of a direct BIH port. Such a BVH would also need per-instance filtering for DespawnableNode state, siege shields and IgnoreProperties.staticId.
- Whether the PNG decoder choice (lodepng vs libspng) meets the vcpkg/CMake conventions was not checked against the existing CMake setup. vcpkg.json currently has no PNG or math library.
- Could not verify at runtime whether CollisionResult.equals in GeoMap.getTerrainMaterialAt:251 ever hits its latent null geometry dereference (terrain collisions have no Geometry). A C++ port should guard it.
- Terrain PNG file names are matched to maps with `mapId.startsWith(String.valueOf(map.getMapId()))` (GeoWorldLoader.java:63), which also matches `_materials` suffixes. Is prefix matching ever ambiguous for the 161 map IDs? Not exhaustively checked.
