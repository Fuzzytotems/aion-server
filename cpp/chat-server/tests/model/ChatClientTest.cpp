// ChatClient: its channels per type, the flood protection times and the gag.

#include <chrono>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/channel/JobChannel.h"
#include "aion/chatserver/model/channel/LfgChannel.h"
#include "aion/chatserver/model/channel/RegionChannel.h"
#include "aion/chatserver/model/channel/TradeChannel.h"
#include "aion/commons/utils/TimeUtils.h"

namespace aion::chatserver::test {
namespace {

using namespace model;
using namespace model::channel;
using Bytes = std::vector<uint8_t>;

TEST(ChatClientTest, OneChannelPerType) {
	ChatClient c(1, Bytes(48), "acc", "Nick", Race::ELYOS, int8_t{0});
	auto poeta = std::make_shared<RegionChannel>(1, Race::ELYOS, "Poeta");
	auto sanctum = std::make_shared<RegionChannel>(1, Race::ELYOS, "Sanctum");
	auto trade = std::make_shared<TradeChannel>(1, Race::ELYOS, "Sanctum");
	c.addChannel(poeta);
	c.addChannel(trade);
	EXPECT_TRUE(c.isInChannel(*poeta));
	EXPECT_TRUE(c.isInChannel(*trade));
	c.addChannel(sanctum); // replaces the other region channel (teleport)
	EXPECT_FALSE(c.isInChannel(*poeta));
	EXPECT_TRUE(c.isInChannel(*sanctum));
	EXPECT_TRUE(c.isInChannel(*trade));
}

TEST(ChatClientTest, TwoJobChannelsThenTheThirdReplacesBoth) {
	ChatClient c(1, Bytes(48), "acc", "Nick", Race::ELYOS, int8_t{0});
	auto warrior1 = std::make_shared<JobChannel>(1, Race::ELYOS, "Gladiator");
	auto warrior2 = std::make_shared<JobChannel>(1, Race::ELYOS, "Templar");
	auto third = std::make_shared<JobChannel>(1, Race::ELYOS, "Cleric");
	c.addChannel(warrior1);
	c.addChannel(warrior2);
	EXPECT_TRUE(c.isInChannel(*warrior1));
	EXPECT_TRUE(c.isInChannel(*warrior2));
	c.addChannel(third);
	EXPECT_FALSE(c.isInChannel(*warrior1));
	EXPECT_FALSE(c.isInChannel(*warrior2));
	EXPECT_TRUE(c.isInChannel(*third));
}

TEST(ChatClientTest, RemoveChannelComparesIds) {
	ChatClient c(1, Bytes(48), "acc", "Nick", Race::ELYOS, int8_t{0});
	auto lfg = std::make_shared<LfgChannel>(1, Race::ELYOS);
	auto other = std::make_shared<LfgChannel>(1, Race::ELYOS);
	EXPECT_FALSE(c.removeChannel(lfg)); // no channel of the type
	c.addChannel(lfg);
	EXPECT_FALSE(c.removeChannel(other));
	EXPECT_FALSE(c.removeChannel(nullptr));
	EXPECT_TRUE(c.removeChannel(lfg));
	EXPECT_FALSE(c.isInChannel(*lfg));
	EXPECT_FALSE(c.removeChannel(lfg));
}

TEST(ChatClientTest, FloodProtectionSecondsPerType) {
	ChatClient c(1, Bytes(48), "acc", "Nick", Race::ELYOS, int8_t{0});
	EXPECT_EQ(c.getLastMessageTime(ChannelType::REGION), 0);
	EXPECT_EQ(c.nextMessageTimeSec(ChannelType::REGION), 0);
	int64_t before = commons::utils::currentTimeMillis();
	c.updateLastMessageTime(ChannelType::REGION);
	c.updateLastMessageTime(ChannelType::LFG);
	c.updateLastMessageTime(ChannelType::TRADE);
	c.updateLastMessageTime(ChannelType::JOB);
	EXPECT_GE(c.getLastMessageTime(ChannelType::REGION), before);
	// in the same millisecond a 30 s delay would still be 30 s: let a few milliseconds pass, the remaining time rounds down
	std::this_thread::sleep_for(std::chrono::milliseconds(20));
	EXPECT_EQ(c.nextMessageTimeSec(ChannelType::REGION), 1); // (1000 - elapsed) / 1000 rounds down to 0, at least 1
	EXPECT_EQ(c.nextMessageTimeSec(ChannelType::JOB), 1);
	EXPECT_EQ(c.nextMessageTimeSec(ChannelType::LFG), 29);
	EXPECT_EQ(c.nextMessageTimeSec(ChannelType::TRADE), 29);
	EXPECT_EQ(c.nextMessageTimeSec(ChannelType::LANG), 0); // per type
}

TEST(ChatClientTest, GagTimeIsAPointInTime) {
	ChatClient c(1, Bytes(48), "acc", "Nick", Race::ELYOS, int8_t{0});
	EXPECT_FALSE(c.isGagged());
	c.setGagTime(commons::utils::currentTimeMillis() + 60000);
	EXPECT_TRUE(c.isGagged());
	c.setGagTime(commons::utils::currentTimeMillis() - 1);
	EXPECT_FALSE(c.isGagged());
	c.setGagTime(60000); // a duration, as the Java game server sends it, is in the past
	EXPECT_FALSE(c.isGagged());
	c.setGagTime(0);
	EXPECT_FALSE(c.isGagged());
	EXPECT_EQ(c.getGagTime(), 0);
}

TEST(ChatClientTest, AccessorsAndToString) {
	ChatClient c(42, Bytes{1, 2, 3}, "Account", "Nick", Race::ASMODIANS, int8_t{3});
	EXPECT_EQ(c.getClientId(), 42);
	EXPECT_EQ(c.getToken(), (Bytes{1, 2, 3}));
	EXPECT_EQ(c.getAccountName(), "Account");
	EXPECT_EQ(c.getName(), "Nick");
	EXPECT_EQ(c.getAccessLevel(), 3);
	EXPECT_EQ(c.getIdentifier(), std::nullopt);
	EXPECT_EQ(c.getChannelHandler(), nullptr);
	c.setIdentifier(Bytes{'N', 0});
	EXPECT_EQ(c.getIdentifier(), (Bytes{'N', 0}));
	EXPECT_EQ(c.toString(), "Player [name=Nick, id=42, race=ASMODIANS]");
	EXPECT_EQ(ChatClient(1, Bytes(), "a", "n", std::nullopt, int8_t{0}).toString(), "Player [name=n, id=1, race=null]");
}

} // namespace
} // namespace aion::chatserver::test
