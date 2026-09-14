#include "aion/gameserver/model/team/legion/LegionHistoryEntry.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/model/team/legion/LegionHistoryAction.h"

namespace aion::gameserver::model::team::legion {

LegionHistoryEntry::LegionHistoryEntry(int32_t id, int32_t epochSeconds, LegionHistoryAction action, std::string_view name,
	std::string_view description)
	: id_(id), epochSeconds_(epochSeconds), action_(action), name_(std::string(name)), description_(std::string(description)) {
}

LegionHistoryEntry::~LegionHistoryEntry() = default;

runtime::Ref<LegionHistoryEntry> LegionHistoryEntry::create(int32_t id, int32_t epochSeconds, LegionHistoryAction action, std::string_view name,
	std::string_view description) {
	return runtime::makeRef<LegionHistoryEntry>(id, epochSeconds, action, name, description);
}

bool LegionHistoryEntry::equals(const LegionHistoryEntry& obj) const {
	return this == &obj ||
		   (id_ == obj.id_ && epochSeconds_ == obj.epochSeconds_ && action_ == obj.action_ && name_ == obj.name_ && description_ == obj.description_);
}

int32_t LegionHistoryEntry::hashCode() const {
	// Java record hashCode: 31 * h + hash(component); String.hashCode over UTF-16 code units; the enum's identity hash is replaced by its ordinal
	const auto stringHash = [](const std::string& value) {
		uint32_t hash = 0;
		for (char16_t c : commons::utils::StringUtils::toUtf16(value))
			hash = 31 * hash + c;
		return hash;
	};
	uint32_t h = static_cast<uint32_t>(id_);
	h = 31 * h + static_cast<uint32_t>(epochSeconds_);
	h = 31 * h + static_cast<uint32_t>(action_);
	h = 31 * h + stringHash(name_);
	h = 31 * h + stringHash(description_);
	return static_cast<int32_t>(h);
}

} // namespace aion::gameserver::model::team::legion
