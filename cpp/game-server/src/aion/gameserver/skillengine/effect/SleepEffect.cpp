#include "aion/gameserver/skillengine/effect/SleepEffect.h"

#include <optional>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

void SleepEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void SleepEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::SLEEP_RESISTANCE, std::nullopt);
}

void SleepEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	effected->getController().cancelCurrentSkill(effect.getEffector());
	if (Ptr<Player> player = runtime::as<Player>(effected)) {
		player->getFlyController().onStopGliding();
		player->getMoveController()->abortMove();
	}
	effect.setAbnormal(AbnormalState::SLEEP);
	effected->getEffectController()->setAbnormal(AbnormalState::SLEEP);
	effect.setCancelOnDmg(true);
}

void SleepEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::SLEEP);
}

} // namespace aion::gameserver::skillengine::effect
