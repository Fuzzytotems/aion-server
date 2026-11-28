// StaticData's setValidationTask and getValidationTask are replaced: the after-unmarshal tasks run inline (LoadContext::runAfterUnmarshalTask;
// upstream 546f96bd2 removed addAfterUnmarshalTask and waitForAfterUnmarshalTasksToFinish) and there is no XSD validation at run time
// (static-data.md §3.4).

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
#include "aion/gameserver/dataholders/PvpExpTable.h"
#include "aion/gameserver/dataholders/PvpExpModTable.h"
#include "aion/gameserver/dataholders/PartyExpModTable.h"
#include "aion/gameserver/dataholders/MenteeExpLimitTable.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/dataholders/Portal2Data.h"
#include "aion/gameserver/dataholders/PortalLocData.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/RecipeData.h"
#include "aion/gameserver/dataholders/RideData.h"
#include "aion/gameserver/dataholders/RiftData.h"
#include "aion/gameserver/dataholders/RoadData.h"
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

} // namespace

void StaticData::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	logCounts();
}

void StaticData::logCounts() const {
	const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.StaticData");
	log.info("Loaded {} maps", field(worldMapsData, "worldMapsData").size());
	log.info("Loaded {} material ids", field(materiaData, "materiaData").size());
	log.info("Loaded weather for {} maps", field(mapWeatherData, "mapWeatherData").size());
	log.info("Loaded {} player experience table entries", field(playerExperienceTable, "playerExperienceTable").getMaxLevel());
	log.info("Loaded {} pvp experience table entries", field(pvpExpTable, "pvpExpTable").getMaxLevel());
	log.info("Loaded {} pvp experience modifier entries", field(pvpExpModTable, "pvpExpModTable").size());
	log.info("Loaded {} party experience modifier entries", field(partyExpModTable, "partyExpModTable").size());
	log.info("Loaded {} mentee experience limit entries", field(menteeExpLimitTable, "menteeExpLimitTable").size());
	log.info("Loaded {} absolute stat templates", field(absoluteStatsData, "absoluteStatsData").size());
	log.info("Loaded {} item cleanup entries", field(itemCleanup, "itemCleanup").size());
	log.info("Loaded {} item templates", field(itemData, "itemData").size());
	log.info("Loaded {} item bonus templates", field(itemRandomBonuses, "itemRandomBonuses").size());
	log.info("Loaded {} bonus item group templates and {} pet food items", field(itemGroupsData, "itemGroupsData").bonusSize(), itemGroupsData->petFoodSize());
	log.info("Loaded {} npc templates", field(npcData, "npcData").size());
	log.info("Loaded {} custom npc drops", field(customNpcDrop, "customNpcDrop").size());
	log.info("Loaded {} system mail templates", field(systemMailTemplates, "systemMailTemplates").size());
	log.info("Loaded {} npc shout templates", field(npcShoutData, "npcShoutData").size());
	log.info("Loaded {} pet templates and {} food flavours", field(petData, "petData").size(), field(petFeedData, "petFeedData").size());
	log.info("Loaded {} pet doping templates", field(petDopingData, "petDopingData").size());
	log.info("Loaded {} pet buffs templates", field(petBuffsData, "petBuffsData").size());
	log.info("Loaded {} initial player templates", field(playerInitialData, "playerInitialData").size());
	log.info("Loaded {} trade lists", field(tradeListData, "tradeListData").size());
	log.info("Loaded {} npc teleporter templates", field(teleporterData, "teleporterData").size());
	log.info("Loaded {} teleport locations", field(teleLocationData, "teleLocationData").size());
	log.info("Loaded {} skill templates", field(skillData, "skillData").size());
	log.info("Loaded {} skill charge entries", field(skillChargeData, "skillChargeData").size());
	log.info("Loaded {} motion times", field(motionData, "motionData").size());
	log.info("Loaded {} skill learn entries", field(skillTreeData, "skillTreeData").size());
	log.info("Loaded {} cube expand entries", field(cubeExpandData, "cubeExpandData").size());
	log.info("Loaded {} warehouse expand entries", field(warehouseExpandData, "warehouseExpandData").size());
	log.info("Loaded {} bind point entries", field(bindPointData, "bindPointData").size());
	log.info("Loaded {} quest data entries", field(questData, "questData").size());
	log.info("Loaded {} gatherable entries", field(gatherableData, "gatherableData").size());
	log.info("Loaded {} title entries", field(titleData, "titleData").size());
	log.info("Loaded {} walker routes", field(walkerData, "walkerData").size());
	log.info("Loaded {} walker group variants", field(walkerVersionsData, "walkerVersionsData").size());
	log.info("Loaded {} zone entries", field(zoneData, "zoneData").size());
	log.info("Loaded {} goodslist entries", field(goodsListData, "goodsListData").size());
	log.info("Loaded {} tribe relation entries", field(tribeRelationsData, "tribeRelationsData").size());
	log.info("Loaded {} recipe entries", field(recipeData, "recipeData").size());
	log.info("Loaded {} chest locations", field(chestData, "chestData").size());
	log.info("Loaded {} static door locations", field(staticDoorData, "staticDoorData").size());
	log.info("Loaded {} item set entries", field(itemSetData, "itemSetData").size());
	log.info("Loaded {} npc factions", field(npcFactionsData, "npcFactionsData").size());
	log.info("Loaded {} npc skill list entries", field(npcSkillData, "npcSkillData").size());
	log.info("Loaded {} pet skill list entries", field(petSkillData, "petSkillData").size());
	log.info("Loaded {} siege location entries", field(siegeLocationData, "siegeLocationData").size());
	log.info("Loaded {} vortex entries", field(vortexData, "vortexData").size());
	log.info("Loaded {} rift entries", field(riftData, "riftData").size());
	log.info("Loaded {} base entries", field(baseData, "baseData").size());
	log.info("Loaded {} fly ring entries", field(flyRingData, "flyRingData").size());
	log.info("Loaded {} pet entries", petData->size());
	log.info("Loaded {} guide entries", field(guideData, "guideData").size());
	log.info("Loaded {} road entries", field(roadData, "roadData").size());
	log.info("Loaded {} instance cooltime entries", field(instanceCooltimeData, "instanceCooltimeData").size());
	log.info("Loaded {} decomposable items entries ({} overridden)", field(decomposableItemsData, "decomposableItemsData").size(),
		decomposableItemsData->overrideCount());
	log.info("Loaded {} ai templates", field(aiData, "aiData").size());
	log.info("Loaded {} flypath templates", field(flyPath, "flyPath").size());
	log.info("Loaded {} windstream entries", field(windstreamsData, "windstreamsData").size());
	log.info("Loaded {} assembled npcs entries", field(assembledNpcData, "assembledNpcData").size());
	log.info("Loaded {} cosmetic items entries", field(cosmeticItemsData, "cosmeticItemsData").size());
	log.info("Loaded {} auto group entries", field(autoGroupData, "autoGroupData").size());
	log.info("Loaded {} spawn maps entries", field(spawnsData, "spawnsData").size());
	log.info("Loaded {} events", field(eventData, "eventData").size());
	log.info("Loaded {} skill panel entries", field(panelSkillsData, "panelSkillsData").size());
	log.info("Loaded {} instance Buffs entries", field(instanceBuffData, "instanceBuffData").size());
	log.info("Loaded {} housing object entries", field(housingObjectData, "housingObjectData").size());
	log.info("Loaded {} ride info entries", field(rideData, "rideData").size());
	log.info("Loaded {} instance exit entries", field(instanceExitData, "instanceExitData").size());
	log.info("Loaded {} portal loc entries", field(portalLocData, "portalLocData").size());
	log.info("Loaded {} portal templates2 entries", field(portalTemplate2, "portalTemplate2").size());
	log.info("Loaded {} housing lands", field(houseData, "houseData").size());
	log.info("Loaded {} house building styles", field(houseBuildingData, "houseBuildingData").size());
	log.info("Loaded {} house parts", field(housePartsData, "housePartsData").size());
	log.info("Loaded {} house spawns", field(houseNpcsData, "houseNpcsData").size());
	log.info("Loaded {} assembly items entries", field(assemblyItemData, "assemblyItemData").size());
	log.info("Loaded {} challenge tasks entries", field(challengeData, "challengeData").size());
	log.info("Loaded {} conqueror and protector entries", field(conquerorAndProtectorData, "conquerorAndProtectorData").size());
	log.info("Loaded {} town spawns", field(townSpawnsData, "townSpawnsData").getSpawnsCount());
	log.info("Loaded {} temperings", field(temperingData, "temperingData").size());
	log.info("Loaded {} enchants", field(enchantData, "enchantData").size());
	log.info("Loaded {} global drop rules{}", field(globalDropData, "globalDropData").size(),
		field(globalExclusionData, "globalExclusionData").isEmpty() ? "" : " with global drop npc exclusions");
	log.info("Loaded {} multi return item entries", field(multiReturnItem, "multiReturnItem").size());
	log.info("Loaded {} hotspot entries", field(hotspotData, "hotspotData").size());
	log.info("Loaded {} item purifications entries", field(itemPurificationData, "itemPurificationData").size());
	log.info("Loaded {} upgrade arcade entries", field(upgradeArcadeData, "upgradeArcadeData").size());
	log.info("Loaded {} atreian passports", field(atreianPassportData, "atreianPassportData").size());
	log.info("Loaded {} world raid locations", field(worldRaidData, "worldRaidData").size());
	log.info("Loaded {} kill bounty templates", field(killBountyData, "killBountyData").size());
	log.info("Loaded {} legion dominion locations", field(legionDominionData, "legionDominionData").size());
	log.info("Loaded {} skill alias locations", field(skillAliasLocationData, "skillAliasLocationData").size());
	log.info("Loaded {} signet data templates", field(signetDataTemplates, "signetDataTemplates").size());
}

} // namespace aion::gameserver::dataholders
