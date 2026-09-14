#pragma once

#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.xml.h"

namespace aion::gameserver::model::stats::calc::functions {

/** Java com.aionemu.gameserver.model.stats.calc.functions.StatAddFunction. @author ATracer */
class StatAddFunction : public ::aion::gameserver::model::stats::calc::functions::StatFunction {
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.xml.inc"
public:
};

} // namespace aion::gameserver::model::stats::calc::functions
