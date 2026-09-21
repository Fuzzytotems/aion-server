#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/condition/Condition.h"
#include "aion/gameserver/skillengine/condition/Conditions.h"

namespace aion::gameserver::model::stats::calc::functions {

namespace {

/** Java: Conditions.validate(Stat2 stat, IStatFunction statFunction) (skillengine.condition, P5-02): every condition must hold */
bool validateConditions(const skillengine::condition::Conditions& conditions, Stat2& stat, IStatFunction& statFunction) {
	for (const std::unique_ptr<skillengine::condition::Condition>& condition : conditions.getConditions()) {
		if (!condition->validate(stat, statFunction)) {
			return false;
		}
	}
	return true;
}

} // namespace

StatFunction::StatFunction(container::StatEnum statValue, int32_t valueValue, bool bonusValue) {
	this->stat = statValue;
	this->value = valueValue;
	this->bonus = bonusValue;
}

runtime::Ptr<StatFunction> StatFunction::ofTemplate(const StatFunction* modifier) {
	// static data is immutable after loading; the non-const IStatFunction interface only reads it (class comment)
	return runtime::Ptr<StatFunction>(const_cast<StatFunction*>(modifier));
}

runtime::Ptr<StatOwner> StatFunction::getOwner() {
	return nullptr;
}

container::StatEnum StatFunction::getName() {
	return stat.value();
}

bool StatFunction::isBonus() const {
	return bonus;
}

int32_t StatFunction::getPriority() const {
	return 0x10;
}

int32_t StatFunction::getValue() {
	return value;
}

bool StatFunction::validate(Stat2& statValue) {
	return validate(statValue, *this);
}

bool StatFunction::validate(Stat2& statValue, IStatFunction& statFunction) {
	// Java: conditions == null || conditions.validate(stat, statFunction); C++ reads the owned or the shared Conditions (class comment)
	const skillengine::condition::Conditions* activeConditions = sharedConditions ? sharedConditions : conditions.get();
	return activeConditions == nullptr || validateConditions(*activeConditions, statValue, statFunction);
}

void StatFunction::apply(Stat2& /*stat*/, const std::unordered_set<utils::stats::CalculationType>& /*calculationTypes*/) {
	// Java: empty body
}

std::string StatFunction::toString() {
	AION_UNPORTED();
}

StatFunction& StatFunction::withConditions(const skillengine::condition::Conditions* conditionsValue) {
	sharedConditions = conditionsValue;
	return *this;
}

bool StatFunction::hasConditions() {
	return conditions != nullptr || sharedConditions != nullptr;
}

} // namespace aion::gameserver::model::stats::calc::functions
