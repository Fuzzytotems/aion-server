#pragma once

#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/skillengine/properties/fwd.h"

namespace aion::gameserver::skillengine::properties {

/**
 * Java com.aionemu.gameserver.skillengine.properties.TargetSpeciesProperty: keeps only the npcs (NPC) or only the players (PC) of the effected
 * list.
 * <p>
 * C++: a static-only class (Java never instantiates it); declarations of m5b2-plan.md S-03, the body is the cast lane's. It includes
 * Properties.h for the nested ValidationResult (hub-headers.md §9.3).
 *
 * @author Luzien
 */
class TargetSpeciesProperty {
public:
	static bool set(const Properties* properties, Properties::ValidationResult& result);
};

} // namespace aion::gameserver::skillengine::properties
