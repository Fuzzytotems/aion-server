#pragma once

#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/templates/materials/MaterialTarget.h"

namespace aion::gameserver::model::templates::materials {

/**
 * Companion of the generated enum MaterialTarget (docs/design/static-data.md §2.5): Java's constructor predicate as a free function found by ADL
 * (`matches(target, creature)` for Java `target.matches(creature)`).
 *
 * @author Rolandas
 */

/** Java MaterialTarget.matches(Creature): the predicate passed to the constant's constructor */
bool matches(MaterialTarget target, gameobjects::Creature& creature);

} // namespace aion::gameserver::model::templates::materials
