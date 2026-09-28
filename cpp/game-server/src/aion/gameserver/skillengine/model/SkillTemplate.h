#pragma once

#include "aion/gameserver/skillengine/model/SkillTemplate.xml.h"

#include <cstdint>
#include <initializer_list>

#include "aion/gameserver/model/templates/L10n.h"
#include "aion/gameserver/skillengine/condition/fwd.h"
#include "aion/gameserver/skillengine/effect/fwd.h"

namespace aion::gameserver::skillengine::model {

/**
 * Java com.aionemu.gameserver.skillengine.model.SkillTemplate: the static data of one skill.
 * <p>
 * Hub header (docs/design/hub-headers.md): the xmlgen behaviour shell (static-data.md §2.1), extended in place. The generated member block
 * SkillTemplate.xml.inc holds the JAXB fields and the trivial getters (`getTargetSlot()` and `getCounterSkill()` return `std::optional` for Java's
 * nullable enums); the hand-written methods follow in Java order. Static data is immutable after load and referenced as `const SkillTemplate*`,
 * so every method is const.
 * - Java `implements L10n`: the second base `model::templates::L10n` (its methods are const for static data), so
 *   `dynamic_cast<const L10n*>` ports `owner instanceof L10n` (admincommands.Stat, ChatCommand); getL10n() is the inherited default.
 * - Varargs `EffectType... effectTypes` are `std::initializer_list<effect::EffectType>` (`hasAnyEffect({EffectType::HIDE})`).
 *
 * @author ATracer, Wakizashi
 */
class SkillTemplate : public ::aion::gameserver::runtime::StaticTemplate, public ::aion::gameserver::model::templates::L10n {
#include "aion/gameserver/skillengine/model/SkillTemplate.xml.inc"
public:
	int32_t getL10nId() const override { return nameId; }

	bool isPassive() const;

	bool isToggle() const;

	bool isProvoked() const;

	bool isMaintain() const;

	bool isActive() const;

	bool isCharge() const;

	/** @return the effect template at the 1-based position, nullptr if there is none */
	const effect::EffectTemplate* getEffectTemplate(int32_t position) const;

	bool hasAnyEffect(std::initializer_list<effect::EffectType> effectTypes) const;

	bool hasAnyEffect(bool checkSubEffects, std::initializer_list<effect::EffectType> effectTypes) const;

	/** @return resurrectbase is excluded because of different behavior */
	bool hasResurrectEffect() const;

	bool hasEvadeEffect() const;

	bool hasRecallInstant() const;

	int32_t getCooldownId() const;

	bool isMultiCast() const;

	const condition::ChainCondition* getChainCondition() const;

	const condition::RideRobotCondition* getRideRobotCondition() const;

	const condition::SkillChargeCondition* getSkillChargeCondition() const;

	const condition::HpCondition* getHpCondition() const;

	const condition::PlayerMovedCondition* getMovedCondition() const;
};

} // namespace aion::gameserver::skillengine::model
