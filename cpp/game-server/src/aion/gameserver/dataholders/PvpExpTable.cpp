#include "aion/gameserver/dataholders/PvpExpTable.h"

#include <algorithm>
#include <string>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::dataholders {

// Java PvpExpTable.java:27-32
void PvpExpTable::afterUnmarshal(xml::LoadContext& ctx, const xml::XmlParent& /*parent*/) {
	rowsByLevel.assign(rows.size(), nullptr);
	for (const Row& row : rows) {
		if (row.level < 1 || row.level > static_cast<int32_t>(rowsByLevel.size())) // Java: ArrayIndexOutOfBoundsException of rowsByLevel[row.level - 1]
			ctx.fail("Index " + std::to_string(row.level - 1) + " out of bounds for length " + std::to_string(rowsByLevel.size()));
		rowsByLevel[static_cast<size_t>(row.level - 1)] = &row;
	}
}

int32_t PvpExpTable::getExp(int32_t victimLevel) const {
	return getRow(victimLevel).exp;
}

int32_t PvpExpTable::getMaxFromAllUser(int32_t level) const {
	return getRow(level).maxFromAllUser;
}

int32_t PvpExpTable::getReduceAmount(int32_t level) const {
	return getRow(level).reduceAmount;
}

int64_t PvpExpTable::getReduceIntervalMillis(int32_t level) const {
	return getRow(level).reduceInterval * 1000LL;
}

int64_t PvpExpTable::getDelayTimeMillis(int32_t level) const {
	return getRow(level).delayTime * 1000LL;
}

int32_t PvpExpTable::getMaxLevel() const {
	return static_cast<int32_t>(rowsByLevel.size());
}

// Java PvpExpTable.java:65-67 (a level without a row is a NullPointerException in Java)
const PvpExpTable::Row& PvpExpTable::getRow(int32_t level) const {
	return *rowsByLevel[static_cast<size_t>(std::clamp(level - 1, 0, static_cast<int32_t>(rowsByLevel.size()) - 1))];
}

} // namespace aion::gameserver::dataholders
