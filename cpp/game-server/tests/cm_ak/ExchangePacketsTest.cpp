// M5c K-01 (m5c-plan.md §5, P5-15): the six player-to-player exchange packets - CM_EXCHANGE_REQUEST (C_ASK_XCHG) with its answer handler,
// CM_EXCHANGE_ADD_ITEM (C_ADD_XCHG), CM_EXCHANGE_ADD_KINAH (C_XCHG_GOLD), CM_EXCHANGE_LOCK (C_CHECK_XCHG), CM_EXCHANGE_OK (C_ACCEPT_XCHG) and
// CM_EXCHANGE_CANCEL (C_CANCEL_XCHG).
//
// Java: CM_EXCHANGE_REQUEST.java:34-99, CM_EXCHANGE_ADD_ITEM.java:23-32, CM_EXCHANGE_ADD_KINAH.java:21-28, CM_EXCHANGE_LOCK.java:20-28,
// CM_EXCHANGE_OK.java:20-28, CM_EXCHANGE_CANCEL.java:20-28. The read cases lay each body out from the Java readImpl; the run cases drive runImpl
// on the economy packet fixture (EconomyPacketTestSupport.h) with a second player "Partner" in the World, each with his own connection:
// - CM_EXCHANGE_REQUEST's refusals in Java order (no such player or himself, a dead player on either side, 5 m apart or more - the range is
//   center to center and strict, PositionUtil.isInRange -, the asker hidden, the other hidden, the other race, trade denied), the question to
//   the other player and his answer: yes registers the exchange (ExchangeService.registerExchange, T-02), no tells the asker; a second question
//   while the first is open is refused.
// - The five exchange packets hand their player and fields to the trade lane's ExchangeService (T-02): the offered kinah and item count, the
//   lock, a one-sided confirmation (the partner's SM_EXCHANGE_CONFIRMATION(2); a trade needs both and writes the inventories to the database,
//   which tests/economy/P5-09b/ExchangeServiceTest.cpp covers), the cancel.

#include "EconomyPacketTestSupport.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/model/gameobjects/player/PlayerSettings.h"
#include "aion/gameserver/model/gameobjects/state/CreatureVisualState.h"
#include "aion/gameserver/model/trade/Exchange.h"
#include "aion/gameserver/model/trade/ExchangeItem.h"
#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_ADD_ITEM.h"
#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_ADD_KINAH.h"
#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_CANCEL.h"
#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_LOCK.h"
#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_OK.h"
#include "aion/gameserver/network/aion/clientpackets/CM_EXCHANGE_REQUEST.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/ExchangeService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_EXCHANGE_REQUEST_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_EXCHANGE_ADD_ITEM_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_EXCHANGE_ADD_KINAH_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_EXCHANGE_LOCK_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_EXCHANGE_OK_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_EXCHANGE_CANCEL_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friend CM_EXCHANGE_ADD_KINAH.h declares: the field readImpl decoded, which Java keeps private */
struct CM_EXCHANGE_ADD_KINAHTestAccess {
	static int64_t kinahCount(const CM_EXCHANGE_ADD_KINAH& p) { return p.kinahCount; }
};

