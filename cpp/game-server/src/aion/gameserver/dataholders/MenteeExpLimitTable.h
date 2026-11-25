#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/dataholders/MenteeExpLimitTable.xml.h"

namespace aion::gameserver::dataholders {

/**
 * The most XP a member of a mentor group can gain from one shared reward, by the member's level.
 * <p>
 * Java com.aionemu.gameserver.dataholders.MenteeExpLimitTable (upstream d725054e1).
 */
class MenteeExpLimitTable : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/dataholders/MenteeExpLimitTable.xml.inc"
private:
	std::vector<int64_t> limitsByLevel;

public:
	int64_t getExpLimit(int32_t level) const;

	int32_t size() const;
};

} // namespace aion::gameserver::dataholders
