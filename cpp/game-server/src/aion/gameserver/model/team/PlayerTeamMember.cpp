#include "aion/gameserver/model/team/PlayerTeamMember.h"

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::team {

PlayerTeamMember::PlayerTeamMember(gameobjects::player::Player& playerValue) : player(playerValue) {
}

PlayerTeamMember::~PlayerTeamMember() = default;

runtime::Ref<PlayerTeamMember> PlayerTeamMember::create(gameobjects::player::Player& playerValue) {
	return runtime::makeRef<PlayerTeamMember>(playerValue);
}

int32_t PlayerTeamMember::getObjectId() {
	return player->getObjectId();
}

std::string PlayerTeamMember::getName() {
	return player->getName();
}

gameobjects::player::Player& PlayerTeamMember::getPlayer() const {
	return *player;
}

runtime::Ptr<gameobjects::AionObject> PlayerTeamMember::getObject() {
	return runtime::Ptr<gameobjects::AionObject>(player);
}

void PlayerTeamMember::updateLastOnlineTime() {
	lastOnlineTime.set(commons::utils::currentTimeMillis());
}

bool PlayerTeamMember::isOnline() {
	return player->isOnline();
}

float PlayerTeamMember::getX() {
	return player->getX();
}

float PlayerTeamMember::getY() {
	return player->getY();
}

float PlayerTeamMember::getZ() {
	return player->getZ();
}

int8_t PlayerTeamMember::getHeading() {
	return player->getHeading();
}

int8_t PlayerTeamMember::getLevel() {
	return player->getLevel();
}

} // namespace aion::gameserver::model::team
