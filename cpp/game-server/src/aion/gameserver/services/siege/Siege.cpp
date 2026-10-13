#include "aion/gameserver/services/siege/Siege.h"

#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/templates/npc/AbyssNpcType.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeReward.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SIEGE_LOCATION_STATE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/abyss/GloryPointsService.h"
#include "aion/gameserver/services/mail/AbyssSiegeLevelInfo.h"
#include "aion/gameserver/services/mail/MailFormatter.h"
#include "aion/gameserver/services/mail/SiegeResult.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/siege/BalaurAssaultService.h"
#include "aion/gameserver/services/siege/SiegeCounter.h"
#include "aion/gameserver/services/siege/SiegeException.h"
#include "aion/gameserver/services/siege/SiegeRaceCounter.h"
#include "aion/gameserver/utils/JavaMath.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/SimpleClassName.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services::siege {

static const auto log = commons::logging::LoggerFactory::getLogger("SIEGE_LOG");

Siege::Siege(model::siege::SiegeLocation& value)
	: siegeCounter(SiegeCounter::create()), siegeLocation(runtime::Ref<model::siege::SiegeLocation>(value)) {
}

// Java Siege.java:46-66
void Siege::startSiege() {
	bool doubleStart = false;

	// keeping synchronization as minimal as possible
	SYNCHRONIZED(*this) {
		if (started.get()) {
			doubleStart = true;
		} else {
			startTime.set(commons::utils::currentTimeMillis());
			started.set(true);
		}
	}

	if (doubleStart) {
		log.error("Attempt to start " + toString() + " twice", runtime::IllegalStateException(""));
	} else {
		onSiegeStart();
	}
}

// Java Siege.java:68-70
void Siege::startSiege(int32_t locationId) {
	SiegeService::getInstance().startSiege(locationId);
}

// Java Siege.java:72-82
void Siege::stopSiege() {
	if (finished.compareAndSet(false, true)) {
		onSiegeFinish();

		if (configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.load()) {
			BalaurAssaultService::getInstance().onSiegeFinish(*this);
		}
	} else {
		log.error("Attempt to stop " + toString() + " twice", runtime::IllegalStateException(""));
	}
}

// Java Siege.java:88-90
int32_t Siege::getSiegeLocationId() {
	return siegeLocation->getLocationId();
}

void Siege::setBoss(runtime::Ptr<model::gameobjects::siege::SiegeNpc> value) {
	this->boss.set(value);
}

// Java Siege.java:112-114
runtime::Ptr<SiegeRaceCounter> Siege::getWinnerRaceCounter() {
	return siegeCounter->getWinnerRaceCounter(getSiegeLocation()->getRace());
}

// Java Siege.java:128-130
bool Siege::isFinished() {
	return finished.get();
}

// Java Siege.java:136-152
void Siege::initSiegeBoss() {
	runtime::Ptr<model::gameobjects::siege::SiegeNpc> foundBoss;

	std::vector<runtime::Ptr<model::gameobjects::siege::SiegeNpc>> npcs = world::World::getInstance().getLocalSiegeNpcs(getSiegeLocationId());
	for (const runtime::Ptr<model::gameobjects::siege::SiegeNpc>& npc : npcs) {
		if (npc->getObjectTemplate()->getAbyssNpcType() == model::templates::npc::AbyssNpcType::BOSS) {
			if (foundBoss)
				throw SiegeException("Found 2 siege bosses for outpost " + std::to_string(getSiegeLocationId()));

			foundBoss = npc;
		}
	}
	if (!foundBoss)
		throw SiegeException("Siege Boss not found for siege " + std::to_string(getSiegeLocationId()));

	setBoss(foundBoss);
}

// Java Siege.java:154-156
void Siege::spawnNpcs(int32_t locationId, model::siege::SiegeRace race, model::siege::SiegeModType type) {
	SiegeService::getInstance().spawnNpcs(locationId, race, type);
}

// Java Siege.java:158-160
void Siege::despawnNpcs(int32_t locationId) {
	SiegeService::getInstance().deSpawnNpcs(locationId);
}

// Java Siege.java:162-164
void Siege::broadcastState(model::siege::SiegeLocation& location) {
	utils::PacketSendUtility::broadcastToWorld(network::aion::serverpackets::SM_SIEGE_LOCATION_STATE(location));
}

// Java Siege.java:166-168
void Siege::broadcastUpdate(model::siege::SiegeLocation& location) {
	SiegeService::getInstance().broadcastUpdate(location);
}

