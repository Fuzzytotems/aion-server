// M5c T-04, W-28 (m5c-plan.md §2.9): the `isTrading()` refusals that ExchangeService.registerExchange makes reachable for the first time. Each
// case puts A into an exchange with B, shows the refusal, cancels the exchange and shows the same call working (the control), so the case
// fails if the refusal is gone or if the exchange never made A trading:
// - ItemMoveService.moveItem and switchItemsInStorages (ItemMoveService.java:48, :103): the item stays where it is;
// - ItemSplitService.splitItem (ItemSplitService.java:34): STR_MSG_INVENTORY_SPLIT_DURING_TRADE;
// - CM_SHOW_DIALOG and CM_DIALOG_SELECT (CM_SHOW_DIALOG.java:33-34, CM_DIALOG_SELECT.java:62-63): the dialog is ignored;
// - CM_QUESTION_RESPONSE (CM_QUESTION_RESPONSE.java:41-42): a "yes" to any question cancels the exchange, a "no" does not.
// PlayerRestrictions.canTrade's isTrading arm is TradeServiceTest.ATradingPlayerNeitherBuysNorSells and
// ExchangeTest.ASecondRequestDuringAnExchangeIsRefusedAndChangesNothing.
//
// The packets are driven through their readImpl and runImpl (tests/cm_ak/InWorldPacketRunSupport.h's Driver) against the merchant of
// TradeTestSupport.h, which gets GeneralNpcAI's dialog hooks as in tests/cm_lz/DialogPacketsTest.cpp (the handler library is not linked here).

#include "TradeTestSupport.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/handler/TalkEventHandler.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/network/aion/clientpackets/CM_DIALOG_SELECT.h"
#include "aion/gameserver/network/aion/clientpackets/CM_QUESTION_RESPONSE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SHOW_DIALOG.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/item/ItemMoveService.h"
#include "aion/gameserver/services/item/ItemSplitService.h"

namespace aion::gameserver::economy::test::trade {
namespace {

using network::aion::clientpackets::CM_DIALOG_SELECT;
using network::aion::clientpackets::CM_QUESTION_RESPONSE;
using network::aion::clientpackets::CM_SHOW_DIALOG;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::ExchangeService;

// ClientPacketInfo.gen.inc (Java AionClientPacketFactory, State.IN_GAME)
constexpr int32_t CM_QUESTION_RESPONSE_OPCODE = 50;
constexpr int32_t CM_SHOW_DIALOG_OPCODE = 52;
constexpr int32_t CM_DIALOG_SELECT_OPCODE = 54;
// ServerPacketsOpcodes.java
constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60;
constexpr int32_t SM_TRADELIST_OPCODE = 253;

constexpr int32_t POTIONS = 910001; // 100 x Minor Life Potion
constexpr int32_t STORED = 910002;  // a Training Sword in the warehouse

/** GeneralNpcAI's dialog hooks (GeneralNpcAI.java:49-57) on the plain NpcAI of this executable */
class TalkingNpcAI final : public ai::NpcAI {
public:
	explicit TalkingNpcAI(Npc& owner) : NpcAI(owner) {}

protected:
	void handleDialogStart(Player& player) override { ai::handler::TalkEventHandler::onTalk(*this, player); }

	void handleDialogFinish(Player& player) override { ai::handler::TalkEventHandler::onFinishTalk(*this, player); }
};

class TradingRefusalsTest : public TradeTest {
protected:
	void SetUp() override {
		TradeTest::SetUp();
		give(a(), POTIONS, MINOR_LIFE_POTION, 100);
		give(a(), STORED, TRAINING_SWORD, 1, StorageType::REGULAR_WAREHOUSE);
		// the keys the dialog packets read, at their Java defaults (AdminConfig.java:59-60): a player without access sees no dialog info
		savedDialogInfo = configs::administration::AdminConfig::DIALOG_INFO.exchange(9);
		xml::LoadContext context;
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(context, MERCHANT_NPC_TEMPLATES_XML));
	}

	void TearDown() override {
		if (f.player)
			f.player->setTarget(nullptr);
		TradeTest::TearDown();
		dataholders::DataManager::NPC_DATA.resetForTests();
		configs::administration::AdminConfig::DIALOG_INFO.store(savedDialogInfo);
	}

	void trade() {
		ExchangeService::getInstance().registerExchange(a(), partner());
		ASSERT_TRUE(a().isTrading());
		clearSent();
		clearSentB();
	}

	void cancel() {
		ExchangeService::getInstance().cancelExchange(a());
		ASSERT_FALSE(a().isTrading());
		clearSent();
		clearSentB();
	}

	int8_t id(StorageType type) { return static_cast<int8_t>(model::items::storage::getId(type)); }

	bool inStorage(StorageType type, int32_t objId) {
		return static_cast<bool>(a().getStorage(model::items::storage::getId(type))->getItemByObjId(objId));
	}

	/** The merchant with the talking AI, known to A */
	Npc& talkingMerchant() {
		Npc& npc = merchant();
		auto ai = std::make_unique<TalkingNpcAI>(npc);
		TalkingNpcAI& talking = *ai;
		npc.replaceAi(std::move(ai));
		talking.setStateIfNot(ai::AIState::IDLE);
		f.knownList().addForTest(npc);
		clearSent();
		return npc;
	}

