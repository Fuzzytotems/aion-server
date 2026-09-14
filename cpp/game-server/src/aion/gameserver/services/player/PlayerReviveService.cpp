#include "aion/gameserver/services/player/PlayerReviveService.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::services::player {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.player.PlayerReviveService@L251:92

void PlayerReviveService::duelRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerReviveService::skillRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerReviveService::rebirthRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerReviveService::bindRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerReviveService::bindRevive(model::gameobjects::player::Player& player, int32_t skillId) {
	AION_UNPORTED();
}

void PlayerReviveService::kiskRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerReviveService::kiskRevive(model::gameobjects::player::Player& player, int32_t skillId) {
	AION_UNPORTED();
}

void PlayerReviveService::instanceRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerReviveService::instanceRevive(model::gameobjects::player::Player& player, int32_t skillId) {
	AION_UNPORTED();
}

void PlayerReviveService::revive(model::gameobjects::player::Player& player, int32_t hpPercent, int32_t mpPercent, bool setSoulSickness,
	int32_t resurrectionSkill) {
	AION_UNPORTED();
}

void PlayerReviveService::itemSelfRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerReviveService::scheduleReviveAtBase(model::gameobjects::player::Player& player, int32_t delayMillis, int32_t skillId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::player
