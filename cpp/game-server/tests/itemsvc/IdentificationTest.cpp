// M5c P-07 (m5c-plan.md §5, P5-07, A-13): identification and tuning - ItemActionService.identifyItem (the 5 s task, its ItemUseObserver and the
// rolls of an unidentified item's hidden values) and applyTuneResult, and TuningAction (canAct, the 5 s task and its observer, the pending tune
// result, getRandomStatBonusIdFor), against ItemActionService.java:23-72 and TuningAction.java:36-113. Tests of P-06.
//
// The items are verbatim rows of item_templates.xml (PlayerItemsTestSupport.h): Modor's Sword (:13282, option_slot_bonus 1, max_enchant_bonus 2,
// rnd_bonus 119, rnd_count 3), Modor's Tunic (:204906), the Mythic Weapon / Armor and the Enduring Eternal Weapon Tuning Scrolls (:839186-839196)
// and the test reidentify items (:839141, :839151); the random bonus set 119 is item_random_bonuses.xml:4623-4682. The rolls follow Java's order
// of the Rnd draws, taken again from the same seed: identifyItem draws the sockets (Rnd.get(0, option_slot_bonus)), the stat bonus
// (ItemRandomBonusData.selectRandomBonusNumber: Rnd.nextFloat(sum of the set's chances), the first group whose running sum reaches it) and the
// enchant bonus (Rnd.get(0, max_enchant_bonus)); TuningAction draws the sockets, the enchant bonus and then the stat bonus. The tasks run on the
// fixture's DeterministicExecutor, on this thread, so the seed set here is the one they draw from. Packets whose fields the action chooses are
// Java's bytes (SM_ITEM_USAGE_ANIMATION.java:73-86) or are read from the bytes (SM_TUNE_RESULT.java:29-37); the item update and the messages are
// compared against the server's own serialization of the packet Java constructs there (their bytes are pinned by the sm tests).

#include "PlayerItemsTestSupport.h"

#include <chrono>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/model/gameobjects/Persistable_PersistentState.h"
#include "aion/gameserver/model/items/PendingTuneResult.h"
#include "aion/gameserver/model/templates/item/actions/TuningAction.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INVENTORY_UPDATE_ITEM.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TUNE_RESULT.h"
#include "aion/gameserver/services/item/ItemActionService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::item::test::playeritems {
namespace {

using namespace std::chrono_literals;
namespace Rnd = commons::utils::Rnd;
using model::gameobjects::Persistable_PersistentState;
using model::items::PendingTuneResult;
using model::templates::item::actions::TuningAction;
using network::aion::serverpackets::SM_INVENTORY_UPDATE_ITEM;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::aion::serverpackets::SM_TUNE_RESULT;

constexpr int32_t SM_TUNE_RESULT_OPCODE = 288; // ServerPacketsOpcodes.java:306

/** The hidden values an identification or a tuning rolled */
struct Rolls {
	int32_t sockets;
	int32_t enchantBonus;
	int32_t statBonusId;
};

class IdentificationTest : public PlayerItemsTest {
protected:
	/** identifyItem's draws from `seed` (ItemActionService.java:44-46): sockets, stat bonus, enchant bonus */
	static Rolls identificationRolls(uint64_t seed) {
		Rnd::seedCurrentThreadForTests(seed);
		Rolls rolls{};
		rolls.sockets = Rnd::get(0, 1);
		rolls.statBonusId = modorsSwordBonusNumber(Rnd::nextFloat(modorsSwordBonusChanceSum()));
		rolls.enchantBonus = Rnd::get(0, 2);
		return rolls;
	}

	/** TuningAction's draws from `seed` (TuningAction.java:94-103): sockets and enchant bonus unless no_reduce, then the stat bonus */
	static Rolls tuningRolls(uint64_t seed, bool attributesOnly, int32_t sockets = 0, int32_t enchantBonus = 0) {
		Rnd::seedCurrentThreadForTests(seed);
		Rolls rolls{sockets, enchantBonus, 0};
		if (!attributesOnly) {
			rolls.sockets = Rnd::get(0, 1);
			rolls.enchantBonus = Rnd::get(0, 2);
		}
		rolls.statBonusId = modorsSwordBonusNumber(Rnd::nextFloat(modorsSwordBonusChanceSum()));
		return rolls;
	}