namespace testing::items {
namespace {

using model::gameobjects::player::Player;
using model::gameobjects::state::CreatureVisualState;
using network::test::LogCapture;
using serverpackets::SM_QUESTION_WINDOW;
using serverpackets::SM_SYSTEM_MESSAGE;
using services::ExchangeService;

const char* CM_EXCHANGE_REQUEST_LOGGER = "com.aionemu.gameserver.network.aion.clientpackets.CM_EXCHANGE_REQUEST";

/** the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:91-97, State.IN_GAME) */
constexpr int32_t CM_EXCHANGE_REQUEST_OPCODE = 63;
constexpr int32_t CM_EXCHANGE_ADD_ITEM_OPCODE = 64;
constexpr int32_t CM_EXCHANGE_ADD_KINAH_OPCODE = 66;
constexpr int32_t CM_EXCHANGE_LOCK_OPCODE = 67;
constexpr int32_t CM_EXCHANGE_OK_OPCODE = 68;
constexpr int32_t CM_EXCHANGE_CANCEL_OPCODE = 69;

constexpr int32_t PARTNER = 710303;
constexpr int32_t POTIONS = 800001;
constexpr int32_t A_KINAH = 800009;

// --- readImpl ---------------------------------------------------------------------------------------------------------------------------------

TEST(ExchangePacketsReadTest, TheRequestReadsTheTarget) {
	int32_t unread = -1;
	auto p = readAlone<CM_EXCHANGE_REQUEST>(CM_EXCHANGE_REQUEST_OPCODE, PacketWriter().D(0x01020304).data, unread);
	ASSERT_NE(p, nullptr);
	ASSERT_TRUE(p->targetObjectId.has_value());
	EXPECT_EQ(*p->targetObjectId, 0x01020304);
	EXPECT_EQ(unread, 0);
}

TEST(ExchangePacketsReadTest, AddItemReadsTheObjectIdAndASignedIntCount) {
	int32_t unread = -1;
	auto p = readAlone<CM_EXCHANGE_ADD_ITEM>(CM_EXCHANGE_ADD_ITEM_OPCODE, PacketWriter().D(0x0A0B0C0D).D(-1).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(p->itemObjId, 0x0A0B0C0D);
	EXPECT_EQ(p->itemCount, -1) << "readD: a count the service refuses (itemCount < 1)";
	EXPECT_EQ(unread, 0);
}

TEST(ExchangePacketsReadTest, AddKinahReadsALong) {
	int32_t unread = -1;
	auto p = readAlone<CM_EXCHANGE_ADD_KINAH>(CM_EXCHANGE_ADD_KINAH_OPCODE, PacketWriter().Q(0x0102030405060708LL).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_EXCHANGE_ADD_KINAHTestAccess::kinahCount(*p), 0x0102030405060708LL);
	EXPECT_EQ(unread, 0);
}

TEST(ExchangePacketsReadTest, LockOkAndCancelReadNothing) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	const std::vector<uint8_t> fourBytes = PacketWriter().D(0x11223344).data;
	int32_t unread = -1;
	ASSERT_NE(readAlone<CM_EXCHANGE_LOCK>(CM_EXCHANGE_LOCK_OPCODE, fourBytes, unread), nullptr);
	EXPECT_EQ(unread, 4);
	ASSERT_NE(readAlone<CM_EXCHANGE_OK>(CM_EXCHANGE_OK_OPCODE, fourBytes, unread), nullptr);
	EXPECT_EQ(unread, 4);
	ASSERT_NE(readAlone<CM_EXCHANGE_CANCEL>(CM_EXCHANGE_CANCEL_OPCODE, fourBytes, unread), nullptr);
	EXPECT_EQ(unread, 4);
	ASSERT_NE(readAlone<CM_EXCHANGE_LOCK>(CM_EXCHANGE_LOCK_OPCODE, {}, unread), nullptr);
	EXPECT_EQ(unread, 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(ExchangePacketsReadTest, TheMarkersRegisterTheClassesUnderTheirJavaOpcodes) {
	const StateSet inGame{AionConnection_State::IN_GAME};
	EXPECT_NE(dynamic_cast<CM_EXCHANGE_REQUEST*>(CM_EXCHANGE_REQUEST_clientPacketFactory(CM_EXCHANGE_REQUEST_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_EXCHANGE_ADD_ITEM*>(CM_EXCHANGE_ADD_ITEM_clientPacketFactory(CM_EXCHANGE_ADD_ITEM_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_EXCHANGE_ADD_KINAH*>(CM_EXCHANGE_ADD_KINAH_clientPacketFactory(CM_EXCHANGE_ADD_KINAH_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_EXCHANGE_LOCK*>(CM_EXCHANGE_LOCK_clientPacketFactory(CM_EXCHANGE_LOCK_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_EXCHANGE_OK*>(CM_EXCHANGE_OK_clientPacketFactory(CM_EXCHANGE_OK_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_EXCHANGE_CANCEL*>(CM_EXCHANGE_CANCEL_clientPacketFactory(CM_EXCHANGE_CANCEL_OPCODE, inGame).get()), nullptr);
	EXPECT_EQ(economyTableEntries("CM_EXCHANGE_REQUEST", CM_EXCHANGE_REQUEST_OPCODE), 1);
	EXPECT_EQ(economyTableEntries("CM_EXCHANGE_ADD_ITEM", CM_EXCHANGE_ADD_ITEM_OPCODE), 1);
	EXPECT_EQ(economyTableEntries("CM_EXCHANGE_ADD_KINAH", CM_EXCHANGE_ADD_KINAH_OPCODE), 1);
	EXPECT_EQ(economyTableEntries("CM_EXCHANGE_LOCK", CM_EXCHANGE_LOCK_OPCODE), 1);
	EXPECT_EQ(economyTableEntries("CM_EXCHANGE_OK", CM_EXCHANGE_OK_OPCODE), 1);
	EXPECT_EQ(economyTableEntries("CM_EXCHANGE_CANCEL", CM_EXCHANGE_CANCEL_OPCODE), 1);
}

// --- runImpl ----------------------------------------------------------------------------------------------------------------------------------

class ExchangePacketsTest : public EconomyPacketTest {
protected:
	/** "Partner" `x` on the x axis (the holder stands at 100) */
	OtherPlayer& partnerAt(float x, model::Race race = model::Race::ELYOS) { return otherPlayer(PARTNER, "Partner", race, x); }

	void request(int32_t target) { readAndRun<CM_EXCHANGE_REQUEST>(CM_EXCHANGE_REQUEST_OPCODE, PacketWriter().D(target).data); }

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serializedFor(std::move(packet)); }

	/** The question the asked player gets (CM_EXCHANGE_REQUEST.java:94-95) */
	std::vector<uint8_t> question(OtherPlayer& asked) {
		return asked.serializedFor(SM_QUESTION_WINDOW(SM_QUESTION_WINDOW::STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE, 0, 0, player().getName()));
	}

	/** The holder asks the partner, who answers `response` (CM_QUESTION_RESPONSE: ResponseRequester.respond) */
	void requestAndAnswer(OtherPlayer& partner, int32_t response) {
		request(PARTNER);
		ASSERT_TRUE(partner.player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE, response));
	}

	/** Nothing was asked or refused: neither player got a packet */
	void expectNothingSent(OtherPlayer& partner) {
		EXPECT_TRUE(sent().empty());
		EXPECT_TRUE(partner.sent().empty());
		EXPECT_FALSE(ExchangeService::getInstance().isPlayerInExchange(player()));
	}
};

// :43-46: no player of that id in the World, or the asker himself - here the partner, who stands in the World, so that only the equals check
// refuses him
TEST_F(ExchangePacketsTest, NobodyToExchangeWithIsRefused) {
	OtherPlayer& partner = partnerAt(102.0f);

	request(999999);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_EXCHANGE_NO_ONE_TO_EXCHANGE())}));

	readAndRun<CM_EXCHANGE_REQUEST>(CM_EXCHANGE_REQUEST_OPCODE, PacketWriter().D(PARTNER).data, partner.client->get());
	EXPECT_EQ(partner.sent(), exactly({partner.serializedFor(SM_SYSTEM_MESSAGE::STR_EXCHANGE_NO_ONE_TO_EXCHANGE())}));
	EXPECT_EQ(sent().size(), 1u);
}

// :48-51: a dead player on either side is logged and nobody is told
TEST_F(ExchangePacketsTest, ADeadTargetIsOnlyLogged) {
	OtherPlayer& partner = partnerAt(102.0f);
	partner.player().setLifeStats(std::make_unique<DeadPlayerLifeStats>(partner.player()));
	LogCapture capture({CM_EXCHANGE_REQUEST_LOGGER});

	request(PARTNER);

	EXPECT_EQ(capture.count("CM_EXCHANGE_REQUEST dead players target from 710101 to 710303"), 1) << capture.dump();
	expectNothingSent(partner);
}

TEST_F(ExchangePacketsTest, ADeadAskerIsOnlyLoggedBeforeTheRangeAndTheHide) {
	OtherPlayer& partner = partnerAt(120.0f); // too far as well
	player().setLifeStats(std::make_unique<DeadPlayerLifeStats>(player()));
	partner.player().setVisualState(CreatureVisualState::HIDE1); // and hidden
	LogCapture capture({CM_EXCHANGE_REQUEST_LOGGER});

	request(PARTNER);

	EXPECT_EQ(capture.count("CM_EXCHANGE_REQUEST dead players target from 710101 to 710303"), 1) << capture.dump();
	expectNothingSent(partner);
}

// :53-56: 5 m or more apart (center to center, strict) is too far; the range comes before the hide checks
TEST_F(ExchangePacketsTest, FiveMetresApartIsTooFar) {
	OtherPlayer& partner = partnerAt(105.0f);
	partner.player().setVisualState(CreatureVisualState::HIDE1);

	request(PARTNER);

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_EXCHANGE_TOO_FAR_TO_EXCHANGE())}));
	EXPECT_TRUE(partner.sent().empty());
}

TEST_F(ExchangePacketsTest, JustUnderFiveMetresTheOtherPlayerIsAsked) {
	OtherPlayer& partner = partnerAt(104.9f);

	request(PARTNER);

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_EXCHANGE_ASKED_EXCHANGE_TO_HIM("Partner"))}));
	EXPECT_EQ(partner.sent(), exactly({question(partner)}));
}

