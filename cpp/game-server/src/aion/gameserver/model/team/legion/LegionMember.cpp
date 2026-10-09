#include "aion/gameserver/model/team/legion/LegionMember.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/team/legion/Legion.h"
#include "aion/gameserver/model/team/legion/LegionPermissionsMaskInfo.h"

namespace aion::gameserver::model::team::legion {

LegionMember::LegionMember(int32_t objectIdValue, Legion& legionValue) : objectId(objectIdValue), legion(legionValue) {
}

LegionMember::~LegionMember() = default;

runtime::Ref<LegionMember> LegionMember::create(int32_t objectIdValue, Legion& legionValue) {
	return runtime::makeRef<LegionMember>(objectIdValue, legionValue);
}

bool LegionMember::isBrigadeGeneral() {
	return rank.get() == LegionRank::BRIGADE_GENERAL;
}

void LegionMember::increaseChallengeScore(int32_t amount) {
	// java-race: unsynchronized read-modify-write of challengeScore (Java `this.challengeScore += amount`)
	this->challengeScore.set(this->challengeScore.get() + amount);
}

void LegionMember::setPlayerData(gameobjects::player::Player& player) {
	setPlayerData(*player.getCommonData());
}

void LegionMember::setPlayerData(gameobjects::player::PlayerCommonData& playerCommonData) {
	name.set(playerCommonData.getName());
	playerClass.set(playerCommonData.getPlayerClass());
	level.set(playerCommonData.getLevel());
	worldId.set(playerCommonData.getMapId());
	// Java: getLastOnline() == null ? 0 : (int) (getLastOnline().getTime() / 1000)
	const std::optional<commons::database::Timestamp> lastOnline = playerCommonData.getLastOnline();
	lastOnlineEpochSeconds.set(!lastOnline ? 0 : static_cast<int32_t>(lastOnline->time_since_epoch().count() / 1000));
	online.set(playerCommonData.isOnline());
}

bool LegionMember::hasRights(LegionPermissionsMask permissions) {
	switch (rank.get()) {
		case LegionRank::BRIGADE_GENERAL:
			return true;
		case LegionRank::DEPUTY:
			return can(permissions, legion->getDeputyPermission());
		case LegionRank::CENTURION:
			return can(permissions, legion->getCenturionPermission());
		case LegionRank::LEGIONARY:
			return can(permissions, legion->getLegionaryPermission());
		case LegionRank::VOLUNTEER:
			return can(permissions, legion->getVolunteerPermission());
	}
	// Java: the switch expression over all constants has no default; javac's synthetic default throws MatchException
	throw runtime::IllegalStateException("MatchException: " + std::to_string(static_cast<int32_t>(rank.get())));
}

} // namespace aion::gameserver::model::team::legion
