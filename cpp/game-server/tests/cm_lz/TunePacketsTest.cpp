// M5c K-02 (m5c-plan.md §5, P5-16): CM_TUNE (C_IDENTIFY_ITEM), the identification of an unidentified item or the re-identification of an
// identified one with a tuning scroll, and CM_TUNE_RESULT (C_ANSWER_REIDENTIFY), the player's answer to the re-identified values.
//
// Java: CM_TUNE.java:25-53, CM_TUNE_RESULT.java:28-50. The read cases lay each body out from the Java readImpl; the run cases drive runImpl on
// the economy packet fixture (EconomyPacketTestSupport.h) into the player-items lane's ItemActionService.identifyItem / applyTuneResult and
// TuningAction (P-07). The rows are item_templates.xml's, verbatim: Modor's Sword (:13282, rnd_count 3: tunable), the Mythic Weapon and Armor
// Tuning Scrolls (:839186, :839191), a Minor Life Potion as a "scroll" whose actions hold no tuning, and a Sparkie Carapace Fragment as one
// without actions. What the called bodies then do (the 5 s tasks, the rolls, SM_TUNE_RESULT) is tests/itemsvc/IdentificationTest.cpp's; here
// the first packet of each body shows which body ran and on which item: identifyItem broadcasts SM_ITEM_USAGE_ANIMATION(item, 5000, 9)
// (ItemActionService.java:25), TuningAction.act SM_ITEM_USAGE_ANIMATION(scroll, 5000, 12) (TuningAction.java:65-66). A pending tune result takes
// the stat bonus 0 (no random bonus set is loaded; Item.setBonusStats(0) clears the effect).

#include "../cm_ak/EconomyPacketTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.bind.h"
#include "aion/gameserver/dataholders/ItemRandomBonusData.h"
#include "aion/gameserver/model/items/PendingTuneResult.h"
#include "aion/gameserver/network/aion/clientpackets/CM_TUNE.h"
#include "aion/gameserver/network/aion/clientpackets/CM_TUNE_RESULT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

std::unique_ptr<AionClientPacket> CM_TUNE_clientPacketFactory(int32_t opcode, const StateSet& validStates);
std::unique_ptr<AionClientPacket> CM_TUNE_RESULT_clientPacketFactory(int32_t opcode, const StateSet& validStates);

/** The friends the two headers declare: the fields readImpl decoded, which Java keeps private */
struct CM_TUNETestAccess {
	static int32_t itemObjectId(const CM_TUNE& p) { return p.itemObjectId; }
	static int32_t tuningScrollObjectId(const CM_TUNE& p) { return p.tuningScrollObjectId; }
};

struct CM_TUNE_RESULTTestAccess {
	static int32_t itemObjectId(const CM_TUNE_RESULT& p) { return p.itemObjectId; }
	static bool hasAccepted(const CM_TUNE_RESULT& p) { return p.hasAccepted; }
};