// :58-66: the asker hidden, then the other hidden (the asker's own hide is named first)
TEST_F(ExchangePacketsTest, AHiddenPlayerOnEitherSideIsRefused) {
	OtherPlayer& partner = partnerAt(102.0f);

	player().setVisualState(CreatureVisualState::HIDE1);
	partner.player().setVisualState(CreatureVisualState::HIDE1);
	request(PARTNER);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_EXCHANGE_WHILE_INVISIBLE())}));

	clearSent();
	player().unsetVisualState(CreatureVisualState::HIDE1);
	request(PARTNER);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_EXCHANGE_WITH_INVISIBLE_USER())}));
	EXPECT_TRUE(partner.sent().empty());
}

// :68-71: the other race is logged and nobody is told; it comes before the other player's trade denial
TEST_F(ExchangePacketsTest, AnotherRaceIsOnlyLogged) {
	OtherPlayer& partner = partnerAt(102.0f, model::Race::ASMODIANS);
	partner.player().getPlayerSettings()->setDeny(2); // DeniedStatus.TRADE
	LogCapture capture({CM_EXCHANGE_REQUEST_LOGGER});

	request(PARTNER);

	EXPECT_TRUE(capture.contains("[AUDIT] Player Holder tried trade with player (Partner) another race.")) << capture.dump();
	expectNothingSent(partner);
}

