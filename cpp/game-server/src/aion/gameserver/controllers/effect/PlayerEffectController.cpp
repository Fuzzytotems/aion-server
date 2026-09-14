#include "aion/gameserver/controllers/effect/PlayerEffectController.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/effect/CumulativeResist.h"
#include "aion/gameserver/controllers/effect/CumulativeResistType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::controllers::effect {

PlayerEffectController::PlayerEffectController(model::gameobjects::Creature& ownerValue) : EffectController(ownerValue) {
}

PlayerEffectController::~PlayerEffectController() = default;

void PlayerEffectController::addEffect(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

void PlayerEffectController::clearEffect(skillengine::model::Effect& effect, bool broadcast) {
	AION_UNPORTED();
}

model::gameobjects::player::Player& PlayerEffectController::getOwner() const {
	return static_cast<model::gameobjects::player::Player&>(EffectController::getOwner());
}

void PlayerEffectController::removeAllEffects(bool logout) {
	AION_UNPORTED();
}

void PlayerEffectController::removeNonStorableEffectsForLogout() {
	AION_UNPORTED();
}

void PlayerEffectController::updatePlayerIconsAndGroup(runtime::Ptr<skillengine::model::Effect> effect) {
	AION_UNPORTED();
}

void PlayerEffectController::updatePlayerEffectIcons(runtime::Ptr<skillengine::model::Effect> effect) {
	AION_UNPORTED();
}

bool PlayerEffectController::checkDuelCondition(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

void PlayerEffectController::addSavedEffect(int32_t skillId, int32_t skillLvl, int32_t remainingTime, int64_t endTime,
	const skillengine::model::Effect_ForceType* forceType, const std::unordered_set<int32_t>* magicalCriticalPositions) {
	AION_UNPORTED();
}

void PlayerEffectController::removeAllEffects() {
	AION_UNPORTED();
}

bool PlayerEffectController::canRemoveOnDie(skillengine::model::Effect& effect) {
	AION_UNPORTED();
}

int64_t PlayerEffectController::calculateAndApplyCumulativeResistDuration(CumulativeResistType type, int64_t duration) {
	AION_UNPORTED();
}

int32_t PlayerEffectController::getCumulativeResistance(CumulativeResistType type) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::effect
