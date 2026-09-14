#include "aion/gameserver/controllers/observer/StanceObserver.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"

namespace aion::gameserver::controllers::observer {

StanceObserver::StanceObserver(model::gameobjects::player::Player& playerValue, int32_t stanceSkillIdValue)
	: ActionObserver(ObserverType::ALL), player(playerValue), stanceSkillId(stanceSkillIdValue) {
}

StanceObserver::~StanceObserver() = default;

runtime::Ref<StanceObserver> StanceObserver::create(model::gameobjects::player::Player& playerValue, int32_t stanceSkillIdValue) {
	return runtime::makeRef<StanceObserver>(playerValue, stanceSkillIdValue);
}

void StanceObserver::startSkillCast(skillengine::model::Skill& skill) {
	AION_UNPORTED();
}

void StanceObserver::itemused(model::gameobjects::Item& item) {
	AION_UNPORTED();
}

void StanceObserver::abnormalsetted(skillengine::effect::AbnormalState state) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::controllers::observer