// :73-76: the other player denies trades
TEST_F(ExchangePacketsTest, ADeniedTradeIsRefusedWithTheOtherPlayersName) {
	OtherPlayer& partner = partnerAt(102.0f);
	partner.player().getPlayerSettings()->setDeny(2); // DeniedStatus.TRADE

	request(PARTNER);

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_REJECTED_TRADE("Partner"))}));
	EXPECT_TRUE(partner.sent().empty());
}

// :91-98 and the answer handler :78-89: the question goes to the other player; his yes registers the exchange of both
TEST_F(ExchangePacketsTest, HisYesStartsTheExchange) {
	OtherPlayer& partner = partnerAt(102.0f);

	request(PARTNER);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_EXCHANGE_ASKED_EXCHANGE_TO_HIM("Partner"))}));
	EXPECT_EQ(partner.sent(), exactly({question(partner)}));
	clearSent();
	partner.clearSent();

	ASSERT_TRUE(partner.player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE, 1));

	// ExchangeService.registerExchange(requester, responder): the responder is told first, the requester second (ExchangeService.java:50-51)
	EXPECT_EQ(sent(), exactly({exchangeRequest("Partner")}));
	EXPECT_EQ(partner.sent(), exactly({exchangeRequest("Holder")}));
	runtime::Ptr<model::trade::Exchange> mine = ExchangeService::getInstance().getCurrentParnterExchange(partner.player());
	ASSERT_TRUE(mine);
	EXPECT_EQ(mine->getActiveplayer().get(), &player()) << "the requester's exchange";
	EXPECT_EQ(mine->getTargetPlayer().get(), &partner.player());
}

TEST_F(ExchangePacketsTest, HisNoTellsTheAsker) {
	OtherPlayer& partner = partnerAt(102.0f);
	request(PARTNER);
	clearSent();
	partner.clearSent();

	ASSERT_TRUE(partner.player().getResponseRequester().respond(SM_QUESTION_WINDOW::STR_EXCHANGE_DO_YOU_ACCEPT_EXCHANGE, 0));

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_EXCHANGE_HE_REJECTED_EXCHANGE("Partner"))}));
	EXPECT_TRUE(partner.sent().empty());
	EXPECT_FALSE(ExchangeService::getInstance().isPlayerInExchange(player()));
}

