#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "aion/gameserver/model/drop/Drop.xml.h"

namespace aion::gameserver::model::drop {

/**
 * Java com.aionemu.gameserver.model.drop.Drop.
 * <p>
 * C++: the public constructor creates a run-time drop (DropRegistrationService, QuestService). A DropItem refers to its drop as a `const Drop*`:
 * DropItem::create(const Drop*) for drops of the static data, DropItem::create(Drop&&) for a run-time drop, which the drop item then owns (Java's
 * `new DropItem(new Drop(...))`).
 *
 * @author MrPoke
 */
class Drop : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/drop/Drop.xml.inc"
private:
	Drop() = default; // Java: private Drop()

	/** C++ only: Java afterUnmarshal's checks and the maxAmount default; returns the IllegalArgumentException message of a failed check */
	std::optional<std::string> validate();

public:
	Drop(int32_t itemId, int32_t minAmount, int32_t maxAmount, float chance);

	/** Java returns a Boolean (never null) */
	bool isEachMember() const { return eachMember; }

	std::string toString() const;
};

} // namespace aion::gameserver::model::drop
