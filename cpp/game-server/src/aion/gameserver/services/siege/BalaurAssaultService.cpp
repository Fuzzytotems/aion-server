#include "aion/gameserver/services/siege/BalaurAssaultService.h"

#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/dataholders/AssembledNpcsData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/assemblednpc/AssembledNpc.h"
#include "aion/gameserver/model/assemblednpc/AssembledNpcPart.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/Influence.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/network/aion/serverpackets/SM_NPC_ASSEMBLER.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/siege/AgentSiege.h"
#include "aion/gameserver/services/siege/ArtifactAssault.h"
#include "aion/gameserver/services/siege/ArtifactSiege.h"
#include "aion/gameserver/services/siege/FortressAssault.h"
#include "aion/gameserver/services/siege/FortressSiege.h"
#include "aion/gameserver/services/siege/OutpostSiege.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::siege {

using model::siege::SiegeRace;

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

namespace {
/** Java siege.getClass().getSimpleName() for the four Siege subclasses */
std::string simpleNameOf(Siege& siege) {
	if (runtime::as<FortressSiege>(siege))
		return "FortressSiege";
	if (runtime::as<ArtifactSiege>(siege))
		return "ArtifactSiege";
	if (runtime::as<OutpostSiege>(siege))
		return "OutpostSiege";
	if (runtime::as<AgentSiege>(siege))
		return "AgentSiege";
	return "Siege";
}
} // namespace

BalaurAssaultService::BalaurAssaultService() = default;

BalaurAssaultService::~BalaurAssaultService() = default;

BalaurAssaultService& BalaurAssaultService::getInstance() {
	static BalaurAssaultService instance; // Java SingletonHolder
	return instance;
}

// Java BalaurAssaultService.java:44-55
void BalaurAssaultService::onSiegeStart(Siege& siege) {
	if (runtime::Ptr<FortressSiege> fortressSiege = runtime::as<FortressSiege>(siege)) {
		if (!calculateFortressAssault(*fortressSiege->getSiegeLocation()))
			return;
		newAssault(siege, commons::utils::Rnd::get(60, 900)); // between 1 and 15 minutes
	} else if (runtime::as<ArtifactSiege>(siege)) {
		if (artifactAssaults.containsKey(siege.getSiegeLocation()->getLocationId()) || siege.getSiegeLocation()->getRace() == SiegeRace::BALAUR)
			return;
		newAssault(siege, commons::utils::Rnd::get(10800, 172800)); // between 3 and 48 hours
	}
}

// Java BalaurAssaultService.java:57-73
void BalaurAssaultService::onSiegeFinish(Siege& siege) {
	const int32_t locId = siege.getSiegeLocationId();
	const bool isBossKilled = siege.isBossKilled();
	if (fortressAssaults.containsKey(locId)) {
		fortressAssaults.remove(locId)->finishAssault(isBossKilled);
		if (isBossKilled && siege.getSiegeLocation()->getRace() == SiegeRace::BALAUR)
			log.info(siege.toString() + " has been captured by Balaur assault!");
		else
			log.info(siege.toString() + " Balaur assault finished without capture!");
	} else if (artifactAssaults.containsKey(locId)) {
		artifactAssaults.remove(locId)->finishAssault(isBossKilled);
		if (isBossKilled && siege.getSiegeLocation()->getRace() == SiegeRace::BALAUR)
			log.info(siege.toString() + " has been captured by Balaur assault!");
		else
			log.info(siege.toString() + " Balaur assault finished without capture!");
	}
}

