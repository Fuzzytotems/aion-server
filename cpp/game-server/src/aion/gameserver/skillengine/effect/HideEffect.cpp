#include "aion/gameserver/skillengine/effect/HideEffect.h"

#include <cstdint>
#include <string>

#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/observer/ActionObserver.h"
#include "aion/gameserver/controllers/observer/ObserverType.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/skillengine/effect/EffectType.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Skill.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"

namespace aion::gameserver::skillengine::effect {

using controllers::observer::ActionObserver;
using controllers::observer::ObserverType;
using gameserver::model::gameobjects::Creature;
using runtime::Ref;

namespace {

/** Java's implicit null check of a dereference of a static data pointer (a plain C++ dereference of nullptr is undefined) */
template <class T>
const T& nonNull(const T* value, const char* what) {
	if (value == nullptr)
		throw runtime::NullPointerException(std::string(what) + " is null");
	return *value;
}

/** Java's implicit null check of the nullable `state` attribute: Creature.setVisualState/unsetVisualState call visualState.getId() */
gameserver::model::gameobjects::state::CreatureVisualState stateOf(
	const std::optional<gameserver::model::gameobjects::state::CreatureVisualState>& state) {
	if (!state)
		throw runtime::NullPointerException("HideEffect.state is null");
	return *state;
}

} // namespace

/**
 * Java: the anonymous ActionObserver(ObserverType.STARTSKILLCAST) of HideEffect.startEffect for a player (HideEffect.java:79-95, fieldmap key
 * HideEffect$1), with its own field buffNumber. It reads the template's protected buffCount through HideEffect's friend declaration. Stored in the
 * effected player's ObserveController and, through the removal task of Effect.addObserver, in Effect.observerRemoveTasks; removeObservers (from
 * Effect.endEffect) removes it from both (cycles.toml "HideEffect$1#effect": java-hook). HideEffect$2..$5 below are held the same way.
 */
struct HideEffect_ActionObserver final : ActionObserver {
	AION_MAKE_REF_FRIEND

	runtime::Field<int32_t> buffNumber{0};
	const HideEffect* hideEffect;	 // captured this HideEffect this (line 92), immortal static data
	const Ref<model::Effect> effect; // captured param Effect effect (line 88)

	static Ref<HideEffect_ActionObserver> create(const HideEffect& hideEffect, model::Effect& effect) {
		return runtime::makeRef<HideEffect_ActionObserver>(hideEffect, effect);
	}

	/**
	 * C++ only: HideEffect$3 reads buffCount as well, and HideEffect.h befriends one observer struct only (header-requests.md, stage 1 part 3), so
	 * the other struct reads it through this one
	 */
	static int32_t buffCountOf(const HideEffect& hideEffect) { return hideEffect.buffCount; }

	void startSkillCast(model::Skill& skill) override {
		// TODO find better way
		if (skill.getSkillMethod() == model::Skill::SkillMethod::ITEM) {
			if (nonNull(skill.getItemTemplate(), "skill.getItemTemplate()").isPotion() || skill.getSkillTemplate()->getDuration() > 0)
				effect->endEffect();
			return;
		}
		bool isShapeChange =
			nonNull(skill.getSkillTemplate()->getEffects(), "skill.getSkillTemplate().getEffects()").hasAnyEffectType({EffectType::SHAPECHANGE});
		// Java: `++buffNumber >= buffCount`, evaluated only when the two operands before it are false
		if (isShapeChange || !skill.isSelfBuff() || incrementBuffNumber() >= hideEffect->buffCount)
			effect->endEffect();
	}

protected:
	HideEffect_ActionObserver(const HideEffect& hideEffectValue, model::Effect& effectValue)
		: ActionObserver(ObserverType::STARTSKILLCAST), hideEffect(&hideEffectValue), effect(Ref<model::Effect>(effectValue)) {}
	~HideEffect_ActionObserver() override = default;

private:
	/** Java `++buffNumber` on the observer's int (wraps); java-race: a plain read-modify-write, like Java's */
	int32_t incrementBuffNumber() {
		buffNumber.set(static_cast<int32_t>(static_cast<uint32_t>(buffNumber.get()) + 1u));
		return buffNumber.get();
	}
};

/** Java: the anonymous ActionObserver(ObserverType.ATTACK) of HideEffect.startEffect for a player (HideEffect.java:96-102, key HideEffect$2) */
struct HideEffect_ActionObserver_2 final : ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<model::Effect> effect; // captured param Effect effect (line 100)

	static Ref<HideEffect_ActionObserver_2> create(model::Effect& effect) { return runtime::makeRef<HideEffect_ActionObserver_2>(effect); }

	void attack(Creature& /*creature*/, int32_t /*skillId*/) override { effect->endEffect(); }

protected:
	explicit HideEffect_ActionObserver_2(model::Effect& effectValue)
		: ActionObserver(ObserverType::ATTACK), effect(Ref<model::Effect>(effectValue)) {}
	~HideEffect_ActionObserver_2() override = default;
};

/** Java: the anonymous ActionObserver(ObserverType.ITEMUSE) of HideEffect.startEffect for a player (HideEffect.java:103-114, key HideEffect$3) */
struct HideEffect_ActionObserver_3 final : ActionObserver {
	AION_MAKE_REF_FRIEND

	const HideEffect* hideEffect;	 // captured this HideEffect this (line 110), immortal static data
	const Ref<model::Effect> effect; // captured param Effect effect (line 111)

	static Ref<HideEffect_ActionObserver_3> create(const HideEffect& hideEffect, model::Effect& effect) {
		return runtime::makeRef<HideEffect_ActionObserver_3>(hideEffect, effect);
	}

