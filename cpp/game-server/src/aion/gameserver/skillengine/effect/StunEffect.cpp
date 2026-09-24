#include "aion/gameserver/skillengine/effect/StunEffect.h"

#include <optional>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using gameserver::model::stats::container::StatEnum;
using runtime::Ptr;

void StunEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void StunEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, StatEnum::STUN_RESISTANCE, std::nullopt);
}

void StunEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	effected->getController().cancelCurrentSkill(effect.getEffector());
	if (Ptr<Player> player = runtime::as<Player>(effected)) {
		player->getFlyController().onStopGliding();
		player->getMoveController()->abortMove();
	}
	effect.getEffected()->getEffectController()->setAbnormal(AbnormalState::STUN);
	effect.setAbnormal(AbnormalState::STUN);
}

void StunEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::STUN);
}

} // namespace aion::gameserver::skillengine::effect
