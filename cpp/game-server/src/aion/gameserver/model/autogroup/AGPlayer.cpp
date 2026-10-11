#include "aion/gameserver/model/autogroup/AGPlayer.h"

#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::model::autogroup {

AGPlayer::AGPlayer(int32_t objectId, Race race, PlayerClass playerClass, std::string_view name)
	: objectId_(objectId), race_(race), playerClass_(playerClass), name_(name) {
}

runtime::Ref<AGPlayer> AGPlayer::create(int32_t objectId, Race race, PlayerClass playerClass, std::string_view name) {
	return runtime::makeRef<AGPlayer>(objectId, race, playerClass, name);
}

// Java AGPlayer.java:11-13
AGPlayer::AGPlayer(gameobjects::player::Player& player)
	: AGPlayer(player.getObjectId(), player.getRace(), player.getPlayerClass(), player.getName()) {
}

runtime::Ref<AGPlayer> AGPlayer::create(gameobjects::player::Player& player) {
	return runtime::makeRef<AGPlayer>(player);
}

AGPlayer::~AGPlayer() = default;

} // namespace aion::gameserver::model::autogroup