namespace testing::items {
namespace {

using namespace std::chrono_literals;
using model::items::PendingTuneResult;
using network::test::LogCapture;
using serverpackets::SM_INVENTORY_UPDATE_ITEM;
using serverpackets::SM_SYSTEM_MESSAGE;

/** the decoded opcodes of ClientPacketInfo.gen.inc (Java AionClientPacketFactory.java:263, :266; State.IN_GAME) */
constexpr int32_t CM_TUNE_OPCODE = 235;
constexpr int32_t CM_TUNE_RESULT_OPCODE = 238;

constexpr int32_t SWORD = 900001;         // Modor's Sword
constexpr int32_t WEAPON_SCROLL = 900002; // Mythic Weapon Tuning Scroll
constexpr int32_t ARMOR_SCROLL = 900003;  // Mythic Armor Tuning Scroll
constexpr int32_t POTION = 900004;        // Minor Life Potion: actions without a tuning
constexpr int32_t FRAGMENT = 900005;      // Sparkie Carapace Fragment: no actions
constexpr int32_t SECOND_SWORD = 900006;  // another Modor's Sword

TEST(TunePacketsReadTest, TuneReadsTheItemAndTheScroll) {
	int32_t unread = -1;
	auto p = readAlone<CM_TUNE>(CM_TUNE_OPCODE, PacketWriter().D(0x0A0B0C0D).D(0x01020304).data, unread);
	ASSERT_NE(p, nullptr);
	EXPECT_EQ(CM_TUNETestAccess::itemObjectId(*p), 0x0A0B0C0D);
	EXPECT_EQ(CM_TUNETestAccess::tuningScrollObjectId(*p), 0x01020304);
	EXPECT_EQ(unread, 0);
}

TEST(TunePacketsReadTest, TheAnswerIsAcceptedForExactlyOne) {
	int32_t unread = -1;
	auto yes = readAlone<CM_TUNE_RESULT>(CM_TUNE_RESULT_OPCODE, PacketWriter().D(0x0A0B0C0D).C(1).data, unread);
	ASSERT_NE(yes, nullptr);
	EXPECT_EQ(CM_TUNE_RESULTTestAccess::itemObjectId(*yes), 0x0A0B0C0D);
	EXPECT_TRUE(CM_TUNE_RESULTTestAccess::hasAccepted(*yes));
	EXPECT_EQ(unread, 0);
	for (int32_t answer : {0, 2, 0xFF}) {
		auto no = readAlone<CM_TUNE_RESULT>(CM_TUNE_RESULT_OPCODE, PacketWriter().D(1).C(answer).data, unread);
		ASSERT_NE(no, nullptr);
		EXPECT_FALSE(CM_TUNE_RESULTTestAccess::hasAccepted(*no)) << answer;
	}
}

TEST(TunePacketsReadTest, TheMarkersRegisterTheClassesUnderTheirJavaOpcodes) {
	const StateSet inGame{AionConnection_State::IN_GAME};
	EXPECT_NE(dynamic_cast<CM_TUNE*>(CM_TUNE_clientPacketFactory(CM_TUNE_OPCODE, inGame).get()), nullptr);
	EXPECT_NE(dynamic_cast<CM_TUNE_RESULT*>(CM_TUNE_RESULT_clientPacketFactory(CM_TUNE_RESULT_OPCODE, inGame).get()), nullptr);
	EXPECT_EQ(economyTableEntries("CM_TUNE", CM_TUNE_OPCODE), 1);
	EXPECT_EQ(economyTableEntries("CM_TUNE_RESULT", CM_TUNE_RESULT_OPCODE), 1);
}

class TunePacketsTest : public EconomyPacketTest {
protected:
	void SetUp() override {
		EconomyPacketTest::SetUp();
		// the 5 s tasks roll a stat bonus (TuningAction.getRandomStatBonusIdFor): the holder has no set of Modor's Sword (rnd_bonus 119), so it is 0.
		// The XSD asks for one set: an unused id
		if (!dataholders::DataManager::ITEM_RANDOM_BONUSES) {
			xml::LoadContext context;
			dataholders::DataManager::ITEM_RANDOM_BONUSES.publish(xml::bindString<dataholders::ItemRandomBonusData>(context,
				R"(<random_bonuses><random_bonus type="INVENTORY" id="999999"><modifiers chance="1.0">)"
				R"(<add name="MAXHP" value="1" bonus="true"/></modifiers></random_bonus></random_bonuses>)"));
			publishedRandomBonuses = true;
		}
	}

	void TearDown() override {
		EconomyPacketTest::TearDown();
		if (publishedRandomBonuses)
			dataholders::DataManager::ITEM_RANDOM_BONUSES.resetForTests();
	}

	void tune(int32_t item, int32_t scroll) { readAndRun<CM_TUNE>(CM_TUNE_OPCODE, PacketWriter().D(item).D(scroll).data); }

	bool publishedRandomBonuses = false;

	void answer(int32_t item, int32_t accepted) { readAndRun<CM_TUNE_RESULT>(CM_TUNE_RESULT_OPCODE, PacketWriter().D(item).C(accepted).data); }

