// P4-17 packets that read static data or configs: SM_L2AUTH_LOGIN_CHECK (the retail server tables and the world maps), SM_TELEPORT_LOC (the
// instance flag of the map), SM_QUEST_ACTION (quests with an extra category are not sent), SM_WEATHER, SM_TRADE_IN_LIST, SM_SKILL_COOLDOWN (the
// cooldown of the skill template), SM_UPGRADE_ARCADE (the arcade levels and rewards) and SM_VERSION_CHECK (the config values). Golden bytes
// written by hand from the Java writeImpls; static data is bound from XML text like the holder tests do.

#include "SmLzTestSupport.h"

#include <chrono>
#include <cstddef>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/EventsConfig.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/configs/network/NetworkConfig.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/UpgradeArcadeData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/model/EventTheme.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/event/ArcadeProgress.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.bind.h"
#include "aion/gameserver/model/templates/world/WeatherEntry.h"
#include "aion/gameserver/network/aion/serverpackets/SM_L2AUTH_LOGIN_CHECK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SKILL_COOLDOWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_LOC.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TRADE_IN_LIST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_UPGRADE_ARCADE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_VERSION_CHECK.h"
#include "aion/gameserver/network/aion/serverpackets/SM_WEATHER.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::network::aion::serverpackets::testing {
namespace {

using runtime::Ref;

class StaticDataPacketsTest : public PacketTest {
protected:
	void TearDown() override {
		dataholders::DataManager::WORLD_MAPS_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::SKILL_DATA.resetForTests();
		dataholders::DataManager::UPGRADE_ARCADE_DATA.resetForTests();
		PacketTest::TearDown();
	}

	void publishWorldMaps() {
		xml::LoadContext context;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(context, R"(<world_maps>)"
			R"(<map id="110010000" cName="A" death_level="0" water_level="0" world_size="1024" flags="RECALL" twin_count="2" beginner_twin_count="1"/>)"
			R"(<map id="300030000" cName="B" death_level="0" water_level="0" world_size="1024" flags="RECALL" twin_count="3" instance="true"/>)"
			R"(</world_maps>)"));
	}
};

TEST_F(StaticDataPacketsTest, LoginCheckServerTablesAndWorldMaps) {
	PACKET_TEST_SCOPE;
	publishWorldMaps();
	// SM_L2AUTH_LOGIN_CHECK.writeImpl with the static initializer: serverIdByIndex[i] = i for 1..60 and [66] = 61, serverIndexById[i] = i for
	// 1..60 and [61] = 66
	Bytes expected;
	expected.header(199).D(0).C(0).C(0).C(0).C(0);
	for (int32_t i = 0; i < 128; i++) {
		int32_t serverId = (i >= 1 && i <= 60) ? i : i == 66 ? 61 : 0;
		expected.C(serverId == 0 ? 0 : i).C(serverId).C(serverId);
	}
	for (int32_t serverId = 0; serverId < 64; serverId++) {
		int32_t index = (serverId >= 1 && serverId <= 60) ? serverId : serverId == 61 ? 66 : 0;
		expected.C(index).C(index == 0 ? 0 : serverId).C(index == 0 ? 0 : serverId);
	}
	// writeH(size) and per map writeD(mapId) writeH(isInstance ? 0 : twinCount), then writeS(accountName)
	expected.H(2).D(110010000).H(2).D(300030000).H(0).S("account");
	EXPECT_BYTES(serialized(SM_L2AUTH_LOGIN_CHECK(true, "account")), expected.data);
	Bytes failed = expected;
	failed.data[5] = 1; // writeD(ok ? 0 : 1)
	EXPECT_BYTES(serialized(SM_L2AUTH_LOGIN_CHECK(false, "account")), failed.data);
}

