#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/world/zone/fwd.h"

namespace aion::gameserver::world::zone {

/**
 * Interned zone name.
 * <p>
 * Hub header (docs/design/hub-headers.md). An interned immortal (fieldmap.toml [immortal], hub-headers.md §5): every instance lives for the
 * whole process and is referenced as `const ZoneName*` (compared by identity like Java's `==`). Java's class initialization (NONE plus the
 * static block putting it into zoneNames) runs during C++ static initialization inside a STARTUP TaskScope, because the ConcurrentHashMap shim
 * needs one (see ZoneName.cpp).
 *
 * @author Rolandas
 */
class ZoneName final : public runtime::Immortal {
private:
	static inline runtime::ConcurrentHashMap<std::string, const ZoneName*> zoneNames{};

public:
	/** Java: `NONE = new ZoneName("NONE")`, put into zoneNames by the static block (both ported, ZoneName.cpp) */
	static const ZoneName* const NONE; // fieldmap: a const pointer, defined with the static block in ZoneName.cpp

private:
	const std::string _name;

	explicit ZoneName(std::string_view name);

public:
	std::string name() const { return _name; }

	int32_t id();

	static const ZoneName* createOrGet(std::string_view name);

	static int32_t getId(std::string_view name);

	static const ZoneName* get(std::string_view name);

	std::string toString() { return _name; }
};

} // namespace aion::gameserver::world::zone