	std::vector<uint8_t> message(SM_SYSTEM_MESSAGE&& packet) { return serializedFor(std::move(packet)); }

	/** An identified Modor's Sword (tune count 0) with a pending result of 1 socket and enchant bonus 2 */
	Item& swordWithPendingResult(bool attributeOnly) {
		Item& sword = storedTuned(SWORD, MODORS_SWORD, 0);
		sword.setPendingTuneResult(runtime::Ptr<PendingTuneResult>(PendingTuneResult::create(1, 2, 0, attributeOnly)));
		return sword;
	}
};

// CM_TUNE.java:36-38
TEST_F(TunePacketsTest, AnItemNotInTheCubeIsNotTuned) {
	stored(WEAPON_SCROLL, MYTHIC_WEAPON_TUNING_SCROLL, 1);

	tune(SWORD, WEAPON_SCROLL);

	EXPECT_TRUE(sent().empty());
}

// :40-41: an unidentified item is identified, whatever scroll the body names
TEST_F(TunePacketsTest, AnUnidentifiedItemIsIdentified) {
	storedTuned(SWORD, MODORS_SWORD, -1);
	stored(WEAPON_SCROLL, MYTHIC_WEAPON_TUNING_SCROLL, 1);

	tune(SWORD, WEAPON_SCROLL);

	EXPECT_EQ(sent(), exactly({usageAnimation(player().getObjectId(), SWORD, MODORS_SWORD, 5000, 9, 0)}));
}

// :42-49: an identified item and a weapon tuning scroll: TuningAction.canAct(player, scroll, item), then act
TEST_F(TunePacketsTest, AWeaponScrollTunesAnIdentifiedWeapon) {
	storedTuned(SWORD, MODORS_SWORD, 0);
	stored(WEAPON_SCROLL, MYTHIC_WEAPON_TUNING_SCROLL, 1);

	tune(SWORD, WEAPON_SCROLL);

	EXPECT_EQ(sent(), exactly({usageAnimation(player().getObjectId(), WEAPON_SCROLL, MYTHIC_WEAPON_TUNING_SCROLL, 5000, 12, 0)}));
}

// :48: canAct refuses an armor scroll on a weapon and names the scroll first, then the item (TuningAction.java:48-51)
TEST_F(TunePacketsTest, AnArmorScrollIsRefusedWithBothNames) {
	Item& sword = storedTuned(SWORD, MODORS_SWORD, 0);
	Item& scroll = stored(ARMOR_SCROLL, MYTHIC_ARMOR_TUNING_SCROLL, 1);

	tune(SWORD, ARMOR_SCROLL);

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_WRONG_SELECT(scroll.getL10n(), sword.getL10n()))}));
}

// :43-45: the scroll must be in the cube
TEST_F(TunePacketsTest, AScrollNotInTheCubeIsNotUsed) {
	storedTuned(SWORD, MODORS_SWORD, 0);
	LogCapture capture({AUDIT_LOGGER});

	tune(SWORD, WEAPON_SCROLL);

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(capture.dump(), "");
}

// :47-48: a scroll whose actions hold no tuning does nothing
TEST_F(TunePacketsTest, AScrollWithoutATuningActionDoesNothing) {
	storedTuned(SWORD, MODORS_SWORD, 0);
	stored(POTION, MINOR_LIFE_POTION, 1);

	tune(SWORD, POTION);

	EXPECT_TRUE(sent().empty());
}

// :47: getActions() is dereferenced unchecked - a scroll without actions is Java's NullPointerException, with the JVM's helpful message
TEST_F(TunePacketsTest, AScrollWithoutActionsIsJavasNullPointerException) {
	storedTuned(SWORD, MODORS_SWORD, 0);
	stored(FRAGMENT, SPARKIE_CARAPACE_FRAGMENT, 1);

	try {
		tune(SWORD, FRAGMENT);
		ADD_FAILURE() << "no exception";
	} catch (const runtime::NullPointerException& e) {
		EXPECT_EQ(std::string(e.what()),
			"Cannot invoke \"ItemActions.getTuningAction()\" because the return value of \"ItemTemplate.getActions()\" is null");
	}
	EXPECT_TRUE(sent().empty());
}

