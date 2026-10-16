#include "aion/gameserver/services/siege/OutpostSiege.h"

#include <string>
#include <vector>

#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/OutpostLocation.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/mail/SiegeResult.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/siege/SiegeCounter.h"
#include "aion/gameserver/services/siege/SiegeRaceCounter.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/ChatUtil.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::siege {

using model::siege::SiegeModType;
using model::siege::SiegeRace;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

OutpostSiege::OutpostSiege(model::siege::OutpostLocation& siegeLocation) : Siege(siegeLocation) {
}

runtime::Ref<OutpostSiege> OutpostSiege::create(model::siege::OutpostLocation& siegeLocation) {
	return runtime::makeRef<OutpostSiege>(siegeLocation);
}

runtime::Ptr<model::siege::OutpostLocation> OutpostSiege::getSiegeLocation() const {
	return runtime::cast<model::siege::OutpostLocation>(Siege::getSiegeLocation());
}

// Java OutpostSiege.java:29-39
void OutpostSiege::onSiegeStart() {
	getSiegeLocation()->setVulnerable(true);
	despawnNpcs(getSiegeLocationId());
	spawnNpcs(getSiegeLocationId(), getSiegeLocation()->getRace(), SiegeModType::SIEGE);
	initSiegeBoss();
	utils::PacketSendUtility::broadcastToWorld(
		getSiegeLocationId() == 2111 ? SM_SYSTEM_MESSAGE::STR_FIELDABYSS_LIGHTBOSS_SPAWN() : SM_SYSTEM_MESSAGE::STR_FIELDABYSS_DARKBOSS_SPAWN());
	broadcastUpdate(*getSiegeLocation());
}

// Java OutpostSiege.java:41-59
void OutpostSiege::onSiegeFinish() {
	getSiegeLocation()->setVulnerable(false);
	despawnSiegeNpcs();
	if (isBossKilled()) {
		onAgentDefeated();
		sendRewardsToParticipants(*getWinnerRaceCounter(), mail::SiegeResult::OCCUPY);
		sendRewardsToParticipants(*getSiegeCounter()->getRaceCounter(getSiegeLocationId() == 2111 ? SiegeRace::ELYOS : SiegeRace::ASMODIANS),
			mail::SiegeResult::FAIL);
	} else {
		utils::PacketSendUtility::broadcastToWorld(
			getSiegeLocationId() == 2111 ? SM_SYSTEM_MESSAGE::STR_FIELDABYSS_LIGHTBOSS_DESPAWN() : SM_SYSTEM_MESSAGE::STR_FIELDABYSS_DARKBOSS_DESPAWN());
		sendRewardsToParticipants(*getSiegeCounter()->getRaceCounter(SiegeRace::ELYOS), mail::SiegeResult::EMPTY);
		sendRewardsToParticipants(*getSiegeCounter()->getRaceCounter(SiegeRace::ASMODIANS), mail::SiegeResult::EMPTY);
	}
	broadcastUpdate(*getSiegeLocation());
	spawnNpcs(getSiegeLocationId(), getSiegeLocation()->getRace(), SiegeModType::PEACE);
}

// Java OutpostSiege.java:61-78
void OutpostSiege::onAgentDefeated() {
	runtime::Ptr<SiegeRaceCounter> winnerCounter = getWinnerRaceCounter();
	const std::vector<std::pair<int32_t, int64_t>> topPlayerDamages = winnerCounter->getPlayerDamageCounter();
	if (!topPlayerDamages.empty()) {
		const int32_t topPlayerId = topPlayerDamages.front().first;
		runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = player::PlayerService::getOrLoadPlayerCommonData(topPlayerId);
		if (!pcd)
			throw runtime::NullPointerException("PlayerCommonData of " + std::to_string(topPlayerId)); // Java: pcd.getName()
		const std::string playerName = pcd->getName();
		const std::string playerRace = utils::ChatUtil::l10n(model::getL10nId(pcd->getRace()));
		utils::PacketSendUtility::broadcastToWorld(getSiegeLocationId() == 2111 ? SM_SYSTEM_MESSAGE::STR_FIELDABYSS_LIGHTBOSS_KILLED(playerName, playerRace)
																				 : SM_SYSTEM_MESSAGE::STR_FIELDABYSS_DARKBOSS_KILLED(playerName, playerRace));
		const model::Race winnerRace = winnerCounter->getSiegeRace() == SiegeRace::ELYOS ? model::Race::ELYOS : model::Race::ASMODIANS;
		world::World::getInstance().forEachPlayer([winnerRace](model::gameobjects::player::Player& p) {
			if (p.getRace() == winnerRace)
				skillengine::SkillEngine::getInstance().applyEffectDirectly(winnerRace == model::Race::ELYOS ? 12120 : 12119, p, p);
		});
	}
}

// Java OutpostSiege.java:80-86
void OutpostSiege::despawnSiegeNpcs() {
	std::vector<runtime::Ptr<model::gameobjects::siege::SiegeNpc>> npcs = world::World::getInstance().getLocalSiegeNpcs(getSiegeLocationId());
	for (const runtime::Ptr<model::gameobjects::siege::SiegeNpc>& npc : npcs) {
		if (npc)
			npc->getController().deleteIfAliveOrCancelRespawn();
	}
}

// Java OutpostSiege.java:88-91
bool OutpostSiege::isEndless() {
	return false;
}

// Java OutpostSiege.java:93-97
void OutpostSiege::onAbyssPointsAdded(model::gameobjects::player::Player& player, int32_t abyssPoints) {
	if (getSiegeLocation()->isVulnerable() && getSiegeLocation()->isInsideLocation(player))
		getSiegeCounter()->addAbyssPoints(player, abyssPoints);
}

OutpostSiege::~OutpostSiege() = default;

} // namespace aion::gameserver::services::siege
