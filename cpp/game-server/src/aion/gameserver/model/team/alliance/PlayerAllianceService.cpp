#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::model::team::alliance {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.model.team.alliance.PlayerAllianceService");

// Java implements Runnable; scheduled at a fixed rate by initializeOfflineCheck (no members: a K3 task)
class PlayerAllianceService::OfflinePlayerAllianceChecker : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	static runtime::Ref<OfflinePlayerAllianceChecker> create() { return runtime::makeRef<OfflinePlayerAllianceChecker>(); }
	void run(); // @Override of a Java library type
protected:
	OfflinePlayerAllianceChecker() = default;
	~OfflinePlayerAllianceChecker() override = default;
};

void PlayerAllianceService::OfflinePlayerAllianceChecker::run() {
	AION_UNPORTED();
}

void PlayerAllianceService::inviteToAlliance(gameobjects::player::Player& inviter, gameobjects::player::Player& invited) {
	AION_UNPORTED();
}

runtime::Ptr<PlayerAlliance> PlayerAllianceService::createAlliance(gameobjects::player::Player& leader, gameobjects::player::Player& invited, TeamType type) {
	AION_UNPORTED();
}

void PlayerAllianceService::initializeOfflineCheck() {
	AION_UNPORTED();
}

runtime::Ptr<PlayerAllianceMember> PlayerAllianceService::addPlayerToAlliance(PlayerAlliance& alliance, gameobjects::player::Player& invited) {
	AION_UNPORTED();
}

void PlayerAllianceService::changeGroupRules(PlayerAlliance& alliance, common::legacy::LootGroupRules& lootRules) {
	AION_UNPORTED();
}

void PlayerAllianceService::onPlayerLogin(gameobjects::player::Player& player) {
	for (const runtime::Ptr<PlayerAlliance>& alliance : alliances.values().toVector()) {
		runtime::Ptr<PlayerAllianceMember> member = alliance->getMember(player.getObjectId());
		if (member) {
			// Java: alliance.onEvent(new PlayerConnectedEvent(alliance, player)); the alliance events are ported with the teams (M5b)
			AION_UNPORTED();
		}
	}
}

void PlayerAllianceService::onPlayerLogout(gameobjects::player::Player& player) {
	runtime::Ptr<PlayerAlliance> alliance = player.getPlayerAlliance();
	if (alliance) {
		// Java: member.updateLastOnlineTime(); alliance.onEvent(new PlayerDisconnectedEvent(alliance, player)); ported with the teams (M5b)
		AION_UNPORTED();
	}
}

void PlayerAllianceService::updateAlliance(gameobjects::player::Player& player, common::legacy::PlayerAllianceEvent allianceEvent) {
	AION_UNPORTED();
}

void PlayerAllianceService::updateAllianceEffects(gameobjects::player::Player& player, int32_t slot) {
	AION_UNPORTED();
}

void PlayerAllianceService::addPlayer(PlayerAlliance& alliance, gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerAllianceService::removePlayer(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerAllianceService::banPlayer(gameobjects::player::Player& bannedPlayer, gameobjects::player::Player& banGiver) {
	AION_UNPORTED();
}

void PlayerAllianceService::disband(PlayerAlliance& alliance, bool onBefore) {
	AION_UNPORTED();
}

void PlayerAllianceService::changeLeader(gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerAllianceService::changeViceCaptain(gameobjects::player::Player& player, events::AssignViceCaptainEvent_AssignType assignType) {
	AION_UNPORTED();
}

runtime::Ptr<PlayerAlliance> PlayerAllianceService::searchAlliance(int32_t playerObjId) {
	for (const runtime::Ptr<PlayerAlliance>& alliance : alliances.values().toVector()) {
		if (alliance->hasMember(playerObjId)) {
			return alliance;
		}
	}
	return nullptr;
}

void PlayerAllianceService::changeMemberGroup(gameobjects::player::Player& player, int32_t firstPlayer, int32_t secondPlayer, int32_t allianceGroupId) {
	AION_UNPORTED();
}

void PlayerAllianceService::checkReady(gameobjects::player::Player& player, common::events::TeamCommand eventCode) {
	AION_UNPORTED();
}

void PlayerAllianceService::distributeKinah(gameobjects::player::Player& player, int64_t amount) {
	AION_UNPORTED();
}

void PlayerAllianceService::distributeKinahInGroup(gameobjects::player::Player& player, int64_t amount) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::team::alliance
