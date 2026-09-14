#pragma once

#include <cstdint>
#include <unordered_set>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::calc::functions {

/**
 * A function applied to one stat of a creature.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). An interface held by `Ref<IStatFunction>` (fieldmap: CreatureGameStats.stats), so
 * it declares the reference count operations (§9.2): StatFunctionProxy and the run-time StatFunctions (RcStatFunction<T>, subclasses deriving
 * RefCounted) forward them to RefCounted; StatFunction itself implements them as no-ops for its immortal static data (StatFunction.h). Java `Comparable<IStatFunction>` with a default compareTo (priority difference): `compareTo(...) const` for the collection shims.
 *
 * @author ATracer, Rolandas
 */
class IStatFunction {
public:
	/** C++ only: Ref<IStatFunction> retains the implementing object (hub-headers.md §9.2). */
	virtual void retain() const noexcept = 0;
	virtual void release() const noexcept = 0;

	virtual container::StatEnum getName() = 0;

	virtual bool isBonus() = 0;

	virtual int32_t getPriority() = 0;

	virtual int32_t getValue() = 0;

	virtual bool validate(Stat2& stat) = 0;

	virtual void apply(Stat2& stat, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) = 0;

	virtual runtime::Ptr<StatOwner> getOwner() = 0;

	virtual bool hasConditions() = 0;

	/** Java default method: getPriority() - o.getPriority() */
	virtual int32_t compareTo(const IStatFunction& o) const;

	virtual ~IStatFunction() = default;

protected:
	IStatFunction() = default;
	IStatFunction(const IStatFunction&) = default;
	IStatFunction& operator=(const IStatFunction&) = default;
};

} // namespace aion::gameserver::model::stats::calc::functions
