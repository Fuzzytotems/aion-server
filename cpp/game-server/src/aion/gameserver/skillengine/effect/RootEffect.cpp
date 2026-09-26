#include "aion/gameserver/skillengine/effect/RootEffect.h"

#include <cstdint>
#include <optional>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/PlayerMoveController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
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
using runtime::Ref;

/**
 * Java: the anonymous ActionObserver(ObserverType.ATTACKED) of RootEffect.startEffect (RootEffect.java:47-54, fieldmap key RootEffect$1). Stored
 * in the effected creature's ObserveController and, through the removal task of Effect.addObserver, in the effect's observerRemoveTasks;
 * Effect.endEffect -> removeObservers removes it from both (cycles.toml "RootEffect$1#effect" and "RootEffect$1#effected": java-hook). The
 * template is immortal static data (fieldmap: captured this: final template reference); the observer reads its protected resistchance, as Java's
 * inner class does (the friend declaration of RootEffect.h).
 */
struct RootEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const RootEffect* rootEffect; // captured this RootEffect this (line 51)
	const Ref<model::Effect> effect; // captured param Effect effect (line 52)
	const Ref<Creature> effected; // captured local Creature effected (line 52)

	static Ref<RootEffect_ActionObserver> create(const RootEffect& rootEffect, model::Effect& effect, Creature& effected) {
		return runtime::makeRef<RootEffect_ActionObserver>(rootEffect, effect, effected);
	}

	void attacked(Creature& /*creature*/, int32_t /*skillId*/) override {
		if (commons::utils::Rnd::chance() >= static_cast<float>(rootEffect->resistchance))
			effected->getEffectController()->removeEffect(effect->getSkillId());
	}

protected:
	RootEffect_ActionObserver(const RootEffect& rootEffectValue, model::Effect& effectValue, Creature& effectedValue)
		: ActionObserver(controllers::observer::ObserverType::ATTACKED), rootEffect(&rootEffectValue), effect(Ref<model::Effect>(effectValue)),
		  effected(Ref<Creature>(effectedValue)) {}
	~RootEffect_ActionObserver() override = default;
};

void RootEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void RootEffect::calculate(model::Effect& effect) const {
	EffectTemplate::calculate(effect, StatEnum::ROOT_RESISTANCE, std::nullopt);
}

void RootEffect::startEffect(model::Effect& effect) const {
	const Ptr<Creature> effected = effect.getEffected();
	effected->getEffectController()->setAbnormal(AbnormalState::ROOT);
	effect.setAbnormal(AbnormalState::ROOT);
	// PacketSendUtility.broadcastPacketAndReceive(effected, new SM_POSITION(effected));
	if (Ptr<Player> player = runtime::as<Player>(effected)) {
		player->getFlyController().onStopGliding();
		player->getMoveController()->abortMove();
	}

	Ref<RootEffect_ActionObserver> observer = RootEffect_ActionObserver::create(*this, effect, *effected);
	effect.addObserver(*effected, *observer);
}

void RootEffect::endEffect(model::Effect& effect) const {
	effect.getEffected()->getEffectController()->unsetAbnormal(AbnormalState::ROOT);
}

} // namespace aion::gameserver::skillengine::effect
