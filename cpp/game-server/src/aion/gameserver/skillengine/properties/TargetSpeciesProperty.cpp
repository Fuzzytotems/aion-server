#include "aion/gameserver/skillengine/properties/TargetSpeciesProperty.h"

#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/skillengine/properties/TargetSpeciesAttribute.h"

namespace aion::gameserver::skillengine::properties {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;

bool TargetSpeciesProperty::set(const Properties* properties, Properties::ValidationResult& result) {
	// Java: `switch (properties.getTargetSpecies())`; Properties.validateEffectedList runs this step only when target_species is set
	switch (*properties->getTargetSpecies()) {
		case TargetSpeciesAttribute::NPC:
			result.getTargets().removeIf([](const Ptr<Creature>& effected) { return !runtime::as<Npc>(effected); });
			break;
		case TargetSpeciesAttribute::PC:
			result.getTargets().removeIf([](const Ptr<Creature>& effected) { return !runtime::as<Player>(effected); });
			break;
	}
	return true;
}

} // namespace aion::gameserver::skillengine::properties