	const TuningAction& tuningOf(int32_t scrollId) { return onlyActionOf<TuningAction>(scrollId); }

	/** SM_ITEM_USAGE_ANIMATION(player, itemObjId, itemId, time, end, 0) (the six-argument constructor, unk3 0) */
	std::vector<uint8_t> animation(int32_t itemObjId, int32_t itemId, int32_t time, int32_t end) {
		return itemUsageAnimation(player().getObjectId(), itemObjId, itemId, time, end, 0);
	}

	std::vector<uint8_t> systemMessage(SM_SYSTEM_MESSAGE&& packet) { return serialized(std::move(packet)); }
};

// ------------------------------------------------------------------------------------------------------------------------- identifyItem

TEST_F(IdentificationTest, IdentifyingTakesFiveSecondsThenRollsTheHiddenValues) {
	// ItemActionService.java:23-55: the start animation (5000 ms, 9) to the player and his watchers, an observer, and after 5 s the end
	// animation (10), the three rolls, tune count -1 -> 0, the inventory marked for saving, SM_INVENTORY_UPDATE_ITEM and
	// STR_MSG_ITEM_IDENTIFY_SUCCEED
	Item& sword = inCube(970001, MODORS_SWORD, 1, -1);
	ASSERT_FALSE(sword.isIdentified()) << "a tunable template keeps tune_count -1 (Item.java:125-127)";
	player().getInventory().setPersistentState(Persistable_PersistentState::UPDATED);
	const Rolls rolls = identificationRolls(7);

	Rnd::seedCurrentThreadForTests(7);
	ItemActionService::identifyItem(player(), sword);
	EXPECT_EQ(sent(), cp::exactly({animation(970001, MODORS_SWORD, 5000, 9)}));
	clearSent();

	executor->advance(4999ms);
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(sword.isIdentified());

	executor->advance(1ms);
	EXPECT_TRUE(sword.isIdentified());
	EXPECT_EQ(sword.getTuneCount(), 0);
	EXPECT_EQ(sword.getOptionalSockets(), rolls.sockets);
	EXPECT_EQ(sword.getBonusStatsId(), rolls.statBonusId);
	EXPECT_EQ(sword.getEnchantBonus(), rolls.enchantBonus);
	EXPECT_EQ(player().getInventory().getPersistentState(), Persistable_PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(sent(), cp::exactly({animation(970001, MODORS_SWORD, 0, 10), serialized(SM_INVENTORY_UPDATE_ITEM(player(), sword)),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_IDENTIFY_SUCCEED(sword.getL10n()))}));

	// the task removed its observer: moving afterwards cancels nothing
	clearSent();
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty());
}

TEST_F(IdentificationTest, TheRollsCoverTheTemplatesRanges) {
	// ItemActionService.java:44-46 over 40 seeds: sockets in 0..option_slot_bonus (1), enchant bonus in 0..max_enchant_bonus (2), a stat bonus of
	// set 119's ten groups; every value of each range comes up, and each identification is the one its seed predicts
	std::set<int32_t> sockets;
	std::set<int32_t> enchantBonuses;
	std::set<int32_t> statBonuses;
	for (uint64_t seed = 1; seed <= 40; seed++) {
		SCOPED_TRACE(seed);
		const int32_t objId = 970100 + static_cast<int32_t>(seed);
		Item& sword = inCube(objId, MODORS_SWORD, 1, -1);
		const Rolls rolls = identificationRolls(seed);
		Rnd::seedCurrentThreadForTests(seed);
		ItemActionService::identifyItem(player(), sword);
		executor->advance(5000ms);
		ASSERT_TRUE(sword.isIdentified());
		EXPECT_EQ(sword.getOptionalSockets(), rolls.sockets);
		EXPECT_EQ(sword.getEnchantBonus(), rolls.enchantBonus);
		EXPECT_EQ(sword.getBonusStatsId(), rolls.statBonusId);
		sockets.insert(sword.getOptionalSockets());
		enchantBonuses.insert(sword.getEnchantBonus());
		statBonuses.insert(sword.getBonusStatsId());
	}
	EXPECT_EQ(sockets, (std::set<int32_t>{0, 1}));
	EXPECT_EQ(enchantBonuses, (std::set<int32_t>{0, 1, 2}));
	EXPECT_GE(*statBonuses.begin(), 1);
	EXPECT_LE(*statBonuses.rbegin(), 10);
	EXPECT_GE(statBonuses.size(), 5u) << "the groups are drawn, not one fixed number";
}

