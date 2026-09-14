#pragma once

#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.xml.h"

namespace aion::gameserver::model::stats::calc::functions {

/** Java com.aionemu.gameserver.model.stats.calc.functions.StatSetFunction. @author ATracer */
class StatSetFunction : public ::aion::gameserver::model::stats::calc::functions::StatFunction {
#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.xml.inc"
public:
};

} // namespace aion::gameserver::model::stats::calc::functions
