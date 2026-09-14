#pragma once

#include "aion/gameserver/model/stats/calc/functions/StatFunction.xml.h"

namespace aion::gameserver::model::stats::calc::functions {

/** Java com.aionemu.gameserver.model.stats.calc.functions.StatFunction. @author ATracer */
class StatFunction : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/stats/calc/functions/StatFunction.xml.inc"
public:
};

} // namespace aion::gameserver::model::stats::calc::functions
