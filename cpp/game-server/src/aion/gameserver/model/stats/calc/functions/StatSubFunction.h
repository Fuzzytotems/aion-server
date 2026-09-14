#pragma once

#include "aion/gameserver/model/stats/calc/functions/StatSubFunction.xml.h"

namespace aion::gameserver::model::stats::calc::functions {

/** Java com.aionemu.gameserver.model.stats.calc.functions.StatSubFunction. @author ATracer */
class StatSubFunction : public ::aion::gameserver::model::stats::calc::functions::StatFunction {
#include "aion/gameserver/model/stats/calc/functions/StatSubFunction.xml.inc"
public:
};

} // namespace aion::gameserver::model::stats::calc::functions