TEST_F(IdentificationTest, MovingCancelsTheIdentification) {
	// ItemActionService.java:28-34: ItemUseObserver.moved -> abort: the task is cancelled, STR_MSG_ITEM_IDENTIFY_CANCELED, the cancel animation
	// (11), and the observer removes itself
	Item& sword = inCube(970201, MODORS_SWORD, 1, -1);
	ItemActionService::identifyItem(player(), sword);
	executor->advance(2000ms);
	clearSent();

	player().getObserveController()->notifyMoveObservers();
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_IDENTIFY_CANCELED(sword.getL10n())),
						  animation(970201, MODORS_SWORD, 0, 11)}));
	clearSent();

	executor->advance(5000ms);
	player().getObserveController()->notifyMoveObservers();
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(sword.isIdentified());
}

// ------------------------------------------------------------------------------------------------------------------------- applyTuneResult

TEST_F(IdentificationTest, ApplyingATuneResultTakesItsValues) {
	// ItemActionService.java:58-71: the pending result's sockets, enchant bonus and stat bonus, the pending result cleared, the item and the
	// inventory marked for saving; nothing is sent
	Item& sword = inCube(970301, MODORS_SWORD, 1, 1);
	sword.setPersistentState(Persistable_PersistentState::UPDATED);
	player().getInventory().setPersistentState(Persistable_PersistentState::UPDATED);
	sword.setPendingTuneResult(PendingTuneResult::create(1, 2, 7, false));

	ItemActionService::applyTuneResult(player(), sword);

	EXPECT_EQ(sword.getOptionalSockets(), 1);
	EXPECT_EQ(sword.getEnchantBonus(), 2);
	EXPECT_EQ(sword.getBonusStatsId(), 7);
	EXPECT_FALSE(sword.getPendingTuneResult());
	EXPECT_EQ(sword.getPersistentState(), Persistable_PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(player().getInventory().getPersistentState(), Persistable_PersistentState::UPDATE_REQUIRED);
	EXPECT_EQ(sword.getTuneCount(), 1) << "the tune count is the tuning's business";
	EXPECT_TRUE(sent().empty());
}

TEST_F(IdentificationTest, ApplyingWithoutATuningIsAnAuditLine) {
	// ItemActionService.java:60-63: no pending result -> AuditLogger.log and return
	network::test::LogCapture audit({"AUDIT_LOG"});
	const bool savedLogAudit = configs::main::LoggingConfig::LOG_AUDIT.exchange(true);
	Item& sword = inCube(970401, MODORS_SWORD, 1, 1);
	sword.setPersistentState(Persistable_PersistentState::UPDATED);

	ItemActionService::applyTuneResult(player(), sword);
	configs::main::LoggingConfig::LOG_AUDIT.store(savedLogAudit);

	EXPECT_EQ(audit.count("attempted to apply a tune result without tuning the item beforehand."), 1) << audit.dump();
	EXPECT_EQ(sword.getPersistentState(), Persistable_PersistentState::UPDATED);
	EXPECT_EQ(sword.getBonusStatsId(), 0);
}

// ------------------------------------------------------------------------------------------------------------------------- TuningAction

TEST_F(IdentificationTest, TuningAcceptsAnIdentifiedTunableItemOfTheScrollsKindAndLevel) {
	// TuningAction.java:36-57, in order: an equipped target -> false without a message; unidentified -> DIDNT_IDENTIFY; a template that cannot be
	// tuned (Training Sword: no bonus, rnd_count 0, ItemTemplate.java:155-161) -> CANNOT_REIDENTIFY; a WEAPON scroll on armour or an ARMOR scroll
	// on a weapon -> WRONG_SELECT (an EQUIPMENT scroll passes this check); a target above the scroll's level -> WRONG_LEVEL; then the tune count
	// below rnd_count (3), or a no_reduce scroll
	Item& weaponScroll = inCube(970501, WEAPON_TUNING_SCROLL, 5);
	Item& armorScroll = inCube(970502, ARMOR_TUNING_SCROLL, 5);
	Item& enduringScroll = inCube(970503, ENDURING_WEAPON_TUNING_SCROLL, 5);
	Item& levelSixtyScroll = inCube(970504, WEAPON_REIDENTIFY_TEST_ITEM, 5);
	Item& equipmentScroll = inCube(970505, EQUIPMENT_REIDENTIFY_TEST_ITEM, 5);
	Item& sword = inCube(970506, MODORS_SWORD, 1, 0);
	Item& unidentified = inCube(970507, MODORS_SWORD, 1, -1);
	Item& training = inCube(970508, TRAINING_SWORD, 1, 0);
	Item& tunic = inCube(970509, MODORS_TUNIC, 1, 0);
	Item& equipped = *itemRow(970510, MODORS_SWORD, 1, 0, true);
	Item& usedUp = inCube(970511, MODORS_SWORD, 1, 3);
	const TuningAction& weapon = tuningOf(WEAPON_TUNING_SCROLL);

	EXPECT_FALSE(weapon.canAct(player(), Ptr<Item>(weaponScroll), Ptr<Item>(equipped)));
	EXPECT_TRUE(sent().empty());
	EXPECT_FALSE(weapon.canAct(player(), Ptr<Item>(weaponScroll), Ptr<Item>(unidentified)));
	EXPECT_FALSE(weapon.canAct(player(), Ptr<Item>(weaponScroll), Ptr<Item>(training)));
	EXPECT_FALSE(weapon.canAct(player(), Ptr<Item>(weaponScroll), Ptr<Item>(tunic)));
	EXPECT_FALSE(tuningOf(ARMOR_TUNING_SCROLL).canAct(player(), Ptr<Item>(armorScroll), Ptr<Item>(sword)));
	EXPECT_FALSE(tuningOf(WEAPON_REIDENTIFY_TEST_ITEM).canAct(player(), Ptr<Item>(levelSixtyScroll), Ptr<Item>(sword)));
	EXPECT_FALSE(tuningOf(EQUIPMENT_REIDENTIFY_TEST_ITEM).canAct(player(), Ptr<Item>(equipmentScroll), Ptr<Item>(tunic)));
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_DIDNT_IDENTIFY(unidentified.getL10n())),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_CANNOT_REIDENTIFY(training.getL10n())),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_WRONG_SELECT(weaponScroll.getL10n(), tunic.getL10n())),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_WRONG_SELECT(armorScroll.getL10n(), sword.getL10n())),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_WRONG_LEVEL(levelSixtyScroll.getL10n(), sword.getL10n())),
						  systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_WRONG_LEVEL(equipmentScroll.getL10n(), tunic.getL10n()))}));
	clearSent();

	EXPECT_TRUE(weapon.canAct(player(), Ptr<Item>(weaponScroll), Ptr<Item>(sword)));
	EXPECT_TRUE(tuningOf(ARMOR_TUNING_SCROLL).canAct(player(), Ptr<Item>(armorScroll), Ptr<Item>(tunic)));
	EXPECT_FALSE(weapon.canAct(player(), Ptr<Item>(weaponScroll), Ptr<Item>(usedUp))) << "3 tunings of rnd_count 3 used";
	EXPECT_TRUE(tuningOf(ENDURING_WEAPON_TUNING_SCROLL).canAct(player(), Ptr<Item>(enduringScroll), Ptr<Item>(usedUp))) << "no_reduce";
	EXPECT_TRUE(sent().empty());
}

