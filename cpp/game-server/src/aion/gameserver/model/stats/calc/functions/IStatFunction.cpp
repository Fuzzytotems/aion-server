#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"

namespace aion::gameserver::model::stats::calc::functions {

int32_t IStatFunction::compareTo(const IStatFunction& o) const {
	return getPriority() - o.getPriority();
}

} // namespace aion::gameserver::model::stats::calc::functions
