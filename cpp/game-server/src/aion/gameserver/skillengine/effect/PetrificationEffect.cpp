#include "aion/gameserver/skillengine/effect/PetrificationEffect.h"

#include <optional>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/CreatureMoveController.h"
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

void PetrificationEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void PetrificationEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::PERIFICATION_RESISTANCE, std::nullopt);
}

void PetrificationEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	effected->getMoveController()->abortMove();
	effected->getController().cancelCurrentSkill(effect.getEffector());
	// removes glide
	if (Ptr<Player> player = runtime::as<Player>(effected); player && player->isInGlidingState()) {
		player->getFlyController().onStopGliding();
	}
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::PETRIFICATION);
	effect.setAbnormal(AbnormalState::PETRIFICATION);
}

void PetrificationEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::PETRIFICATION);
}

} // namespace aion::gameserver::skillengine::effect
