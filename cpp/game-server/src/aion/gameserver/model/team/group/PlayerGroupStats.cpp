#include "aion/gameserver/model/team/group/PlayerGroupStats.h"

#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"

namespace aion::gameserver::model::team::group {

PlayerGroupStats::PlayerGroupStats(PlayerGroup& groupValue) : OwnedPart(groupValue), group(groupValue) {
}

PlayerGroupStats::~PlayerGroupStats() = default;

void PlayerGroupStats::onAddPlayer(PlayerGroupMember& member) {
	updateMinMaxLevelPlayers();
	calculateExpLevels();
}

void PlayerGroupStats::onRemovePlayer(PlayerGroupMember& member) {
	// Java keeps the stale references: a member that left stays referenced until the next onAddPlayer (m5g-plan.md D7, a Java bug that skews
	// getMinExpPlayerLevel only through the next join; docs/deviations/P5-10b.md)
	updateMinMaxLevelPlayers();
}

void PlayerGroupStats::calculateExpLevels() {
	minExpPlayerLevel.set((*minLevelPlayer.get()).getLevel());
	maxExpPlayerLevel.set((*maxLevelPlayer.get()).getLevel());
	minLevelPlayer.set(nullptr);
	maxLevelPlayer.set(nullptr);
}

void PlayerGroupStats::updateMinMaxLevelPlayers() {
	group.forEach([this](gameobjects::AionObject& object) {
		gameobjects::player::Player& player = *runtime::cast<gameobjects::player::Player>(object);
		if (!minLevelPlayer.get() || !maxLevelPlayer.get()) {
			minLevelPlayer.set(runtime::Ptr<gameobjects::player::Player>(player));
			maxLevelPlayer.set(runtime::Ptr<gameobjects::player::Player>(player));
		} else {
			if (player.getCommonData()->getExp() < minLevelPlayer.get()->getCommonData()->getExp()) {
				minLevelPlayer.set(runtime::Ptr<gameobjects::player::Player>(player));
			}
			if (!player.isMentor() && player.getCommonData()->getExp() > maxLevelPlayer.get()->getCommonData()->getExp()) {
				maxLevelPlayer.set(runtime::Ptr<gameobjects::player::Player>(player));
			}
		}
	});
}

void PlayerGroupStats::releasePlayers() noexcept {
	minLevelPlayer.set(nullptr);
	maxLevelPlayer.set(nullptr);
}

} // namespace aion::gameserver::model::team::group
