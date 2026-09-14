#pragma once

#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.xml.h"

namespace aion::gameserver::model::stats::calc::functions {

/** Java com.aionemu.gameserver.model.stats.calc.functions.StatRateFunction. @author ATracer */
class StatRateFunction : public ::aion::gameserver::model::stats::calc::functions::StatFunction {
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.xml.inc"
public:
};

} // namespace aion::gameserver::model::stats::calc::functions
