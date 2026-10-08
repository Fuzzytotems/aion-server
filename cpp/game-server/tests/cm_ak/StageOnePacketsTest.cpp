// P5-15 client packets of M5j stage 1 CP5 (m5j-plan.md §18.1, S-05): CM_GF_WEBSHOP_TOKEN_REQUEST, CM_CAPTCHA, CM_ABYSS_RANKING_PLAYERS,
// CM_ABYSS_RANKING_LEGIONS and CM_CHARACTER_EDIT's ticket check, read from their bytes and run on a party-fixture member (tests/team/P5-10b,
// by relative path, as SocialPacketsTest). The abyss ranking cache has no database here: its lists are empty.
//
// Not driven: CM_ATREIAN_PASSPORT's run (AtreianPassportService.takeReward is PR #132's, not on this stack), CM_CHANGE_CHANNEL and
// CM_CHARACTER_EDIT's run (an enter world; the gate's real client does both). Expectations from the Java files of the packets.

#include "../team/P5-10b/TeamTestSupport.h"

#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/network/aion/clientpackets/CM_ABYSS_RANKING_LEGIONS.h"
#include "aion/gameserver/network/aion/clientpackets/CM_ABYSS_RANKING_PLAYERS.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CAPTCHA.h"
#include "aion/gameserver/network/aion/clientpackets/CM_CHARACTER_EDIT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_GF_WEBSHOP_TOKEN_REQUEST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_LEGIONS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABYSS_RANKING_PLAYERS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CAPTCHA.h"
#include "aion/gameserver/network/aion/serverpackets/SM_GF_WEBSHOP_TOKEN_RESPONSE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Array.h"
#include "aion/gameserver/services/abyss/AbyssRankingCache.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::team {
namespace {

using network::test::PacketWriter;
using model::gameobjects::player::AbyssRank_AbyssRankUpdateType;

class StageOnePacketsTest : public TeamTest {
protected:
	template <class P>
	void run(Member& m, int32_t opcode, const std::vector<uint8_t>& body) {
		Driver<P> packet(opcode);
		packet.readAndRun(body, m.client->get());
	}
};

/** CM_GF_WEBSHOP_TOKEN_REQUEST.java:23-26: an empty token */
TEST_F(StageOnePacketsTest, TheWebshopTokenIsEmpty) {
	Member& a = addMember("Alpha");
	run<CM_GF_WEBSHOP_TOKEN_REQUEST>(a, 229, {});
	EXPECT_EQ(a.count(serverpackets::SM_GF_WEBSHOP_TOKEN_RESPONSE("")), 1);
}

/** CM_CAPTCHA.java:52-82: /ExtractStatus tells the restriction; a right word lifts it, a wrong one sends the next captcha */
TEST_F(StageOnePacketsTest, TheCaptchaAnswerLiftsOrExtendsTheRestriction) {
	Member& a = addMember("Alpha");
	ConfigScope<int32_t> ban(configs::main::SecurityConfig::CAPTCHA_EXTRACTION_BAN_TIME, 300);
	ConfigScope<int32_t> add(configs::main::SecurityConfig::CAPTCHA_EXTRACTION_BAN_ADD_TIME, 60);
	run<CM_CAPTCHA>(a, 14, PacketWriter().C(4).data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_NOT_RESTRICTED()), 1);

	a.player().setCaptchaWord("ABC123");
	a.player().setCaptchaImage(runtime::Array<int8_t>::of({1, 2}));
	a.clearSent();
	run<CM_CAPTCHA>(a, 14, PacketWriter().C(2).C(1).S("wrong").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_UNRESTRICT_FAILED_RETRY(2)), 1);
	EXPECT_EQ(a.count(serverpackets::SM_CAPTCHA(false, 360)), 1) << "300 + 60 * 1";
	EXPECT_TRUE(a.player().isGatherRestricted());
	EXPECT_GE(a.player().getGatherRestrictionDurationSeconds(), 355);

	a.clearSent();
	run<CM_CAPTCHA>(a, 14, PacketWriter().C(4).data);
	EXPECT_EQ(a.count(opcodeOf<SM_SYSTEM_MESSAGE>), 1) << "STR_MSG_CAPTCHA_RESTRICTED(seconds)";
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_NOT_RESTRICTED()), 0);

	a.clearSent();
	run<CM_CAPTCHA>(a, 14, PacketWriter().C(2).C(1).S("abc123").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_UNRESTRICT()), 1) << "equalsIgnoreCase";
	EXPECT_EQ(a.count(serverpackets::SM_CAPTCHA(true, 0)), 1);
	EXPECT_FALSE(a.player().isGatherRestricted());

	a.player().setCaptchaWord("ABC123");
	a.player().setGatherRestrictionExpirationTime(commons::utils::currentTimeMillis() + 60'000);
	a.clearSent();
	run<CM_CAPTCHA>(a, 14, PacketWriter().C(2).C(3).S("wrong").data);
	EXPECT_EQ(a.count(SM_SYSTEM_MESSAGE::STR_MSG_CAPTCHA_UNRESTRICT_FAILED()), 1) << "the third wrong answer";
	EXPECT_EQ(a.count(opcodeOf<serverpackets::SM_CAPTCHA>), 0);
}