// :50-51: an identified item without a scroll is audited
TEST_F(TunePacketsTest, AnIdentifiedItemWithoutAScrollIsAudited) {
	storedTuned(SWORD, MODORS_SWORD, 0);
	LogCapture capture({AUDIT_LOGGER});

	tune(SWORD, 0);

	EXPECT_TRUE(capture.contains(player().toString() + " attempted to tune an already identified item without tuning scroll.")) << capture.dump();
	EXPECT_TRUE(sent().empty());
}

// :33-34
TEST_F(TunePacketsTest, WithoutAPlayerNothingIsTuned) {
	storedTuned(SWORD, MODORS_SWORD, -1);
	TestClient loggedOut;
	EconomyDriver<CM_TUNE> packet(CM_TUNE_OPCODE);
	ASSERT_TRUE(packet.readOn(PacketWriter().D(SWORD).D(0).data, loggedOut.get()));

	EXPECT_NO_THROW(packet.runNow());
	EXPECT_TRUE(sent().empty());
}

// Deviation (play-session fixes 2026-09-28, docs/deviations/P5-16.md): a CM_TUNE while an item use runs aborts that use before it starts the new
// one (cancelUseItem, as CM_CASTSPELL and CM_EQUIP_ITEM do). Java starts the second use over the first: addTask(ITEM_USE) cancels the first task
// silently, the first item stays greyed, and the first use's observer stays attached until a later move reports "Canceled tuning of" it.
TEST_F(TunePacketsTest, ASecondIdentificationCancelsTheRunningOneFirst) {
	Item& first = storedTuned(SWORD, MODORS_SWORD, -1);
	Item& second = storedTuned(SECOND_SWORD, MODORS_SWORD, -1);
	tune(SWORD, 0);
	executor->advance(1000ms);
	clearSent();

	tune(SECOND_SWORD, 0);

	// the first use's abort (ItemActionService.java:28-34): its message and its cancel animation (11) un-grey the first item at once
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_IDENTIFY_CANCELED(first.getL10n())),
						  usageAnimation(player().getObjectId(), SWORD, MODORS_SWORD, 0, 11, 0),
						  usageAnimation(player().getObjectId(), SECOND_SWORD, MODORS_SWORD, 5000, 9, 0)}));
	executor->advance(5000ms);
	EXPECT_TRUE(second.isIdentified()) << "the second identification completes";
	EXPECT_FALSE(first.isIdentified());
	clearSent();

	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "no observer of the first use is left to report a late cancel";
	EXPECT_FALSE(first.isIdentified());
}

TEST_F(TunePacketsTest, ASecondTuningCancelsTheRunningOneFirst) {
	Item& first = storedTuned(SWORD, MODORS_SWORD, 0);
	Item& second = storedTuned(SECOND_SWORD, MODORS_SWORD, 0);
	Item& scroll = stored(WEAPON_SCROLL, MYTHIC_WEAPON_TUNING_SCROLL, 2);
	tune(SWORD, WEAPON_SCROLL);
	executor->advance(1000ms);
	clearSent();

	tune(SECOND_SWORD, WEAPON_SCROLL);

	// the first tuning's abort (TuningAction.java:69-76): REIDENTIFY_CANCELED of its target and the failure animation (14) of the scroll
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_CANCELED(first.getL10n())),
						  usageAnimation(player().getObjectId(), WEAPON_SCROLL, MYTHIC_WEAPON_TUNING_SCROLL, 0, 14, 0),
						  usageAnimation(player().getObjectId(), WEAPON_SCROLL, MYTHIC_WEAPON_TUNING_SCROLL, 5000, 12, 0)}));
	executor->advance(5000ms);
	EXPECT_TRUE(second.getPendingTuneResult()) << "the second tuning completes";
	EXPECT_EQ(second.getTuneCount(), 1);
	EXPECT_FALSE(first.getPendingTuneResult());
	EXPECT_EQ(first.getTuneCount(), 0);
	EXPECT_EQ(scroll.getItemCount(), 1) << "one scroll used, by the second tuning";
	clearSent();

	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty()) << "no observer of the first use is left to report a late cancel";
}

