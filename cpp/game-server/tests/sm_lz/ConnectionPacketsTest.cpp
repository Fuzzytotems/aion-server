// The PER_RECIPIENT packets of P4-17 that read the recipient's connection (runtime-architecture.md §8.3), serialized for a real AionConnection
// with an active player: SM_MESSAGE (race filter for staff and non-staff readers, SHOUT coordinates, the length limits), SM_MAIL_SERVICE (the
// mailbox counts and the service variants), SM_PRICES, SM_MARK_FRIENDLIST, SM_UNK_3_5_1, SM_PLAY_MOVIE (its side effect on the recipient),
// SM_PLAYER_SEARCH (the group status and faction prefixed names) and SM_SIEGE_LOCATION_INFO with sieges disabled. Golden bytes written by hand
// from the Java writeImpls.

#include "SmLzTestSupport.h"

#include <chrono>
#include <string>
#include <vector>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/model/ChatType.h"
#include "aion/gameserver/model/gameobjects/Letter.h"
#include "aion/gameserver/model/gameobjects/LetterType.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatus.h"
#include "aion/gameserver/model/gameobjects/player/DeniedStatusInfo.h"
#include "aion/gameserver/model/gameobjects/player/Mailbox.h"
#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/templates/mail/MailMessage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MAIL_SERVICE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MARK_FRIENDLIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SEARCH.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAY_MOVIE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PRICES.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SIEGE_LOCATION_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UNK_3_5_1.h"

namespace aion::gameserver::network::aion::serverpackets::testing {
namespace {

using model::ChatType;
using runtime::Ref;

class ConnectionPacketsTest : public PacketTest {
protected:
	void SetUp() override {
		PacketTest::SetUp();
		PACKET_TEST_SCOPE;
		reader = makePlayer(100001, 9001, "Reader", model::Race::ELYOS);
		connection = std::make_unique<TestConnection>();
		connection->get()->setActivePlayer(runtime::Ptr<model::gameobjects::player::Player>(*reader.player));
	}

	void TearDown() override {
		{
			PACKET_TEST_SCOPE;
			connection.reset();
			reader = {};
		}
		PacketTest::TearDown();
	}

	AionConnection* con() const { return connection->get(); }

