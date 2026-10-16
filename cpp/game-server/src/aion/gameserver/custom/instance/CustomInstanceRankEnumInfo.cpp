#include "aion/gameserver/custom/instance/CustomInstanceRankEnumInfo.h"

#include <array>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::custom::instance {

namespace {

struct RankData {
	int32_t minRank;
	int32_t minReward;
	char16_t rankIcon;
};

// Java CustomInstanceRankEnum.java:10-18, in ordinal order
constexpr std::array<RankData, 9> RANKS{{
	{0, 4, u''}, // IRON
	{1, 10, u''}, // BRONZE
	{3, 16, u''}, // SILVER
	{5, 22, u''}, // GOLD
	{8, 28, u''}, // PLATINUM
	{12, 34, u''}, // MITHRIL
	{16, 40, u''}, // CERANIUM
	{20, 46, u''}, // ANCIENT
	{24, 50, u''}, // ANCIENT_PLUS
}};

const RankData& data(CustomInstanceRankEnum rank) {
	return RANKS[static_cast<size_t>(rank)];
}

} // namespace

// Java CustomInstanceRankEnum.java:30-32
int32_t getMinRank(CustomInstanceRankEnum rank) {
	return data(rank).minRank;
}

// Java CustomInstanceRankEnum.java:34-36
int32_t getMinReward(CustomInstanceRankEnum rank) {
	return data(rank).minReward;
}

// Java CustomInstanceRankEnum.java:38-40
char16_t getRankIcon(CustomInstanceRankEnum rank) {
	return data(rank).rankIcon;
}

// Java CustomInstanceRankEnum.java:42-45
std::string getRankDescription(int32_t value) {
	CustomInstanceRankEnum rank = getByRank(value);
	return commons::utils::StringUtils::toUtf8(std::u16string(1, data(rank).rankIcon)) + " " +
		(rank == CustomInstanceRankEnum::ANCIENT_PLUS ? "ANCIENT +" + std::to_string(value + 1 - data(rank).minRank)
													  : std::string(xml::enumName(rank)));
}

// Java CustomInstanceRankEnum.java:47-53
CustomInstanceRankEnum getByRank(int32_t rank) {
	for (int32_t i = static_cast<int32_t>(RANKS.size()) - 1; i >= 0; i--) {
		if (RANKS[static_cast<size_t>(i)].minRank <= rank)
			return static_cast<CustomInstanceRankEnum>(i);
	}
	throw runtime::IllegalArgumentException(std::to_string(rank) + " is no valid rank");
}

} // namespace aion::gameserver::custom::instance
