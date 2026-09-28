#include "aion/gameserver/dataholders/DataManager.h"

#include <charconv>
#include <string>
#include <utility>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dataholders/AIData.h"
#include "aion/gameserver/dataholders/AbsoluteStatsData.h"
#include "aion/gameserver/dataholders/AssembledNpcsData.h"
#include "aion/gameserver/dataholders/AssemblyItemsData.h"
#include "aion/gameserver/dataholders/AtreianPassportData.h"
#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/dataholders/BaseData.h"
#include "aion/gameserver/dataholders/BindPointData.h"
#include "aion/gameserver/dataholders/ChallengeData.h"
#include "aion/gameserver/dataholders/ChestData.h"
#include "aion/gameserver/dataholders/ConquerorAndProtectorData.h"
#include "aion/gameserver/dataholders/CosmeticItemsData.h"
#include "aion/gameserver/dataholders/CubeExpandData.h"
#include "aion/gameserver/dataholders/CuringObjectsData.h"
#include "aion/gameserver/dataholders/CustomDrop.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.h"
#include "aion/gameserver/dataholders/EnchantData.h"
#include "aion/gameserver/dataholders/EventData.h"
#include "aion/gameserver/dataholders/FlyPathData.h"
#include "aion/gameserver/dataholders/FlyRingData.h"
#include "aion/gameserver/dataholders/GatherableData.h"
#include "aion/gameserver/dataholders/GlobalDropData.h"
#include "aion/gameserver/dataholders/GlobalNpcExclusionData.h"
#include "aion/gameserver/dataholders/GoodsListData.h"
#include "aion/gameserver/dataholders/GuideHtmlData.h"
#include "aion/gameserver/dataholders/HotspotData.h"
#include "aion/gameserver/dataholders/HouseBuildingData.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/HouseNpcsData.h"
#include "aion/gameserver/dataholders/HousePartsData.h"
#include "aion/gameserver/dataholders/HousingObjectData.h"
#include "aion/gameserver/dataholders/InstanceBuffData.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/dataholders/InstanceExitData.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemGroupsData.h"
#include "aion/gameserver/dataholders/ItemPurificationData.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/ItemSetData.h"
#include "aion/gameserver/dataholders/KillBountyData.h"
#include "aion/gameserver/dataholders/LegionDominionData.h"
#include "aion/gameserver/dataholders/MapWeatherData.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/MotionData.h"
#include "aion/gameserver/dataholders/MultiReturnItemData.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/NpcFactionsData.h"
#include "aion/gameserver/dataholders/NpcShoutData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/PanelSkillsData.h"
#include "aion/gameserver/dataholders/PetBuffsData.h"
#include "aion/gameserver/dataholders/PetData.h"
#include "aion/gameserver/dataholders/PetDopingData.h"
#include "aion/gameserver/dataholders/PetFeedData.h"
#include "aion/gameserver/dataholders/PetSkillData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/dataholders/Portal2Data.h"
#include "aion/gameserver/dataholders/PortalLocData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/dataholders/RideData.h"
#include "aion/gameserver/dataholders/RiftData.h"
#include "aion/gameserver/dataholders/RoadData.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/SiegeLocationData.h"
#include "aion/gameserver/dataholders/SignetDataTemplates.h"
#include "aion/gameserver/dataholders/SkillAliasLocationData.h"
#include "aion/gameserver/dataholders/SkillChargeData.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/StaticData.h"
#include "aion/gameserver/dataholders/StaticDataHolders.xml.h"
#include "aion/gameserver/dataholders/StaticDoorData.h"
#include "aion/gameserver/dataholders/TeleLocationData.h"
#include "aion/gameserver/dataholders/TeleporterData.h"
#include "aion/gameserver/dataholders/TemperingData.h"
#include "aion/gameserver/dataholders/TitleData.h"
#include "aion/gameserver/dataholders/TownSpawnsData.h"
#include "aion/gameserver/dataholders/TradeListData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/UpgradeArcadeData.h"
#include "aion/gameserver/dataholders/VortexData.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/dataholders/WalkerVersionsData.h"
#include "aion/gameserver/dataholders/WarehouseExpandData.h"
#include "aion/gameserver/dataholders/WindstreamData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/WorldRaidData.h"
#include "aion/gameserver/dataholders/XMLQuests.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/detail/StaticDataRegistry.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/templates/item/actions/DecomposeAction.h"
#include "aion/gameserver/model/templates/mail/Mails.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.DataManager");

