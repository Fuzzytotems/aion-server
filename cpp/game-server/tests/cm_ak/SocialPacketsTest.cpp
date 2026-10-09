// The friend, block and duel client packets of M5j stage 1 CP1 (P5-15; m5j-plan.md §18.1, S-01/S-02 with their packets): CM_FRIEND_ADD,
// CM_FRIEND_DEL, CM_FRIEND_SET_MEMO, CM_BLOCK_ADD, CM_BLOCK_DEL, CM_BLOCK_SET_REASON and CM_DUEL_REQUEST, read from their Java field order and
// run on real Players of the party fixture (tests/team/P5-10b, by relative path). The lists are filled in memory; what SocialService stores
// (FriendListDAO, BlockListDAO) is tests/playersvc/SocialDuelServiceTest's, against the DAO test database.
//
// Expectations are derived by hand from CM_FRIEND_ADD.java:34-92, CM_FRIEND_DEL.java:24-37, CM_FRIEND_SET_MEMO.java:25-39,
// CM_BLOCK_ADD.java:28-50, CM_BLOCK_DEL.java:27-40, CM_BLOCK_SET_REASON.java:25-41 and CM_DUEL_REQUEST.java:30-38.

#include "../team/P5-10b/TeamTestSupport.h"

#include <string>
#include <string_view>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/model/gameobjects/player/Friend.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BLOCK_ADD.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BLOCK_DEL.h"
#include "aion/gameserver/network/aion/clientpackets/CM_BLOCK_SET_REASON.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DUEL_REQUEST.h"
#include "aion/gameserver/network/aion/clientpackets/CM_FRIEND_ADD.h"
#include "aion/gameserver/network/aion/clientpackets/CM_FRIEND_DEL.h"
#include "aion/gameserver/network/aion/clientpackets/CM_FRIEND_SET_MEMO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BLOCK_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FRIEND_RESPONSE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/services/DuelService.h"
#include "aion/gameserver/world/World.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using network::test::PacketWriter;
using serverpackets::SM_BLOCK_RESPONSE;
using serverpackets::SM_FRIEND_RESPONSE;
using serverpackets::SM_QUESTION_WINDOW;

/** AionClientPacketFactory packets[111], [112], [114], [166], [167], [179], [239] (ClientPacketInfo.gen.inc) */
constexpr int32_t FRIEND_ADD = 111, FRIEND_DEL = 112, DUEL_REQUEST = 114, BLOCK_ADD = 166, BLOCK_DEL = 167, BLOCK_SET_REASON = 179, FRIEND_SET_MEMO = 239;

class SocialPacketsTest : public TeamTest {
protected:
	/** gameserver.friendlist.size: Java's default is 90 (CustomConfig.java:89); the C++ atomic starts at 0 until a config is loaded */
	ConfigScope<int32_t> friendListSize{configs::main::CustomConfig::FRIENDLIST_SIZE, 90};

	void TearDown() override {
		for (Member& m : members) {
			if (services::DuelService::getInstance().isDueling(m.player()))
				services::DuelService::getInstance().loseDuel(m.player());
			m.player().getResponseRequester().denyAll();
			world::World::getInstance().removeObject(*m.f.player);
		}
		TeamTest::TearDown();
	}

	Member& addStoredMember(std::string_view name, float x = 100.0f) {
		Member& m = addMember(name, x);
		world::World::getInstance().storeObject(*m.f.player);
		m.player().getCommonData()->setOnline(true);
		return m;
	}

	template <class P>
	void run(Member& m, int32_t opcode, const std::vector<uint8_t>& body) {
		Driver<P> packet(opcode);
		packet.readAndRun(body, m.client->get());
	}

	/** an in-memory friend (FriendList.addFriend, no DAO) */
	static void befriend(Member& m, Member& other) {
		m.player().getFriendList().addFriend(*model::gameobjects::player::Friend::create(*other.player().getCommonData(), ""));
	}

	/** an in-memory block (BlockList.add, no DAO) */
	static void block(Member& m, Member& other, std::string_view reason = "") {
		m.player().getBlockList()->add(*model::gameobjects::player::BlockedPlayer::create(other.player().getObjectId(), other.player().getName(), reason));
	}
};

