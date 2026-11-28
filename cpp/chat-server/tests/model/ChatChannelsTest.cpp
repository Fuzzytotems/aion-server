// ChatChannels.getOrCreate: parsing of the client's channel identifiers, the race rule and which request gets which channel.

#include <atomic>
#include <barrier>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/channel/ChatChannels.h"
#include "aion/chatserver/model/channel/JobChannel.h"
#include "aion/chatserver/model/channel/LangChannel.h"
#include "aion/chatserver/model/channel/LfgChannel.h"
#include "aion/chatserver/model/channel/RegionChannel.h"
#include "aion/chatserver/model/channel/TradeChannel.h"
#include "aion/chatserver/service/GameServerService.h"
#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/Numbers.h"
#include "support/TestUtils.h"

namespace aion::chatserver::test {
namespace {

using namespace model;
using namespace model::channel;

// upstream b0317f967: channels can only be requested for the registered game server, so the tests' requests use game server 1
const bool gameServer1Registered = [] {
	service::GameServerService::GAMESERVER_ID = 1;
	return true;
}();

std::shared_ptr<ChatClient> player(std::optional<Race> race, int8_t accessLevel = 0) {
	static std::atomic<int32_t> nextId = 1;
	return std::make_shared<ChatClient>(nextId++, Bytes(48), "acc", "Name", race, accessLevel);
}

/** "@<U+0001><type>_<meta><U+0001><gs>.<race>.AION.KOR" with a meta unique to this process */
std::string unique(std::string_view prefix) {
	static std::atomic<int32_t> next = 1;
	return std::string(prefix) + std::to_string(next++);
}

std::string request(std::string_view typeAndMeta, std::string_view restrictions) {
	return "@\x01" + std::string(typeAndMeta) + "\x01" + std::string(restrictions);
}

TEST(ChatChannelsTest, SameRequestGivesTheSameChannel) {
	auto elyos = player(Race::ELYOS);
	std::string meta = unique("map");
	auto channel = ChatChannels::getOrCreate(elyos, request("public_" + meta, "1.0.AION.KOR"));
	ASSERT_NE(channel, nullptr);
	EXPECT_NE(dynamic_cast<RegionChannel*>(channel.get()), nullptr);
	EXPECT_EQ(dynamic_cast<RegionChannel&>(*channel).getMapIdentifier(), meta);
	EXPECT_EQ(ChatChannels::getOrCreate(player(Race::ELYOS), request("public_" + meta, "1.0.AION.KOR")), channel);
	EXPECT_EQ(ChatChannels::getChannelById(channel->getChannelId()), channel);
	// the meta keeps further underscores: Housing_barrack
	auto trade = ChatChannels::getOrCreate(elyos, request("trade_" + meta + "_barrack", "1.0.AION.KOR"));
	ASSERT_NE(dynamic_cast<TradeChannel*>(trade.get()), nullptr);
	EXPECT_EQ(dynamic_cast<TradeChannel&>(*trade).getMapIdentifier(), meta + "_barrack");
}

TEST(ChatChannelsTest, GameServerRaceAndTypeSeparateChannels) {
	auto elyos = player(Race::ELYOS);
	auto asmodian = player(Race::ASMODIANS);
	std::string meta = unique("map");
	auto first = ChatChannels::getOrCreate(elyos, request("public_" + meta, "1.0.AION.KOR"));
	auto otherRace = ChatChannels::getOrCreate(asmodian, request("public_" + meta, "1.1.AION.KOR"));
	auto otherType = ChatChannels::getOrCreate(elyos, request("trade_" + meta, "1.0.AION.KOR"));
	ASSERT_NE(first, nullptr);
	EXPECT_EQ(first->getGameServerId(), 1);
	EXPECT_NE(first, otherRace);
	EXPECT_NE(first, otherType);
	EXPECT_EQ(dynamic_cast<RaceChannel&>(*otherRace).getRace(), Race::ASMODIANS);
}

TEST(ChatChannelsTest, OnlyStaffMayRequestChannelsOfAnotherRace) {
	std::string meta = unique("map");
	EXPECT_EQ(ChatChannels::getOrCreate(player(Race::ELYOS), request("public_" + meta, "1.1.AION.KOR")), nullptr);
	EXPECT_EQ(ChatChannels::getOrCreate(player(Race::ELYOS), request("public_" + meta, "1.7.AION.KOR")), nullptr); // unknown race
	auto gm = player(Race::ELYOS, 1);
	auto asmodianChannel = ChatChannels::getOrCreate(gm, request("public_" + meta, "1.1.AION.KOR"));
	ASSERT_NE(asmodianChannel, nullptr);
	EXPECT_EQ(dynamic_cast<RaceChannel&>(*asmodianChannel).getRace(), Race::ASMODIANS);
	// upstream b0317f967: channels of unknown races are refused, also to staff and to players of an unknown race
	EXPECT_EQ(ChatChannels::getOrCreate(gm, request("public_" + meta, "1.7.AION.KOR")), nullptr);
	EXPECT_EQ(ChatChannels::getOrCreate(player(std::nullopt), request("public_" + meta, "1.7.AION.KOR")), nullptr);
}

TEST(ChatChannelsTest, LfgIsOnePerRaceAndGameServer) {
	auto elyos = player(Race::ELYOS);
	auto lfg = ChatChannels::getOrCreate(elyos, request("partyFind_", "1.0.AION.KOR"));
	ASSERT_NE(dynamic_cast<LfgChannel*>(lfg.get()), nullptr);
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("partyFind_whatever", "1.0.AION.KOR")), lfg);
}

