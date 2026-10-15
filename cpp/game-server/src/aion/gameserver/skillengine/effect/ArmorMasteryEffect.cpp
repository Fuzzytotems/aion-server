#include "aion/gameserver/skillengine/effect/ArmorMasteryEffect.h"

#include <cstdint>
#include <vector>

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/StatOwner.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatArmorMasteryFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect {

namespace functions = gameserver::model::stats::calc::functions;

using runtime::Ptr;
using runtime::Ref;

void ArmorMasteryEffect::startEffect(model::Effect& effect) const {
	// Java: `if (change == null) return;` - JAXB leaves the list null without a <change>, where the bound C++ list is empty
	if (change.empty())
		return;
	int32_t fixedBonus = calculateBaseValue(effect);
	std::vector<Ref<functions::IStatFunction>> modifiersValue = getModifiers(effect); // Java: modifiers (the name of the template's member)
	std::vector<Ref<functions::IStatFunction>> masteryModifiers;
	std::vector<Ptr<gameserver::model::gameobjects::Item>> equipment =
		runtime::cast<gameserver::model::gameobjects::player::Player>(effect.getEffected())->getEquipment().getEquippedItems();
	for (const Ref<functions::IStatFunction>& modifier : modifiersValue) {
		// Java passes the nullable armor attribute on; StatArmorMasteryFunction's C++ constructor takes a non-optional ItemSubType (header request,
		// docs/deviations/P5-03.md), so a template without armor= is a NullPointerException here. The data has none (38 of 38 <armormastery>).
		if (!armorType)
			throw runtime::NullPointerException("ArmorMasteryEffect.armorType is null");
		masteryModifiers.push_back(
			functions::StatArmorMasteryFunction::create(*armorType, modifier->getName(), modifier->getValue(), modifier->isBonus(), fixedBonus, equipment));
	}
	std::vector<Ptr<functions::IStatFunction>> borrowed(masteryModifiers.begin(), masteryModifiers.end());
	effect.getEffected()->getGameStats()->addEffect(
		Ptr<gameserver::model::stats::calc::StatOwner>(static_cast<gameserver::model::stats::calc::StatOwner&>(effect)), borrowed);
}

} // namespace aion::gameserver::skillengine::effect