// Java BalaurAssaultService.java:75-96
bool BalaurAssaultService::calculateFortressAssault(model::siege::FortressLocation& fortress) {
	if (fortress.getRace() == SiegeRace::BALAUR || !fortress.isVulnerable())
		return false;

	const bool isBalaurea = fortress.getWorldId() == 210050000 || fortress.getWorldId() == 220070000;

	if (fortressAssaults.containsKey(fortress.getLocationId()))
		return false;

	int32_t count = 0;
	for (const runtime::Ptr<FortressAssault>& fa : fortressAssaults.values()) {
		if (fa->getWorldId() == fortress.getWorldId())
			count++;
	}
	if (count >= (isBalaurea ? 1 : 2)) // Allow only 2 Balaur attacks per map, 1 per Balaurea map
		return false;

	const float influence = fortress.getRace() == SiegeRace::ASMODIANS ? model::siege::Influence::getInstance().getAsmodianInfluenceRate()
	                                                                   : model::siege::Influence::getInstance().getElyosInfluenceRate();

	return commons::utils::Rnd::chance() < influence * 100.0f * configs::main::SiegeConfig::BALAUR_ASSAULT_RATE.load();
}

// Java BalaurAssaultService.java:98-105
bool BalaurAssaultService::startAssault(int32_t location, int32_t delay) {
	runtime::Ptr<Siege> siege = SiegeService::getInstance().getSiege(location);
	if (!siege || fortressAssaults.containsKey(location) || artifactAssaults.containsKey(location)) {
		return false;
	}
	newAssault(*siege, delay);
	return true;
}

// Java BalaurAssaultService.java:107-121
void BalaurAssaultService::newAssault(Siege& siege, int32_t delay) {
	if (runtime::Ptr<FortressSiege> fortressSiege = runtime::as<FortressSiege>(siege)) {
		runtime::Ref<FortressAssault> assault = FortressAssault::create(*fortressSiege);
		assault->startAssault(delay);
		fortressAssaults.put(siege.getSiegeLocationId(), assault);
	} else if (runtime::Ptr<ArtifactSiege> artifactSiege = runtime::as<ArtifactSiege>(siege)) {
		runtime::Ref<ArtifactAssault> assault = ArtifactAssault::create(*artifactSiege);
		assault->startAssault(delay);
		artifactAssaults.put(siege.getSiegeLocationId(), assault);
	} else {
		throw runtime::IllegalArgumentException("Unsupported fortress siege type: " + simpleNameOf(siege));
	}
	if (configs::main::LoggingConfig::LOG_SIEGE.load())
		log.info("Scheduled assault of " + siege.toString() + " in " + std::to_string(delay) + " seconds");
}

// Java BalaurAssaultService.java:123-133
void BalaurAssaultService::spawnDredgion(int32_t spawnId) {
	const model::templates::assemblednpc::AssembledNpcTemplate* template_ = dataholders::DataManager::ASSEMBLED_NPC_DATA->getAssembledNpcTemplate(spawnId);
	if (template_ == nullptr)
		throw runtime::NullPointerException("AssembledNpcTemplate"); // Java: template.getAssembledNpcPartTemplates() on null
	std::vector<runtime::Ref<model::assemblednpc::AssembledNpcPart>> parts;
	for (const auto& part : template_->getAssembledNpcPartTemplates())
		parts.push_back(model::assemblednpc::AssembledNpcPart::create(utils::idfactory::IDFactory::getInstance().nextId(), &part));
	const std::vector<runtime::Ptr<model::assemblednpc::AssembledNpcPart>> assembledParts(parts.begin(), parts.end());

	runtime::Ref<model::assemblednpc::AssembledNpc> npc =
		model::assemblednpc::AssembledNpc::create(template_->getRouteId(), template_->getMapId(), template_->getLiveTime(), assembledParts);
	world::World::getInstance().forEachPlayer([&npc](model::gameobjects::player::Player& p) {
		utils::PacketSendUtility::sendPacket(p, network::aion::serverpackets::SM_NPC_ASSEMBLER(npc));
		utils::PacketSendUtility::sendPacket(p, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_ABYSS_CARRIER_SPAWN());
	});
}

// Java BalaurAssaultService.java:139-141
runtime::Ptr<FortressAssault> BalaurAssaultService::getFortressAssaultBySiegeId(int32_t siegeId) {
	return fortressAssaults.get(siegeId);
}

} // namespace aion::gameserver::services::siege