TEST_F(StaticDataPacketsTest, TeleportLocInstanceFlag) {
	PACKET_TEST_SCOPE;
	publishWorldMaps();
	// writeC(portAnimation id) writeD(mapId) writeD(isInstance ? instanceId : mapId) writeF x3 writeC(heading); TeleportAnimation.FADE_OUT_BEAM(1)
	EXPECT_BYTES(serialized(SM_TELEPORT_LOC(110010000, 2, 1.0f, 2.0f, 3.0f, int8_t{30}, model::animations::TeleportAnimation::FADE_OUT_BEAM)),
		Bytes().header(20).C(1).D(110010000).D(110010000).F(1.0f).F(2.0f).F(3.0f).C(30).data);
	EXPECT_BYTES(serialized(SM_TELEPORT_LOC(300030000, 7, 1.0f, 2.0f, 3.0f, int8_t{0}, model::animations::TeleportAnimation::NONE)),
		Bytes().header(20).C(0).D(300030000).D(7).F(1.0f).F(2.0f).F(3.0f).C(0).data);
	EXPECT_THROW(SM_TELEPORT_LOC(400010000, 1, 0.0f, 0.0f, 0.0f, int8_t{0}, model::animations::TeleportAnimation::NONE), runtime::NullPointerException);
}

TEST_F(StaticDataPacketsTest, QuestActionVariantsAndExtraCategory) {
	PACKET_TEST_SCOPE;
	xml::LoadContext context;
	dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(context,
		R"(<quests><quest id="1000"/><quest id="2000" extra_category="COIN_QUEST"/></quests>)"));
	// TIMER: writeC(4) writeD(questId) writeD(timer) writeC(timer > 0); SHARE: writeC(5) writeD writeD(sharer) writeD(alliance); UNK: writeH(1) writeH(0)
	EXPECT_BYTES(serialized(SM_QUEST_ACTION(1000, 900)), Bytes().header(124).C(4).D(1000).D(900).C(1).data);
	EXPECT_BYTES(serialized(SM_QUEST_ACTION(1000, 0)), Bytes().header(124).C(4).D(1000).D(0).C(0).data);
	EXPECT_BYTES(serialized(SM_QUEST_ACTION(1000, 100002, true)), Bytes().header(124).C(5).D(1000).D(100002).D(1).data);
	EXPECT_BYTES(serialized(SM_QUEST_ACTION(1000)), Bytes().header(124).C(6).D(1000).H(1).H(0).data);
	// a quest without template is sent; a quest with an extra category writes nothing after the header
	EXPECT_BYTES(serialized(SM_QUEST_ACTION(3000)), Bytes().header(124).C(6).D(3000).H(1).H(0).data);
	EXPECT_BYTES(serialized(SM_QUEST_ACTION(2000, 900)), Bytes().header(124).data);
}

TEST_F(StaticDataPacketsTest, WeatherAndTradeInList) {
	PACKET_TEST_SCOPE;
	const model::templates::world::WeatherEntry rain(1, 3);
	const model::templates::world::WeatherEntry snow(2, 5);
	const std::vector<const model::templates::world::WeatherEntry*> entries{&rain, &snow};
	EXPECT_BYTES(serialized(SM_WEATHER(entries)), Bytes().header(67).C(0).C(2).C(3).C(5).data);

	PlayerFixture owner = makePlayer(100001, 9001, "Owner");
	Ref<TestNpc> npc = createNpc(npcTemplate(R"(<npc_template npc_id="203001" level="1" name_id="1"/>)"));
	xml::LoadContext context;
	const auto* trade = xml::bindString<model::templates::tradelist::TradeListTemplate>(context,
		R"(<tradelist_template npc_id="203001" npc_type="ABYSS"><tradelist id="11"/><tradelist id="12"/></tradelist_template>)").release();
	// writeD(npc) writeC(tradeNpcType.index()) writeD(buyPriceModifier) writeD(100) writeH(count) and the tab ids; TradeNpcType.ABYSS index 2
	EXPECT_BYTES(serialized(SM_TRADE_IN_LIST(*npc, trade, 110)), Bytes().header(151).D(npc->getObjectId()).C(2).D(110).D(100).H(2).D(11).D(12).data);
	EXPECT_BYTES(serialized(SM_TRADE_IN_LIST(*npc, nullptr, 110)), Bytes().header(151).data);
	const auto* empty = xml::bindString<model::templates::tradelist::TradeListTemplate>(context, R"(<tradelist_template npc_id="203001"/>)").release();
	EXPECT_BYTES(serialized(SM_TRADE_IN_LIST(*npc, empty, 110)), Bytes().header(151).data);
}

