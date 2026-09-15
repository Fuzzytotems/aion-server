#pragma once

#include <cstdint>

namespace aion::gameserver::model::items {

/**
 * Well-known item ids.
 * <p>
 * C++: a static-only class (hub-headers.md §11.1); Java's private constructor is deleted.
 *
 * @author ATracer
 */
class ItemId {
public:
	static constexpr int32_t KINAH = 182400001;

	ItemId() = delete;
};

} // namespace aion::gameserver::model::items