TEST_F(IdentificationTest, ATuningRollsANewResultAfterFiveSecondsAndUsesATuning) {
	// TuningAction.java:59-109: the start animation (5000 ms, 12), after 5 s the success animation (13), one scroll used, a tuning used (tune
	// count + 1, the inventory marked for saving), sockets, enchant bonus and stat bonus rolled into a pending result the item keeps (applied
	// only by CM_TUNE_RESULT), SM_TUNE_RESULT with it and STR_MSG_ITEM_REIDENTIFY_SUCCEED; the item itself keeps its values
	Item& scroll = inCube(970601, WEAPON_TUNING_SCROLL, 2);
	Item& sword = inCube(970602, MODORS_SWORD, 1, 0);
	player().getInventory().setPersistentState(Persistable_PersistentState::UPDATED);
	const Rolls rolls = tuningRolls(11, false);

	Rnd::seedCurrentThreadForTests(11);
	tuningOf(WEAPON_TUNING_SCROLL).act(player(), Ptr<Item>(scroll), Ptr<Item>(sword));
	EXPECT_EQ(sent(), cp::exactly({animation(970601, WEAPON_TUNING_SCROLL, 5000, 12)}));
	clearSent();
	executor->advance(4999ms);
	EXPECT_TRUE(sent().empty());
	executor->advance(1ms);

	EXPECT_EQ(scroll.getItemCount(), 1);
	EXPECT_EQ(sword.getTuneCount(), 1);
	EXPECT_EQ(player().getInventory().getPersistentState(), Persistable_PersistentState::UPDATE_REQUIRED);
	Ptr<PendingTuneResult> result = sword.getPendingTuneResult();
	ASSERT_TRUE(result);
	EXPECT_EQ(result->getOptionalSockets(), rolls.sockets);
	EXPECT_EQ(result->getEnchantBonus(), rolls.enchantBonus);
	EXPECT_EQ(result->getStatBonusId(), rolls.statBonusId);
	EXPECT_FALSE(result->isAttributeOnly());
	EXPECT_EQ(sword.getOptionalSockets(), 0);
	EXPECT_EQ(sword.getBonusStatsId(), 0);

	std::vector<std::vector<uint8_t>> tuneResults = sentWithOpcode(SM_TUNE_RESULT_OPCODE);
	ASSERT_EQ(tuneResults.size(), 1u);
	// SM_TUNE_RESULT.java:29-37: D target object id, D scroll item id, C stat bonus id, the enchant info blob, C 0 (show manastone slots),
	// C 0 (tune cancel possible), both 0 for a result that is not attribute-only
	std::vector<uint8_t> body = cp::bodyOf(tuneResults[0]);
	PacketReader reader(body);
	EXPECT_EQ(reader.D(), 970602);
	EXPECT_EQ(reader.D(), WEAPON_TUNING_SCROLL);
	EXPECT_EQ(reader.C(), rolls.statBonusId);
	ASSERT_GE(body.size(), 2u);
	EXPECT_EQ(body[body.size() - 2], 0);
	EXPECT_EQ(body[body.size() - 1], 0);
	EXPECT_EQ(tuneResults[0], serialized(SM_TUNE_RESULT(sword, WEAPON_TUNING_SCROLL, *result)));

	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_GE(packets.size(), 4u);
	EXPECT_EQ(packets.front(), animation(970601, WEAPON_TUNING_SCROLL, 0, 13));
	EXPECT_EQ(packets[packets.size() - 2], tuneResults[0]);
	EXPECT_EQ(packets.back(), systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_SUCCEED(sword.getL10n())));
}

