#include "aion/gameserver/skillengine/effect/SpinEffect.h"

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
#include "aion/gameserver/skillengine/model/SpellStatus.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

void SpinEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void SpinEffect::calculate(model::Effect& effect) const {
	if (effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::PULLED)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::SPIN)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::OPENAERIAL)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::STAGGER)
		|| effect.getEffected()->getEffectController()->isAbnormalSet(AbnormalState::STUMBLE))
		return;
	EffectTemplate::calculate(effect, gameserver::model::stats::container::StatEnum::SPIN_RESISTANCE, model::SpellStatus::SPIN);
}

void SpinEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	effected->getController().cancelCurrentSkill(effect.getEffector());
	if (Ptr<Player> player = runtime::as<Player>(effected)) {
		player->getFlyController().onStopGliding();
		player->getMoveController()->abortMove();
	}
	effect.getEffected()->getEffectController()->removeParalyzeEffects();
	effected->getEffectController()->setAbnormal(AbnormalState::SPIN);
	effect.setAbnormal(AbnormalState::SPIN);
}

void SpinEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::SPIN);
}

} // namespace aion::gameserver::skillengine::effect
