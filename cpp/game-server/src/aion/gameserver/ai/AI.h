#pragma once

#include <any>
#include <cstdint>
#include <initializer_list>
#include <string>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/ai/event/fwd.h"
#include "aion/gameserver/ai/fwd.h"
#include "aion/gameserver/ai/poll/fwd.h"
#include "aion/gameserver/model/animations/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/skillengine/model/fwd.h"

namespace aion::gameserver::ai {

/**
 * The AI of a creature: receives its events and answers questions about its behaviour.
 * <p>
 * Hub header (docs/design/hub-headers.md §9.2): an interface without data members, implemented only by AbstractAI.
 * `Object... args` of onCustomEvent is `std::initializer_list<std::any>` (§7.4): objects are passed as `runtime::Ref<C>` of the class the
 * receiving AI casts to (e.g. `Ref<Npc>`), numbers as the C++ primitive, strings as std::string; `= {}` keeps Java's call without varargs.
 *
 * @author ATracer
 */
class AI {
public:
	virtual void onCreatureEvent(event::AIEventType event, model::gameobjects::Creature& creature) = 0;

	virtual void onCustomEvent(int32_t eventId, std::initializer_list<std::any> args = {}) = 0;

	virtual void onGeneralEvent(event::AIEventType event) = 0;

	/**
	 * If already handled dialog return true.
	 */
	virtual bool onDialogSelect(model::gameobjects::player::Player& player, int32_t dialogActionId, int32_t questId, int32_t extendedRewardIndex) = 0;

	virtual void think() = 0;

	virtual bool canThink() = 0;

	virtual AIState getState() = 0;

	virtual AISubState getSubState() = 0;

	virtual std::string getName() = 0;

	/**
	 * Ask AI instance for the answer to the specified question.
	 *
	 * @param question
	 * @return The answer, true or false.
	 */
	virtual bool ask(poll::AIQuestion question) = 0;

	virtual bool isLogging() = 0;

	/**
	 * @param attacker
	 * @param damage
	 *          - The calculated damage from given attacker
	 * @param effect
	 *          - The effect which caused the damage (may be null)
	 * @return The effectively received damage
	 */
	virtual float modifyDamage(model::gameobjects::Creature& attacker, float damage, runtime::Ptr<skillengine::model::Effect> effect) = 0;

	/**
	 * @param damage
	 *          - The calculated damage output of this creature
	 * @param effected
	 * @param effect
	 * @return The effective damage output of this creature
	 */
	virtual float modifyOwnerDamage(float damage, model::gameobjects::Creature& effected, runtime::Ptr<skillengine::model::Effect> effect) = 0;

	/**
	 * Used to manipulate any game stat of the owner.
	 *
	 * @param stat
	 */
	virtual void modifyOwnerStat(model::stats::calc::Stat2& stat) = 0;

	virtual model::templates::item::ItemAttackType modifyAttackType(model::templates::item::ItemAttackType type) = 0;

	virtual int32_t modifyAggroRange(int32_t value) = 0;

	virtual int32_t modifyAggroAngle(int32_t value) = 0;

	virtual void onStartUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) = 0;

	virtual void onEndUseSkill(const skillengine::model::SkillTemplate* skillTemplate, int32_t skillLevel) = 0;

	virtual void onEffectApplied(skillengine::model::Effect& effect) = 0;

	virtual void onEffectEnd(runtime::Ptr<skillengine::model::Effect> effect) = 0;

	virtual model::animations::AttackHandAnimation modifyAttackHandAnimation(model::animations::AttackHandAnimation attackHandAnimation) = 0;

	virtual model::animations::AttackTypeAnimation getAttackTypeAnimation(model::gameobjects::Creature& target) = 0;

	virtual int32_t modifyInitialSkillDelay(int32_t delay) = 0;

	virtual bool isDestinationReached() = 0;

	virtual ~AI() = default;

protected:
	AI() = default;
	AI(const AI&) = default;
	AI& operator=(const AI&) = default;
};

} // namespace aion::gameserver::ai