TEST_F(IdentificationTest, ANoReduceScrollRollsOnlyTheStatBonus) {
	// TuningAction.java:94-97, 101-104: no_reduce keeps the item's sockets and enchant bonus, uses no tuning (even with all used, :56) and draws
	// only the stat bonus; the result is attribute-only, so SM_TUNE_RESULT's two closing bytes are 1 (no manastone slots, no cancel)
	Item& scroll = inCube(970701, ENDURING_WEAPON_TUNING_SCROLL, 1);
	Item& sword = inCube(970702, MODORS_SWORD, 1, 3);
	sword.setOptionalSockets(1);
	sword.setEnchantBonus(2);
	const Rolls rolls = tuningRolls(12, true, 1, 2);

	Rnd::seedCurrentThreadForTests(12);
	tuningOf(ENDURING_WEAPON_TUNING_SCROLL).act(player(), Ptr<Item>(scroll), Ptr<Item>(sword));
	executor->advance(5000ms);

	EXPECT_FALSE(player().getInventory().getItemByObjId(970701)) << "the only scroll is used up";
	EXPECT_EQ(sword.getTuneCount(), 3);
	Ptr<PendingTuneResult> result = sword.getPendingTuneResult();
	ASSERT_TRUE(result);
	EXPECT_EQ(result->getOptionalSockets(), 1);
	EXPECT_EQ(result->getEnchantBonus(), 2);
	EXPECT_EQ(result->getStatBonusId(), rolls.statBonusId);
	EXPECT_TRUE(result->isAttributeOnly());
	std::vector<std::vector<uint8_t>> tuneResults = sentWithOpcode(SM_TUNE_RESULT_OPCODE);
	ASSERT_EQ(tuneResults.size(), 1u);
	std::vector<uint8_t> body = cp::bodyOf(tuneResults[0]);
	ASSERT_GE(body.size(), 2u);
	EXPECT_EQ(body[body.size() - 2], 1);
	EXPECT_EQ(body[body.size() - 1], 1);
}