TEST(ChatChannelsTest, JobChannelsAreSharedByAllLanguagesOfAClass) {
	std::string restrictions = "1.1.AION.KOR";
	auto asmodian = player(Race::ASMODIANS);
	auto english = ChatChannels::getOrCreate(asmodian, request("job_Songweaver", restrictions));
	ASSERT_NE(dynamic_cast<JobChannel*>(english.get()), nullptr);
	EXPECT_EQ(ChatChannels::getOrCreate(asmodian, request("job_Barde", restrictions)), english);
	EXPECT_EQ(ChatChannels::getOrCreate(asmodian, request("job_음유성", restrictions)), english);
	EXPECT_NE(ChatChannels::getOrCreate(asmodian, request("job_Chanter", restrictions)), english);
}

TEST(ChatChannelsTest, UnknownClassIsLogged) {
	LogCapture log({"com.aionemu.chatserver.model.channel.ChatChannels"});
	auto elyos = player(Race::ELYOS);
	std::string job = unique("Juggler");
	auto channel = ChatChannels::getOrCreate(elyos, request("job_" + job, "1.0.AION.KOR"));
	ASSERT_NE(channel, nullptr);
	EXPECT_TRUE(log.contains("requested channel for unknown class: " + job)) << log.dump();
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("job_Cleric", "1.0.AION.KOR"))->name(), "Cleric (E)");
	EXPECT_EQ(log.count("unknown class"), 1) << log.dump();
}

TEST(ChatChannelsTest, LanguageChannelsPerLanguage) {
	auto elyos = player(Race::ELYOS);
	std::string language = unique("lang");
	auto channel = ChatChannels::getOrCreate(elyos, request("User_" + language, "1.0.AION.KOR"));
	ASSERT_NE(dynamic_cast<LangChannel*>(channel.get()), nullptr);
	EXPECT_EQ(channel->name(), "LANG: " + language + " (E)");
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("User_" + language, "1.0.AION.KOR")), channel);
}

TEST(ChatChannelsTest, IdentifiersWithoutThreePartsGiveNull) {
	auto elyos = player(Race::ELYOS);
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, "@\x01public_map"), nullptr);
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, "@\x01public_map\x01" "1.0\x01x"), nullptr);
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, ""), nullptr);
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, "\x01"), nullptr); // Java: split gives an empty array
	// trailing empty parts are removed by split: "@|public_map|" has two parts
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, "@\x01public_map\x01"), nullptr);
	// the client is not used before the identifier has three parts (Java would throw a NullPointerException only afterwards)
	EXPECT_EQ(ChatChannels::getOrCreate(nullptr, "@\x01public_map"), nullptr);
	EXPECT_THROW(ChatChannels::getOrCreate(nullptr, request("public_" + unique("map"), "1.0.AION.KOR")), commons::utils::IllegalStateException);
}