TEST_F(StaticDataPacketsTest, SkillCooldownOfOneSkill) {
	PACKET_TEST_SCOPE;
	xml::LoadContext context;
	dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(context,
		R"(<skill_data><skill_template skill_id="1001" name="a" nameId="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE")"
		R"( duration="0" cooldown="300" stack="A"/></skill_data>)"));
	// writeH(size) writeC(notify) and writeH(skillId) writeD(remaining seconds) writeD(template cooldown * 100)
	EXPECT_BYTES(serialized(SM_SKILL_COOLDOWN(1001, 0)), Bytes().header(51).H(1).C(1).H(1001).D(0).D(30000).data);
	int64_t expiration = commons::utils::currentTimeMillis() + 3600 * 1000 + 500;
	std::vector<uint8_t> bytes = serialized(SM_SKILL_COOLDOWN(1001, expiration));
	ASSERT_EQ(bytes.size(), 5u + 3u + 10u);
	int32_t remaining = bytes[10] | bytes[11] << 8 | bytes[12] << 16 | static_cast<int32_t>(static_cast<uint32_t>(bytes[13]) << 24);
	EXPECT_TRUE(remaining == 3600 || remaining == 3599) << remaining;
	// expired: Math.max(0, negative) = 0
	EXPECT_BYTES(serialized(SM_SKILL_COOLDOWN(1001, 1)), Bytes().header(51).H(1).C(1).H(1001).D(0).D(30000).data);
	EXPECT_THROW(static_cast<void>(serialized(SM_SKILL_COOLDOWN(4242, 0))), runtime::NullPointerException) << "unknown skill template";
}

TEST_F(StaticDataPacketsTest, UpgradeArcadeWithProgressAndData) {
	PACKET_TEST_SCOPE;
	xml::LoadContext context;
	dataholders::DataManager::UPGRADE_ARCADE_DATA.publish(xml::bindString<dataholders::UpgradeArcadeData>(context, R"(<arcadelist>)"
		R"(<levels min_resumable_level="2"><level level="1" icon="a" upgrade_chance="100"/><level level="2" icon="b" upgrade_chance="50"/></levels>)"
		R"(<rewards min_level="1"><item item_id="186000001" normal_count="1" frenzy_count="2"/></rewards>)"
		R"(<rewards min_level="2"><item item_id="186000002" normal_count="3" frenzy_count="4"/><item item_id="186000003" normal_count="5" frenzy_count="6"/></rewards>)"
		R"(</arcadelist>)"));
	Ref<model::event::ArcadeProgress> progress = model::event::ArcadeProgress::create(100001);
	// 1: session, frenzy points, the min level of each reward list, the max level, 1, levels * 2, the icons
	EXPECT_BYTES(serialized(SM_UPGRADE_ARCADE(*progress, 77)),
		Bytes().header(298).C(1).D(77).D(progress->getFrenzyPoints()).D(1).D(2).D(2).C(1).C(4).S("a").S("b").data);
	EXPECT_BYTES(serialized(SM_UPGRADE_ARCADE(true, *progress)), Bytes().header(298).C(3).C(1).D(progress->getFrenzyPoints()).data);
	EXPECT_BYTES(serialized(SM_UPGRADE_ARCADE(*progress)), Bytes().header(298).C(4).D(progress->getCurrentLevel()).data);
	configs::main::EventsConfig::ARCADE_RESUME_TOKEN = 3;
	EXPECT_BYTES(serialized(SM_UPGRADE_ARCADE(*progress, false)),
		Bytes().header(298).C(5).D(progress->getCurrentLevel()).C(progress->getResumeLevel() > 0 ? 1 : 0).Q(3).data);
	configs::main::EventsConfig::ARCADE_RESUME_TOKEN = 0;
	// 10: the item count of each reward list, then the items of all lists
	const dataholders::UpgradeArcadeData& data = *dataholders::DataManager::UPGRADE_ARCADE_DATA;
	std::vector<const model::templates::event::upgradearcade::ArcadeRewards*> rewards{&data.getRewards()[0], &data.getRewards()[1]};
	EXPECT_BYTES(serialized(SM_UPGRADE_ARCADE(rewards)),
		Bytes().header(298).C(10).C(1).C(2).D(186000001).Q(1).Q(2).D(186000002).Q(3).Q(4).D(186000003).Q(5).Q(6).data);
}

