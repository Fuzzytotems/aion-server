#pragma once

#include "aion/gameserver/model/stats/calc/functions/StatAbsFunction.xml.h"

namespace aion::gameserver::model::stats::calc::functions {

/** Java com.aionemu.gameserver.model.stats.calc.functions.StatAbsFunction. @author kecimis */
class StatAbsFunction : public ::aion::gameserver::model::stats::calc::functions::StatFunction {
#include "aion/gameserver/model/stats/calc/functions/StatAbsFunction.xml.inc"
public:
};

} // namespace aion::gameserver::model::stats::calc::functions
