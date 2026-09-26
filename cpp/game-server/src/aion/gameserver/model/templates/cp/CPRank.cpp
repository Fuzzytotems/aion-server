#include "aion/gameserver/model/templates/cp/CPRank.h"

#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/templates/stats/ModifiersTemplate.h"

namespace aion::gameserver::model::templates::cp {

const std::vector<std::unique_ptr<::aion::gameserver::model::stats::calc::functions::StatFunction>>& CPRank::getStatModifiers() const {
	static const std::vector<std::unique_ptr<::aion::gameserver::model::stats::calc::functions::StatFunction>> empty;
	return statModifiers == nullptr ? empty : statModifiers->getModifiers();
}

} // namespace aion::gameserver::model::templates::cp