// only a scroll canAct accepts cancels the running use: a refused one answers with canAct's message and nothing else
TEST_F(TunePacketsTest, ARefusedScrollLeavesTheRunningTuningAlone) {
	Item& sword = storedTuned(SWORD, MODORS_SWORD, 0);
	Item& scroll = stored(WEAPON_SCROLL, MYTHIC_WEAPON_TUNING_SCROLL, 2);
	Item& armorScroll = stored(ARMOR_SCROLL, MYTHIC_ARMOR_TUNING_SCROLL, 1);
	tune(SWORD, WEAPON_SCROLL);
	executor->advance(1000ms);
	clearSent();

	tune(SWORD, ARMOR_SCROLL);

	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_WRONG_SELECT(armorScroll.getL10n(), sword.getL10n()))}));
	executor->advance(4000ms);
	EXPECT_TRUE(sword.getPendingTuneResult()) << "the running tuning completes";
	EXPECT_EQ(sword.getTuneCount(), 1);
	EXPECT_EQ(scroll.getItemCount(), 1);
	EXPECT_EQ(armorScroll.getItemCount(), 1);
}

// and no CM_TUNE that starts nothing cancels it: an item not in the cube (:36-38), a scroll not in the cube (:43-45), a scroll whose actions hold
// no tuning (:47-48), a scroll without actions (:47, Java's NullPointerException) and an identified item without a scroll (:50-51, the audit)
// each leave the running identification alone, as in Java
TEST_F(TunePacketsTest, APacketThatStartsNothingLeavesTheRunningIdentificationAlone) {
	constexpr int32_t NOT_IN_THE_CUBE = 900099;
	Item& sword = storedTuned(SWORD, MODORS_SWORD, -1);
	storedTuned(SECOND_SWORD, MODORS_SWORD, 0);
	stored(POTION, MINOR_LIFE_POTION, 1);
	stored(FRAGMENT, SPARKIE_CARAPACE_FRAGMENT, 1);
	tune(SWORD, 0);
	executor->advance(1000ms);
	clearSent();
	LogCapture audit({AUDIT_LOGGER});

	{
		SCOPED_TRACE("an item not in the cube");
		tune(NOT_IN_THE_CUBE, 0);
		EXPECT_TRUE(sent().empty());
	}
	{
		SCOPED_TRACE("a scroll not in the cube");
		tune(SECOND_SWORD, WEAPON_SCROLL);
		EXPECT_TRUE(sent().empty());
	}
	{
		SCOPED_TRACE("a scroll whose actions hold no tuning");
		tune(SECOND_SWORD, POTION);
		EXPECT_TRUE(sent().empty());
	}
	{
		SCOPED_TRACE("a scroll without actions");
		EXPECT_THROW(tune(SECOND_SWORD, FRAGMENT), runtime::NullPointerException);
		EXPECT_TRUE(sent().empty());
	}
	{
		SCOPED_TRACE("an identified item without a scroll");
		tune(SECOND_SWORD, 0);
		EXPECT_TRUE(audit.contains("attempted to tune an already identified item without tuning scroll.")) << audit.dump();
		EXPECT_TRUE(sent().empty());
	}

	executor->advance(4000ms);
	EXPECT_TRUE(sword.isIdentified()) << "the running identification completes";
}

