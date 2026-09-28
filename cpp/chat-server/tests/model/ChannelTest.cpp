// The channel classes: which requests (type, game server, race, meta) a channel matches, and its name.

#include <gtest/gtest.h>

#include "aion/chatserver/model/channel/JobChannel.h"
#include "aion/chatserver/model/channel/LangChannel.h"
#include "aion/chatserver/model/channel/LfgChannel.h"
#include "aion/chatserver/model/channel/RegionChannel.h"
#include "aion/chatserver/model/channel/TradeChannel.h"
#include "aion/commons/utils/Exception.h"

namespace aion::chatserver::test {
namespace {

using namespace model;
using namespace model::channel;

TEST(ChannelTest, RegionChannelMatchesItsMapRaceAndGameServer) {
	RegionChannel channel(1, Race::ELYOS, "Poeta");
	EXPECT_TRUE(channel.matches(ChannelType::REGION, 1, Race::ELYOS, "Poeta"));
	EXPECT_FALSE(channel.matches(ChannelType::REGION, 1, Race::ELYOS, "poeta"));
	EXPECT_FALSE(channel.matches(ChannelType::REGION, 1, Race::ASMODIANS, "Poeta"));
	EXPECT_FALSE(channel.matches(ChannelType::REGION, 2, Race::ELYOS, "Poeta"));
	EXPECT_FALSE(channel.matches(ChannelType::TRADE, 1, Race::ELYOS, "Poeta"));
	EXPECT_FALSE(channel.matches(std::nullopt, 1, Race::ELYOS, "Poeta"));
	EXPECT_EQ(channel.name(), "REGION (E)");
	EXPECT_EQ(channel.getChannelType(), ChannelType::REGION);
	EXPECT_EQ(channel.getGameServerId(), 1);
}

TEST(ChannelTest, TradeChannelMatchesItsMap) {
	TradeChannel channel(1, Race::ASMODIANS, "Housing_barrack");
	EXPECT_TRUE(channel.matches(ChannelType::TRADE, 1, Race::ASMODIANS, "Housing_barrack"));
	EXPECT_FALSE(channel.matches(ChannelType::TRADE, 1, Race::ASMODIANS, "Housing"));
	EXPECT_FALSE(channel.matches(ChannelType::REGION, 1, Race::ASMODIANS, "Housing_barrack"));
	EXPECT_EQ(channel.name(), "TRADE (A)");
}

TEST(ChannelTest, LfgChannelIgnoresTheMeta) {
	LfgChannel channel(1, Race::ELYOS);
	EXPECT_TRUE(channel.matches(ChannelType::LFG, 1, Race::ELYOS, ""));
	EXPECT_TRUE(channel.matches(ChannelType::LFG, 1, Race::ELYOS, "anything"));
	EXPECT_FALSE(channel.matches(ChannelType::LFG, 1, Race::ASMODIANS, ""));
	EXPECT_EQ(channel.name(), "LFG (E)");
}

TEST(ChannelTest, LangChannelMatchesItsLanguage) {
	LangChannel channel(1, Race::ELYOS, "Deutsch");
	EXPECT_TRUE(channel.matches(ChannelType::LANG, 1, Race::ELYOS, "Deutsch"));
	EXPECT_FALSE(channel.matches(ChannelType::LANG, 1, Race::ELYOS, "English"));
	EXPECT_EQ(channel.name(), "LANG: Deutsch (E)");
}

TEST(ChannelTest, JobChannelMatchesEveryLocalizedNameOfItsClass) {
	JobChannel channel(1, Race::ELYOS, "Kleriker");
	EXPECT_TRUE(channel.hasAliases());
	for (const char* name : {"Cleric", "Kleriker", "Clérigo", "Chierico", "Clerc", "Kleryk", "Ruhban", "Целитель", "治愈星", "치유성"})
		EXPECT_TRUE(channel.matches(ChannelType::JOB, 1, Race::ELYOS, name)) << name;
	EXPECT_FALSE(channel.matches(ChannelType::JOB, 1, Race::ELYOS, "Chanter"));
	EXPECT_FALSE(channel.matches(ChannelType::JOB, 1, Race::ASMODIANS, "Cleric"));
	EXPECT_EQ(channel.name(), "Cleric (E)"); // the first alias
	// the "[f:" suffix is cut off for the lookup, but a request with it does not match (Java compares the requested name as it is)
	JobChannel female(1, Race::ASMODIANS, "Gladiator[f:1]");
	EXPECT_TRUE(female.hasAliases());
	EXPECT_EQ(female.name(), "Gladiator (A)");
	EXPECT_TRUE(female.matches(ChannelType::JOB, 1, Race::ASMODIANS, "Gladiateur"));
	EXPECT_FALSE(female.matches(ChannelType::JOB, 1, Race::ASMODIANS, "Gladiator[f:1]"));
}

TEST(ChannelTest, JobChannelOfAnUnknownClassHasOnlyItsName) {
	JobChannel channel(1, Race::ELYOS, "Painter");
	EXPECT_FALSE(channel.hasAliases());
	EXPECT_TRUE(channel.matches(ChannelType::JOB, 1, Race::ELYOS, "Painter"));
	EXPECT_EQ(channel.name(), "Painter (E)");
	// Java: "[f:".split("\\[f:") is empty, so [0] throws (after the channel id was taken)
	EXPECT_THROW(JobChannel(1, Race::ELYOS, "[f:"), commons::utils::IndexOutOfBoundsException);
	JobChannel empty(1, Race::ELYOS, "");
	EXPECT_EQ(empty.name(), " (E)");
}

TEST(ChannelTest, ChannelsOfAnUnknownRaceMatchOnlyThatAndHaveNoName) {
	// staff members may request channels of an unknown race id (Race.getById returns null)
	RegionChannel channel(1, std::nullopt, "map");
	EXPECT_TRUE(channel.matches(ChannelType::REGION, 1, std::nullopt, "map"));
	EXPECT_FALSE(channel.matches(ChannelType::REGION, 1, Race::ELYOS, "map"));
	EXPECT_THROW(static_cast<void>(channel.name()), commons::utils::IllegalStateException); // Java: NullPointerException
	EXPECT_THROW(static_cast<void>(JobChannel(1, std::nullopt, "Cleric").name()), commons::utils::IllegalStateException);
}

TEST(ChannelTest, ToStringNamesTheJavaClass) {
	RegionChannel channel(1, Race::ELYOS, "map");
	std::string text = channel.toString();
	EXPECT_EQ(text.rfind("com.aionemu.chatserver.model.channel.RegionChannel@", 0), 0u) << text;
}

} // namespace
} // namespace aion::chatserver::test