// upstream b0317f967: the retail validations, each refused request is logged
TEST(ChatChannelsTest, MalformedRequestsAreRefusedAndLogged) {
	LogCapture log({"com.aionemu.chatserver.model.channel.ChatChannels"});
	auto elyos = player(Race::ELYOS);
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("public", "1.0.AION.KOR")), nullptr);
	EXPECT_TRUE(log.contains("requested a channel without name: ")) << log.dump();
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("", "1.0.AION.KOR")), nullptr); // "".split("_", 2) is [""]
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("private_" + unique("x"), "1.0.AION.KOR")), nullptr);
	EXPECT_TRUE(log.contains("requested a channel of an unknown type: ")) << log.dump();
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("public_" + std::string(65, 'm'), "1.0.AION.KOR")), nullptr);
	EXPECT_TRUE(log.contains("requested a channel with a too long name: ")) << log.dump();
	EXPECT_NE(ChatChannels::getOrCreate(elyos, request("public_" + std::string(64, 'n'), "1.0.AION.KOR")), nullptr);
	for (std::string_view restrictions : {"...", "1", "x.0.AION.KOR", "1. 0.AION.KOR"})
		EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("public_map", restrictions)), nullptr) << restrictions;
	EXPECT_EQ(log.count("requested a channel with malformed restrictions: "), 4) << log.dump();
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("public_map", "2.0.AION.KOR")), nullptr); // not the registered game server
	EXPECT_TRUE(log.contains("requested a channel of game server 2: ")) << log.dump();
	service::GameServerService::GAMESERVER_ID = 99;
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("public_map", "99.0.AION.KOR")), nullptr); // 99 is forbidden
	service::GameServerService::GAMESERVER_ID = 1;
	EXPECT_TRUE(log.contains("requested a channel of game server 99: ")) << log.dump();
	EXPECT_EQ(ChatChannels::getOrCreate(elyos, request("public_map", "1.2.AION.KOR")), nullptr);
	EXPECT_TRUE(log.contains("requested a channel of an unknown race: ")) << log.dump();
}

TEST(ChatChannelsTest, UnknownIdIsNull) {
	EXPECT_EQ(ChatChannels::getChannelById(-1), nullptr);
}

TEST(ChatChannelsTest, ConcurrentRequestsOfANewChannelGetTheSameOne) {
	// finding and creating a channel is one step (deviation: Java can create two channels for the same identifier)
	constexpr size_t THREADS = 8;
	constexpr size_t ROUNDS = 40; // upstream b0317f967: at most 256 channels, and the tests of this process share them
	std::vector<std::string> identifiers;
	for (size_t round = 0; round < ROUNDS; round++)
		identifiers.push_back(request("public_" + unique("race"), "1.0.AION.KOR"));
	std::vector<std::vector<int32_t>> ids(THREADS, std::vector<int32_t>(ROUNDS));
	std::barrier sync(static_cast<ptrdiff_t>(THREADS));
	std::vector<std::thread> threads;
	for (size_t t = 0; t < THREADS; t++) {
		threads.emplace_back([&, t] {
			auto elyos = player(Race::ELYOS);
			for (size_t round = 0; round < ROUNDS; round++) {
				sync.arrive_and_wait();
				ids[t][round] = ChatChannels::getOrCreate(elyos, identifiers[round])->getChannelId();
			}
		});
	}
	for (std::thread& thread : threads)
		thread.join();
	int differing = 0;
	for (size_t round = 0; round < ROUNDS; round++) {
		for (size_t t = 1; t < THREADS; t++) {
			if (ids[t][round] != ids[0][round])
				differing++;
		}
	}
	EXPECT_EQ(differing, 0);
}

} // namespace
} // namespace aion::chatserver::test
