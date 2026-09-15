#include "aion/gameserver/dataholders/StaticData.h"

#include <string>

#include "aion/commons/logging/LoggerFactory.h"
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
#include "aion/gameserver/model/templates/mail/Mails.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

namespace {

/** Java: the field access of an absent holder (the import is missing) throws NullPointerException */
template <class H>
const H& field(const std::unique_ptr<H>& holder, const char* name) {
	if (holder == nullptr)
		throw runtime::NullPointerException(std::string("Cannot invoke a method because \"this.") + name + "\" is null");
	return *holder;
}

std::string n(int64_t value) {
	return std::to_string(value);
}

} // namespace

void StaticData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	logCounts();
}

void StaticData::logCounts() const {
	const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.StaticData");
	log.info("Loaded " + n(field(worldMapsData, "worldMapsData").size()) + " maps");
	log.info("Loaded " + n(field(materiaData, "materiaData").size()) + " material ids");
	log.info("Loaded weather for " + n(field(mapWeatherData, "mapWeatherData").size()) + " maps");
	log.info("Loaded " + n(field(playerExperienceTable, "playerExperienceTable").getMaxLevel()) + " player experience table entries");
	log.info("Loaded " + n(field(absoluteStatsData, "absoluteStatsData").size()) + " absolute stat templates");
	log.info("Loaded " + n(field(itemCleanup, "itemCleanup").size()) + " item cleanup entries");
	log.info("Loaded " + n(field(itemData, "itemData").size()) + " item templates");
	log.info("Loaded " + n(field(itemRandomBonuses, "itemRandomBonuses").size()) + " item bonus templates");
	log.info("Loaded " + n(field(itemGroupsData, "itemGroupsData").bonusSize()) + " bonus item group templates and " +
	         n(itemGroupsData->petFoodSize()) + " pet food items");
	log.info("Loaded " + n(field(npcData, "npcData").size()) + " npc templates");
	log.info("Loaded " + n(field(customNpcDrop, "customNpcDrop").size()) + " custom npc drops");
	log.info("Loaded " + n(field(systemMailTemplates, "systemMailTemplates").size()) + " system mail templates");
	log.info("Loaded " + n(field(npcShoutData, "npcShoutData").size()) + " npc shout templates");
	log.info("Loaded " + n(field(petData, "petData").size()) + " pet templates and " + n(field(petFeedData, "petFeedData").size()) + " food flavours");
	log.info("Loaded " + n(field(petDopingData, "petDopingData").size()) + " pet doping templates");
	log.info("Loaded " + n(field(petBuffsData, "petBuffsData").size()) + " pet buffs templates");
	log.info("Loaded " + n(field(playerInitialData, "playerInitialData").size()) + " initial player templates");
	log.info("Loaded " + n(field(tradeListData, "tradeListData").size()) + " trade lists");
	log.info("Loaded " + n(field(teleporterData, "teleporterData").size()) + " npc teleporter templates");
	log.info("Loaded " + n(field(teleLocationData, "teleLocationData").size()) + " teleport locations");
	log.info("Loaded " + n(field(skillData, "skillData").size()) + " skill templates");
	log.info("Loaded " + n(field(skillChargeData, "skillChargeData").size()) + " skill charge entries");
	log.info("Loaded " + n(field(motionData, "motionData").size()) + " motion times");
	log.info("Loaded " + n(field(skillTreeData, "skillTreeData").size()) + " skill learn entries");
	log.info("Loaded " + n(field(cubeExpandData, "cubeExpandData").size()) + " cube expand entries");
	log.info("Loaded " + n(field(warehouseExpandData, "warehouseExpandData").size()) + " warehouse expand entries");
	log.info("Loaded " + n(field(bindPointData, "bindPointData").size()) + " bind point entries");
	log.info("Loaded " + n(field(questData, "questData").size()) + " quest data entries");
	log.info("Loaded " + n(field(gatherableData, "gatherableData").size()) + " gatherable entries");
	log.info("Loaded " + n(field(titleData, "titleData").size()) + " title entries");
	log.info("Loaded " + n(field(walkerData, "walkerData").size()) + " walker routes");
	log.info("Loaded " + n(field(walkerVersionsData, "walkerVersionsData").size()) + " walker group variants");
	log.info("Loaded " + n(field(zoneData, "zoneData").size()) + " zone entries");
	log.info("Loaded " + n(field(goodsListData, "goodsListData").size()) + " goodslist entries");
	log.info("Loaded " + n(field(tribeRelationsData, "tribeRelationsData").size()) + " tribe relation entries");
	log.info("Loaded " + n(field(recipeData, "recipeData").size()) + " recipe entries");
	log.info("Loaded " + n(field(chestData, "chestData").size()) + " chest locations");
	log.info("Loaded " + n(field(staticDoorData, "staticDoorData").size()) + " static door locations");
	log.info("Loaded " + n(field(itemSetData, "itemSetData").size()) + " item set entries");
	log.info("Loaded " + n(field(npcFactionsData, "npcFactionsData").size()) + " npc factions");
	log.info("Loaded " + n(field(npcSkillData, "npcSkillData").size()) + " npc skill list entries");
	log.info("Loaded " + n(field(petSkillData, "petSkillData").size()) + " pet skill list entries");
	log.info("Loaded " + n(field(siegeLocationData, "siegeLocationData").size()) + " siege location entries");
	log.info("Loaded " + n(field(vortexData, "vortexData").size()) + " vortex entries");
	log.info("Loaded " + n(field(riftData, "riftData").size()) + " rift entries");
	log.info("Loaded " + n(field(baseData, "baseData").size()) + " base entries");
	log.info("Loaded " + n(field(flyRingData, "flyRingData").size()) + " fly ring entries");
	log.info("Loaded " + n(field(shieldData, "shieldData").size()) + " shield entries");
	log.info("Loaded " + n(petData->size()) + " pet entries");
	log.info("Loaded " + n(field(guideData, "guideData").size()) + " guide entries");
	log.info("Loaded " + n(field(roadData, "roadData").size()) + " road entries");
	log.info("Loaded " + n(field(instanceCooltimeData, "instanceCooltimeData").size()) + " instance cooltime entries");
	log.info("Loaded " + n(field(decomposableItemsData, "decomposableItemsData").size()) + " decomposable items entries");
	log.info("Loaded " + n(field(aiData, "aiData").size()) + " ai templates");
	log.info("Loaded " + n(field(flyPath, "flyPath").size()) + " flypath templates");
	log.info("Loaded " + n(field(windstreamsData, "windstreamsData").size()) + " windstream entries");
	log.info("Loaded " + n(field(assembledNpcData, "assembledNpcData").size()) + " assembled npcs entries");
	log.info("Loaded " + n(field(cosmeticItemsData, "cosmeticItemsData").size()) + " cosmetic items entries");
	log.info("Loaded " + n(field(autoGroupData, "autoGroupData").size()) + " auto group entries");
	log.info("Loaded " + n(field(spawnsData, "spawnsData").size()) + " spawn maps entries");
	log.info("Loaded " + n(field(eventData, "eventData").size()) + " events");
	log.info("Loaded " + n(field(panelSkillsData, "panelSkillsData").size()) + " skill panel entries");
	log.info("Loaded " + n(field(instanceBuffData, "instanceBuffData").size()) + " instance Buffs entries");
	log.info("Loaded " + n(field(housingObjectData, "housingObjectData").size()) + " housing object entries");
	log.info("Loaded " + n(field(rideData, "rideData").size()) + " ride info entries");
	log.info("Loaded " + n(field(instanceExitData, "instanceExitData").size()) + " instance exit entries");
	log.info("Loaded " + n(field(portalLocData, "portalLocData").size()) + " portal loc entries");
	log.info("Loaded " + n(field(portalTemplate2, "portalTemplate2").size()) + " portal templates2 entries");
	log.info("Loaded " + n(field(houseData, "houseData").size()) + " housing lands");
	log.info("Loaded " + n(field(houseBuildingData, "houseBuildingData").size()) + " house building styles");
	log.info("Loaded " + n(field(housePartsData, "housePartsData").size()) + " house parts");
	log.info("Loaded " + n(field(houseNpcsData, "houseNpcsData").size()) + " house spawns");
	log.info("Loaded " + n(field(curingObjectsData, "curingObjectsData").size()) + " curing object entries");
	log.info("Loaded " + n(field(assemblyItemData, "assemblyItemData").size()) + " assembly items entries");
	log.info("Loaded " + n(field(challengeData, "challengeData").size()) + " challenge tasks entries");
	log.info("Loaded " + n(field(conquerorAndProtectorData, "conquerorAndProtectorData").size()) + " conqueror and protector entries");
	log.info("Loaded " + n(field(townSpawnsData, "townSpawnsData").getSpawnsCount()) + " town spawns");
	log.info("Loaded " + n(field(temperingData, "temperingData").size()) + " temperings");
	log.info("Loaded " + n(field(enchantData, "enchantData").size()) + " enchants");
	log.info("Loaded " + n(field(globalDropData, "globalDropData").size()) + " global drop rules" +
	         (field(globalExclusionData, "globalExclusionData").isEmpty() ? "" : " with global drop npc exclusions"));
	log.info("Loaded " + n(field(multiReturnItem, "multiReturnItem").size()) + " multi return item entries");
	log.info("Loaded " + n(field(hotspotData, "hotspotData").size()) + " hotspot entries");
	log.info("Loaded " + n(field(itemPurificationData, "itemPurificationData").size()) + " item purifications entries");
	log.info("Loaded " + n(field(upgradeArcadeData, "upgradeArcadeData").size()) + " upgrade arcade entries");
	log.info("Loaded " + n(field(atreianPassportData, "atreianPassportData").size()) + " atreian passports");
	log.info("Loaded " + n(field(worldRaidData, "worldRaidData").size()) + " world raid locations");
	log.info("Loaded " + n(field(killBountyData, "killBountyData").size()) + " kill bounty templates");
	log.info("Loaded " + n(field(legionDominionData, "legionDominionData").size()) + " legion dominion locations");
	log.info("Loaded " + n(field(skillAliasLocationData, "skillAliasLocationData").size()) + " skill alias locations");
	log.info("Loaded " + n(field(signetDataTemplates, "signetDataTemplates").size()) + " signet data templates");
}

} // namespace aion::gameserver::dataholders