// CM_TUNE_RESULT.java:39-43, :48: yes applies the pending result (ItemActionService.applyTuneResult), tells the player and updates the item
TEST_F(TunePacketsTest, AcceptingAppliesThePendingResult) {
	Item& sword = swordWithPendingResult(false);

	answer(SWORD, 1);

	EXPECT_EQ(sword.getOptionalSockets(), 1);
	EXPECT_EQ(sword.getEnchantBonus(), 2);
	EXPECT_FALSE(sword.getPendingTuneResult());
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_APPLY_YES(sword.getL10n())),
						  serializedFor(SM_INVENTORY_UPDATE_ITEM(player(), sword))}));
}

// :38: only a no to an attribute-only result is audited - a yes to it is a plain yes
TEST_F(TunePacketsTest, AcceptingAnAttributeOnlyResultIsNotAudited) {
	Item& sword = swordWithPendingResult(true);
	LogCapture capture({AUDIT_LOGGER});

	answer(SWORD, 1);

	EXPECT_EQ(capture.dump(), "");
	EXPECT_EQ(sword.getOptionalSockets(), 1);
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_APPLY_YES(sword.getL10n())),
						  serializedFor(SM_INVENTORY_UPDATE_ITEM(player(), sword))}));
}

// :44-48: no drops the pending result, keeps the values and updates the item
TEST_F(TunePacketsTest, DecliningDropsThePendingResult) {
	Item& sword = swordWithPendingResult(false);
	LogCapture capture({AUDIT_LOGGER});

	answer(SWORD, 0);

	EXPECT_EQ(sword.getOptionalSockets(), 0);
	EXPECT_EQ(sword.getEnchantBonus(), 0);
	EXPECT_FALSE(sword.getPendingTuneResult());
	EXPECT_EQ(sent(),
		exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_APPLY_NO()), serializedFor(SM_INVENTORY_UPDATE_ITEM(player(), sword))}));
	EXPECT_EQ(capture.dump(), "");
}

// :38-43: an attribute-only re-identification cannot be declined: the no is audited and the result applied
TEST_F(TunePacketsTest, DecliningAnAttributeOnlyResultIsAuditedAndApplied) {
	Item& sword = swordWithPendingResult(true);
	LogCapture capture({AUDIT_LOGGER});

	answer(SWORD, 0);

	EXPECT_TRUE(capture.contains(player().toString() + " tried to cancel a attribute re-identification which is not possible by default"))
		<< capture.dump();
	EXPECT_EQ(sword.getOptionalSockets(), 1);
	EXPECT_EQ(sword.getEnchantBonus(), 2);
	EXPECT_FALSE(sword.getPendingTuneResult());
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_APPLY_YES(sword.getL10n())),
						  serializedFor(SM_INVENTORY_UPDATE_ITEM(player(), sword))}));
}

// :38: without a pending result a no is a plain no, and a yes reaches applyTuneResult's own audit (ItemActionService.java:57-60)
TEST_F(TunePacketsTest, AnswersWithoutAPendingResult) {
	Item& sword = storedTuned(SWORD, MODORS_SWORD, 0);
	LogCapture capture({AUDIT_LOGGER});

	answer(SWORD, 0);
	EXPECT_EQ(sent(),
		exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_APPLY_NO()), serializedFor(SM_INVENTORY_UPDATE_ITEM(player(), sword))}));
	EXPECT_EQ(capture.dump(), "");

	clearSent();
	answer(SWORD, 1);
	EXPECT_TRUE(capture.contains(player().toString() + " attempted to apply a tune result without tuning the item beforehand.")) << capture.dump();
	EXPECT_EQ(sent(), exactly({message(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_APPLY_YES(sword.getL10n())),
						  serializedFor(SM_INVENTORY_UPDATE_ITEM(player(), sword))}));
}

// :36-37: an item not in the cube gets no answer
TEST_F(TunePacketsTest, AnAnswerForAnItemNotInTheCubeDoesNothing) {
	LogCapture capture({AUDIT_LOGGER});

	answer(SWORD, 1);

	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(capture.dump(), "");
}

} // namespace
} // namespace testing::items
} // namespace aion::gameserver::network::aion::clientpackets