	PlayerFixture reader;
	std::unique_ptr<TestConnection> connection;
};

TEST_F(ConnectionPacketsTest, MessageRaceFilterAndShoutCoordinates) {
	PACKET_TEST_SCOPE;
	// the manual constructor has no sender: senderRace 0, no coordinates; writeC(chat type id) writeC(race) writeD writeS writeS
	SM_MESSAGE announcement(0, "", "Server restart", ChatType::BRIGHT_YELLOW_CENTER);
	EXPECT_EQ(announcement.recipients(), AionServerPacket::Recipients::PER_RECIPIENT);
	EXPECT_BYTES(serialized(announcement, con()), Bytes().header(24).C(36).C(0).D(0).S("").S("Server restart").data);

	// an Npc sender: no race filter; SHOUT writes the sender's coordinates of the construction time
	Ref<TestNpc> npc = createNpc(npcTemplate(R"(<npc_template npc_id="203001" level="1" name_id="1" name="guard"/>)"));
	npc->setPosition(world::WorldPosition::create(210010000, 1.5f, 2.5f, 3.5f, int8_t{0}));
	SM_MESSAGE shout(*npc, "Halt!", ChatType::SHOUT);
	npc->setPosition(world::WorldPosition::create(210010000, 9.0f, 9.0f, 9.0f, int8_t{0}));
	EXPECT_BYTES(serialized(shout, con()), Bytes().header(24).C(3).C(0).D(npc->getObjectId()).S("guard").S("Halt!").F(1.5f).F(2.5f).F(3.5f).data);
	EXPECT_BYTES(serialized(SM_MESSAGE(*npc, "hi", ChatType::NORMAL), con()), Bytes().header(24).C(0).C(0).D(npc->getObjectId()).S("guard").S("hi").data);

	// a staff reader always gets race 0 (writeC(activePlayer.isStaff() ? 0 : senderRace)); a reader without active player gets nothing
	reader.account->setAccessLevel(int8_t{1});
	EXPECT_BYTES(serialized(announcement, con()), Bytes().header(24).C(36).C(0).D(0).S("").S("Server restart").data);
	connection->get()->setActivePlayer(nullptr);
	EXPECT_BYTES(serialized(announcement, con()), Bytes().header(24).data);
}

TEST_F(ConnectionPacketsTest, MessageLengthLimits) {
	PACKET_TEST_SCOPE;
	// Java String.length() counts UTF-16 units: 1022 characters are sent unchanged, more than 4000 are cut to 4000 (after a warning)
	std::string limit(1022, 'a');
	EXPECT_BYTES(serialized(SM_MESSAGE(0, "", limit, ChatType::NORMAL), con()), Bytes().header(24).C(0).C(0).D(0).S("").S(limit).data);
	std::string euro;
	for (int i = 0; i < 4001; i++)
		euro += "\xE2\x82\xAC"; // one UTF-16 unit, three UTF-8 bytes
	std::string cut;
	for (int i = 0; i < 4000; i++)
		cut += "\xE2\x82\xAC";
	EXPECT_BYTES(serialized(SM_MESSAGE(0, "", euro, ChatType::NORMAL), con()), Bytes().header(24).C(0).C(0).D(0).S("").S(cut).data);
	std::string between(2000, 'b');
	// warned but not cut
	EXPECT_BYTES(serialized(SM_MESSAGE(0, "", between, ChatType::NORMAL), con()), Bytes().header(24).C(0).C(0).D(0).S("").S(between).data);
}

TEST_F(ConnectionPacketsTest, MailServiceVariants) {
	PACKET_TEST_SCOPE;
	auto mailbox = std::make_unique<model::gameobjects::player::Mailbox>(*reader.player);
	auto timestamp = commons::database::Timestamp(std::chrono::milliseconds(1700000000123LL));
	Ref<model::gameobjects::Letter> express =
		model::gameobjects::Letter::create(500001, 100001, nullptr, 250, "Title", "Body", "Sender", timestamp, true, model::gameobjects::LetterType::EXPRESS);
	Ref<model::gameobjects::Letter> read =
		model::gameobjects::Letter::create(500002, 100001, nullptr, 0, "Old", "", "Admin", timestamp, false, model::gameobjects::LetterType::NORMAL);
	Ref<model::gameobjects::Letter> blackCloud =
		model::gameobjects::Letter::create(500003, 100001, nullptr, 0, "BC", "", "Cloud", timestamp, true, model::gameobjects::LetterType::BLACKCLOUD);
	mailbox->putLetterToMailbox(*express);
	mailbox->putLetterToMailbox(*read);
	mailbox->putLetterToMailbox(*blackCloud);
	reader.player->setMailbox(std::move(mailbox));
	// counts: total 3, unread 2, express 1, black cloud 1

	// 0: writeC(0) writeH(total) writeH(unread) writeH(express) writeH(blackCloud)
	EXPECT_BYTES(serialized(SM_MAIL_SERVICE(), con()), Bytes().header(161).C(0).H(3).H(2).H(1).H(1).data);
	// 1: writeC(1) writeC(mailMessage id) - MailMessage.RECIPIENT_MAILBOX_FULL(2)
	EXPECT_BYTES(serialized(SM_MAIL_SERVICE(model::templates::mail::MailMessage::RECIPIENT_MAILBOX_FULL), con()), Bytes().header(161).C(1).C(2).data);
	// 2: writeD(player) writeC(0) writeH(isLast ? -size : size) and per letter: id, sender, title, isRead, item id, item template, kinah, type
	Bytes list;
	list.header(161).C(2).D(100001).C(0).H(-2);
	list.D(500001).S("Sender").S("Title").C(0).D(0).D(0).Q(250).C(1 /* EXPRESS */);
	list.D(500002).S("Admin").S("Old").C(1).D(0).D(0).Q(0).C(0 /* NORMAL */);
	EXPECT_BYTES(serialized(SM_MAIL_SERVICE(*reader.player, {runtime::Ptr<model::gameobjects::Letter>(*express), runtime::Ptr<model::gameobjects::Letter>(*read)}, true), con()),
		list.data);
	// 3 without an attachment: recipient, total + unread * 0x10000, express + black cloud, id, recipient, sender, title, message, 8+8+4 zero bytes
	Bytes letterRead;
	letterRead.header(161).C(3).D(100001).D(3 + 2 * 0x10000).D(2).D(500001).D(100001).S("Sender").S("Title").S("Body").Q(0).Q(0).D(0);
	letterRead.D(250).D(0).C(0).D(1700000000).C(1);
	EXPECT_BYTES(serialized(SM_MAIL_SERVICE(*reader.player, *express, 1700000000123LL), con()), letterRead.data);
	// 5: writeD(letterId) writeC(attachmentType) writeC(1)
	EXPECT_BYTES(serialized(SM_MAIL_SERVICE(500001, int8_t{2}), con()), Bytes().header(161).C(5).D(500001).C(2).C(1).data);
	// 6: total + unread * 0x10000, express + black cloud, writeH(ids.length), the ids
	const std::vector<int32_t> ids{500002, 500003};
	EXPECT_BYTES(serialized(SM_MAIL_SERVICE(std::span<const int32_t>(ids)), con()), Bytes().header(161).C(6).D(3 + 2 * 0x10000).D(2).H(2).D(500002).D(500003).data);
}

TEST_F(ConnectionPacketsTest, PricesMarkFriendListFastTrackAndMovie) {
	PACKET_TEST_SCOPE;
	detail::PacketLookupsForTests lookups;
	lookups.globalPrices = [](model::Race race) { return race == model::Race::ELYOS ? 110 : 90; };
	lookups.globalPricesModifier = []() { return 100; };
	lookups.taxes = [](model::Race) { return 105; };
	LookupsGuard guard(lookups);
	EXPECT_BYTES(serialized(SM_PRICES(), con()), Bytes().header(252).C(110).C(100).C(105).data);
	EXPECT_BYTES(serialized(SM_MARK_FRIENDLIST(), con()), Bytes().header(279).D(100001).C(1).H(0).data);
	configs::network::NetworkConfig::GAMESERVER_ID = 7;
	EXPECT_BYTES(serialized(SM_UNK_3_5_1(), con()), Bytes().header(150).D(1).D(0).D(100001).D(7).D(0).D(0).data);
	configs::network::NetworkConfig::GAMESERVER_ID = 0;
	// SM_PLAY_MOVIE sets WATCHING_CUTSCENE on the recipient while it is serialized
	EXPECT_FALSE(reader.player->isInCustomState(model::gameobjects::player::CustomPlayerState::WATCHING_CUTSCENE));
	EXPECT_BYTES(serialized(SM_PLAY_MOVIE(false, 300001, 1000, 55, false), con()), Bytes().header(105).C(0).D(300001).D(1000).D(55).C(0).C(1).data);
	EXPECT_TRUE(reader.player->isInCustomState(model::gameobjects::player::CustomPlayerState::WATCHING_CUTSCENE));
	EXPECT_BYTES(serialized(SM_PLAY_MOVIE(true, 0, 0, 1, true), con()), Bytes().header(105).C(1).D(0).D(0).D(1).C(0).C(0).data);
}

TEST_F(ConnectionPacketsTest, PlayerSearchGroupStatusAndFactionPrefix) {
	PACKET_TEST_SCOPE;
	PlayerFixture denying = makePlayer(100002, 9002, "Denying", model::Race::ASMODIANS);
	PlayerFixture looking = makePlayer(100003, 9003, "Looking", model::Race::ELYOS);
	PlayerFixture plain = makePlayer(100004, 9004, "Plain", model::Race::ELYOS);
	for (PlayerFixture* f : {&denying, &looking, &plain})
		f->player->setPlayerSettings(model::gameobjects::player::PlayerSettings::create());
	denying.player->getPlayerSettings()->setDeny(model::gameobjects::player::getId(model::gameobjects::player::DeniedStatus::GROUP));
	looking.player->setLookingForGroup(true);
	denying.player->setPosition(world::WorldPosition::create(220010000, 1.0f, 2.0f, 3.0f, int8_t{0}));
	looking.player->setPosition(world::WorldPosition::create(210010000, 4.0f, 5.0f, 6.0f, int8_t{0}));
	plain.player->setPosition(world::WorldPosition::create(210010000, 7.0f, 8.0f, 9.0f, int8_t{0}));
	SM_PLAYER_SEARCH packet({runtime::Ptr<model::gameobjects::player::Player>(*denying.player), runtime::Ptr<model::gameobjects::player::Player>(*looking.player),
		runtime::Ptr<model::gameobjects::player::Player>(*plain.player)});
	// per player: world, x, y, z, class id, gender id, level, 1 denied / 3 in team / 2 looking / 0, the name in a 27 char field
	Bytes expected;
	expected.header(211).H(3);
	expected.D(220010000).F(1.0f).F(2.0f).F(3.0f).C(0).C(0).C(0).C(1).S("Denying", 27);
	expected.D(210010000).F(4.0f).F(5.0f).F(6.0f).C(0).C(0).C(0).C(2).S("Looking", 27);
	expected.D(210010000).F(7.0f).F(8.0f).F(9.0f).C(0).C(0).C(0).C(0).S("Plain", 27);
	EXPECT_BYTES(serialized(packet, con()), expected.data);
	// a staff reader sees the faction prefixes ChatUtil.ELYOS_NAME_PREFIX '' and ASMO_NAME_PREFIX ''
	reader.account->setAccessLevel(int8_t{3});
	Bytes staff;
	staff.header(211).H(3);
	staff.D(220010000).F(1.0f).F(2.0f).F(3.0f).C(0).C(0).C(0).C(1).S("\xEE\x81\x93" "Denying", 27);
	staff.D(210010000).F(4.0f).F(5.0f).F(6.0f).C(0).C(0).C(0).C(2).S("\xEE\x81\x92" "Looking", 27);
	staff.D(210010000).F(7.0f).F(8.0f).F(9.0f).C(0).C(0).C(0).C(0).S("\xEE\x81\x92" "Plain", 27);
	EXPECT_BYTES(serialized(packet, con()), staff.data);
}

TEST_F(ConnectionPacketsTest, SiegeLocationInfoWithSiegesDisabled) {
	PACKET_TEST_SCOPE;
	configs::main::SiegeConfig::SIEGE_ENABLED = false;
	// writeC(0) writeH(0) whatever the locations are (the no-argument constructor reads the service only in writeImpl)
	EXPECT_BYTES(serialized(SM_SIEGE_LOCATION_INFO(), con()), Bytes().header(209).C(0).H(0).data);
	detail::PacketLookupsForTests lookups;
	lookups.siegeLocations = []() { return std::vector<std::pair<int32_t, runtime::Ptr<model::siege::SiegeLocation>>>{}; };
	lookups.remainingSiegeTimeInSeconds = [](int32_t) { return 0; };
	LookupsGuard guard(lookups);
	configs::main::SiegeConfig::SIEGE_ENABLED = true;
	// enabled without locations: infoType 0, size 0
	EXPECT_BYTES(serialized(SM_SIEGE_LOCATION_INFO(), con()), Bytes().header(209).C(0).H(0).data);
	configs::main::SiegeConfig::SIEGE_ENABLED = false;
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::testing