DataManager& DataManager::getInstance() {
	static DataManager instance; // Java SingletonHolder
	return instance;
}

DataManager::DataManager() {
	init();
}

void DataManager::init() {
	int64_t start = commons::utils::currentTimeMillis();
	xml::LoadOptions options;
	// M4 and Java parity: unknown elements and attributes, missing required values and unknown enum constants fail the startup. Java validates
	// changed data against the XSDs and does not start on an error (XmlDataLoader.validateAsync, GameServer.java:177), which also rejects
	// unknown attributes; the strict binder takes that place (docs/deviations/P4-09.md)
	options.strict = true;
	options.countryCode = configs::main::GSConfig::SERVER_COUNTRY_CODE.load();
	options.parallelParse = true;
	xml::LoadContext context(options);
	std::unique_ptr<StaticData> data = loadStaticData(context, MAIN_XML_FILE);
	// Java: xmlValidationTask = data.getValidationTask() (always null in C++); data.waitForAfterUnmarshalTasksToFinish() (the tasks ran inline)

	// subsequent data processing: Java assigns the DataManager fields first; C++ post-processes the unpublished holders and publishes afterwards
	postProcess(*data);
	publish(*data);

	int64_t time = commons::utils::currentTimeMillis() - start;
	log.info("##### [Static Data loaded in " + formatSeconds(static_cast<float>(time) / 1000.0f) + " seconds] #####");
}

void DataManager::waitForValidationToFinishAndShutdownOnFail() {
	// Java: if (xmlValidationTask == null) return; C++ has no XSD validation at run time (static-data.md §3.4), so the task is always null
}

std::unique_ptr<StaticData> DataManager::loadStaticData(xml::LoadContext& context, const std::filesystem::path& staticDataXml) {
	auto data = std::make_unique<StaticData>();
	context.setRoot(xml::XmlParent::of(*data));
	xml::StaticDataLoader(detail::staticDataRegistry()).load(context, staticDataXml);
	context.resolveIdRefs();
#define AION_TAKE_HOLDER(rootTag, HolderClass, fieldName) data->fieldName = context.takeHolder<HolderClass>();
	AION_STATIC_DATA_HOLDERS(AION_TAKE_HOLDER)
#undef AION_TAKE_HOLDER
	std::vector<xml::ErasedHolder> retired = context.takeRetired();
	if (!retired.empty()) // objects replaced in lenient mode: resolved IDREFs may point into them; immortal like published holders
		static_cast<void>(new std::vector<xml::ErasedHolder>(std::move(retired)));
	data->logCounts(); // Java: StaticData.afterUnmarshal, at the end of the unmarshal
	return data;
}

void DataManager::postProcess(StaticData& data) {
	auto require = [](const auto& holder, const char* name) -> auto& {
		if (holder == nullptr)
			throw runtime::NullPointerException(std::string("DataManager.") + name + " is null");
		return *holder;
	};
	require(data.itemData, "ITEM_DATA").cleanup(require(data.itemCleanup, "ITEM_CLEAN_UP"));
	const std::vector<const model::templates::npc::NpcTemplate*>& npcTemplates = require(data.npcData, "NPC_DATA").getNpcData();
	require(data.globalDropData, "GLOBAL_DROP_DATA").processRules(npcTemplates);
	require(data.tradeListData, "TRADE_LIST_DATA").validateBuyLists(npcTemplates);
	require(data.skillData, "SKILL_DATA").validateMotions(require(data.motionData, "MOTION_DATA"));
	model::templates::item::actions::DecomposeAction::validateRandomItemIds(*data.itemData);
}