/** CM_CAPTCHA.java:59: an answer without a captcha word is Java's NullPointerException */
TEST_F(StageOnePacketsTest, AnAnswerWithoutACaptchaThrows) {
	Member& a = addMember("Alpha");
	Driver<CM_CAPTCHA> packet(14);
	std::vector<uint8_t> body = PacketWriter().C(2).C(0).S("x").data;
	packet.setBuffer(commons::utils::ByteBuffer::wrap(body));
	packet.setConnection(a.client->get());
	ASSERT_TRUE(packet.read());
	EXPECT_THROW(packet.runNow(), runtime::NullPointerException);
}

/**
 * CM_ABYSS_RANKING_PLAYERS.java:38-67, CM_ABYSS_RANKING_LEGIONS.java:38-68: a list already sent answers the short packet with the cache's
 * update time; an invalid race only logs
 */
TEST_F(StageOnePacketsTest, AnAlreadySentRankingAnswersTheUpdateTime) {
	Member& a = addMember("Alpha");
	const int32_t lastUpdate = services::abyss::AbyssRankingCache::getInstance().getLastUpdate();
	a.player().setAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType::PLAYER_ASMODIANS);
	run<CM_ABYSS_RANKING_PLAYERS>(a, 188, PacketWriter().C(1).data);
	EXPECT_EQ(a.count(serverpackets::SM_ABYSS_RANKING_PLAYERS(lastUpdate, model::Race::ASMODIANS)), 1);

	a.player().setAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType::LEGION_ELYOS);
	a.clearSent();
	run<CM_ABYSS_RANKING_LEGIONS>(a, 118, PacketWriter().C(0).data);
	EXPECT_EQ(a.count(serverpackets::SM_ABYSS_RANKING_LEGIONS(lastUpdate, model::Race::ELYOS)), 1);

	a.clearSent();
	network::test::LogCapture capture({"com.aionemu.gameserver.network.aion.clientpackets.CM_ABYSS_RANKING_PLAYERS"});
	run<CM_ABYSS_RANKING_PLAYERS>(a, 188, PacketWriter().C(2).data);
	EXPECT_EQ(capture.count("Received invalid raceId (2)"), 1) << capture.dump();
	EXPECT_TRUE(a.sent().empty());
}

/** CM_ABYSS_RANKING_PLAYERS.java:60-65: a list not yet sent is the cache's packets (none without a database), then marked sent */
TEST_F(StageOnePacketsTest, AFirstRankingRequestMarksTheListSent) {
	Member& a = addMember("Alpha");
	ASSERT_FALSE(a.player().isAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType::PLAYER_ELYOS));
	run<CM_ABYSS_RANKING_PLAYERS>(a, 188, PacketWriter().C(0).data);
	EXPECT_TRUE(a.player().isAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType::PLAYER_ELYOS));
	EXPECT_FALSE(a.player().isAbyssRankListUpdated(AbyssRank_AbyssRankUpdateType::PLAYER_ASMODIANS));
}

/** CM_CHARACTER_EDIT.java:62-72: a plastic surgery ticket or a gender switch ticket, checked or taken */
TEST_F(StageOnePacketsTest, TheEditTicketIsCheckedOrTaken) {
	Member& a = addMember("Alpha");
	EXPECT_FALSE(CM_CHARACTER_EDIT::checkOrRemoveTicket(a.player(), false, false));
	EXPECT_FALSE(CM_CHARACTER_EDIT::checkOrRemoveTicket(a.player(), true, true));
	giveKinah(a, 720950, 5); // the kinah is no ticket
	EXPECT_FALSE(CM_CHARACTER_EDIT::checkOrRemoveTicket(a.player(), false, false));
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::team
