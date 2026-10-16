#pragma once

#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_set>

#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.xml.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::calc::functions {

/**
 * Java com.aionemu.gameserver.model.stats.calc.functions.StatSetFunction.
 * <p>
 * C++: static data modifier or, created at run time, `RcStatFunction<StatSetFunction>` (lifetimes: StatFunction.h).
 *
 * @author ATracer
 */
class StatSetFunction : public ::aion::gameserver::model::stats::calc::functions::StatFunction {
#include "aion/gameserver/model/stats/calc/functions/StatSetFunction.xml.inc"
public:
	/** Java `public StatSetFunction()` */
	StatSetFunction() = default;

	/** Java `public StatSetFunction(StatEnum name, int value)` */
	StatSetFunction(container::StatEnum name, int32_t value);

	void apply(Stat2& stat, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	int32_t getPriority() const override final;

	std::string toString() override;
};

} // namespace aion::gameserver::model::stats::calc::functions

namespace aion::gameserver::runtime {

/** Not a static template (StatFunction.h). */
template <>
struct IsStaticTemplate<model::stats::calc::functions::StatSetFunction> : std::false_type {};

} // namespace aion::gameserver::runtime
