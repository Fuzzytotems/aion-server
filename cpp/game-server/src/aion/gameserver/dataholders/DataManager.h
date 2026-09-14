#pragma once

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/dataholders/fwd.h"
#include "aion/gameserver/dataholders/loadingutils/HolderRef.h"
#include "aion/gameserver/model/templates/mail/fwd.h"

namespace aion::gameserver::dataholders {

/**
 * This class is centralized storage of all static data (holders of data/static_data). It is loaded by {@link #init()} when the singleton is
 * created: every holder is bound, post-processed while unpublished and then published once.
 * <p>
 * Hub header (docs/design/hub-headers.md, docs/design/static-data.md §3.3 and amendments §5, §6). Java's `public static X X_DATA` fields are
 * published holder references: call sites read `DataManager::ITEM_DATA->getItemTemplate(id)` (lock-free; NullPointerException before
 * publication). Holders are immortal and const, except the explicitly mutable, internally synchronized families SpawnsData, WalkerData and
 * EventData (MutableHolderRef). Holder types are only forward-declared. //reload is deferred (D3): the in-place reload setters stay unported.
 * XML validation against the XSDs does not exist in C++ (static-data.md §3.4), so xmlValidationTask stays null.
 *
 * @author Luno , orz, Wakizashi, Neon
 */
class DataManager final : public runtime::Immortal {
public:
	static inline xml::HolderRef<AbsoluteStatsData> ABSOLUTE_STATS_DATA;
	static inline xml::HolderRef<AIData> AI_DATA;
	static inline xml::HolderRef<UpgradeArcadeData> UPGRADE_ARCADE_DATA;
	static inline xml::HolderRef<AssembledNpcsData> ASSEMBLED_NPC_DATA;
	static inline xml::HolderRef<AssemblyItemsData> ASSEMBLY_ITEM_DATA;
	static inline xml::HolderRef<AtreianPassportData> ATREIAN_PASSPORT_DATA;
	static inline xml::HolderRef<AutoGroupData> AUTO_GROUP;
	static inline xml::HolderRef<BaseData> BASE_DATA;
	static inline xml::HolderRef<BindPointData> BIND_POINT_DATA;
	static inline xml::HolderRef<ChallengeData> CHALLENGE_DATA;
	static inline xml::HolderRef<ChestData> CHEST_DATA;
	static inline xml::HolderRef<CosmeticItemsData> COSMETIC_ITEMS_DATA;
	static inline xml::HolderRef<CubeExpandData> CUBEEXPANDER_DATA;
	static inline xml::HolderRef<CuringObjectsData> CURING_OBJECTS_DATA;
	static inline xml::HolderRef<CustomDrop> CUSTOM_NPC_DROP;
	static inline xml::HolderRef<DecomposableItemsData> DECOMPOSABLE_ITEMS_DATA;
	static inline xml::HolderRef<EnchantData> ENCHANT_DATA;
	static inline xml::MutableHolderRef<EventData> EVENT_DATA;
	static inline xml::HolderRef<FlyPathData> FLY_PATH;
	static inline xml::HolderRef<FlyRingData> FLY_RING_DATA;
	static inline xml::HolderRef<GuideHtmlData> GUIDE_HTML_DATA;
	static inline xml::HolderRef<ItemData> ITEM_DATA;
	static inline xml::HolderRef<ItemRandomBonusData> ITEM_RANDOM_BONUSES;
	static inline xml::HolderRef<ItemSetData> ITEM_SET_DATA;
	static inline xml::HolderRef<NpcData> NPC_DATA;
	static inline xml::HolderRef<GatherableData> GATHERABLE_DATA;
	static inline xml::HolderRef<GlobalDropData> GLOBAL_DROP_DATA;
	static inline xml::HolderRef<GlobalNpcExclusionData> GLOBAL_EXCLUSION_DATA;
	static inline xml::HolderRef<GoodsListData> GOODSLIST_DATA;
	static inline xml::HolderRef<HotspotData> HOTSPOT_DATA;
	static inline xml::HolderRef<HouseData> HOUSE_DATA;
	static inline xml::HolderRef<HouseBuildingData> HOUSE_BUILDING_DATA;
	static inline xml::HolderRef<HouseNpcsData> HOUSE_NPCS_DATA;
	static inline xml::HolderRef<HousePartsData> HOUSE_PARTS_DATA;
	static inline xml::HolderRef<HousingObjectData> HOUSING_OBJECT_DATA;
	static inline xml::HolderRef<KillBountyData> KILL_BOUNTY_DATA;
	static inline xml::HolderRef<InstanceBuffData> INSTANCE_BUFF_DATA;
	static inline xml::HolderRef<InstanceCooltimeData> INSTANCE_COOLTIME_DATA;
	static inline xml::HolderRef<InstanceExitData> INSTANCE_EXIT_DATA;
	static inline xml::HolderRef<ItemGroupsData> ITEM_GROUPS_DATA;
	static inline xml::HolderRef<ItemPurificationData> ITEM_PURIFICATION_DATA;
	static inline xml::HolderRef<ItemRestrictionCleanupData> ITEM_CLEAN_UP;
	static inline xml::HolderRef<model::templates::mail::Mails> SYSTEM_MAIL_TEMPLATES;
	static inline xml::HolderRef<MapWeatherData> MAP_WEATHER_DATA;
	static inline xml::HolderRef<MaterialData> MATERIAL_DATA;
	static inline xml::HolderRef<MotionData> MOTION_DATA;
	static inline xml::HolderRef<MultiReturnItemData> MULTIRETURN_DATA;
	static inline xml::HolderRef<NpcFactionsData> NPC_FACTIONS_DATA;
	static inline xml::HolderRef<NpcShoutData> NPC_SHOUT_DATA;
	static inline xml::HolderRef<NpcSkillData> NPC_SKILL_DATA;
	static inline xml::HolderRef<PanelSkillsData> PANEL_SKILL_DATA;
	static inline xml::HolderRef<PetData> PET_DATA;
	static inline xml::HolderRef<PetBuffsData> PET_BUFFS_DATA;
	static inline xml::HolderRef<PetDopingData> PET_DOPING_DATA;
	static inline xml::HolderRef<PetFeedData> PET_FEED_DATA;
	static inline xml::HolderRef<PetSkillData> PET_SKILL_DATA;
	static inline xml::HolderRef<PlayerExperienceTable> PLAYER_EXPERIENCE_TABLE;
	static inline xml::HolderRef<PlayerInitialData> PLAYER_INITIAL_DATA;
	static inline xml::HolderRef<Portal2Data> PORTAL2_DATA;
	static inline xml::HolderRef<PortalLocData> PORTAL_LOC_DATA;
	static inline xml::HolderRef<QuestsData> QUEST_DATA;
	static inline xml::HolderRef<RecipeData> RECIPE_DATA;
	static inline xml::HolderRef<RideData> RIDE_DATA;
	static inline xml::HolderRef<RiftData> RIFT_DATA;
	static inline xml::HolderRef<RoadData> ROAD_DATA;
	static inline xml::HolderRef<ConquerorAndProtectorData> CONQUEROR_AND_PROTECTOR_DATA;
	static inline xml::HolderRef<ShieldData> SHIELD_DATA;
	static inline xml::HolderRef<SiegeLocationData> SIEGE_LOCATION_DATA;
	static inline xml::HolderRef<SkillChargeData> SKILL_CHARGE_DATA;
	static inline xml::HolderRef<SkillData> SKILL_DATA;
	static inline xml::HolderRef<SkillTreeData> SKILL_TREE_DATA;
	static inline xml::MutableHolderRef<SpawnsData> SPAWNS_DATA;
	static inline xml::HolderRef<StaticDoorData> STATICDOOR_DATA;
	static inline xml::HolderRef<TeleLocationData> TELELOCATION_DATA;
	static inline xml::HolderRef<TeleporterData> TELEPORTER_DATA;
	static inline xml::HolderRef<TemperingData> TEMPERING_DATA;
	static inline xml::HolderRef<TitleData> TITLE_DATA;
	static inline xml::HolderRef<TownSpawnsData> TOWN_SPAWNS_DATA;
	static inline xml::HolderRef<TradeListData> TRADE_LIST_DATA;
	static inline xml::HolderRef<TribeRelationsData> TRIBE_RELATIONS_DATA;
	static inline xml::HolderRef<VortexData> VORTEX_DATA;
	static inline xml::MutableHolderRef<WalkerData> WALKER_DATA;
	static inline xml::HolderRef<WalkerVersionsData> WALKER_VERSIONS_DATA;
	static inline xml::HolderRef<WarehouseExpandData> WAREHOUSEEXPANDER_DATA;
	static inline xml::HolderRef<WindstreamData> WINDSTREAM_DATA;
	static inline xml::HolderRef<WorldMapsData> WORLD_MAPS_DATA;
	static inline xml::HolderRef<WorldRaidData> WORLD_RAID_DATA;
	static inline xml::HolderRef<XMLQuests> XML_QUESTS;
	static inline xml::HolderRef<ZoneData> ZONE_DATA;
	static inline xml::HolderRef<LegionDominionData> LEGION_DOMINION_DATA;
	static inline xml::HolderRef<SkillAliasLocationData> SKILL_ALIAS_LOCATION_DATA;
	static inline xml::HolderRef<SignetDataTemplates> SIGNET_DATA_TEMPLATES;

private:
	static inline runtime::Field<runtime::FutureRef> xmlValidationTask{};

public:
	/**
	 * Java: getInstance() - the first call loads all static data (the private constructor runs init()). Not ported yet: this function keeps the
	 * AION_UNPORTED site that main.cpp and gs.smoke.startup expect; the port is `static DataManager instance; return instance;`.
	 */
	static DataManager& getInstance();

private:
	/** Constructor: runs init(). */
	DataManager();

	/**
	 * C++: the Java constructor body (static-data.md §3.3): loads the static data, patches IDREFs and keeps the retired objects of the load forever,
	 * post-processes the unpublished holders in Java order (items cleanup, global drop rules, buy list and motion validation, decompose random item
	 * ids), publishes every holder and logs "##### [Static Data loaded in X seconds] #####".
	 */
	void init();

public:
	/** Java: waits for the asynchronous XSD validation and rethrows its failure. C++ has no XSD validation (xmlValidationTask is always null). */
	static void waitForValidationToFinishAndShutdownOnFail();
};

} // namespace aion::gameserver::dataholders
