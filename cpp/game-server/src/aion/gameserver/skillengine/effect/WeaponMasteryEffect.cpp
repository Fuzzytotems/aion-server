#include "aion/gameserver/skillengine/effect/WeaponMasteryEffect.h"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroupInfo.h"
#include "aion/gameserver/model/templates/item/enums/ItemSubType.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/stats/CalculationType.h"

namespace aion::gameserver::skillengine::effect {

using gameserver::model::gameobjects::player::Player;
using gameserver::model::stats::calc::Stat2;
using gameserver::model::stats::calc::functions::IStatFunction;
using gameserver::model::stats::calc::functions::StatRateFunction;
using gameserver::model::stats::container::StatEnum;
using gameserver::model::templates::item::enums::ItemGroup;
using gameserver::model::templates::item::enums::ItemSubType;
using runtime::Ptr;
using runtime::Ref;
using utils::stats::CalculationType;

namespace {

/**
 * Java com.aionemu.gameserver.model.stats.calc.functions.StatWeaponMasteryFunction (StatWeaponMasteryFunction.java:15-58, P5-01), ported
 * statement by statement in this file because the class has no C++ file: model/stats/calc/functions/fwd.h declares it and nothing defines it
 * (docs/deviations/P5-01.md "B-06 is deferred": its only constructor is this effect). Header request (P5-01): a declaration header and
 * StatWeaponMasteryFunction.cpp in model/stats/calc/functions, shaped like StatArmorMasteryFunction.h, to which this class moves unchanged
 * (docs/deviations/P5-04.md). A run-time StatFunction subclass with its own member, so it derives RefCounted itself and forwards IStatFunction's
 * retain/release (StatFunction.h).
 *
 * @author ATracer (based on Mr.Poke WeaponMasteryModifier)
 */
// fieldmap-class: com.aionemu.gameserver.model.stats.calc.functions.StatWeaponMasteryFunction
class StatWeaponMasteryFunction final : public runtime::RefCounted, public StatRateFunction {
	AION_MAKE_REF_FRIEND
private:
	const ItemGroup itemGroup;

protected:
	StatWeaponMasteryFunction(ItemGroup itemGroupValue, StatEnum name, int32_t valueValue, bool bonusValue)
		: StatRateFunction(name, valueValue, bonusValue), itemGroup(itemGroupValue) {}
	~StatWeaponMasteryFunction() override = default;

public:
	/** Java: new StatWeaponMasteryFunction(itemGroup, name, value, bonus) */
	static Ref<StatWeaponMasteryFunction> create(ItemGroup itemGroup, StatEnum name, int32_t value, bool bonus) {
		return runtime::makeRef<StatWeaponMasteryFunction>(itemGroup, name, value, bonus);
	}

	void retain() const noexcept override { runtime::RefCounted::retain(); }
	void release() const noexcept override { runtime::RefCounted::release(); }

	void apply(Stat2& statValue, const std::unordered_set<CalculationType>& calculationTypes) override {
		Ptr<Player> player = runtime::cast<Player>(statValue.getOwner());
		std::optional<ItemGroup> mainWeapon = player->getEquipment().getMainHandWeaponType();
		std::optional<ItemGroup> offHandWeapon = player->getEquipment().getOffHandWeaponType();
		switch (getName()) { // Java: switch (this.stat), the field getName() answers
			case StatEnum::MAIN_HAND_POWER:
				if (mainWeapon && *mainWeapon == itemGroup) {
					applyTo(statValue, calculationTypes);
				}
				break;
			case StatEnum::OFF_HAND_POWER:
				if (offHandWeapon && *offHandWeapon == itemGroup)
					applyTo(statValue, calculationTypes);
				break;
			default:
				if (mainWeapon && *mainWeapon == itemGroup)
					applyTo(statValue, calculationTypes);
		}
	}

private:
	void applyTo(Stat2& statValue, const std::unordered_set<CalculationType>& calculationTypes) {
		if (isBonus()) {
			int32_t bonusRate = getValue();
			if (calculationTypes.contains(CalculationType::SKILL) && calculationTypes.contains(CalculationType::DUAL_WIELD)) {
				bonusRate = commons::utils::Rnd::get(0, getValue());
			}
			statValue.setFixedBonusRate(static_cast<float>(bonusRate) / 100.0f);
		} else {
			// TODO: Check if calculations differ if its not a bonus type.
			statValue.setBase(statValue.getExactBaseWithoutBaseRate() * statValue.calculatePercent(getValue()));
		}
	}
};

/** Java: the ItemGroup field dereferenced by `itemGroup.getItemSubType()` - NullPointerException when the XML had no weapon attribute */
ItemGroup unboxItemGroup(const std::optional<ItemGroup>& itemGroup) {
	if (!itemGroup)
		throw runtime::NullPointerException("Cannot invoke \"ItemGroup.getItemSubType()\" because \"this.itemGroup\" is null");
	return *itemGroup;
}

} // namespace

void WeaponMasteryEffect::startEffect(model::Effect& effect) const {
	if (change.empty()) // Java: change == null - JAXB leaves the list null without <change> elements, the bound vector is empty
		return;

	std::vector<Ref<IStatFunction>> statModifiers = getModifiers(effect); // Java: modifiers (the name of an EffectTemplate member in C++)
	std::vector<Ref<IStatFunction>> masteryModifiers;
	for (const Ref<IStatFunction>& modifier : statModifiers) {
		if (getItemSubType(unboxItemGroup(itemGroup)) == ItemSubType::TWO_HAND) {
			masteryModifiers.push_back(StatWeaponMasteryFunction::create(*itemGroup, modifier->getName(), modifier->getValue(), modifier->isBonus()));
		} else if (modifier->getName() == StatEnum::PHYSICAL_ATTACK || modifier->getName() == StatEnum::MAGICAL_ATTACK) {
			masteryModifiers.push_back(
				StatWeaponMasteryFunction::create(*itemGroup, StatEnum::MAIN_HAND_POWER, modifier->getValue(), modifier->isBonus()));
			masteryModifiers.push_back(
				StatWeaponMasteryFunction::create(*itemGroup, StatEnum::OFF_HAND_POWER, modifier->getValue(), modifier->isBonus()));
		}
	}
	// masteryModifiers holds the references until the stat container has taken its own (CreatureGameStats.addEffectOnly)
	effect.getEffected()->getGameStats()->addEffect(Ptr<gameserver::model::stats::calc::StatOwner>(effect),
		std::vector<Ptr<IStatFunction>>(masteryModifiers.begin(), masteryModifiers.end()));
}

} // namespace aion::gameserver::skillengine::effect
