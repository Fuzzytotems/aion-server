#pragma once

#include "aion/gameserver/model/templates/event/Buff_BuffMapType.h"
#include "aion/gameserver/world/fwd.h"

namespace aion::gameserver::model::templates::event {

/**
 * Companion of the generated enum Buff.BuffMapType (docs/design/static-data.md §2.5): Java's constructor predicate as a free function found by
 * ADL (`matches(type, instance)` for Java `type.matches(instance)`).
 *
 * @author Neon
 */

/** Java BuffMapType.matches(WorldMapInstance): the predicate passed to the constant's constructor */
bool matches(Buff_BuffMapType type, ::aion::gameserver::world::WorldMapInstance& worldMapInstance);

} // namespace aion::gameserver::model::templates::event
