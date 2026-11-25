#include "aion/gameserver/dataholders/PvpExpModTable.h"

#include <algorithm>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::dataholders {

// Java PvpExpModTable.java:29-36 (min().orElseThrow(): an empty table fails the load)
void PvpExpModTable::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	if (rows.empty())
		ctx.fail("No value present");
	auto [minRow, maxRow] = std::ranges::minmax_element(rows, {}, &Row::levelDiff);
	minLevelDiff = minRow->levelDiff;
	int32_t maxLevelDiff = maxRow->levelDiff;
	modsByLevelDiff.assign(static_cast<size_t>(maxLevelDiff - minLevelDiff + 1), {});
	for (const Row& row : rows)
		modsByLevelDiff[static_cast<size_t>(row.levelDiff - minLevelDiff)] = *row.mods; // mods is required
	rows.clear();
}

// Java PvpExpModTable.java:41-44 (a level difference without a row is a NullPointerException in Java; at() throws on the empty row here)
float PvpExpModTable::getMultiplier(int32_t killerLevel, int32_t victimLevel) const {
	const std::vector<int32_t>& mods = modsByLevelDiff[static_cast<size_t>(
		std::clamp(killerLevel - victimLevel - minLevelDiff, 0, static_cast<int32_t>(modsByLevelDiff.size()) - 1))];
	return static_cast<float>(mods.at(static_cast<size_t>(std::clamp((killerLevel - 1) / 10, 0, static_cast<int32_t>(mods.size()) - 1)))) / 100.0f;
}

int32_t PvpExpModTable::size() const {
	return static_cast<int32_t>(modsByLevelDiff.size());
}

} // namespace aion::gameserver::dataholders
