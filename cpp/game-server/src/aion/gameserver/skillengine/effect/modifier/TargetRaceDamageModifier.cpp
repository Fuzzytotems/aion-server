#include "aion/gameserver/skillengine/effect/modifier/TargetRaceDamageModifier.h"

#include <cstdint>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/skillengine/model/Effect.h"

namespace aion::gameserver::skillengine::effect::modifier {

namespace {

using gameserver::model::Race;

/** Java's unboxing of the nullable `race` attribute (`switch (skillTargetRace)`, `skillTargetRace.toString()`): NullPointerException when absent */
Race targetRaceOf(const std::optional<Race>& skillTargetRace) {
	if (!skillTargetRace)
		throw runtime::NullPointerException("skillTargetRace is null");
	return *skillTargetRace;
}

} // namespace

int32_t TargetRaceDamageModifier::analyze(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();

	// Java: (value + effect.getSkillLevel() * delta) - int arithmetic, wrapping on overflow
	int32_t newValue = static_cast<int32_t>(static_cast<uint32_t>(value) + static_cast<uint32_t>(effect.getSkillLevel()) * static_cast<uint32_t>(delta));
	if (runtime::Ptr<gameserver::model::gameobjects::player::Player> player = runtime::as<gameserver::model::gameobjects::player::Player>(effected)) {
		switch (targetRaceOf(skillTargetRace)) {
			case Race::ASMODIANS:
				if (player->getRace() == Race::ASMODIANS)
					return newValue;
				break;
			case Race::ELYOS:
				if (player->getRace() == Race::ELYOS)
					return newValue;
				break;
			default:
				break;
		}
	} else if (runtime::Ptr<gameserver::model::gameobjects::Npc> npc = runtime::as<gameserver::model::gameobjects::Npc>(effected)) {
		// Java: race.toString().equals(skillTargetRace.toString()) - Race does not override toString, so the names of one enum: value equality
		if (npc->getObjectTemplate()->getRace() == targetRaceOf(skillTargetRace))
			return newValue;
		else
			return 0;
	}

	return 0;
}

bool TargetRaceDamageModifier::check(model::Effect& effect) const {
	runtime::Ptr<gameserver::model::gameobjects::Creature> effected = effect.getEffected();
	if (runtime::Ptr<gameserver::model::gameobjects::player::Player> player = runtime::as<gameserver::model::gameobjects::player::Player>(effected)) {
		Race race = player->getRace();
		return (race == Race::ASMODIANS && skillTargetRace == Race::ASMODIANS) || (race == Race::ELYOS && skillTargetRace == Race::ELYOS);
	} else if (runtime::Ptr<gameserver::model::gameobjects::Npc> npc = runtime::as<gameserver::model::gameobjects::Npc>(effected)) {
		// Java: `if (race == null) return false;` - NpcTemplate.race defaults to Race.NONE and is never null in the C++ template
		Race race = npc->getObjectTemplate()->getRace();
		return race == targetRaceOf(skillTargetRace);
	}

	return false;
}

} // namespace aion::gameserver::skillengine::effect::modifier