// Java Siege.java:170-172
void Siege::updateOutpostStatusByFortress(model::siege::FortressLocation& location) {
	SiegeService::getInstance().updateOutpostSiegeState(location);
}

// Java Siege.java:174-215
void Siege::sendRewardsToParticipants(SiegeRaceCounter& raceCounter, mail::SiegeResult raceResult) {
	try {
		const std::vector<std::pair<int32_t, int64_t>> playerAbyssPoints = raceCounter.getPlayerAbyssPoints();
		std::vector<int32_t> topPlayersIds;
		for (const auto& [playerId, points] : playerAbyssPoints)
			topPlayersIds.push_back(playerId);
		const bool isWinner = raceResult == mail::SiegeResult::OCCUPY || raceResult == mail::SiegeResult::DEFENDER;

		size_t playerIndex = 0;
		int32_t rewardLevel = 0;
		const int64_t timeMillis = commons::utils::currentTimeMillis();
		for (const model::templates::siegelocation::SiegeReward* topGrade : getSiegeLocation()->getRewards()) {
			std::vector<int32_t> rewardedGpPlayers;
			const int64_t kinahRewardForRewardLevel = getSiegeLocation()->getTemplate()->getKinahRewardByRewardLevel(rewardLevel);
			// java-bug (kept, m5i-plan.md D8): getLevelById(5) throws IllegalArgumentException for the four fortresses with five reward grades,
			// caught below like Java's catch (Exception e)
			const mail::AbyssSiegeLevel level = mail::getLevelById(++rewardLevel);
			const int32_t gp = isWinner ? topGrade->getGpForWin() : topGrade->getGpForDefeat();

			for (int32_t i = 0; i < topGrade->getTop() && playerIndex < topPlayersIds.size(); i++, playerIndex++) {
				const int32_t playerId = topPlayersIds[playerIndex];
				// Java: Math.round(long / (float) top) - a float division rounded to an int
				int64_t kinahReward = utils::JavaMath::round(static_cast<float>(kinahRewardForRewardLevel) / static_cast<float>(topGrade->getTop()));
				if (kinahReward > 40000000)
					kinahReward = 40000000;
				if (isWinner && topGrade->hasItemRewardsForWin()) {
					runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = player::PlayerService::getOrLoadPlayerCommonData(playerId);
					if (!pcd)
						throw runtime::NullPointerException("PlayerCommonData of " + std::to_string(playerId)); // Java: NPE inside the formatter
					mail::MailFormatter::sendAbyssRewardMail(*getSiegeLocation(), *pcd, level, raceResult, timeMillis, topGrade->getItemId(),
						topGrade->getItemCount(), kinahReward);
				} else if (!isWinner && topGrade->hasItemRewardsForDefeat()) {
					runtime::Ref<model::gameobjects::player::PlayerCommonData> pcd = player::PlayerService::getOrLoadPlayerCommonData(playerId);
					if (!pcd)
						throw runtime::NullPointerException("PlayerCommonData of " + std::to_string(playerId));
					mail::MailFormatter::sendCustomAbyssDefeatRewardMail(*pcd, topGrade->getItemIdDefeat(), topGrade->getItemCountDefeat());
				}

				if (gp > 0) {
					rewardedGpPlayers.push_back(playerId);
					abyss::GloryPointsService::addGp(playerId, gp); // Rates.GP.calcResult() cannot be fairly applied here, because some players could be offline
				}
			}
			if (configs::main::LoggingConfig::LOG_SIEGE.load() && !rewardedGpPlayers.empty()) {
				std::string ids = "[";
				for (size_t i = 0; i < rewardedGpPlayers.size(); i++)
					ids += (i == 0 ? "" : ", ") + std::to_string(rewardedGpPlayers[i]); // Java: List.toString()
				ids += "]";
				log.info(toString() + ": Distributed " + std::to_string(gp) + " " + (isWinner ? "winner" : "loser") + " GP each, to the following players (rank " +
					std::to_string(rewardLevel) + "): " + ids);
			}
		}
	} catch (const std::exception& e) {
		log.error("Error while distributing rewards for " + toString(), e);
	}
}

// Java Siege.java:217-220
std::string Siege::toString() {
	return utils::simpleClassName(typeid(*this)) + " [locationId=" + std::to_string(getSiegeLocationId()) + "]";
}

Siege::~Siege() = default;

} // namespace aion::gameserver::services::siege