TEST_F(IdentificationTest, TheTaskAsksForTheItemAndCanActAgain) {
	// TuningAction.java:82-86: at the end the target must still be in the inventory and pass canAct, else the failure animation (14) and nothing
	// else - no scroll used, no tuning; a scroll that left the inventory (:89-90) ends the task after the success animation
	Item& scroll = inCube(970801, WEAPON_TUNING_SCROLL, 3);
	Item& sword = inCube(970802, MODORS_SWORD, 1, 0);
	const TuningAction& tuning = tuningOf(WEAPON_TUNING_SCROLL);

	tuning.act(player(), Ptr<Item>(scroll), Ptr<Item>(sword));
	sword.setEquipped(true); // equipped meanwhile: canAct says no (:37-38)
	clearSent();
	executor->advance(5000ms);
	EXPECT_EQ(sent(), cp::exactly({animation(970801, WEAPON_TUNING_SCROLL, 0, 14)}));
	EXPECT_EQ(scroll.getItemCount(), 3);
	EXPECT_EQ(sword.getTuneCount(), 0);
	EXPECT_FALSE(sword.getPendingTuneResult());
	sword.setEquipped(false);

	Item& loose = *itemRow(970803, MODORS_SWORD, 1, 0); // not in the inventory
	tuning.act(player(), Ptr<Item>(scroll), Ptr<Item>(loose));
	clearSent();
	executor->advance(5000ms);
	EXPECT_EQ(sent(), cp::exactly({animation(970801, WEAPON_TUNING_SCROLL, 0, 14)}));
	EXPECT_EQ(scroll.getItemCount(), 3);

	Item& otherScroll = *itemRow(970804, WEAPON_TUNING_SCROLL, 1); // a scroll the inventory does not hold
	tuning.act(player(), Ptr<Item>(otherScroll), Ptr<Item>(sword));
	clearSent();
	executor->advance(5000ms);
	EXPECT_EQ(sent(), cp::exactly({animation(970804, WEAPON_TUNING_SCROLL, 0, 13)}));
	EXPECT_EQ(sword.getTuneCount(), 0);
	EXPECT_FALSE(sword.getPendingTuneResult());
}

TEST_F(IdentificationTest, MovingCancelsTheTuning) {
	// TuningAction.java:69-76: abort -> the task is cancelled, STR_MSG_ITEM_REIDENTIFY_CANCELED of the target, the failure animation (14)
	Item& scroll = inCube(970901, WEAPON_TUNING_SCROLL, 2);
	Item& sword = inCube(970902, MODORS_SWORD, 1, 0);
	tuningOf(WEAPON_TUNING_SCROLL).act(player(), Ptr<Item>(scroll), Ptr<Item>(sword));
	executor->advance(1000ms);
	clearSent();

	player().getObserveController()->notifyMoveObservers();
	EXPECT_EQ(sent(), cp::exactly({systemMessage(SM_SYSTEM_MESSAGE::STR_MSG_ITEM_REIDENTIFY_CANCELED(sword.getL10n())),
						  animation(970901, WEAPON_TUNING_SCROLL, 0, 14)}));
	clearSent();
	executor->advance(5000ms);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(scroll.getItemCount(), 2);
	EXPECT_EQ(sword.getTuneCount(), 0);
}

TEST_F(IdentificationTest, AnItemWithoutARandomBonusSetRollsNoStatBonus) {
	// TuningAction.java:111-113 -> ItemRandomBonusData.selectRandomBonusNumber: no set for rnd_bonus 0 -> 0
	Item& training = inCube(971001, TRAINING_SWORD, 1);
	EXPECT_EQ(TuningAction::getRandomStatBonusIdFor(training), 0);
	Item& sword = inCube(971002, MODORS_SWORD, 1);
	Rnd::seedCurrentThreadForTests(13);
	const int32_t expected = modorsSwordBonusNumber(Rnd::nextFloat(modorsSwordBonusChanceSum()));
	Rnd::seedCurrentThreadForTests(13);
	EXPECT_EQ(TuningAction::getRandomStatBonusIdFor(sword), expected);
}

} // namespace
} // namespace aion::gameserver::services::item::test::playeritems
