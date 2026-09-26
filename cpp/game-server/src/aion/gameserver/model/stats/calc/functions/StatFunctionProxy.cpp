#include "aion/gameserver/model/stats/calc/functions/StatFunctionProxy.h"

#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::stats::calc::functions {

StatFunctionProxy::StatFunctionProxy(runtime::Ptr<StatOwner> ownerValue, IStatFunction& statFunction)
	: owner(ownerValue), proxiedFunction(statFunction) {
}

StatFunctionProxy::~StatFunctionProxy() = default;

runtime::Ref<StatFunctionProxy> StatFunctionProxy::create(runtime::Ptr<StatOwner> ownerValue, IStatFunction& statFunction) {
	return runtime::makeRef<StatFunctionProxy>(ownerValue, statFunction);
}

container::StatEnum StatFunctionProxy::getName() {
	return proxiedFunction->getName();
}

bool StatFunctionProxy::isBonus() const {
	return proxiedFunction->isBonus();
}

int32_t StatFunctionProxy::getPriority() const {
	return proxiedFunction->getPriority();
}

int32_t StatFunctionProxy::getValue() {
	return proxiedFunction->getValue();
}

bool StatFunctionProxy::validate(Stat2& stat) {
	// Java: ((StatFunction) proxiedFunction).validate(stat, this) - a ClassCastException for any other function type
	StatFunction* function = dynamic_cast<StatFunction*>(&*proxiedFunction);
	if (!function)
		throw runtime::ClassCastException("StatFunctionProxy: the proxied function is no StatFunction");
	return function->validate(stat, *this);
}

void StatFunctionProxy::apply(Stat2& stat, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	proxiedFunction->apply(stat, calculationTypes);
}

bool StatFunctionProxy::hasConditions() {
	return proxiedFunction->hasConditions();
}

std::string StatFunctionProxy::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::calc::functions