TEST_F(StaticDataPacketsTest, VersionCheckOfTheInternalVersion) {
	PACKET_TEST_SCOPE;
	using configs::main::GSConfig;
	GSConfig::CHARACTER_LIMIT_COUNT = 8;
	GSConfig::CHARACTER_FACTION_LIMITATION_MODE = 1;
	GSConfig::CHARACTER_CREATION_MODE = 2;
	GSConfig::SERVER_COUNTRY_CODE = 1;
	GSConfig::MIN_SKILL_CAST_INTERVAL_MILLIS = 200;
	GSConfig::CHAT_SERVER_MIN_LEVEL = int8_t{10};
	GSConfig::CHARACTER_REENTRY_TIME = 20;
	GSConfig::ITEM_WRAP_LIMIT = 5;
	configs::main::MembershipConfig::CHARACTER_ADDITIONAL_COUNT = int8_t{10};
	configs::main::MembershipConfig::CHARACTER_ADDITIONAL_ENABLE = int8_t{0};
	configs::network::NetworkConfig::GAMESERVER_ID = 1;
	detail::PacketLookupsForTests lookups;
	lookups.atreianPassportDisabled = []() { return true; };
	LookupsGuard guard(lookups);
	const std::chrono::time_zone* previousZone = GSConfig::TIME_ZONE_ID.load();
	GSConfig::TIME_ZONE_ID = std::chrono::locate_zone("Europe/Berlin");

	std::vector<uint8_t> bytes = serialized(SM_VERSION_CHECK(model::EventTheme::CHRISTMAS));
	// characterLimitCount 10 (the membership count is larger and enabled != 10), LoginServer.gameServerCount starts at 1 before the login
	// server link: (10 * 1 * 0x10) | (1 * 4) | 2 = 0xA6
	ASSERT_EQ(network::loginserver::LoginServer::getInstance().getGameServerCount(), 1);
	Bytes head;
	head.header(0).C(0).C(1).D(150602).D(150326).D(0).D(150317);
	ASSERT_GT(bytes.size(), head.data.size() + 4);
	EXPECT_EQ(std::vector<uint8_t>(bytes.begin(), bytes.begin() + static_cast<std::ptrdiff_t>(head.data.size())), head.data);
	size_t offset = head.data.size() + 4; // START_TIME_SECONDS
	Bytes middle;
	middle.C(0).C(1).C(0).C(0xA6);
	EXPECT_EQ(std::vector<uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.begin() + static_cast<std::ptrdiff_t>(offset + 4)), middle.data);
	offset += 4 + 4; // PacketGenTimeOnServ
	Bytes tail;
	tail.H(200).C(1).C(10).C(1).C(10).C(10).C(20).C(20).C(1).H(2).C(20).D(1 /* CHRISTMAS */).C(0).D(-utils::time::ServerTime::getStandardOffset());
	tail.C(4).D(40014200).C(1).D(0).D(0).H(3000).H(1).C(0).C(1).D(-utils::time::ServerTime::getDaylightSavings()).C(1).C(1).D(0).C(0);
	tail.C(1 /* passport disabled */).C(0).C(0).C(0).C(0).C(0).C(0).D(5);
	for (int i = 0; i < 11; i++)
		tail.D(1000);
	tail.C(0).F(3.0f).H(0); // no chat server address
	EXPECT_BYTES(std::vector<uint8_t>(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end()), tail.data);

	GSConfig::CHARACTER_LIMIT_COUNT = 0;
	GSConfig::CHARACTER_FACTION_LIMITATION_MODE = 0;
	GSConfig::CHARACTER_CREATION_MODE = 0;
	GSConfig::SERVER_COUNTRY_CODE = 0;
	GSConfig::MIN_SKILL_CAST_INTERVAL_MILLIS = 0;
	GSConfig::CHAT_SERVER_MIN_LEVEL = int8_t{0};
	GSConfig::CHARACTER_REENTRY_TIME = 0;
	GSConfig::ITEM_WRAP_LIMIT = 0;
	configs::main::MembershipConfig::CHARACTER_ADDITIONAL_COUNT = int8_t{0};
	configs::network::NetworkConfig::GAMESERVER_ID = 0;
	GSConfig::TIME_ZONE_ID = previousZone;
}

} // namespace
} // namespace aion::gameserver::network::aion::serverpackets::testing
