#include "aion/gameserver/dataholders/detail/StaticDataRegistry.h"

#include <typeinfo>

#include "aion/gameserver/dataholders/AIData.bind.h"
#include "aion/gameserver/dataholders/AbsoluteStatsData.bind.h"
#include "aion/gameserver/dataholders/AssembledNpcsData.bind.h"
#include "aion/gameserver/dataholders/AssemblyItemsData.bind.h"
#include "aion/gameserver/dataholders/AtreianPassportData.bind.h"
#include "aion/gameserver/dataholders/AutoGroupData.bind.h"
#include "aion/gameserver/dataholders/BaseData.bind.h"
#include "aion/gameserver/dataholders/BindPointData.bind.h"
#include "aion/gameserver/dataholders/ChallengeData.bind.h"
#include "aion/gameserver/dataholders/ChestData.bind.h"
#include "aion/gameserver/dataholders/ConquerorAndProtectorData.bind.h"
#include "aion/gameserver/dataholders/CosmeticItemsData.bind.h"
#include "aion/gameserver/dataholders/CubeExpandData.bind.h"
#include "aion/gameserver/dataholders/CuringObjectsData.bind.h"
#include "aion/gameserver/dataholders/CustomDrop.bind.h"
#include "aion/gameserver/dataholders/DecomposableItemsData.bind.h"
#include "aion/gameserver/dataholders/EnchantData.bind.h"
#include "aion/gameserver/dataholders/EventData.bind.h"
#include "aion/gameserver/dataholders/FlyPathData.bind.h"
#include "aion/gameserver/dataholders/FlyRingData.bind.h"
#include "aion/gameserver/dataholders/GatherableData.bind.h"
#include "aion/gameserver/dataholders/GlobalDropData.bind.h"
#include "aion/gameserver/dataholders/GlobalNpcExclusionData.bind.h"
#include "aion/gameserver/dataholders/GoodsListData.bind.h"
#include "aion/gameserver/dataholders/GuideHtmlData.bind.h"
#include "aion/gameserver/dataholders/HotspotData.bind.h"
#include "aion/gameserver/dataholders/HouseBuildingData.bind.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseNpcsData.bind.h"
#include "aion/gameserver/dataholders/HousePartsData.bind.h"
#include "aion/gameserver/dataholders/HousingObjectData.bind.h"
#include "aion/gameserver/dataholders/InstanceBuffData.bind.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.bind.h"
#include "aion/gameserver/dataholders/InstanceExitData.bind.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemGroupsData.bind.h"
#include "aion/gameserver/dataholders/ItemPurificationData.bind.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.bind.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.bind.h"
#include "aion/gameserver/dataholders/ItemSetData.bind.h"
#include "aion/gameserver/dataholders/KillBountyData.bind.h"
#include "aion/gameserver/dataholders/LegionDominionData.bind.h"
#include "aion/gameserver/dataholders/MapWeatherData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MotionData.bind.h"
#include "aion/gameserver/dataholders/MultiReturnItemData.bind.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcFactionsData.bind.h"
#include "aion/gameserver/dataholders/NpcShoutData.bind.h"
#include "aion/gameserver/dataholders/NpcSkillData.bind.h"
#include "aion/gameserver/dataholders/PanelSkillsData.bind.h"
#include "aion/gameserver/dataholders/PetBuffsData.bind.h"
#include "aion/gameserver/dataholders/PetData.bind.h"
#include "aion/gameserver/dataholders/PetDopingData.bind.h"
#include "aion/gameserver/dataholders/PetFeedData.bind.h"
#include "aion/gameserver/dataholders/PetSkillData.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerInitialData.bind.h"
#include "aion/gameserver/dataholders/Portal2Data.bind.h"
#include "aion/gameserver/dataholders/PortalLocData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/RecipeData.bind.h"
#include "aion/gameserver/dataholders/RideData.bind.h"
#include "aion/gameserver/dataholders/RiftData.bind.h"
#include "aion/gameserver/dataholders/RoadData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/SiegeLocationData.bind.h"
#include "aion/gameserver/dataholders/SignetDataTemplates.bind.h"
#include "aion/gameserver/dataholders/SkillAliasLocationData.bind.h"
#include "aion/gameserver/dataholders/SkillChargeData.bind.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SpawnsData.bind.h"
#include "aion/gameserver/dataholders/StaticDataHolders.xml.h"
#include "aion/gameserver/dataholders/StaticDoorData.bind.h"
#include "aion/gameserver/dataholders/TeleLocationData.bind.h"
#include "aion/gameserver/dataholders/TeleporterData.bind.h"
#include "aion/gameserver/dataholders/TemperingData.bind.h"
#include "aion/gameserver/dataholders/TitleData.bind.h"
#include "aion/gameserver/dataholders/TownSpawnsData.bind.h"
#include "aion/gameserver/dataholders/TradeListData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/UpgradeArcadeData.bind.h"
#include "aion/gameserver/dataholders/VortexData.bind.h"
#include "aion/gameserver/dataholders/WalkerData.bind.h"
#include "aion/gameserver/dataholders/WalkerVersionsData.bind.h"
#include "aion/gameserver/dataholders/WarehouseExpandData.bind.h"
#include "aion/gameserver/dataholders/WindstreamData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldRaidData.bind.h"
#include "aion/gameserver/dataholders/XMLQuests.bind.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/templates/mail/Mails.bind.h"

namespace aion::gameserver::dataholders::detail {

namespace {

/**
 * The hand-maintained dependency table of static-data.md §3.3: the holders whose hooks may read other holders through LoadContext::holder<H>().
 * Sequential loading in import order satisfies it (the item holder is imported first).
 */
std::vector<std::type_index> dependenciesOf(std::string_view rootTag) {
	if (rootTag == "item_groups" || rootTag == "decomposable_items" || rootTag == "global_rules" || rootTag == "timed_events")
		return {typeid(ItemData)};
	return {};
}

xml::HolderRegistry createRegistry() {
	xml::HolderRegistry registry;
#define AION_REGISTER_HOLDER(rootTag, HolderClass, fieldName)                                                                                        \
	registry.add(xml::HolderRegistration::of<HolderClass>(rootTag, dependenciesOf(rootTag)));
	AION_STATIC_DATA_HOLDERS(AION_REGISTER_HOLDER)
#undef AION_REGISTER_HOLDER
	return registry;
}

} // namespace

const xml::HolderRegistry& staticDataRegistry() {
	static const xml::HolderRegistry registry = createRegistry();
	return registry;
}

} // namespace aion::gameserver::dataholders::detail