// :91-97: ResponseRequester.putRequest refuses a second question of the same id while the first is open
TEST_F(ExchangePacketsTest, ASecondRequestWhileHeIsAskedIsRefused) {
	OtherPlayer& partner = partnerAt(102.0f);

	request(PARTNER);
	request(PARTNER);

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_EXCHANGE_ASKED_EXCHANGE_TO_HIM("Partner")),
						  message(SM_SYSTEM_MESSAGE::STR_EXCHANGE_CANT_ASK_WHEN_HE_IS_ASKED_QUESTION("Partner"))}));
	EXPECT_EQ(partner.sent(), exactly({question(partner)}));
}

// CM_EXCHANGE_ADD_KINAH, _ADD_ITEM, _LOCK, _OK and _CANCEL hand the connection's player and their fields to ExchangeService
TEST_F(ExchangePacketsTest, TheExchangePacketsDriveTheSendersExchange) {
	OtherPlayer& partner = partnerAt(102.0f);
	stored(POTIONS, MINOR_LIFE_POTION, 100);
	stored(A_KINAH, KINAH, 1000);
	requestAndAnswer(partner, 1);
	ExchangeService& exchanges = ExchangeService::getInstance();
	runtime::Ptr<model::trade::Exchange> mine = exchanges.getCurrentParnterExchange(partner.player());
	ASSERT_TRUE(mine);
	clearSent();
	partner.clearSent();

	// the kinah: the partner's own exchange stays empty
	readAndRun<CM_EXCHANGE_ADD_KINAH>(CM_EXCHANGE_ADD_KINAH_OPCODE, PacketWriter().Q(150).data);
	EXPECT_EQ(mine->getKinahCount(), 150);
	EXPECT_EQ(sent(), exactly({exchangeAddKinah(150, 0)}));
	EXPECT_EQ(partner.sent(), exactly({exchangeAddKinah(150, 1)}));
	EXPECT_EQ(exchanges.getCurrentParnterExchange(player())->getKinahCount(), 0);

	// 3 of the 100 potions
	clearSent();
	partner.clearSent();
	readAndRun<CM_EXCHANGE_ADD_ITEM>(CM_EXCHANGE_ADD_ITEM_OPCODE, PacketWriter().D(POTIONS).D(3).data);
	ASSERT_EQ(mine->getItems().size(), 1);
	runtime::Ptr<model::trade::ExchangeItem> offered = mine->getItems().get(POTIONS);
	ASSERT_TRUE(offered);
	EXPECT_EQ(offered->getItemCount(), 3);
	EXPECT_EQ(packetsOf(partner.sent(), SM_EXCHANGE_ADD_ITEM_OPCODE).size(), 1u);

	// the lock: the partner is told (3)
	partner.clearSent();
	readAndRun<CM_EXCHANGE_LOCK>(CM_EXCHANGE_LOCK_OPCODE, {});
	EXPECT_TRUE(mine->isLocked());
	EXPECT_EQ(partner.sent(), exactly({exchangeConfirmation(3)}));

	// the partner confirms alone: the holder is told (2), nothing is traded
	clearSent();
	readAndRun<CM_EXCHANGE_OK>(CM_EXCHANGE_OK_OPCODE, {}, partner.client->get());
	EXPECT_TRUE(exchanges.getCurrentParnterExchange(player())->isConfirmed());
	EXPECT_FALSE(mine->isConfirmed());
	EXPECT_EQ(sent(), exactly({exchangeConfirmation(2)}));
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 100);

	// the holder cancels: the partner is told (1) and neither is in an exchange any more
	partner.clearSent();
	readAndRun<CM_EXCHANGE_CANCEL>(CM_EXCHANGE_CANCEL_OPCODE, {});
	EXPECT_EQ(partner.sent(), exactly({exchangeConfirmation(1)}));
	EXPECT_FALSE(exchanges.isPlayerInExchange(player()));
	EXPECT_FALSE(exchanges.isPlayerInExchange(partner.player()));
	EXPECT_EQ(countOf(player(), MINOR_LIFE_POTION), 100);
	EXPECT_EQ(kinahOf(player()), 1000);
}

// CM_EXCHANGE_OK.java:26: the service takes the player as it is (confirmExchange checks null): a connection without one does nothing
TEST_F(ExchangePacketsTest, AConfirmationWithoutAPlayerDoesNothing) {
	TestClient loggedOut;
	EconomyDriver<CM_EXCHANGE_OK> packet(CM_EXCHANGE_OK_OPCODE);
	ASSERT_TRUE(packet.readOn({}, loggedOut.get()));
	EXPECT_NO_THROW(packet.runNow());
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
