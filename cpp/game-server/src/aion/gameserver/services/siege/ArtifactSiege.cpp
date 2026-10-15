#include "aion/gameserver/services/siege/ArtifactSiege.h"

#include <optional>
#include <string>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/dao/SiegeDAO.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/siege/BalaurAssaultService.h"
#include "aion/gameserver/services/siege/SiegeRaceCounter.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::siege {

using model::siege::SiegeModType;
using model::siege::SiegeRace;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.siege.ArtifactSiege");

ArtifactSiege::ArtifactSiege(model::siege::ArtifactLocation& siegeLocation) : Siege(siegeLocation) {
}

runtime::Ref<ArtifactSiege> ArtifactSiege::create(model::siege::ArtifactLocation& siegeLocation) {
	return runtime::makeRef<ArtifactSiege>(siegeLocation);
}

runtime::Ptr<model::siege::ArtifactLocation> ArtifactSiege::getSiegeLocation() const {
	return runtime::cast<model::siege::ArtifactLocation>(Siege::getSiegeLocation());
}

// Java ArtifactSiege.java:32-39
void ArtifactSiege::onSiegeStart() {
	initSiegeBoss();
	getSiegeLocation()->setInitialDelay(getStartTime());
	// Check for Balaur Assault
	if (configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.load())
		BalaurAssaultService::getInstance().onSiegeStart(*this);
}

// Java ArtifactSiege.java:41-60
void ArtifactSiege::onSiegeFinish() {
	// despawn npcs
	despawnNpcs(getSiegeLocationId());

	// for artifact should be always true
	if (isBossKilled())
		onCapture();
	else
		log.error("Artifact siege (artifactId:" + std::to_string(getSiegeLocationId()) + ") ended without killing a boss.");

	// add new spawns
	spawnNpcs(getSiegeLocationId(), getSiegeLocation()->getRace(), SiegeModType::PEACE);

	// Store siege results in DB
	dao::SiegeDAO::updateSiegeLocation(*getSiegeLocation());

	broadcastUpdate(*getSiegeLocation());
	startSiege(getSiegeLocationId());
}

// Java ArtifactSiege.java:62-94
void ArtifactSiege::onCapture() {
	// Update winner counter
	runtime::Ptr<SiegeRaceCounter> wRaceCounter = getWinnerRaceCounter();
	getSiegeLocation()->setRace(wRaceCounter->getSiegeRace());

	// Update legion
	const std::optional<int32_t> wLegionId = wRaceCounter->getWinnerLegionId();
	getSiegeLocation()->setLegionId(wLegionId ? *wLegionId : 0);

	// misc stuff to send player system message
	if (getSiegeLocation()->getRace() == SiegeRace::BALAUR) {
		utils::PacketSendUtility::broadcastToWorld(SM_SYSTEM_MESSAGE::STR_GUILD_EVENT_LOSE_ARTIFACT(getSiegeLocation()->getL10n(),
			utils::ChatUtil::l10n(model::siege::getL10nId(getSiegeLocation()->getRace()))));
	} else {
		// Prepare packet data
		std::string wPlayerName;
		const model::Race wRace = wRaceCounter->getSiegeRace() == SiegeRace::ELYOS ? model::Race::ELYOS : model::Race::ASMODIANS;
		runtime::Ptr<model::team::legion::Legion> wLegion = wLegionId ? LegionService::getInstance().getLegion(*wLegionId) : nullptr;
		const std::vector<std::pair<int32_t, int64_t>> playerDamages = wRaceCounter->getPlayerDamageCounter();
		if (!playerDamages.empty()) {
			const int32_t wPlayerId = playerDamages.front().first;
			wPlayerName = player::PlayerService::getPlayerName(wPlayerId).value_or(""); // Java: null, written as an empty string
		}
		const std::string winnerName = wLegion ? wLegion->getName() : wPlayerName;

		// prepare packets, we can use single packet instance
		const std::string wRaceL10n = utils::ChatUtil::l10n(model::getL10nId(wRace));
		SM_SYSTEM_MESSAGE wRacePacket = SM_SYSTEM_MESSAGE::STR_GUILD_EVENT_WIN_ARTIFACT(wRaceL10n, winnerName, getSiegeLocation()->getL10n());
		SM_SYSTEM_MESSAGE lRacePacket = SM_SYSTEM_MESSAGE::STR_GUILD_EVENT_LOSE_ARTIFACT(getSiegeLocation()->getL10n(), wRaceL10n);

		// send update to players
		world::World::getInstance().forEachPlayer([&](model::gameobjects::player::Player& p) {
			utils::PacketSendUtility::sendPacket(p, p.getRace() == wRace ? wRacePacket : lRacePacket);
		});
	}
}

// Java ArtifactSiege.java:96-99
bool ArtifactSiege::isEndless() {
	return true;
}

// Java ArtifactSiege.java:101-104
void ArtifactSiege::onAbyssPointsAdded(model::gameobjects::player::Player& player, int32_t abysPoints) {
	// No need to control AP
}

ArtifactSiege::~ArtifactSiege() = default;

} // namespace aion::gameserver::services::siege
