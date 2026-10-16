#pragma once

#include <cstdint>
#include <optional>

#include "aion/gameserver/model/autogroup/EntryRequestType.h"

namespace aion::gameserver::model::autogroup {

/**
 * Companion of the generated enum EntryRequestType (docs/design/static-data.md §2.5): the constructor's id and the static lookup as free
 * functions found by ADL. Java's null of getTypeById is `std::nullopt`.
 *
 * @author xTz
 */

/** Java: ert.getId() (the constructor's byte: the ordinal of the three constants) */
int8_t getId(EntryRequestType ert);

/** Java: EntryRequestType.getTypeById(id) */
std::optional<EntryRequestType> getTypeById(int8_t id);

} // namespace aion::gameserver::model::autogroup
