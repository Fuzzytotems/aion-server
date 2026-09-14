#include "aion/gameserver/model/stats/calc/functions/StatFunctionProxy.h"

#include "aion/gameserver/model/stats/calc/StatOwner.h"
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
	AION_UNPORTED();
}

bool StatFunctionProxy::isBonus() {
	AION_UNPORTED();
}

int32_t StatFunctionProxy::getPriority() {
	AION_UNPORTED();
}

int32_t StatFunctionProxy::getValue() {
	AION_UNPORTED();
}

bool StatFunctionProxy::validate(Stat2& stat) {
	AION_UNPORTED();
}

void StatFunctionProxy::apply(Stat2& stat, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) {
	AION_UNPORTED();
}

bool StatFunctionProxy::hasConditions() {
	AION_UNPORTED();
}

std::string StatFunctionProxy::toString() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::stats::calc::functions