	void showDialog(int32_t target) {
		cp::Driver<CM_SHOW_DIALOG> packet(CM_SHOW_DIALOG_OPCODE);
		packet.readAndRun(PacketWriter().D(target).data, client->get());
	}

	void dialogSelect(int32_t target, int32_t action) {
		cp::Driver<CM_DIALOG_SELECT> packet(CM_DIALOG_SELECT_OPCODE);
		packet.readAndRun(PacketWriter().D(target).H(action).H(0).H(0).D(0).H(0).data, client->get());
	}

	void answer(int32_t questionId, int32_t response) {
		cp::Driver<CM_QUESTION_RESPONSE> packet(CM_QUESTION_RESPONSE_OPCODE);
		packet.readAndRun(PacketWriter().D(questionId).C(response).C(1).H(0).D(0).D(0).H(0).data, client->get());
	}

	int8_t savedDialogInfo = 0;
};

// ItemMoveService.java:43-51
TEST_F(TradingRefusalsTest, NoItemMovesToAnotherStorageWhileTrading) {
	trade();
	services::item::ItemMoveService::moveItem(a(), POTIONS, id(StorageType::CUBE), id(StorageType::REGULAR_WAREHOUSE), -1);
	EXPECT_TRUE(inStorage(StorageType::CUBE, POTIONS));
	EXPECT_FALSE(inStorage(StorageType::REGULAR_WAREHOUSE, POTIONS));

	cancel();
	services::item::ItemMoveService::moveItem(a(), POTIONS, id(StorageType::CUBE), id(StorageType::REGULAR_WAREHOUSE), -1);
	EXPECT_FALSE(inStorage(StorageType::CUBE, POTIONS)) << "the control";
	EXPECT_TRUE(inStorage(StorageType::REGULAR_WAREHOUSE, POTIONS));
}

// ItemMoveService.java:98-109
TEST_F(TradingRefusalsTest, NoItemsAreSwitchedBetweenStoragesWhileTrading) {
	trade();
	services::item::ItemMoveService::switchItemsInStorages(a(), id(StorageType::CUBE), POTIONS, id(StorageType::REGULAR_WAREHOUSE), STORED);
	EXPECT_TRUE(inStorage(StorageType::CUBE, POTIONS));
	EXPECT_TRUE(inStorage(StorageType::REGULAR_WAREHOUSE, STORED));

	cancel();
	services::item::ItemMoveService::switchItemsInStorages(a(), id(StorageType::CUBE), POTIONS, id(StorageType::REGULAR_WAREHOUSE), STORED);
	EXPECT_TRUE(inStorage(StorageType::REGULAR_WAREHOUSE, POTIONS)) << "the control";
	EXPECT_TRUE(inStorage(StorageType::CUBE, STORED));
}

// ItemSplitService.java:30-37
TEST_F(TradingRefusalsTest, NoStackIsSplitWhileTrading) {
	trade();
	services::item::ItemSplitService::splitItem(a(), POTIONS, 0, 10, 5, id(StorageType::CUBE), id(StorageType::CUBE));
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_INVENTORY_SPLIT_DURING_TRADE())}));
	EXPECT_EQ(a().getInventory().getItemsByItemId(MINOR_LIFE_POTION).size(), 1u);

	cancel();
	services::item::ItemSplitService::splitItem(a(), POTIONS, 0, 10, 5, id(StorageType::CUBE), id(StorageType::CUBE));
	EXPECT_EQ(a().getInventory().getItemsByItemId(MINOR_LIFE_POTION).size(), 2u) << "the control";
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 100);
}

// CM_SHOW_DIALOG.java:33-34
TEST_F(TradingRefusalsTest, NoDialogStartsWhileTrading) {
	Npc& npc = talkingMerchant();
	trade();
	showDialog(npc.getObjectId());
	EXPECT_TRUE(sent().empty());

	cancel();
	showDialog(npc.getObjectId());
	EXPECT_EQ(ofOpcode(sent(), SM_DIALOG_WINDOW_OPCODE).size(), 1u) << "the control: the merchant's page";
}

// CM_DIALOG_SELECT.java:62-63
TEST_F(TradingRefusalsTest, NoDialogActionIsSelectedWhileTrading) {
	Npc& npc = talkingMerchant();
	trade();
	dialogSelect(npc.getObjectId(), model::DialogAction::BUY);
	EXPECT_TRUE(sent().empty());

	cancel();
	dialogSelect(npc.getObjectId(), model::DialogAction::BUY);
	EXPECT_EQ(ofOpcode(sent(), SM_TRADELIST_OPCODE).size(), 1u) << "the control: the merchant's trade list";
}

// CM_QUESTION_RESPONSE.java:39-43: answering "yes" to any question during an exchange cancels it; "no" does not
TEST_F(TradingRefusalsTest, AYesDuringAnExchangeCancelsItAndANoDoesNot) {
	trade();
	answer(123456, 0);
	EXPECT_TRUE(a().isTrading());
	EXPECT_TRUE(sentB().empty());

	answer(123456, 1);
	EXPECT_FALSE(a().isTrading());
	EXPECT_FALSE(partner().isTrading());
	EXPECT_EQ(ofOpcode(sentB(), SM_EXCHANGE_CONFIRMATION_OPCODE), cp::exactly({exchangeConfirmation(1)}));
}

} // namespace
} // namespace aion::gameserver::economy::test::trade
