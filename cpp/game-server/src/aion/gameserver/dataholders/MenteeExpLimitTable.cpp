#include "aion/gameserver/dataholders/MenteeExpLimitTable.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::dataholders {

// Java MenteeExpLimitTable.java:23-28
void MenteeExpLimitTable::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	limitsByLevel.assign(rows.size(), 0);
	for (const Row& row : rows) {
		if (row.level < 1 || row.level > static_cast<int32_t>(limitsByLevel.size())) // Java: ArrayIndexOutOfBoundsException
			ctx.fail("Index " + std::to_string(row.level - 1) + " out of bounds for length " + std::to_string(limitsByLevel.size()));
		limitsByLevel[static_cast<size_t>(row.level - 1)] = row.expLimit;
	}
	rows.clear();
}

int64_t MenteeExpLimitTable::getExpLimit(int32_t level) const {
	return limitsByLevel[static_cast<size_t>(std::clamp(level - 1, 0, static_cast<int32_t>(limitsByLevel.size()) - 1))];
}

int32_t MenteeExpLimitTable::size() const {
	return static_cast<int32_t>(limitsByLevel.size());
}

} // namespace aion::gameserver::dataholders
