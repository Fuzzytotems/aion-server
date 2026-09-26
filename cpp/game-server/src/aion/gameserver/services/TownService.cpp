#include "aion/gameserver/services/TownService.h"

#include <string>
#include <unordered_map>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/dao/TownDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/TribeClass.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/model/templates/spawns/housing/TownSpawnTemplate.h"
#include "aion/gameserver/model/town/Town.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TOWNS_LIST.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.TownService");

using model::Race;
using model::town::Town;

namespace {

/** Java Map<Integer, Town> passed to SM_TOWNS_LIST: the packet takes borrowed values */
std::unordered_map<int32_t, runtime::Ptr<Town>> borrowTowns(runtime::HashMap<int32_t, runtime::Ref<Town>>& towns) {
	std::unordered_map<int32_t, runtime::Ptr<Town>> result;
	for (const auto& entry : towns.snapshot())
		result.emplace(entry.key, entry.value);
	return result;
}

} // namespace

TownService& TownService::getInstance() {
	static TownService instance; // Java SingletonHolder
	return instance;
}

TownService::TownService() {
	elyosTowns.putAll(dao::TownDAO::load(Race::ELYOS));
	asmosTowns.putAll(dao::TownDAO::load(Race::ASMODIANS));
	if (elyosTowns.size() == 0 && asmosTowns.size() == 0) {
		for (const model::templates::housing::HousingLand& land : dataholders::DataManager::HOUSE_DATA->getLands()) {
			if (!land.getAddresses()) // Java: NullPointerException iterating a land without addresses
				throw runtime::NullPointerException("HousingLand " + std::to_string(land.getId()) + " has no addresses");
			for (const model::templates::housing::HouseAddress& address : *land.getAddresses()) {
				if (address.getTownId() == 0)
					continue;
				else {
					const model::templates::npc::NpcTemplate* managerTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(land.getManagerNpcId());
					if (managerTemplate == nullptr) // Java: NullPointerException on getTribe()
						throw runtime::NullPointerException("No npc template for house manager " + std::to_string(land.getManagerNpcId()));
					Race townRace = managerTemplate->getTribe() == model::TribeClass::GENERAL ? Race::ELYOS : Race::ASMODIANS;
					if ((townRace == Race::ELYOS && !elyosTowns.containsKey(address.getTownId())) ||
						(townRace == Race::ASMODIANS && !asmosTowns.containsKey(address.getTownId()))) {
						runtime::Ref<Town> town = Town::create(address.getTownId(), townRace);
						if (townRace == Race::ELYOS)
							elyosTowns.put(town->getId(), town);
						else
							asmosTowns.put(town->getId(), town);
						dao::TownDAO::store(*town);
					}
				}
			}
		}
	}
	log.info("Loaded " + std::to_string(elyosTowns.size()) + " elyos towns.");
	log.info("Loaded " + std::to_string(asmosTowns.size()) + " asmodian towns.");
}

runtime::Ptr<model::town::Town> TownService::getTownById(int32_t townId) {
	if (elyosTowns.containsKey(townId))
		return elyosTowns.get(townId);
	else
		return asmosTowns.get(townId);
}

int32_t TownService::getTownResidence(model::gameobjects::player::Player& player) {
	runtime::Ptr<model::house::House> house = player.getActiveHouse();
	if (!house)
		return 0;
	else
		return house->getAddress()->getTownId();
}

int32_t TownService::getTownIdByPosition(model::gameobjects::Creature& creature) {
	if (runtime::Ptr<model::templates::spawns::housing::TownSpawnTemplate> townSpawnTemplate =
			runtime::as<model::templates::spawns::housing::TownSpawnTemplate>(creature.getSpawn())) {
		return townSpawnTemplate->getTownId();
	}
	if (creature.isSpawned()) {
		for (const runtime::Ptr<world::zone::ZoneInstance>& zone : creature.findZones()) {
			if (zone->getTownId() > 0)
				return zone->getTownId();
		}
	}
	return 0;
}

void TownService::onEnterWorld(model::gameobjects::player::Player& player) {
	switch (player.getRace()) {
		case Race::ELYOS:
			if (player.getWorldId() == 700010000)
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TOWNS_LIST(borrowTowns(elyosTowns)));
			break;
		case Race::ASMODIANS:
			if (player.getWorldId() == 710010000)
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TOWNS_LIST(borrowTowns(asmosTowns)));
			break;
		default:
			break;
	}
}

} // namespace aion::gameserver::services
