#pragma once

#include <cstdint>
#include <string_view>

#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/services/rift/RiftEnum.h"

namespace aion::gameserver::services::rift {

/**
 * Companion of the generated enum RiftEnum (docs/design/static-data.md §2.5): Java's constructor data and accessors as free functions found
 * by ADL (`getMaster(rift)` for Java `rift.getMaster()`), and the two static lookups. Pure data, ported as a whole (M5i I-5).
 *
 * @author Source
 */

/** Java RiftEnum's constructor data of one constant */
struct RiftEnumInfo {
	RiftEnum rift;
	int32_t id;
	std::string_view master;
	std::string_view slave;
	int32_t entries;
	int32_t minLevel;
	int32_t maxLevel;
	model::Race destination;
	bool vortex;
	bool canBeVolatile;
	bool isInvasionRift;
};

/** the data of the constant */
const RiftEnumInfo& infoOf(RiftEnum rift);

/** Java RiftEnum.getRift(int): @throws IllegalArgumentException "Unsupported rift id: " for an unknown id */
RiftEnum getRift(int32_t id);

/** Java RiftEnum.getVortex(Race): the first vortex with that destination; @throws IllegalArgumentException "Unsupported vortex race: " */
RiftEnum getVortex(model::Race race);

inline int32_t getId(RiftEnum rift) { return infoOf(rift).id; }
inline std::string_view getMaster(RiftEnum rift) { return infoOf(rift).master; }
inline std::string_view getSlave(RiftEnum rift) { return infoOf(rift).slave; }
inline int32_t getEntries(RiftEnum rift) { return infoOf(rift).entries; }
inline int32_t getMinLevel(RiftEnum rift) { return infoOf(rift).minLevel; }
inline int32_t getMaxLevel(RiftEnum rift) { return infoOf(rift).maxLevel; }
inline model::Race getDestination(RiftEnum rift) { return infoOf(rift).destination; }
inline bool isVortex(RiftEnum rift) { return infoOf(rift).vortex; }
inline bool canBeVolatile(RiftEnum rift) { return infoOf(rift).canBeVolatile; }
inline bool isInvasionRift(RiftEnum rift) { return infoOf(rift).isInvasionRift; }

} // namespace aion::gameserver::services::rift
