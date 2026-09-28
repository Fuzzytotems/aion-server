#pragma once

#include <cstdint>

#include "aion/gameserver/model/broker/BrokerItemMask.h"
#include "aion/gameserver/model/gameobjects/fwd.h"

namespace aion::gameserver::model::broker {

/**
 * Companion of the generated enum BrokerItemMask (docs/design/static-data.md §2.5): the Java constructor data and methods as free functions found
 * by ADL (`getId(mask)` for Java `mask.getId()`).
 * <p>
 * C++: the constructor data (type id, parent, children flag) is a constexpr table; the filter objects of the constants are created on first use
 * and never released (Java enum constants live for the whole run), see BrokerItemMaskInfo.cpp.
 *
 * @author kosyachok, Simple, ATracer
 */

int32_t getId(BrokerItemMask mask) noexcept;

/** Java mask.isMatches(item): the filter of the mask accepts the item's template */
bool isMatches(BrokerItemMask mask, gameobjects::Item& item);

/** Java mask.isChildrenMask(maskId): true if an ancestor of the mask has the type id */
bool isChildrenMask(BrokerItemMask mask, int32_t maskId) noexcept;

/** Java BrokerItemMask.getBrokerMaskById(id): UNKNOWN if no constant has the type id */
BrokerItemMask getBrokerMaskById(int32_t id) noexcept;

bool hasChildren(BrokerItemMask mask) noexcept;

} // namespace aion::gameserver::model::broker
