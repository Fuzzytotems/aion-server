#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/geoEngine/scene/DespawnableNode_DespawnableType.h"

namespace aion::gameserver::geoEngine::scene {

/** Companion of the generated nested enum DespawnableNode.DespawnableType (docs/design/static-data.md §2.5): Java's constructor data (ADL). */

/** Java: DespawnableType.getId() - the ids equal the ordinals (NONE 0 ... SHIELD 8) */
constexpr int8_t getId(DespawnableNode_DespawnableType type) noexcept {
	return static_cast<int8_t>(type);
}

/**
 * Java: DespawnableType.getById(byte)
 * @throws IllegalArgumentException("Invalid ID " + id) if no constant has the id
 */
inline DespawnableNode_DespawnableType getById(int8_t id) {
	if (id >= 0 && id <= static_cast<int8_t>(DespawnableNode_DespawnableType::SHIELD))
		return static_cast<DespawnableNode_DespawnableType>(id);
	throw runtime::IllegalArgumentException("Invalid ID " + std::to_string(id));
}

} // namespace aion::gameserver::geoEngine::scene
