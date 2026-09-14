#include "aion/gameserver/model/templates/BoundRadius.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <map>
#include <memory>

#include "aion/gameserver/runtime/sync/LockClass.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::model::templates {

const BoundRadius BoundRadius::DEFAULT(0.0f, 0.0f, 0.0f);

BoundRadius::BoundRadius(float frontValue, float sideValue, float upperValue) : front(frontValue), side(sideValue), upper(upperValue) {
}

namespace {

/** The interned run-time radii, keyed by the bit patterns of (front, side, upper) */
using InternKey = std::array<uint32_t, 3>;

struct InternTable {
	runtime::Monitor lock{AION_LOCK_CLASS(BoundRadius::intern)};
	std::map<InternKey, std::unique_ptr<const BoundRadius>> radii;
};

/** leaked immortal: the radii are referenced by PlayerCommonData until the process exits */
InternTable& internTable() {
	static auto* const table = new InternTable();
	return *table;
}

} // namespace

// lint: L7 C++ only: interning replaces Java's unsynchronized per-call `new BoundRadius`, so the table needs its own lock
const BoundRadius* BoundRadius::intern(float frontValue, float sideValue, float upperValue) {
	const InternKey key{std::bit_cast<uint32_t>(frontValue), std::bit_cast<uint32_t>(sideValue), std::bit_cast<uint32_t>(upperValue)};
	InternTable& table = internTable();
	SYNCHRONIZED(table.lock) {
		std::unique_ptr<const BoundRadius>& radius = table.radii[key];
		if (!radius)
			radius = std::make_unique<const BoundRadius>(frontValue, sideValue, upperValue);
		return radius.get();
	}
}

float BoundRadius::getMaxOfFrontAndSide() const {
	return std::max(front, side); // Java Math.max (no NaN in static data)
}

} // namespace aion::gameserver::model::templates
