#include "aion/gameserver/skillengine/effect/MagicCounterAtkEffect.h"

#include <algorithm>
#include <cstdint>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/templates/detail/JavaCasts.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_LOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS_TYPE.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::Creature;
using network::aion::serverpackets::SM_ATTACK_STATUS_LOG;
using network::aion::serverpackets::SM_ATTACK_STATUS_TYPE;
using runtime::Ref;

namespace {

/** Java int a * b (wraps on overflow) */
constexpr int32_t mulInt(int32_t a, int32_t b) noexcept {
	return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

} // namespace

/**
 * Java: the anonymous ActionObserver(ObserverType.ENDSKILLCAST) of MagicCounterAtkEffect.startEffect (MagicCounterAtkEffect.java:36-49, fieldmap
 * key MagicCounterAtkEffect$1): every magical skill the effected finishes casting (not an item's) hits it for a percent of its base max HP,
 * capped by maxdmg. Stored in the effected creature's ObserveController and the effect's observerRemoveTasks (Effect.addObserver);
 * Effect.endEffect -> removeObservers removes it. It captures the template (immutable static data: its protected maxdmg, element, hopType and
 * calculateBaseValue, through the friend line of MagicCounterAtkEffect.h), the effect and the effected.
 */
struct MagicCounterAtkEffect_ActionObserver final : controllers::observer::ActionObserver {
	AION_MAKE_REF_FRIEND

	const MagicCounterAtkEffect* magicCounterAtkEffect; // captured this (immutable static data)
	const Ref<model::Effect> effect;                    // captured final Effect effect
	const Ref<Creature> effected;                       // captured local Creature effected

	static Ref<MagicCounterAtkEffect_ActionObserver> create(const MagicCounterAtkEffect& magicCounterAtkEffect, model::Effect& effect,
		Creature& effected) {
		return runtime::makeRef<MagicCounterAtkEffect_ActionObserver>(magicCounterAtkEffect, effect, effected);
	}

	void endSkillCast(model::Skill& skill) override {
		if (skill.getSkillMethod() != model::Skill::SkillMethod::ITEM && skill.getSkillTemplate()->getType() == model::SkillType::MAGICAL) {
			// Java: an int product (wrapping), divided as a float
			float maxHpDamage =
				static_cast<float>(mulInt(effected->getGameStats()->getMaxHp()->getBase(), magicCounterAtkEffect->calculateBaseValue(*effect))) / 100.0f;
			float adjustedDamage = utils::stats::StatFunctions::adjustDamageByPvpOrPveModifiers(*effect->getEffector(), *effect->getEffected(),
				maxHpDamage, effect->getSkillTemplate()->getPvpDamage(), false, magicCounterAtkEffect->element);
			// Java (int) Math.min(maxdmg, adjustedDamage): a float min, cast to int
			int32_t finalDamage =
				gameserver::model::templates::detail::floatToInt(std::min(static_cast<float>(magicCounterAtkEffect->maxdmg), adjustedDamage));
			// Java passes the nullable hopType field; the C++ overload takes a HopType (SpellAttackEffect.cpp's note): its only reader,
			// AggroList.addDamage, asks `hopType == HopType.DAMAGE`, which SKILLLV answers as Java's null does
			effected->getController().onAttack(*effect, SM_ATTACK_STATUS_TYPE::MAGICCOUNTERATK, finalDamage, true, SM_ATTACK_STATUS_LOG::MAGICCOUNTERATK,
				magicCounterAtkEffect->hopType.value_or(model::HopType::SKILLLV));
		}
	}

protected:
	MagicCounterAtkEffect_ActionObserver(const MagicCounterAtkEffect& magicCounterAtkEffectValue, model::Effect& effectValue, Creature& effectedValue)
		: ActionObserver(controllers::observer::ObserverType::ENDSKILLCAST), magicCounterAtkEffect(&magicCounterAtkEffectValue),
		  effect(Ref<model::Effect>(effectValue)), effected(Ref<Creature>(effectedValue)) {}
	~MagicCounterAtkEffect_ActionObserver() override = default;
};

void MagicCounterAtkEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

// Anonymous class com.aionemu.gameserver.skillengine.effect.MagicCounterAtkEffect$1: the struct above
void MagicCounterAtkEffect::startEffect(model::Effect& effect) const {
	runtime::Ptr<Creature> effected = effect.getEffected();
	effect.addObserver(*effected, *MagicCounterAtkEffect_ActionObserver::create(*this, effect, *effected));
}

} // namespace aion::gameserver::skillengine::effect
