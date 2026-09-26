#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/team/legion/fwd.h"

namespace aion::gameserver::model::team::legion {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). Java `record LegionHistoryEntry(int id, int epochSeconds, LegionHistoryAction action,
 * String name, String description)`: RefCounted (fieldmap K3, `Legion.legionHistoryByType`), created with create(). Java interns the strings;
 * the C++ record stores copies (equal by value, which is what the record's equals compares).
 *
 * @author Simple, xTz
 */
class LegionHistoryEntry : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const int32_t id_;
	const int32_t epochSeconds_;
	const LegionHistoryAction action_;
	const std::string name_;
	const std::string description_;

protected:
	LegionHistoryEntry(int32_t id, int32_t epochSeconds, LegionHistoryAction action, std::string_view name, std::string_view description);
	~LegionHistoryEntry() override;

public:
	/** Java: new LegionHistoryEntry(id, epochSeconds, action, name, description) */
	static runtime::Ref<LegionHistoryEntry> create(int32_t id, int32_t epochSeconds, LegionHistoryAction action, std::string_view name,
		std::string_view description);

	int32_t id() const { return id_; }

	int32_t epochSeconds() const { return epochSeconds_; }

	LegionHistoryAction action() const { return action_; }

	std::string name() const { return name_; }

	std::string description() const { return description_; }

	/** Java record equals (all components) */
	bool equals(const LegionHistoryEntry& obj) const;

	/** Java record hashCode (the enum component hashes by identity in Java; here its ordinal) */
	int32_t hashCode() const;
};

} // namespace aion::gameserver::model::team::legion
