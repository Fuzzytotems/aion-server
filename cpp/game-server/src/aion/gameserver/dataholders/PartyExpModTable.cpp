#include "aion/gameserver/dataholders/PartyExpModTable.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::dataholders {

// Java PartyExpModTable.java:25-31
void PartyExpModTable::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	if (rows.empty()) // Java: rows.getFirst() throws NoSuchElementException
		ctx.fail("No rows in party_exp_mod_table");
	columns = static_cast<int32_t>(rows.front().mods->size()); // mods is required
	mods.assign(rows.size() * static_cast<size_t>(columns), 0);
	for (const Row& row : rows) {
		// Java: System.arraycopy(row.mods, 0, mods, row.levelDiff * columns, columns) throws IndexOutOfBoundsException outside either array
		int64_t offset = static_cast<int64_t>(row.levelDiff) * columns;
		if (row.levelDiff < 0 || static_cast<int32_t>(row.mods->size()) < columns || offset + columns > static_cast<int64_t>(mods.size()))
			ctx.fail("arraycopy: last destination index " + std::to_string(offset + columns) + " out of bounds for int[" + std::to_string(mods.size()) + "]");
		std::copy_n(row.mods->begin(), columns, mods.begin() + offset);
	}
	rows.clear();
}

// Java PartyExpModTable.java:38-44
float PartyExpModTable::getWeight(int32_t memberLevel, int32_t highestLevel) const {
	int32_t levelDiff = highestLevel - memberLevel;
	if (levelDiff < 0)
		return 0;
	int32_t index = std::min(levelDiff, size() - 1) * columns + highestLevel / 10 + 1;
	return static_cast<float>(mods[static_cast<size_t>(std::min(index, static_cast<int32_t>(mods.size()) - 1))]) / 100.0f;
}

int32_t PartyExpModTable::size() const {
	return static_cast<int32_t>(mods.size()) / columns;
}

} // namespace aion::gameserver::dataholders
