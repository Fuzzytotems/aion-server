#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_set>

#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.xml.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::calc::functions {

/**
 * Java com.aionemu.gameserver.model.stats.calc.functions.StatRateFunction.
 * <p>
 * C++: static data modifier or, created at run time, `RcStatFunction<StatRateFunction>` (lifetimes: StatFunction.h).
 *
 * @author ATracer
 */
class StatRateFunction : public ::aion::gameserver::model::stats::calc::functions::StatFunction {
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.xml.inc"
public:
	/** Java `public StatRateFunction()` */
	StatRateFunction() = default;

	/** Java `public StatRateFunction(StatEnum name, int value, boolean bonus)` */
	StatRateFunction(container::StatEnum name, int32_t value, bool bonus);

	void apply(Stat2& stat, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	int32_t getPriority() override final;

	std::string toString() override;
};

} // namespace aion::gameserver::model::stats::calc::functions

namespace aion::gameserver::runtime {

/** Not a static template (StatFunction.h). */
template <>
struct IsStaticTemplate<model::stats::calc::functions::StatRateFunction> : std::false_type {};

} // namespace aion::gameserver::runtime
