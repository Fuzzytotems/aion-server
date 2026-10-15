#include "aion/gameserver/services/siege/MercenaryLocation.h"

#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeMercenaryZone.h"
#include "aion/gameserver/model/templates/spawns/Spawn.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"
#include "aion/gameserver/model/templates/spawns/mercenaries/MercenaryRace.h"
#include "aion/gameserver/model/templates/spawns/mercenaries/MercenarySpawn.h"
#include "aion/gameserver/model/templates/spawns/mercenaries/MercenaryZone.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/spawnengine/SpawnEngine.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::services::siege {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.siege.MercenaryLocation");

// Java MercenaryLocation.java:39-45
MercenaryLocation::MercenaryLocation(const model::templates::siegelocation::SiegeMercenaryZone* templateValue, model::siege::SiegeRace raceValue,
	int32_t siegeIdValue)
	: smz(templateValue), race(model::getRaceByString(xml::enumName(raceValue))), siegeId(siegeIdValue) {
	if (this->race)
		spawns = getSpawnZone();
}

runtime::Ref<MercenaryLocation> MercenaryLocation::create(const model::templates::siegelocation::SiegeMercenaryZone* templateValue,
	model::siege::SiegeRace raceValue, int32_t siegeIdValue) {
	return runtime::makeRef<MercenaryLocation>(templateValue, raceValue, siegeIdValue);
}

// Java MercenaryLocation.java:47-63
void MercenaryLocation::spawn() {
	despawnCurrentMercs();
	if (spawns == nullptr)
		return;
	std::vector<runtime::Ref<model::gameobjects::VisibleObject>> mercs;
	for (const std::unique_ptr<model::templates::spawns::Spawn>& spawn : spawns->getSpawns()) {
		for (const model::templates::spawns::SpawnSpotTemplate& sst : spawn->getSpawnSpotTemplates()) {
			runtime::Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate> spawnTemplate = spawnengine::SpawnEngine::newSiegeSpawn(
				spawns->getWorldId(), spawn->getNpcId(), siegeId, model::siege::getByRace(*race), model::siege::SiegeModType::SIEGE, sst.getX(), sst.getY(),
				sst.getZ(), sst.getHeading());
			spawnTemplate->setStaticId(sst.getStaticId());
			runtime::Ptr<model::gameobjects::VisibleObject> newMerc = spawnengine::SpawnEngine::spawnObject(*spawnTemplate, 1);
			mercs.push_back(runtime::Ref<model::gameobjects::VisibleObject>(newMerc)); // Java adds a null as null
		}
	}
	for (const runtime::Ref<model::gameobjects::VisibleObject>& merc : mercs) // Java: spawnedMercs.addAll(mercs)
		spawnedMercs.add(merc);
	lastSpawn.set(commons::utils::currentTimeMillis());
	runtime::Ptr<model::siege::FortressLocation> fortress = SiegeService::getInstance().getFortress(siegeId);
	if (!fortress)
		throw runtime::NullPointerException("no fortress " + std::to_string(siegeId)); // Java: NullPointerException on getFortress(siegeId)
	fortress->forEachPlayer([this](model::gameobjects::player::Player& p) {
		utils::PacketSendUtility::sendPacket(p, network::aion::serverpackets::SM_SYSTEM_MESSAGE(smz->getAnnounceId(), std::vector<std::string>{}));
	});
}

// Java MercenaryLocation.java:65-72
void MercenaryLocation::despawnCurrentMercs() {
	if (spawnedMercs.isEmpty())
		return;
	for (const runtime::Ptr<model::gameobjects::VisibleObject>& merc : spawnedMercs.snapshot()) {
		merc->getController().deleteIfAliveOrCancelRespawn();
	}
	spawnedMercs.clear();
}

// Java MercenaryLocation.java:77-79
bool MercenaryLocation::isRequestValid() {
	return spawns != nullptr && (commons::utils::currentTimeMillis() - lastSpawn.get()) > smz->getCooldown() && !hasEnoughMercsAlive();
}

// Java MercenaryLocation.java:86-96
bool MercenaryLocation::hasEnoughMercsAlive() {
	const int32_t totalMercs = spawnedMercs.size();
	int32_t livingMercs = 0;
	for (const runtime::Ptr<model::gameobjects::VisibleObject>& vo : spawnedMercs.snapshot()) {
		if (auto* npc = dynamic_cast<model::gameobjects::Npc*>(vo.get())) {
			if (vo->isSpawned() && !npc->isDead())
				livingMercs++;
		}
	}
	return livingMercs < (totalMercs / 2);
}

// Java MercenaryLocation.java:98-125
const model::templates::spawns::mercenaries::MercenaryZone* MercenaryLocation::getSpawnZone() {
	const model::templates::spawns::mercenaries::MercenaryZone* tempZone = nullptr;
	const model::templates::spawns::mercenaries::MercenarySpawn* spawn = dataholders::DataManager::SPAWNS_DATA->getMercenarySpawnBySiegeId(siegeId);
	if (spawn == nullptr) {
		log.error("[MERC] There is no mercenaries spawns for siege " + std::to_string(siegeId) + " and zone" + std::to_string(smz->getId()));
		return tempZone;
	}
	const model::templates::spawns::mercenaries::MercenaryRace* targetRace = nullptr;
	for (const model::templates::spawns::mercenaries::MercenaryRace& mrace : spawn->getMercenaryRaces()) {
		if (mrace.getRace() == race) {
			targetRace = &mrace;
			break;
		}
	}
	const std::string raceName(race ? xml::enumName(*race) : std::string_view("null")); // Java: race.toString()
	if (targetRace == nullptr) {
		log.error("[MERC] There is no mercenary race for siege " + std::to_string(siegeId) + ", zone" + std::to_string(smz->getId()) + ", race:" + raceName);
		return tempZone;
	}
	for (const model::templates::spawns::mercenaries::MercenaryZone& mzone : targetRace->getMercenaryZones()) {
		if (mzone.getZoneId() == smz->getId()) {
			tempZone = &mzone;
			break;
		}
	}
	if (tempZone == nullptr) {
		log.error("[MERC] There is no mercenary zone for siege " + std::to_string(siegeId) + ", zone" + std::to_string(smz->getId()) + ", race:" + raceName);
		return tempZone;
	}
	return tempZone;
}

// Java MercenaryLocation.java:127-129
int32_t MercenaryLocation::getCosts() {
	return smz->getCosts();
}

// Java MercenaryLocation.java:131-133
int32_t MercenaryLocation::getMsgId() {
	return smz->getMsgId();
}

MercenaryLocation::~MercenaryLocation() = default;

} // namespace aion::gameserver::services::siege
