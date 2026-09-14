#include "aion/gameserver/skillengine/effect/EffectTemplate.h"

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::effect {

// Java: LoggerFactory.getLogger(EffectTemplate.class) inside getPenetrationStat (hub-headers.md §11.3)
static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.skillengine.effect.EffectTemplate");

const modifier::ActionModifier* EffectTemplate::getActionModifiers(model::Effect& effect) const {
	AION_UNPORTED();
}

int32_t EffectTemplate::calculateBaseValue(model::Effect& effect) const {
	AION_UNPORTED();
}

int32_t EffectTemplate::calculateCritAddDmg(model::Effect& effect) const {
	AION_UNPORTED();
}

int32_t EffectTemplate::calculateCritProbMod(model::Effect& effect) const {
	AION_UNPORTED();
}

void EffectTemplate::calculate(model::Effect& effect) const {
	AION_UNPORTED();
}

bool EffectTemplate::calculate(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum,
	std::optional<model::SpellStatus> spellStatus) const {
	AION_UNPORTED();
}

bool EffectTemplate::calculate(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum,
	std::optional<model::SpellStatus> spellStatus, gameserver::model::SkillElement elementValue) const {
	AION_UNPORTED();
}

void EffectTemplate::resolveMagicalCritical(model::Effect& effect) const {
	AION_UNPORTED();
}

bool EffectTemplate::validateEffectConditions(model::Effect& effect) const {
	AION_UNPORTED();
}

bool EffectTemplate::validatePreEffects(model::Effect& effect) const {
	AION_UNPORTED();
}

bool EffectTemplate::isDodgedOrResisted(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum) const {
	AION_UNPORTED();
}

bool EffectTemplate::checkDodgeOrResistRate(model::Effect& effect) const {
	AION_UNPORTED();
}

void EffectTemplate::addSuccessEffect(model::Effect& effect, std::optional<model::SpellStatus> spellStatus) const {
	AION_UNPORTED();
}

void EffectTemplate::startEffect(model::Effect& effect) const {
	AION_UNPORTED();
}

void EffectTemplate::calculateDamage(model::Effect& effect) const {
	AION_UNPORTED();
}

void EffectTemplate::calculateSubEffect(model::Effect& effect) const {
	AION_UNPORTED();
}

bool EffectTemplate::effectSubConditionsCheck(model::Effect& effect) const {
	AION_UNPORTED();
}

int32_t EffectTemplate::calculateHate(model::Effect& effect) const {
	AION_UNPORTED();
}

void EffectTemplate::startSubEffect(model::Effect& effect) const {
	AION_UNPORTED();
}

void EffectTemplate::onPeriodicAction(model::Effect& effect) const {
	AION_UNPORTED();
}

void EffectTemplate::endEffect(model::Effect& effect) const {
	AION_UNPORTED();
}

bool EffectTemplate::checkEffectResistRate(model::Effect& effect, std::optional<gameserver::model::stats::container::StatEnum> statEnum) const {
	AION_UNPORTED();
}

bool EffectTemplate::isImmuneToAbnormal(model::Effect& effect, gameserver::model::stats::container::StatEnum statEnum) const {
	AION_UNPORTED();
}

bool EffectTemplate::isAlteredState(gameserver::model::stats::container::StatEnum stat) const {
	AION_UNPORTED();
}

bool EffectTemplate::isProtectedByShield(gameserver::model::gameobjects::Creature& effected, gameserver::model::stats::container::StatEnum stat) const {
	AION_UNPORTED();
}

std::optional<gameserver::model::stats::container::StatEnum> EffectTemplate::getPenetrationStat(gameserver::model::stats::container::StatEnum statEnum) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::effect
