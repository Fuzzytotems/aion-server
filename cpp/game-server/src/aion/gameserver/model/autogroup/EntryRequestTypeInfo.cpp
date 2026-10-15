#include "aion/gameserver/model/autogroup/EntryRequestTypeInfo.h"

#include <array>
#include <cstddef>

namespace aion::gameserver::model::autogroup {

namespace {

/** the constructor arguments in ordinal order (EntryRequestType.java:8-10) */
constexpr std::array<int8_t, 3> IDS{0, 1, 2};

static_assert(IDS.size() == xml::EnumTraits<EntryRequestType>::names.size(), "one id per EntryRequestType constant");

} // namespace

// Java EntryRequestType.java:18-20
int8_t getId(EntryRequestType ert) {
	return IDS.at(static_cast<size_t>(ert));
}

// Java EntryRequestType.java:22-29
std::optional<EntryRequestType> getTypeById(int8_t id) {
	for (size_t ordinal = 0; ordinal < IDS.size(); ordinal++) {
		const EntryRequestType ert = static_cast<EntryRequestType>(ordinal);
		if (getId(ert) == id)
			return ert;
	}
	return std::nullopt;
}

} // namespace aion::gameserver::model::autogroup
