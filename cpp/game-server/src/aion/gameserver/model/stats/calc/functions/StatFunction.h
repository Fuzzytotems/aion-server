#pragma once

#include <concepts>
#include <cstdint>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <utility>

#include "aion/gameserver/model/stats/calc/functions/StatFunction.xml.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/skillengine/condition/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::calc::functions {

/**
 * Java com.aionemu.gameserver.model.stats.calc.functions.StatFunction (JAXB type SimpleModifier).
 * <p>
 * C++ (S0c freeze): the xmlgen behaviour shell implements IStatFunction. Java creates StatFunction objects with three lifetimes, and the
 * reference count operations follow them:
 * - static data modifiers (ModifiersTemplate, bound with a unique_ptr) and the objects of static initializers (PlayerStatFunctions.FUNCTIONS)
 *   are immortal: StatFunction's retain()/release() are no-ops, so `Ref<IStatFunction>`/`Ref<StatFunction>` may hold them
 *   (ofTemplate() turns a `const StatFunction*` of static data into such a reference);
 * - objects created at run time (`new StatAddFunction(stat, value, bonus)` in EnchantEffect, TemperingEffect, BufEffect, InstanceBuff,
 *   ItemEquipmentListener and handlers) are `RcStatFunction<StatAddFunction>::create(stat, value, bonus)`: RefCounted, retain/release
 *   forwarded;
 * - a run-time subclass with its own members (InstanceScaler::InstanceScalerStatFunction, the mastery functions) derives RefCounted itself and
 *   forwards retain/release the same way (hub-headers.md §9.2).
 * Because one static type covers both lifetimes, StatFunction and its subclasses are no static templates (IsStaticTemplate is false below):
 * a `const StatFunction*` is neither a TaskArg nor pinnable; tasks capture `Ref<StatFunction>` (a no-op retain for static data).
 * withConditions(Conditions) shares the Conditions of an effect template (BufEffect.java:79), which the owning `conditions` member of static
 * data cannot hold: the C++-only `sharedConditions` keeps that template pointer and validate() reads whichever is set.
 *
 * @author ATracer
 */
class StatFunction : public ::aion::gameserver::runtime::StaticTemplate, public IStatFunction {
#include "aion/gameserver/model/stats/calc/functions/StatFunction.xml.inc"
private:
	// C++ only (K1 class, non-retaining template pointer): the effect template's Conditions given to withConditions (Java stores it into
	// `conditions`, which static data owns)
	const skillengine::condition::Conditions* sharedConditions = nullptr;

	/** C++ only: Java's package access of StatFunctionProxy.validate to the protected validate(Stat2, IStatFunction) */
	friend class StatFunctionProxy;

public:
	/** Java `public StatFunction()` (the binder's default construction) */
	StatFunction() = default;
	/** Java `public StatFunction(StatEnum stat, int value, boolean bonus)` */
	StatFunction(container::StatEnum stat, int32_t value, bool bonus);

	/** C++ only: static data and static-initializer instances are immortal; RcStatFunction and run-time subclasses forward to RefCounted. */
	void retain() const noexcept override {}
	void release() const noexcept override {}

	/**
	 * C++ only: a static data modifier (`ItemTemplate::getModifiers()`, `ManaStone::getModifiers()`) as a borrowed function for the stat
	 * containers and Item.currentModifiers (Java passes the template object itself). Static data is never modified after loading, and the
	 * no-op retain/release keep it out of reclamation. Null for null.
	 */
	static runtime::Ptr<StatFunction> ofTemplate(const StatFunction* modifier);

	runtime::Ptr<StatOwner> getOwner() override;

	container::StatEnum getName() override final;

	bool isBonus() const override final;

	/**
	 * priorities RATE 20 ADD 30 SUB 30 SET 40 RATE bonus 50 ADD bonus 60 SUB bonus 60 SET bonus 70 ABS 80 ABS debuff 90 ABS bonus 100 ABS debuff bonus
	 * 110
	 */
	int32_t getPriority() const override;

	int32_t getValue() override;

	bool validate(Stat2& stat) override;

protected:
	virtual bool validate(Stat2& stat, IStatFunction& statFunction);

public:
	void apply(Stat2& stat, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	virtual std::string toString();

	/** Java returns `this`; the Conditions belong to an effect template (nullable, immortal). */
	StatFunction& withConditions(const skillengine::condition::Conditions* conditions);

	bool hasConditions() override;
};

/**
 * C++ only: a StatFunction subclass created at run time (Java `new StatAddFunction(stat, value, bonus)` outside static data), RefCounted with
 * retain/release forwarded (hub-headers.md §9.2). `T` keeps its Java constructors; create() takes their arguments.
 */
template <std::derived_from<StatFunction> T>
class RcStatFunction final : public runtime::RefCounted, public T {
	AION_MAKE_REF_FRIEND

public:
	template <class... Args>
		requires std::constructible_from<T, Args&&...>
	[[nodiscard]] static runtime::Ref<RcStatFunction> create(Args&&... args) {
		return runtime::makeRef<RcStatFunction>(std::forward<Args>(args)...);
	}

	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

protected:
	template <class... Args>
	explicit RcStatFunction(Args&&... args) : T(std::forward<Args>(args)...) {}
	~RcStatFunction() override = default;
};

} // namespace aion::gameserver::model::stats::calc::functions

namespace aion::gameserver::runtime {

/** StatFunction objects are immortal static data or RefCounted run-time objects (see the class comment): never a static template. */
template <>
struct IsStaticTemplate<model::stats::calc::functions::StatFunction> : std::false_type {};

} // namespace aion::gameserver::runtime