/** CM_FRIEND_ADD.java:42-63: every refusal, in the order of the Java checks */
TEST_F(SocialPacketsTest, FriendAddRefusals) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	run<CM_FRIEND_ADD>(a, FRIEND_ADD, PacketWriter().S("nobody").S("hi").data);
	EXPECT_EQ(a.count(std::move(*SM_FRIEND_RESPONSE::TARGET_OFFLINE)), 1) << "no such player online";
	run<CM_FRIEND_ADD>(a, FRIEND_ADD, PacketWriter().S("alpha").S("hi").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_BUDDYLIST_BUSY()), 1) << "himself (Util.convertName)";

	befriend(a, b);
	run<CM_FRIEND_ADD>(a, FRIEND_ADD, PacketWriter().S("Bravo").S("hi").data);
	EXPECT_EQ(a.count(std::move(*SM_FRIEND_RESPONSE::TARGET_ALREADY_FRIEND)), 1);
	a.player().getFriendList().delFriend(b.player().getObjectId());

	block(a, b);
	run<CM_FRIEND_ADD>(a, FRIEND_ADD, PacketWriter().S("Bravo").S("hi").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_BUDDYLIST_NO_BLOCKED_CHARACTER()), 1);
	a.player().getBlockList()->remove(b.player().getObjectId());

	block(b, a);
	run<CM_FRIEND_ADD>(a, FRIEND_ADD, PacketWriter().S("Bravo").S("hi").data);
	EXPECT_EQ(a.count(std::move(*SM_FRIEND_RESPONSE::TARGET_BLOCKED_YOU)), 1);
	EXPECT_EQ(b.count(SM_QUESTION_WINDOW_OPCODE), 0) << "nobody was asked";
}

/** CM_FRIEND_ADD.java:64-90: the question to Bravo with Alpha's message; his no tells Alpha (the packet's sendPacket: Alpha's connection) */
TEST_F(SocialPacketsTest, FriendAddAsksAndADeclineTellsTheAsker) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	run<CM_FRIEND_ADD>(a, FRIEND_ADD, PacketWriter().S("Bravo").S("be my friend").data);
	EXPECT_EQ(b.count(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_BUDDYLIST_ADD_BUDDY_REQUEST, a.player().getObjectId(), 0, "Alpha", "be my friend")), 1);
	run<CM_FRIEND_ADD>(a, FRIEND_ADD, PacketWriter().S("Bravo").S("again").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_BUDDYLIST_BUSY()), 1) << "the question is still open: putRequest refuses a second one";
	clearAll();
	ASSERT_TRUE(b.player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_BUDDYLIST_ADD_BUDDY_REQUEST, 0));
	EXPECT_EQ(a.count(SM_FRIEND_RESPONSE::TARGET_DENIED("Bravo")), 1);
	EXPECT_EQ(a.player().getFriendList().getFriend(b.player().getObjectId()), nullptr);
}

/** CM_FRIEND_DEL.java:29-37, CM_FRIEND_SET_MEMO.java:31-39, CM_BLOCK_DEL.java:32-40, CM_BLOCK_SET_REASON.java:32-41: a name not in the list */
TEST_F(SocialPacketsTest, ANameNotInTheListIsAnswered) {
	Member& a = addStoredMember("Alpha");
	run<CM_FRIEND_DEL>(a, FRIEND_DEL, PacketWriter().S("Bravo").data);
	run<CM_FRIEND_SET_MEMO>(a, FRIEND_SET_MEMO, PacketWriter().S("Bravo").S("memo").data);
	run<CM_BLOCK_DEL>(a, BLOCK_DEL, PacketWriter().S("Bravo").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_BUDDYLIST_NOT_IN_LIST()), 3) << "the two friend packets and CM_BLOCK_DEL";
	run<CM_BLOCK_SET_REASON>(a, BLOCK_SET_REASON, PacketWriter().S("Bravo").S("why").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_BLOCKLIST_NOT_IN_LIST()), 1);
}

/** CM_BLOCK_ADD.java:34-50: himself, a friend, someone already blocked */
TEST_F(SocialPacketsTest, BlockAddRefusals) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	run<CM_BLOCK_ADD>(a, BLOCK_ADD, PacketWriter().S("ALPHA").S("x").data);
	EXPECT_EQ(a.count(SM_BLOCK_RESPONSE(SM_BLOCK_RESPONSE::CANT_BLOCK_SELF, "ALPHA")), 1) << "equalsIgnoreCase";
	befriend(a, b);
	run<CM_BLOCK_ADD>(a, BLOCK_ADD, PacketWriter().S("bravo").S("x").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_BLOCKLIST_NO_BUDDY()), 1) << "a friend (the online Bravo's common data)";
	a.player().getFriendList().delFriend(b.player().getObjectId());
	block(a, b);
	run<CM_BLOCK_ADD>(a, BLOCK_ADD, PacketWriter().S("Bravo").S("x").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_BLOCKLIST_ALREADY_BLOCKED()), 1);
}

/** CM_DUEL_REQUEST.java:35-38: the object id is looked up in the asker's known list */
TEST_F(SocialPacketsTest, DuelRequestFindsThePartnerInTheKnownList) {
	Member& a = addStoredMember("Alpha");
	Member& b = addStoredMember("Bravo");
	run<CM_DUEL_REQUEST>(a, DUEL_REQUEST, PacketWriter().D(b.player().getObjectId()).data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_DUEL_NO_USER_TO_REQUEST()), 1) << "Bravo is not known to Alpha";
	static_cast<TestKnownList&>(a.player().getKnownList()).addForTest(b.player());
	run<CM_DUEL_REQUEST>(a, DUEL_REQUEST, PacketWriter().D(b.player().getObjectId()).data);
	EXPECT_EQ(b.count(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_DUEL_DO_YOU_ACCEPT_REQUEST, 0, 0, "Alpha")), 1);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
