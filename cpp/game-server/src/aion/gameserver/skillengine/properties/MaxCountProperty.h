#pragma once

#include "aion/gameserver/skillengine/properties/Properties.h"
#include "aion/gameserver/skillengine/properties/fwd.h"

namespace aion::gameserver::skillengine::properties {

/**
 * Java com.aionemu.gameserver.skillengine.properties.MaxCountProperty: keeps the `target_maxcount` targets nearest to the first target (and,
 * for PARTY_WITHPET, their summons) of an AREA or PARTY skill.
 * <p>
 * C++: a static-only class (Java never instantiates it); declarations of m5b2-plan.md S-03, the bodies are the cast lane's. It includes
 * Properties.h for the nested ValidationResult (hub-headers.md §9.3).
 *
 * @author MrPoke, Neon
 */
class MaxCountProperty {
public:
	static bool set(const Properties* properties, Properties::ValidationResult& result);
};

} // namespace aion::gameserver::skillengine::properties
