// The small model classes: Race, ChannelType, GsAuthResponse, IdFactory and Message.

#include <memory>

#include <gtest/gtest.h>

#include "aion/chatserver/model/ChannelType.h"
#include "aion/chatserver/model/ChatClient.h"
#include "aion/chatserver/model/Race.h"
#include "aion/chatserver/model/channel/LfgChannel.h"
#include "aion/chatserver/model/message/Message.h"
#include "aion/chatserver/network/gameserver/GsAuthResponse.h"
#include "aion/chatserver/utils/IdFactory.h"

namespace aion::chatserver::test {
namespace {

using namespace model;
using Bytes = std::vector<uint8_t>;

TEST(RaceTest, IdsAndLookup) {
	EXPECT_EQ(getRaceId(Race::ELYOS), 0);
	EXPECT_EQ(getRaceId(Race::ASMODIANS), 1);
	EXPECT_EQ(getById(0), Race::ELYOS);
	EXPECT_EQ(getById(1), Race::ASMODIANS);
	EXPECT_EQ(getById(2), std::nullopt);
	EXPECT_EQ(getById(-1), std::nullopt);
	EXPECT_EQ(toString(Race::ASMODIANS), "ASMODIANS");
	EXPECT_EQ(toString(std::nullopt), "null");
}

TEST(ChannelTypeTest, IdentifiersAndLookup) {
	EXPECT_EQ(getByIdentifier("public"), ChannelType::REGION);
	EXPECT_EQ(getByIdentifier("trade"), ChannelType::TRADE);
	EXPECT_EQ(getByIdentifier("partyFind"), ChannelType::LFG);
	EXPECT_EQ(getByIdentifier("job"), ChannelType::JOB);
	EXPECT_EQ(getByIdentifier("User"), ChannelType::LANG);
	EXPECT_EQ(getByIdentifier("user"), std::nullopt); // case sensitive (HashMap)
	EXPECT_EQ(getByIdentifier(""), std::nullopt);
	EXPECT_EQ(name(ChannelType::LFG), "LFG");
}

TEST(GsAuthResponseTest, ResponseIds) {
	using network::gameserver::GsAuthResponse;
	EXPECT_EQ(getResponseId(GsAuthResponse::AUTHED), 0);
	EXPECT_EQ(getResponseId(GsAuthResponse::NOT_AUTHED), 1);
	EXPECT_EQ(getResponseId(GsAuthResponse::ALREADY_REGISTERED), 2);
}

TEST(IdFactoryTest, CountsUpByOne) {
	int32_t first = utils::IdFactory::getInstance().nextId();
	EXPECT_GT(first, 0);
	EXPECT_EQ(utils::IdFactory::getInstance().nextId(), first + 1);
	// channels take their id from it
	channel::LfgChannel channel(1, Race::ELYOS);
	EXPECT_EQ(channel.getChannelId(), first + 2);
}

TEST(MessageTest, TextBytesAndString) {
	auto channel = std::make_shared<channel::LfgChannel>(1, Race::ELYOS);
	auto sender = std::make_shared<ChatClient>(1, Bytes(48), "acc", "Nick", Race::ELYOS, int8_t{0});
	message::Message message(channel, Bytes{'h', 0, 'i', 0}, sender);
	EXPECT_EQ(message.size(), 4);
	EXPECT_EQ(message.getTextString(), "hi");
	message.setText("You can chat again");
	EXPECT_EQ(message.size(), 36);
	EXPECT_EQ(message.getText()[0], 'Y');
	EXPECT_EQ(message.getText()[1], 0);
	EXPECT_EQ(message.getTextString(), "You can chat again");
	EXPECT_EQ(message.getChannel(), channel);
	EXPECT_EQ(message.getSender(), sender);
}

} // namespace
} // namespace aion::chatserver::test
