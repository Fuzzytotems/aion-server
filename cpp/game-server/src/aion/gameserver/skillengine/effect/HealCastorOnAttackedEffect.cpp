#include "aion/gameserver/skillengine/effect/HealCastorOnAttackedEffect.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/team/TemporaryPlayerTeam.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PositionUtil.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::player::Player;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using runtime::Ptr;
using runtime::Ref;

/**
 * Java: the anonymous ActionObserver(ObserverType.ATTACKED) of HealCastorOnAttackedEffect.startEffect (HealCastorOnAttackedEffect.java:35-53,
 * fieldmap key HealCastorOnAttackedEffect$1): each hit on the effected heals the effector, or the members of its group in range of the
 * effected. Stored in the effected creature's ObserveController and the effect's observerRemoveTasks (Effect.addObserver); Effect.endEffect ->
 * removeObservers removes it. It captures the template (immutable static data: its protected range and calculateBaseValue, through the friend
 * line of HealCastorOnAttackedEffect.h) and the effect.
 */
struct HealCastorOnAttackedEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const HealCastorOnAttackedEffect* healEffect; // captured this (immutable static data)
	const Ref<model::Effect> effect;              // captured final Effect effect

	static Ref<HealCastorOnAttackedEffect_ActionObserver> create(const HealCastorOnAttackedEffect& healEffect, model::Effect& effect) {
		return runtime::makeRef<HealCastorOnAttackedEffect_ActionObserver>(healEffect, effect);
	}

	void attacked(Creature& /*creature*/, int32_t /*skillId*/) override {
		const Ptr<Creature> effector = effect->getEffector();
		const Ptr<Player> player = runtime::as<Player>(effector);
		const Ptr<gameserver::model::team::TemporaryPlayerTeam> group = player ? player->getCurrentGroup() : nullptr;
		int32_t healValue = healEffect->calculateBaseValue(*effect);
		if (group == nullptr) {
			if (utils::PositionUtil::isInRange(*effect->getEffected(), *effector, healEffect->range, false))
				effector->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::HP, healValue, *effect, SM_ATTACK_STATUS_LOG::REGULAR);
		} else {
			for (const Ptr<Player>& p : group->getOnlineMembers()) {
				if (utils::PositionUtil::isInRange(*effect->getEffected(), *p, healEffect->range, false))
					p->getLifeStats()->increaseHp(SM_ATTACK_STATUS_TYPE::HP, healValue, *effect, SM_ATTACK_STATUS_LOG::REGULAR);
			}
		}
	}

protected:
	HealCastorOnAttackedEffect_ActionObserver(const HealCastorOnAttackedEffect& healEffectValue, model::Effect& effectValue)
		: ActionObserver(controllers::observer::ObserverType::ATTACKED), healEffect(&healEffectValue), effect(Ref<model::Effect>(effectValue)) {}
	~HealCastorOnAttackedEffect_ActionObserver() override = default;
};

void HealCastorOnAttackedEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.HealCastorOnAttackedEffect$1: the struct above
void HealCastorOnAttackedEffect::startEffect(model::Effect& effect) const {
	effect.addObserver(*effect.getEffected(), *HealCastorOnAttackedEffect_ActionObserver::create(*this, effect));
}

} // namespace aion::gameserver::skillengine::effect