	void itemused(gameserver::model::gameobjects::Item& item) override {
		// [4.5] Buff items do not affect Hide II. Hide I is cancelled.
		const gameserver::model::templates::item::actions::ItemActions* actions = item.getItemTemplate()->getActions();
		if (actions != nullptr) {
			if (HideEffect_ActionObserver::buffCountOf(*hideEffect) == 0 || actions->getSkillUseAction() == nullptr)
				effect->endEffect();
		}
	}

protected:
	HideEffect_ActionObserver_3(const HideEffect& hideEffectValue, model::Effect& effectValue)
		: ActionObserver(ObserverType::ITEMUSE), hideEffect(&hideEffectValue), effect(Ref<model::Effect>(effectValue)) {}
	~HideEffect_ActionObserver_3() override = default;
};

/** Java: the anonymous ActionObserver(ObserverType.ATTACK) of HideEffect.startEffect for an npc (HideEffect.java:124-131, key HideEffect$4) */
struct HideEffect_ActionObserver_4 final : ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<model::Effect> effect; // captured param Effect effect (line 128)

	static Ref<HideEffect_ActionObserver_4> create(model::Effect& effect) { return runtime::makeRef<HideEffect_ActionObserver_4>(effect); }

	void attack(Creature& /*creature*/, int32_t /*skillId*/) override { effect->endEffect(); }

protected:
	explicit HideEffect_ActionObserver_4(model::Effect& effectValue)
		: ActionObserver(ObserverType::ATTACK), effect(Ref<model::Effect>(effectValue)) {}
	~HideEffect_ActionObserver_4() override = default;
};

/**
 * Java: the anonymous ActionObserver(ObserverType.STARTSKILLCAST) of HideEffect.startEffect for an npc (HideEffect.java:134-141, key
 * HideEffect$5)
 */
struct HideEffect_ActionObserver_5 final : ActionObserver {
	AION_MAKE_REF_FRIEND

	const Ref<model::Effect> effect; // captured param Effect effect (line 138)

	static Ref<HideEffect_ActionObserver_5> create(model::Effect& effect) { return runtime::makeRef<HideEffect_ActionObserver_5>(effect); }

	void startSkillCast(model::Skill& /*skill*/) override { effect->endEffect(); }

protected:
	explicit HideEffect_ActionObserver_5(model::Effect& effectValue)
		: ActionObserver(ObserverType::STARTSKILLCAST), effect(Ref<model::Effect>(effectValue)) {}
	~HideEffect_ActionObserver_5() override = default;
};

void HideEffect::applyEffect(model::Effect& effect) const {
	effect.addToEffectedController();
}

void HideEffect::endEffect(model::Effect& effect) const {
	BufEffect::endEffect(effect);

	runtime::Ptr<Creature> effected = effect.getEffected();
	effected->unsetVisualState(stateOf(state));
	effected->getEffectController()->unsetAbnormal(AbnormalState::HIDE);
	effected->getController().onHideEnd();
	utils::PacketSendUtility::broadcastPacketAndReceive(*effected, network::aion::serverpackets::SM_PLAYER_STATE(*effected)); // update visibility
}

// Anonymous classes com.aionemu.gameserver.skillengine.effect.HideEffect$1 to $5: the callback structs HideEffect_ActionObserver (_2 to _5) above.
// Stored lambda com.aionemu.gameserver.skillengine.effect.HideEffect@L69:44 (the delayed removeTargetFrom, pin {&effected}): a one-shot task that
// releases its pin when it has run 500 ms later; nothing stores its Future (fieldmap: storage task, no cycle edge)
void HideEffect::startEffect(model::Effect& effect) const {
	BufEffect::startEffect(effect);

	Creature& effected = *effect.getEffected();
	effected.getEffectController()->setAbnormal(AbnormalState::HIDE);
	effect.setAbnormal(AbnormalState::HIDE);

	effected.setVisualState(stateOf(state));

	// Cancel targeted enemy cast
	controllers::attack::AttackUtil::cancelCastOn(effected);

	// send all to set new 'effected' visual state (remove all visual targetting from 'effected')
	utils::PacketSendUtility::broadcastPacketAndReceive(effected, network::aion::serverpackets::SM_PLAYER_STATE(effected));

	utils::ThreadPoolManager::getInstance().schedule({&effected}, [&effected] {
		// do on all who targetting on 'effected' (set target null, cancel attack skill, cancel npc pursuit)
		controllers::attack::AttackUtil::removeTargetFrom(effected, true);
	}, 500);

	effected.getController().onHide();
	// for player adding: Remove Hide when using any item action . when requesting dialog to any npc . when being attacked . when attacking
	if (runtime::as<gameserver::model::gameobjects::player::Player>(effected)) {

		// Remove Hide when use skill / item skill
		effect.addObserver(effected, *HideEffect_ActionObserver::create(*this, effect));
		effect.addObserver(effected, *HideEffect_ActionObserver_2::create(effect));
		effect.addObserver(effected, *HideEffect_ActionObserver_3::create(*this, effect));

		// type >= 1, hide is maintained even after damage
		if (type == 0)
			effect.setCancelOnDmg(true);
	} else { // effected is npc
		if (type == 0) { // type >= 1, hide is maintained even after damage
			effect.setCancelOnDmg(true);

			// Remove Hide when attacking
			effect.addObserver(effected, *HideEffect_ActionObserver_4::create(effect));

			// Remove Hide when use skill
			effect.addObserver(effected, *HideEffect_ActionObserver_5::create(effect));
		}
	}
}

} // namespace aion::gameserver::skillengine::effect
