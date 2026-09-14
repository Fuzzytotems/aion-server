#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::calc::functions {

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

bool StatFunction::isBonus() {
	return bonus;
}

int32_t StatFunction::getPriority() {
	return 0x10;
}

int32_t StatFunction::getValue() {
	return value;
}

bool StatFunction::validate(Stat2& statValue) {
	AION_UNPORTED();
}

bool StatFunction::validate(Stat2& statValue, IStatFunction& statFunction) {
	AION_UNPORTED();
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