void DataManager::publish(StaticData& data) {
	auto publishIfLoaded = [](auto& ref, auto& holder) {
		if (holder != nullptr)
			ref.publish(std::move(holder));
	};
	publishIfLoaded(NPC_DATA, data.npcData);
	publishIfLoaded(CUSTOM_NPC_DROP, data.customNpcDrop);
	publishIfLoaded(WORLD_MAPS_DATA, data.worldMapsData);
	publishIfLoaded(MATERIAL_DATA, data.materiaData);
	publishIfLoaded(MAP_WEATHER_DATA, data.mapWeatherData);
	publishIfLoaded(PLAYER_EXPERIENCE_TABLE, data.playerExperienceTable);
	publishIfLoaded(ABSOLUTE_STATS_DATA, data.absoluteStatsData);
	publishIfLoaded(ITEM_CLEAN_UP, data.itemCleanup);
	publishIfLoaded(ITEM_DATA, data.itemData);
	publishIfLoaded(ITEM_RANDOM_BONUSES, data.itemRandomBonuses);
	publishIfLoaded(NPC_SHOUT_DATA, data.npcShoutData);
	publishIfLoaded(GATHERABLE_DATA, data.gatherableData);
	publishIfLoaded(PLAYER_INITIAL_DATA, data.playerInitialData);
	publishIfLoaded(SKILL_DATA, data.skillData);
	publishIfLoaded(SKILL_CHARGE_DATA, data.skillChargeData);
	publishIfLoaded(MOTION_DATA, data.motionData);
	publishIfLoaded(SKILL_TREE_DATA, data.skillTreeData);
	publishIfLoaded(TITLE_DATA, data.titleData);
	publishIfLoaded(TRADE_LIST_DATA, data.tradeListData);
	publishIfLoaded(TELEPORTER_DATA, data.teleporterData);
	publishIfLoaded(TELELOCATION_DATA, data.teleLocationData);
	publishIfLoaded(CUBEEXPANDER_DATA, data.cubeExpandData);
	publishIfLoaded(WAREHOUSEEXPANDER_DATA, data.warehouseExpandData);
	publishIfLoaded(BIND_POINT_DATA, data.bindPointData);
	publishIfLoaded(QUEST_DATA, data.questData);
	publishIfLoaded(XML_QUESTS, data.questsScriptData);
	publishIfLoaded(ZONE_DATA, data.zoneData);
	publishIfLoaded(WALKER_DATA, data.walkerData);
	publishIfLoaded(WALKER_VERSIONS_DATA, data.walkerVersionsData);
	publishIfLoaded(GOODSLIST_DATA, data.goodsListData);
	publishIfLoaded(TRIBE_RELATIONS_DATA, data.tribeRelationsData);
	publishIfLoaded(RECIPE_DATA, data.recipeData);
	publishIfLoaded(CHEST_DATA, data.chestData);
	publishIfLoaded(STATICDOOR_DATA, data.staticDoorData);
	publishIfLoaded(ITEM_SET_DATA, data.itemSetData);
	publishIfLoaded(NPC_FACTIONS_DATA, data.npcFactionsData);
	publishIfLoaded(NPC_SKILL_DATA, data.npcSkillData);
	publishIfLoaded(PET_SKILL_DATA, data.petSkillData);
	publishIfLoaded(SIEGE_LOCATION_DATA, data.siegeLocationData);
	publishIfLoaded(VORTEX_DATA, data.vortexData);
	publishIfLoaded(RIFT_DATA, data.riftData);
	publishIfLoaded(BASE_DATA, data.baseData);
	publishIfLoaded(FLY_RING_DATA, data.flyRingData);
	publishIfLoaded(SHIELD_DATA, data.shieldData);
	publishIfLoaded(PET_DATA, data.petData);
	publishIfLoaded(PET_FEED_DATA, data.petFeedData);
	publishIfLoaded(PET_DOPING_DATA, data.petDopingData);
	publishIfLoaded(PET_BUFFS_DATA, data.petBuffsData);
	publishIfLoaded(GUIDE_HTML_DATA, data.guideData);
	publishIfLoaded(ROAD_DATA, data.roadData);
	publishIfLoaded(INSTANCE_COOLTIME_DATA, data.instanceCooltimeData);
	publishIfLoaded(DECOMPOSABLE_ITEMS_DATA, data.decomposableItemsData);
	publishIfLoaded(AI_DATA, data.aiData);
	publishIfLoaded(FLY_PATH, data.flyPath);
	publishIfLoaded(WINDSTREAM_DATA, data.windstreamsData);
	publishIfLoaded(ASSEMBLED_NPC_DATA, data.assembledNpcData);
	publishIfLoaded(COSMETIC_ITEMS_DATA, data.cosmeticItemsData);
	publishIfLoaded(SPAWNS_DATA, data.spawnsData);
	publishIfLoaded(ITEM_GROUPS_DATA, data.itemGroupsData);
	publishIfLoaded(ASSEMBLY_ITEM_DATA, data.assemblyItemData);
	publishIfLoaded(AUTO_GROUP, data.autoGroupData);
	publishIfLoaded(EVENT_DATA, data.eventData);
	publishIfLoaded(PANEL_SKILL_DATA, data.panelSkillsData);
	publishIfLoaded(INSTANCE_BUFF_DATA, data.instanceBuffData);
	publishIfLoaded(HOUSING_OBJECT_DATA, data.housingObjectData);
	publishIfLoaded(RIDE_DATA, data.rideData);
	publishIfLoaded(INSTANCE_EXIT_DATA, data.instanceExitData);
	publishIfLoaded(PORTAL_LOC_DATA, data.portalLocData);
	publishIfLoaded(PORTAL2_DATA, data.portalTemplate2);
	publishIfLoaded(HOUSE_DATA, data.houseData);
	publishIfLoaded(HOUSE_BUILDING_DATA, data.houseBuildingData);
	publishIfLoaded(HOUSE_PARTS_DATA, data.housePartsData);
	publishIfLoaded(CURING_OBJECTS_DATA, data.curingObjectsData);
	publishIfLoaded(HOUSE_NPCS_DATA, data.houseNpcsData);
	publishIfLoaded(SYSTEM_MAIL_TEMPLATES, data.systemMailTemplates);
	publishIfLoaded(CHALLENGE_DATA, data.challengeData);
	publishIfLoaded(TOWN_SPAWNS_DATA, data.townSpawnsData);
	publishIfLoaded(CONQUEROR_AND_PROTECTOR_DATA, data.conquerorAndProtectorData);
	publishIfLoaded(ENCHANT_DATA, data.enchantData);
	publishIfLoaded(TEMPERING_DATA, data.temperingData);
	publishIfLoaded(GLOBAL_DROP_DATA, data.globalDropData);
	publishIfLoaded(GLOBAL_EXCLUSION_DATA, data.globalExclusionData);
	publishIfLoaded(MULTIRETURN_DATA, data.multiReturnItem);
	publishIfLoaded(HOTSPOT_DATA, data.hotspotData);
	publishIfLoaded(ITEM_PURIFICATION_DATA, data.itemPurificationData);
	publishIfLoaded(UPGRADE_ARCADE_DATA, data.upgradeArcadeData);
	publishIfLoaded(ATREIAN_PASSPORT_DATA, data.atreianPassportData);
	publishIfLoaded(WORLD_RAID_DATA, data.worldRaidData);
	publishIfLoaded(KILL_BOUNTY_DATA, data.killBountyData);
	publishIfLoaded(LEGION_DOMINION_DATA, data.legionDominionData);
	publishIfLoaded(SKILL_ALIAS_LOCATION_DATA, data.skillAliasLocationData);
	publishIfLoaded(SIGNET_DATA_TEMPLATES, data.signetDataTemplates);
}

std::string DataManager::formatSeconds(float seconds) {
	// java.util.Formatter: %.1f of a float formats (double) seconds, rounding the shortest decimal representation HALF_UP
	char buffer[64];
	auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), static_cast<double>(seconds), std::chars_format::fixed);
	if (error != std::errc())
		return "?";
	std::string digits(buffer, end);
	size_t dot = digits.find('.');
	if (dot == std::string::npos)
		return digits + ".0";
	std::string integerPart = digits.substr(0, dot);
	std::string fraction = digits.substr(dot + 1);
	char first = fraction.empty() ? '0' : fraction[0];
	bool roundUp = fraction.size() > 1 && fraction[1] >= '5';
	if (!roundUp)
		return integerPart + "." + first;
	if (first < '9')
		return integerPart + "." + static_cast<char>(first + 1);
	// carry into the integer part (seconds are never negative)
	int64_t whole = std::stoll(integerPart) + 1;
	return std::to_string(whole) + ".0";
}

} // namespace aion::gameserver::dataholders
