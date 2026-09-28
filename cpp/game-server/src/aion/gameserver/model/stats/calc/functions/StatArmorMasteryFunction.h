#pragma once

#include <cstdint>
#include <type_traits>
#include <unordered_set>
#include <vector>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/model/stats/calc/fwd.h"
#include "aion/gameserver/model/stats/container/fwd.h"
#include "aion/gameserver/model/templates/item/enums/fwd.h"
#include "aion/gameserver/utils/stats/fwd.h"

namespace aion::gameserver::model::stats::calc::functions {

/**
 * A rate function of an armor mastery effect whose value scales with the equipped armor pieces of its type.
 * <p>
 * C++: a run-time StatFunction subclass with its own mutable member, so it derives RefCounted itself and forwards IStatFunction's
 * retain/release (StatFunction.h, fieldmap K4). Created with create() (ArmorMasteryEffect, P5-03).
 *
 * @author ATracer (based on Mr.Poke ArmorMasteryModifier)
 */
class StatArmorMasteryFunction final : public runtime::RefCounted, public StatRateFunction {
	AION_MAKE_REF_FRIEND
private:
	const templates::item::enums::ItemSubType armorType;
	const int32_t fixedBonus;
	runtime::Field<int32_t> equipmentFactor{};

protected:
	StatArmorMasteryFunction(templates::item::enums::ItemSubType armorType, container::StatEnum name, int32_t value, bool bonus, int32_t fixedBonus,
		const std::vector<runtime::Ptr<gameobjects::Item>>& equipment);
	~StatArmorMasteryFunction() override;

public:
	/** Java: new StatArmorMasteryFunction(armorType, name, value, bonus, fixedBonus, equipment) */
	static runtime::Ref<StatArmorMasteryFunction> create(templates::item::enums::ItemSubType armorType, container::StatEnum name, int32_t value,
		bool bonus, int32_t fixedBonus, const std::vector<runtime::Ptr<gameobjects::Item>>& equipment);

	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

	void updateEquipmentFactor(const std::vector<runtime::Ptr<gameobjects::Item>>& equipment);

private:
	int32_t getEquipmentFactor(items::ItemSlot itemSlot);

public:
	void apply(Stat2& stat, const std::unordered_set<utils::stats::CalculationType>& calculationTypes) override;

	int32_t getValue() override;
};

} // namespace aion::gameserver::model::stats::calc::functions

namespace aion::gameserver::runtime {

/** Not a static template (StatFunction.h). */
template <>
struct IsStaticTemplate<model::stats::calc::functions::StatArmorMasteryFunction> : std::false_type {};

} // namespace aion::gameserver::runtime
