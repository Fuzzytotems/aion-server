#pragma once

#include <cstdint>
#include <string>
#include <unordered_set>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::calc::functions {

/**
 * A stat function applied on behalf of another owner (CreatureGameStats.addEffect wraps functions whose owner differs).
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap base), the first class of its hierarchy with a runtime base,
 * so it forwards IStatFunction's retain/release (§9.2). The owner is nullable: CreatureGameStats.addEffect passes its `statOwner` parameter,
 * which Java callers set to null (PlayerStatFunctions.addPredefinedStatFunctions).
 *
 * @author ATracer
 */
class StatFunctionProxy : public runtime::RefCounted, public IStatFunction {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<StatOwner> owner;
	const runtime::Ref<IStatFunction> proxiedFunction;

protected:
	StatFunctionProxy(runtime::Ptr<StatOwner> owner, IStatFunction& statFunction);
	~StatFunctionProxy() override;

public:
	/** Java `new StatFunctionProxy(owner, statFunction)` */
	static runtime::Ref<StatFunctionProxy> create(runtime::Ptr<StatOwner> owner, IStatFunction& statFunction);

	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

	runtime::Ptr<IStatFunction> getProxiedFunction() { return proxiedFunction; }

	runtime::Ptr<StatOwner> getOwner() override { return owner; }

	container::StatEnum getName() override;

	bool isBonus() const override;

	int32_t getPriority() const override;

	int32_t getValue() override;

	bool validate(Stat2& stat) override;

	void apply(Stat2& stat, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	bool hasConditions() override;

	std::string toString();
};

} // namespace aion::gameserver::model::stats::calc::functions
