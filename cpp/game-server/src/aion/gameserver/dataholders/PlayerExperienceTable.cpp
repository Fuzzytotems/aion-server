#include "aion/gameserver/dataholders/PlayerExperienceTable.h"

#include <string>

#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::dataholders {

int64_t PlayerExperienceTable::getStartExpForLevel(int32_t level) const {
	if (level > static_cast<int32_t>(experience.size()))
		throw runtime::IllegalArgumentException("The given level is higher than possible max");
	if (level == 0)
		return 0;
	if (level < 0) // Java: ArrayIndexOutOfBoundsException of experience[level - 1]
		throw runtime::IndexOutOfBoundsException("Index " + std::to_string(level - 1) + " out of bounds for length " + std::to_string(experience.size()));
	return experience[static_cast<size_t>(level - 1)];
}

int32_t PlayerExperienceTable::getLevelForExp(int64_t expValue) const {
	int32_t level = 0;
	for (int32_t i = static_cast<int32_t>(experience.size()); i > 0; i--) {
		if (expValue >= experience[static_cast<size_t>(i - 1)]) {
			level = i;
			break;
		}
	}
	if (getMaxLevel() <= level)
		return getMaxLevel() - 1;
	return level;
}

int32_t PlayerExperienceTable::getMaxLevel() const {
	return static_cast<int32_t>(experience.size()); // Java: experience == null ? 0 : experience.length
}

} // namespace aion::gameserver::dataholders
