// MailService (m5c-plan.md M-01, M-03): sendMail's refusals, validateRecipient's table, the commission in Java's float arithmetic, the
// untradeable, packed and unpacked arms, the offline and the online recipient, the stones, a failed item store; readMail, getAttachments (the
// item, a full cube, an object id the cube holds already, an expired item, and the kinah stored before it is added), deleteMail and
// sendMailList. The fixture is MailTestSupport.h's; the expected totals are
// `oracle.py m5c-economy --no-profile --set gameserver.siege.enable=false --mail ...` (Java's float arithmetic, tools/oracle/m5c/economy.py).

#include "MailTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dao/MailDAO.h"
#include "aion/gameserver/model/gameobjects/player/BlockList.h"
#include "aion/gameserver/model/gameobjects/player/BlockedPlayer.h"
#include "aion/gameserver/model/items/ManaStone.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MAIL_SERVICE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/services/mail/MailService.h"
#include "aion/gameserver/services/player/PlayerMailboxState.h"
#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"

namespace aion::gameserver::economy::test::mail {
namespace {

using model::Race;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using services::mail::MailService;
using services::player::PlayerMailboxState;

constexpr int32_t STACK = 500001;       // the attached item's object id
constexpr int32_t KINAH_ITEM = 500002;  // the sender's kinah item
constexpr int32_t OTHER_ITEM = 500003;  // a second item of the sender
constexpr int32_t B_KINAH_ITEM = 600002; // the recipient's kinah item

const int32_t MAILBOX_LOCATION = model::items::storage::getId(StorageType::MAILBOX);

std::string sqlOf(std::string_view prefix, int32_t id) {
	return std::string(prefix) + std::to_string(id);
}

class MailServiceTest : public MailTest {
protected:
	/** sendMail from A to B with the fixture's title and message */
	void send(int32_t itemObjId, int64_t count, int64_t kinahValue, LetterType type = LetterType::NORMAL, std::string_view to = B_NAME) {
		MailService::sendMail(a(), to, "Title", "Message", itemObjId, count, kinahValue, type);
	}

	std::vector<uint8_t> notEnoughMoney() { return serializedFor(SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY()); }

	/** A letter in B's mailbox as the DAO loads it */
	runtime::Ref<Letter> putLetter(int32_t letterId, runtime::Ptr<Item> item, int64_t kinahValue, int64_t millis, bool unread = true,
		LetterType type = LetterType::NORMAL, std::string_view title = "Loaded") {
		runtime::Ref<Letter> letter = loadedLetter(letterId, B_ID, item, kinahValue, title, "Sender", millis, unread, type);
		partner().getMailbox()->putLetterToMailbox(*letter);
		return letter;
	}
};

// ---------------------------------------------------------------------------------------------------------------------------------------------
// sendMail: the refusals before the recipient (MailService.java:58-69)
// ---------------------------------------------------------------------------------------------------------------------------------------------

// MailService.java:58: recipientName.length() > 16 counts UTF-16 units: a name of 16 two-byte letters (32 UTF-8 bytes) is looked up, 17 letters
// are not
TEST_F(MailServiceTest, ARecipientNameLongerThanSixteenUtf16UnitsSendsNothing) {
	const std::string sixteen = "\xC3\x84\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4\xC3\xA4";
	ASSERT_EQ(sixteen.size(), 32u);
	b.commonData->setName(sixteen);
	goOnline(partner());

	send(0, 0, 0, LetterType::NORMAL, "Abcdefghijklmnopq"); // 17 units
	EXPECT_TRUE(sent().empty());

	send(0, 0, 0, LetterType::NORMAL, sixteen); // found online, then refused only by the price (A has no kinah)
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()}));
}

// MailService.java:60-63
TEST_F(MailServiceTest, ABlackCloudLetterOrNegativeKinahIsAuditedAndSendsNothing) {
	goOnline(partner());
	setKinah(a(), KINAH_ITEM, 1000);
	LogCapture audit("AUDIT_LOG");

	send(0, 0, 0, LetterType::BLACKCLOUD);
	send(0, 0, -1);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(kinah(a()), 1000);
	EXPECT_TRUE(audit.contains("tried to send letter of type BLACKCLOUD with 0 Kinah"));
	EXPECT_TRUE(audit.contains("tried to send letter of type NORMAL with -1 Kinah"));
	EXPECT_EQ(audit.lines().size(), 2u);

	// no kinah is not negative: the letter goes on to the price (13 kinah for a normal letter without attachments)
	a().getInventory().decreaseKinah(988);
	clearSent();
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()}));
	EXPECT_EQ(audit.lines().size(), 2u);
}

// MailService.java:58: sender.isTrading()
TEST_F(MailServiceTest, ASenderInAnExchangeSendsNothing) {
	goOnline(partner());
	services::ExchangeService::getInstance().registerExchange(a(), partner());
	ASSERT_TRUE(a().isTrading());
	clearSent();

	send(0, 0, 0);
	EXPECT_TRUE(sent().empty());
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// validateRecipient (MailService.java:172-184), through the SM_MAIL_SERVICE(1, message) sendMail answers with (:72-75)
// ---------------------------------------------------------------------------------------------------------------------------------------------

TEST_F(MailServiceTest, ARecipientOfTheOtherRaceIsRefused) {
	b.commonData->setRace(Race::ASMODIANS);
	goOnline(partner());
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({mailMessage(MAIL_IS_ONE_RACE_ONLY)}));

	// the recipient is checked before the attached item (MailService.java:71-76, then :86-92): an unknown object id is not answered
	clearSent();
	send(999, 1, 0);
	EXPECT_EQ(sent(), cp::exactly({mailMessage(MAIL_IS_ONE_RACE_ONLY)}));
}

TEST_F(MailServiceTest, AStaffMemberMayWriteToTheOtherRace) {
	b.commonData->setRace(Race::ASMODIANS);
	f.account->setAccessLevel(1);
	goOnline(partner());
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()})); // past validateRecipient, refused by the price
}

TEST_F(MailServiceTest, AMailboxOfOneHundredLettersIsFull) {
	goOnline(partner());
	b.commonData->setMailboxLetters(100);
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({mailMessage(RECIPIENT_MAILBOX_FULL)}));

	b.commonData->setMailboxLetters(99);
	clearSent();
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()}));
}

TEST_F(MailServiceTest, ASenderOnTheOnlineRecipientsBlockListIsRefused) {
	goOnline(partner());
	runtime::Ref<model::gameobjects::player::BlockedPlayer> other = model::gameobjects::player::BlockedPlayer::create(123456, "Other", "");
	partner().getBlockList()->add(*other);
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()})); // somebody else is blocked

	runtime::Ref<model::gameobjects::player::BlockedPlayer> sender = model::gameobjects::player::BlockedPlayer::create(A_ID, A_NAME, "");
	partner().getBlockList()->add(*sender);
	clearSent();
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({mailMessage(YOU_ARE_IN_RECIPIENT_IGNORE_LIST)}));
}

// the checks run in Java's order: the race, the mailbox, then the block list
TEST_F(MailServiceTest, TheRecipientChecksRunInJavasOrder) {
	b.commonData->setRace(Race::ASMODIANS);
	b.commonData->setMailboxLetters(100);
	runtime::Ref<model::gameobjects::player::BlockedPlayer> sender = model::gameobjects::player::BlockedPlayer::create(A_ID, A_NAME, "");
	partner().getBlockList()->add(*sender);
	goOnline(partner());
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({mailMessage(MAIL_IS_ONE_RACE_ONLY)}));

	b.commonData->setRace(Race::ELYOS);
	clearSent();
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({mailMessage(RECIPIENT_MAILBOX_FULL)}));
}

// PlayerService.getOrLoadPlayerCommonData(name) finds no row: NO_SUCH_CHARACTER_NAME
TEST_F(MailServiceTest, AnUnknownNameIsNoSuchCharacter) {
	MAIL_REQUIRE_DATABASE();
	send(0, 0, 0, LetterType::NORMAL, "Nobody");
	EXPECT_EQ(sent(), cp::exactly({mailMessage(NO_SUCH_CHARACTER_NAME)}));
}

// an offline recipient's block list comes from BlockListDAO.load (MailService.java:180)
TEST_F(MailServiceTest, AnOfflineRecipientsBlockListIsLoaded) {
	MAIL_REQUIRE_DATABASE();
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()}));

	execute(sqlOf("INSERT INTO blocks (player, blocked_player, reason) VALUES (710202, ", A_ID) + ", '')");
	clearSent();
	send(0, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({mailMessage(YOU_ARE_IN_RECIPIENT_IGNORE_LIST)}));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// the attached item's refusals (MailService.java:86-103)
// ---------------------------------------------------------------------------------------------------------------------------------------------

TEST_F(MailServiceTest, AMissingItemOrAStackTooSmallIsRefused) {
	goOnline(partner());
	give(a(), STACK, MINOR_LIFE_POTION, 5);
	setKinah(a(), KINAH_ITEM, 1000);
	std::vector<uint8_t> usedItem = serializedFor(SM_SYSTEM_MESSAGE::STR_MAIL_SEND_USED_ITEM());

	send(999, 1, 0);
	EXPECT_EQ(sent(), cp::exactly({usedItem}));
	clearSent();
	send(STACK, 6, 0);
	EXPECT_EQ(sent(), cp::exactly({usedItem}));
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 5);
	EXPECT_EQ(kinah(a()), 1000);
}

// A worn item is not in the inventory at all (Equipment holds it), so it is "used" (MailService.java:87-92); the isEquipped arm (:94-97) answers
// an inventory item that carries the equipped flag
TEST_F(MailServiceTest, AnEquippedItemIsRefused) {
	goOnline(partner());
	equipped(STACK, TRAINING_SWORD, 1); // ItemSlot MAIN_HAND
	setKinah(a(), KINAH_ITEM, 1000);
	send(STACK, 1, 0);
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MAIL_SEND_USED_ITEM())}));

	give(a(), OTHER_ITEM, TRAINING_SWORD, 1).setEquipped(true); // PlayerStorage.onLoadHandler would hand a flagged item to the equipment
	clearSent();
	send(OTHER_ITEM, 1, 0);
	EXPECT_EQ(sent(), cp::exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MAIL_SEND_CAN_NOT_SEND_EQUIPPED_ITEM())}));
	EXPECT_EQ(kinah(a()), 1000);
}

// attachedItemObjId != 0 && attachedItemCount > 0 (MailService.java:86): a count of 0 attaches nothing, so an unknown object id is never looked
// up; nor is an object id of 0 with a count (a lookup would answer STR_MAIL_SEND_USED_ITEM)
TEST_F(MailServiceTest, ACountOfZeroAttachesNoItem) {
	goOnline(partner());
	send(999, 0, 0);
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()}));

	clearSent();
	send(0, 5, 0);
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()}));
}

// CM_SEND_MAIL reads the count with readQ, so a client can send a negative one: it attaches nothing either. The letter costs the plain 13 kinah
// (oracle: --mail 162000002:-1:0 -> 13) and the stack stays whole; a looked-up item would pass `getItemCount() < -1`, lower the price to
// getPriceForService(10 - 5) = 6 and "decrease" the stack by -1
TEST_F(MailServiceTest, ANegativeCountAttachesNoItem) {
	goOnline(partner());
	give(a(), STACK, MINOR_LIFE_POTION, 5);
	setKinah(a(), KINAH_ITEM, 12);

	send(STACK, -1, 0);
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()}));
	EXPECT_EQ(kinah(a()), 12);

	// the plain letter's 13 kinah are taken; without a database MailDAO.storeLetter fails and ends the send
	a().getInventory().increaseKinah(1);
	clearSent();
	send(STACK, -1, 0);
	EXPECT_EQ(kinah(a()), 0);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 5);
	EXPECT_EQ(a().getInventory().getItemByObjId(STACK)->getItemCount(), 5);
	EXPECT_TRUE(ofOpcode(sent(), SM_SYSTEM_MESSAGE_OPCODE).empty());
	EXPECT_TRUE(ofOpcode(sent(), SM_DELETE_ITEM_OPCODE).empty());
}

// AdminService.canOperate: a staff member below gameserver.administration.unrestricted_itemtrade may mail no item outside the admin list
TEST_F(MailServiceTest, AStaffMemberWithoutUnrestrictedTradeCannotMailAnItem) {
	goOnline(partner());
	f.account->setAccessLevel(1);
	ConfigScope<int8_t> unrestricted(configs::administration::AdminConfig::UNRESTRICTED_ITEMTRADE, int8_t{2});
	give(a(), STACK, MINOR_LIFE_POTION, 5);
	setKinah(a(), KINAH_ITEM, 1000);
	send(STACK, 1, 0);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_EQ(packets.size(), 1u);
	EXPECT_EQ(itemtest::javaOpcodeOf(packets[0]), SM_MESSAGE_OPCODE);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 5);
	EXPECT_EQ(kinah(a()), 1000);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// the commission (MailService.java:79-113), in Java's float arithmetic
// ---------------------------------------------------------------------------------------------------------------------------------------------

struct CommissionRow {
	const char* name;
	int32_t itemId; // 0 for none
	int64_t stock;  // the sender's stack of the item
	int64_t count;
	int64_t kinah;
	LetterType type;
	int64_t total; // oracle.py m5c-economy ... --mail itemId:count:kinah[:express] -> byRace.ELYOS.total
};

// Each total is the oracle's (see the file comment). What each row pins, beside its quality rate:
const CommissionRow COMMISSION_ROWS[] = {
	// the gate's letter (X13): 250 * 0.02f * 5 = 25, 200 * 0.01f = 2; getPriceForService(37) = 51, + 200
	{"GatePotionsAndKinah", MINOR_LIFE_POTION, 20, 5, 200, LetterType::NORMAL, 251},
	// the gate's kinah letter: getPriceForService(10) = 13, + 10
	{"GateTenKinah", 0, 0, 0, 10, LetterType::NORMAL, 23},
	// 100 * 0.01f rounds to 1.0f; in double arithmetic (100 * (double) 0.01f = 0.99999997...) it would be 0 and the total 113
	{"HundredKinahIsOneInFloat", 0, 0, 0, 100, LetterType::NORMAL, 114},
	// UNIQUE: 5 * 0.04f * 5 = 0.99999994f, cast to 0 (exact decimal arithmetic would give 1 and the total 14)
	{"UniqueCourierPassesFloatTruncation", COURIER_PASS_ABYSS_FABLED, 10, 5, 0, LetterType::NORMAL, 13},
	{"UniqueCourierPassesHundred", COURIER_PASS_ABYSS_FABLED, 100, 100, 0, LetterType::NORMAL, 40},
	// RARE: 26 * 0.03f * 10 = 7
	{"RareOre", ROSE_QUARTZ_ORE, 10, 10, 0, LetterType::NORMAL, 23},
	// LEGEND: 9045 * 0.04f = 361.8
	{"LegendOre", PURE_VOLUSPAR_ORE, 1, 1, 0, LetterType::NORMAL, 523},
	{"LegendStigma", SURE_STRIKE_STIGMA, 1, 1, 0, LetterType::NORMAL, 237},
	// EPIC and MYTHIC: 0.05f
	{"EpicCourierPasses", COURIER_PASS_ETERNAL, 100, 100, 0, LetterType::NORMAL, 48},
	{"MythicCourierPasses", COURIER_PASS_MYTHIC, 100, 100, 0, LetterType::NORMAL, 48},
	// JUNK falls to the default 0.02f: 300 * 0.02f * 9 = 54
	{"JunkIsTheDefaultRate", SPARKIE_CARAPACE_FRAGMENT, 9, 9, 0, LetterType::NORMAL, 90},
	// COMMON 10 * 0.02f * 25: the products in Java's order give 4 (price * (rate * count) would give 5, the total 21)
	{"CommonManastonesProductOrder", MANASTONE_HP_20, 30, 25, 0, LetterType::NORMAL, 19},
	// EXPRESS: base cost 500, cost factor 5 on both commissions
	{"ExpressEpicAndKinah", COURIER_PASS_ETERNAL, 3, 3, 1000, LetterType::EXPRESS, 1780},
	{"ExpressMythicAndKinah", COURIER_PASS_MYTHIC, 7, 7, 99, LetterType::EXPRESS, 822},
	{"ExpressThousandPotions", MINOR_LIFE_POTION, 1000, 1000, 0, LetterType::EXPRESS, 36018},
	{"ExpressWithoutAttachments", 0, 0, 0, 0, LetterType::EXPRESS, 706},
};

/** gtest/ctest display of the parameter: the row's name (gtest_discover_tests names a case after it) */
void PrintTo(const CommissionRow& row, std::ostream* out) {
	*out << row.name;
}

class MailCommissionTest : public MailServiceTest, public ::testing::WithParamInterface<CommissionRow> {};

// One kinah short of the total is refused with nothing taken; the total itself is taken whole (the kinah leaves before any DAO write, so this
// runs without a database: the store that follows fails and ends the send)
TEST_P(MailCommissionTest, TheSenderPaysJavasTotal) {
	const CommissionRow& row = GetParam();
	goOnline(partner());
	if (row.itemId != 0)
		give(a(), STACK, row.itemId, row.stock);
	setKinah(a(), KINAH_ITEM, row.total - 1);

	send(row.itemId != 0 ? STACK : 0, row.count, row.kinah, row.type);
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()}));
	EXPECT_EQ(kinah(a()), row.total - 1);
	if (row.itemId != 0)
		EXPECT_EQ(countOf(a(), row.itemId), row.stock);

	a().getInventory().increaseKinah(1);
	clearSent();
	send(row.itemId != 0 ? STACK : 0, row.count, row.kinah, row.type);
	EXPECT_EQ(kinah(a()), 0);
	EXPECT_TRUE(ofOpcode(sent(), SM_SYSTEM_MESSAGE_OPCODE).empty());
	// without a database the store fails (InventoryDAO.store or MailDAO.storeLetter answer false) and the send ends: no success, no letter
	EXPECT_TRUE(ofOpcode(sent(), SM_MAIL_SERVICE_OPCODE).empty());
	EXPECT_TRUE(sentB().empty());
	EXPECT_TRUE(partner().getMailbox()->getLetters().empty());
}

INSTANTIATE_TEST_SUITE_P(Oracle, MailCommissionTest, ::testing::ValuesIn(COMMISSION_ROWS),
	[](const ::testing::TestParamInfo<CommissionRow>& info) { return std::string(info.param.name); });

// ---------------------------------------------------------------------------------------------------------------------------------------------
// untradeable items (MailService.java:117-127) and packs (:143-144)
// ---------------------------------------------------------------------------------------------------------------------------------------------

// Fruit juice is not tradeable and its template has no disposition: "can not be traded, hack" - after the price check, before anything is taken
TEST_F(MailServiceTest, AnUntradeableItemWithoutADispositionIsNotSent) {
	goOnline(partner());
	give(a(), STACK, FRUIT_JUICE, 1);
	setKinah(a(), KINAH_ITEM, 13);
	send(STACK, 1, 0);
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(kinah(a()), 13);
	EXPECT_EQ(countOf(a(), FRUIT_JUICE), 1);

	a().getInventory().decreaseKinah(1); // the price comes first
	clearSent();
	send(STACK, 1, 0);
	EXPECT_EQ(sent(), cp::exactly({notEnoughMoney()}));
}

// The shield is not tradeable; its disposition asks 4 Special Courier Passes (188950008), which the letter takes before the shield leaves
TEST_F(MailServiceTest, AnUntradeableItemTakesItsDispositionItems) {
	goOnline(partner());
	give(a(), STACK, SQUAD_LEADERS_SHIELD, 1);
	give(a(), OTHER_ITEM, COURIER_PASS_ABYSS_FABLED, 3);
	setKinah(a(), KINAH_ITEM, 3486); // oracle: --mail 115000892:1:0 -> 3486

	send(STACK, 1, 0); // three passes are too few: nothing is taken, nothing is said
	EXPECT_TRUE(sent().empty());
	EXPECT_EQ(countOf(a(), COURIER_PASS_ABYSS_FABLED), 3);
	EXPECT_EQ(countOf(a(), SQUAD_LEADERS_SHIELD), 1);
	EXPECT_EQ(kinah(a()), 3486);

	a().getInventory().increaseItemCount(*a().getInventory().getItemByObjId(OTHER_ITEM), 1); // exactly the four the disposition asks
	clearSent();
	send(STACK, 1, 0);
	EXPECT_EQ(countOf(a(), COURIER_PASS_ABYSS_FABLED), 0);
	EXPECT_EQ(countOf(a(), SQUAD_LEADERS_SHIELD), 0);
	EXPECT_EQ(kinah(a()), 0);
	std::vector<std::vector<uint8_t>> deleted = ofOpcode(sent(), SM_DELETE_ITEM_OPCODE);
	ASSERT_FALSE(deleted.empty());
	EXPECT_EQ(deleted.back(), deleteItem(STACK)); // after the passes' own deletion
}

// A packed untradeable item needs no disposition, and the letter unpacks it (pack count * -1) and moves it to the mailbox
TEST_F(MailServiceTest, APackedItemNeedsNoDispositionAndIsUnpacked) {
	goOnline(partner());
	runtime::Ref<Item> shield = Item::create(STACK, SQUAD_LEADERS_SHIELD, 1, std::nullopt, 0, "", 0, 0, false, false, 0,
		model::items::storage::getId(StorageType::CUBE), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* packCount */ 1, false, 0, 0);
	a().getInventory().onLoadHandler(*shield);
	given.push_back(shield);
	give(a(), OTHER_ITEM, COURIER_PASS_ABYSS_FABLED, 4);
	setKinah(a(), KINAH_ITEM, 3486);

	send(STACK, 1, 0);
	EXPECT_EQ(countOf(a(), COURIER_PASS_ABYSS_FABLED), 4);
	EXPECT_EQ(countOf(a(), SQUAD_LEADERS_SHIELD), 0);
	EXPECT_EQ(kinah(a()), 0);
	EXPECT_EQ(shield->getPackCount(), -1);
	EXPECT_EQ(shield->getItemLocation(), MAILBOX_LOCATION);
}

// An unpacked item (pack count -1) is untradeable again, so it takes its disposition items, and the letter leaves its pack count alone
// (`if (attachedItem.getPackCount() > 0)`, MailService.java:143): flipping -1 to 1 would pack it, tradeable, once more
TEST_F(MailServiceTest, AnUnpackedItemIsNotPackedAgain) {
	goOnline(partner());
	runtime::Ref<Item> shield = Item::create(STACK, SQUAD_LEADERS_SHIELD, 1, std::nullopt, 0, "", 0, 0, false, false, 0,
		model::items::storage::getId(StorageType::CUBE), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* packCount */ -1, false, 0, 0);
	a().getInventory().onLoadHandler(*shield);
	given.push_back(shield);
	give(a(), OTHER_ITEM, COURIER_PASS_ABYSS_FABLED, 4);
	setKinah(a(), KINAH_ITEM, 3486);

	send(STACK, 1, 0);
	EXPECT_EQ(countOf(a(), COURIER_PASS_ABYSS_FABLED), 0);
	EXPECT_EQ(countOf(a(), SQUAD_LEADERS_SHIELD), 0);
	EXPECT_EQ(kinah(a()), 0);
	EXPECT_EQ(shield->getPackCount(), -1);
	EXPECT_EQ(shield->getItemLocation(), MAILBOX_LOCATION);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// the letter (MailService.java:115-169) and the recipient's mailbox (SystemMailService.updateRecipientMailbox, :116-138)
// ---------------------------------------------------------------------------------------------------------------------------------------------

// Part of a stack: a new item of the count goes to the letter; the offline recipient's mailbox counter rises in the players row
TEST_F(MailServiceTest, PartOfAStackGoesToAnOfflineRecipient) {
	MAIL_REQUIRE_DATABASE();
	give(a(), STACK, MINOR_LIFE_POTION, 20);
	setKinah(a(), KINAH_ITEM, 1000);

	MailService::sendMail(a(), B_NAME, "Potions", "Five for you", STACK, 5, 200, LetterType::NORMAL);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), mailMessage(MAIL_SEND_SUCCESS));
	EXPECT_TRUE(ofOpcode(packets, SM_DELETE_ITEM_OPCODE).empty());
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 15);
	EXPECT_EQ(kinah(a()), 1000 - 251);

	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail"), 1);
	std::optional<int64_t> itemId = queryLong("SELECT attached_item_id FROM mail WHERE mail_recipient_id = 710202");
	ASSERT_TRUE(itemId);
	EXPECT_NE(*itemId, STACK);
	EXPECT_EQ(queryLong("SELECT attached_kinah_count FROM mail"), 200);
	EXPECT_EQ(queryLong("SELECT unread FROM mail"), 1);
	EXPECT_EQ(queryLong("SELECT express FROM mail"), 0);
	EXPECT_EQ(queryString("SELECT sender_name FROM mail"), std::string(A_NAME));
	EXPECT_EQ(queryString("SELECT mail_title FROM mail"), "Potions");
	EXPECT_EQ(queryString("SELECT mail_message FROM mail"), "Five for you");
	std::string row = sqlOf(" FROM inventory WHERE item_unique_id = ", static_cast<int32_t>(*itemId));
	EXPECT_EQ(queryLong("SELECT item_id" + row), MINOR_LIFE_POTION);
	EXPECT_EQ(queryLong("SELECT item_count" + row), 5);
	EXPECT_EQ(queryLong("SELECT item_owner" + row), B_ID);
	EXPECT_EQ(queryLong("SELECT item_location" + row), MAILBOX_LOCATION);
	EXPECT_EQ(queryLong("SELECT mailbox_letters FROM players WHERE id = 710202"), 1);
}

// The whole stack: the item itself goes (SM_DELETE_ITEM to the sender), its inventory row changes owner and location
TEST_F(MailServiceTest, AWholeStackKeepsItsObjectId) {
	MAIL_REQUIRE_DATABASE();
	give(a(), STACK, MINOR_LIFE_POTION, 5);
	execute(sqlOf("INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner, item_location) VALUES (", STACK) +
		", 162000002, 5, 710101, 0)");
	setKinah(a(), KINAH_ITEM, 1000);

	send(STACK, 5, 0);
	std::vector<std::vector<uint8_t>> packets = sent();
	EXPECT_EQ(ofOpcode(packets, SM_DELETE_ITEM_OPCODE), cp::exactly({deleteItem(STACK)}));
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), mailMessage(MAIL_SEND_SUCCESS));
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 0);
	EXPECT_EQ(queryLong("SELECT attached_item_id FROM mail"), STACK);
	std::string row = sqlOf(" FROM inventory WHERE item_unique_id = ", STACK);
	EXPECT_EQ(queryLong("SELECT item_owner" + row), B_ID);
	EXPECT_EQ(queryLong("SELECT item_location" + row), MAILBOX_LOCATION);
	EXPECT_EQ(queryLong("SELECT item_count" + row), 5);
}

// A socketed item's manastones are saved with it (ItemStoneListDAO.save, MailService.java:157-158), after its own row (the item_stones foreign
// key); the Training Sword costs the plain 13 kinah (oracle: --mail 100000094:1:0 -> 13)
TEST_F(MailServiceTest, ASocketedItemsManastonesAreSaved) {
	MAIL_REQUIRE_DATABASE();
	Item& sword = give(a(), STACK, TRAINING_SWORD, 1);
	execute(sqlOf("INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner, item_location) VALUES (", STACK) +
		", 100000094, 1, 710101, 0)");
	sword.getItemStones()->add(model::items::ManaStone::create(STACK, MANASTONE_HP_20, 0, model::gameobjects::Persistable_PersistentState::NEW));
	setKinah(a(), KINAH_ITEM, 1000);

	send(STACK, 1, 0);
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), mailMessage(MAIL_SEND_SUCCESS));
	EXPECT_EQ(kinah(a()), 1000 - 13);
	std::string stones = sqlOf(" FROM item_stones WHERE item_unique_id = ", STACK);
	EXPECT_EQ(queryLong("SELECT COUNT(*)" + stones), 1);
	EXPECT_EQ(queryLong("SELECT item_id" + stones), MANASTONE_HP_20);
	EXPECT_EQ(queryLong("SELECT slot" + stones), 0);
	EXPECT_EQ(queryLong("SELECT item_owner FROM inventory WHERE item_unique_id = " + std::to_string(STACK)), B_ID);
}

// MailService.java:155-156: a failed InventoryDAO.store ends the send before the letter is stored (m5c-plan.md risk 1) - the sender has paid and
// the stack has left him, and no letter exists. The stack is new (never saved: its INSERT runs) and a row with its object id exists already, so
// the INSERT fails; the letter's own row could be written (the mail table has no foreign key on attached_item_id)
TEST_F(MailServiceTest, AFailedItemStoreSendsNoLetter) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	give(a(), STACK, MINOR_LIFE_POTION, 5).setPersistentState(model::gameobjects::Persistable_PersistentState::NEW);
	execute(sqlOf("INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner, item_location) VALUES (", STACK) +
		", 162000002, 5, 710101, 0)");
	setKinah(a(), KINAH_ITEM, 1000);

	send(STACK, 5, 0);
	EXPECT_EQ(countOf(a(), MINOR_LIFE_POTION), 0);
	EXPECT_EQ(kinah(a()), 1000 - 48); // oracle: --mail 162000002:5:0 -> 48
	EXPECT_TRUE(ofOpcode(sent(), SM_MAIL_SERVICE_OPCODE).empty());
	EXPECT_TRUE(sentB().empty());
	EXPECT_TRUE(partner().getMailbox()->getLetters().empty());
	EXPECT_EQ(b.commonData->getMailboxLetters(), 0);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail"), 0);
	EXPECT_EQ(queryLong(sqlOf("SELECT item_owner FROM inventory WHERE item_unique_id = ", STACK)), A_ID); // the old row, untouched
}

// An online recipient with a closed mailbox gets the letter in memory and SM_MAIL_SERVICE(0); the players row is not touched. His letter counter
// is set to the mailbox's size (SystemMailService.java:124), not raised by one; the letter is stamped with the time of the send
// (MailService.java:151)
TEST_F(MailServiceTest, AnOnlineRecipientGetsTheLetterAtOnce) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	setKinah(a(), KINAH_ITEM, 1000);
	b.commonData->setMailboxLetters(5); // a stale counter over an empty mailbox

	const int64_t before = commons::utils::currentTimeMillis();
	send(0, 0, 200);
	const int64_t after = commons::utils::currentTimeMillis();
	std::vector<std::vector<uint8_t>> packets = sent();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), mailMessage(MAIL_SEND_SUCCESS));
	EXPECT_EQ(sentB(), cp::exactly({mailboxState(1, 1, 0, 0)}));
	std::vector<runtime::Ptr<Letter>> letters = partner().getMailbox()->getLetters();
	ASSERT_EQ(letters.size(), 1u);
	EXPECT_EQ(letters[0]->getSenderName(), A_NAME);
	EXPECT_EQ(letters[0]->getTitle(), "Title");
	EXPECT_EQ(letters[0]->getAttachedKinah(), 200);
	EXPECT_TRUE(letters[0]->isUnread());
	ASSERT_TRUE(letters[0]->getTimeStamp());
	EXPECT_GE(letters[0]->getTimeStamp()->time_since_epoch().count(), before);
	EXPECT_LE(letters[0]->getTimeStamp()->time_since_epoch().count(), after);
	EXPECT_EQ(b.commonData->getMailboxLetters(), 1);
	EXPECT_EQ(queryLong("SELECT mailbox_letters FROM players WHERE id = 710202"), 0);
	EXPECT_EQ(queryLong(sqlOf("SELECT COUNT(*) FROM mail WHERE mail_unique_id = ", letters[0]->getObjectId())), 1);
}

// A recipient who looks into his mailbox (state REGULAR) also gets the refreshed list (SystemMailService.java:128-132)
TEST_F(MailServiceTest, AnOpenMailboxIsRefreshed) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	partner().getMailbox()->mailBoxState.set(PlayerMailboxState::REGULAR);
	give(a(), STACK, MINOR_LIFE_POTION, 20);
	setKinah(a(), KINAH_ITEM, 1000);

	send(STACK, 5, 200);
	std::vector<std::vector<uint8_t>> packets = sentB();
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(packets[0], mailboxState(1, 1, 0, 0));
	LetterList list = decodeLetterList(packets[1]);
	EXPECT_EQ(list.playerObjId, B_ID);
	EXPECT_EQ(list.signedCount, -1);
	ASSERT_EQ(list.letters.size(), 1u);
	EXPECT_EQ(list.letters[0].senderName, A_NAME);
	EXPECT_EQ(list.letters[0].title, "Title");
	EXPECT_FALSE(list.letters[0].isRead);
	EXPECT_EQ(list.letters[0].attachedItemId, MINOR_LIFE_POTION);
	EXPECT_EQ(list.letters[0].attachedKinah, 200);
	EXPECT_EQ(list.letters[0].letterType, 0);
	EXPECT_EQ(queryLong("SELECT attached_item_id FROM mail"), list.letters[0].attachedItemObjId);
}

// An express letter to a recipient at the postman (state EXPRESS): the list holds only the unread express letters, then STR_POSTMAN_NOTIFY
TEST_F(MailServiceTest, AnExpressLetterReachesThePostman) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	putLetter(700001, nullptr, 0, 1000); // an unread normal letter
	b.commonData->setMailboxLetters(1);
	partner().getMailbox()->mailBoxState.set(PlayerMailboxState::EXPRESS);
	setKinah(a(), KINAH_ITEM, 1000);

	send(0, 0, 0, LetterType::EXPRESS);
	EXPECT_EQ(kinah(a()), 1000 - 706);
	std::vector<std::vector<uint8_t>> packets = sentB();
	ASSERT_EQ(packets.size(), 3u);
	EXPECT_EQ(packets[0], mailboxState(2, 2, 1, 0));
	LetterList list = decodeLetterList(packets[1]);
	ASSERT_EQ(list.letters.size(), 1u);
	EXPECT_EQ(list.letters[0].letterType, 1);
	EXPECT_NE(list.letters[0].letterId, 700001);
	EXPECT_EQ(packets[2], serializedForB(SM_SYSTEM_MESSAGE::STR_POSTMAN_NOTIFY()));
	EXPECT_EQ(b.commonData->getMailboxLetters(), 2);
	EXPECT_EQ(queryLong("SELECT express FROM mail"), 1);
}

// title.substring(0, 20) and message.substring(0, 1000) in UTF-16 units
TEST_F(MailServiceTest, TheTitleAndTheMessageAreCutToJavasLengths) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	setKinah(a(), KINAH_ITEM, 1000);
	std::string title;
	for (int i = 0; i < 21; i++)
		title += "\xC3\xA4"; // 21 x U+00E4
	std::string message(999, 'm');
	message += "\xC3\xA4\xC3\xA4"; // 1001 units

	MailService::sendMail(a(), B_NAME, title, message, 0, 0, 0, LetterType::NORMAL);
	std::vector<runtime::Ptr<Letter>> letters = partner().getMailbox()->getLetters();
	ASSERT_EQ(letters.size(), 1u);
	EXPECT_EQ(letters[0]->getTitle(), title.substr(0, 40));
	EXPECT_EQ(letters[0]->getMessage(), message.substr(0, 1001)); // 999 'm' + one U+00E4
	EXPECT_EQ(queryString("SELECT mail_title FROM mail"), title.substr(0, 40));

	MailService::sendMail(a(), B_NAME, title.substr(0, 40), std::string(1000, 'n'), 0, 0, 0, LetterType::NORMAL); // 20 and 1000 stay whole
	letters = partner().getMailbox()->getLetters();
	ASSERT_EQ(letters.size(), 2u);
	for (const runtime::Ptr<Letter>& letter : letters) {
		if (letter->getMessage().starts_with("n")) {
			EXPECT_EQ(letter->getTitle(), title.substr(0, 40));
			EXPECT_EQ(letter->getMessage(), std::string(1000, 'n'));
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// readMail (MailService.java:204-213)
// ---------------------------------------------------------------------------------------------------------------------------------------------

TEST_F(MailServiceTest, ReadingALetterSendsItAndMarksItRead) {
	runtime::Ref<Letter> letter = putLetter(700001, nullptr, 0, 1'700'000'123'456, true, LetterType::NORMAL, "Hello");
	LogCapture mailLog("MAIL_LOG");

	MailService::readMail(partner(), 700002);
	EXPECT_TRUE(sentB().empty());
	EXPECT_TRUE(mailLog.contains("Cannot read mail 710202 700002"));

	MailService::readMail(partner(), 700001);
	std::vector<std::vector<uint8_t>> packets = sentB();
	ASSERT_EQ(packets.size(), 1u);
	PacketReader reader(cp::bodyOf(packets[0]));
	EXPECT_EQ(reader.C(), 3);
	EXPECT_EQ(reader.D(), B_ID); // recipient
	reader.D();                  // total + unread * 0x10000 (the counts at serialization)
	EXPECT_EQ(reader.D(), 0);    // unread express + black cloud
	EXPECT_EQ(reader.D(), 700001);
	EXPECT_EQ(reader.D(), B_ID);
	EXPECT_EQ(reader.S(), "Sender");
	EXPECT_EQ(reader.S(), "Hello");
	EXPECT_EQ(reader.S(), "");
	EXPECT_EQ(reader.Q(), 0); // no item
	EXPECT_EQ(reader.Q(), 0);
	EXPECT_EQ(reader.D(), 0);
	EXPECT_EQ(reader.D(), 0); // kinah
	EXPECT_EQ(reader.D(), 0);
	EXPECT_EQ(reader.C(), 0);
	EXPECT_EQ(reader.D(), 1'700'000'123); // the time stamp in seconds
	EXPECT_EQ(reader.C(), 0);
	EXPECT_FALSE(letter->isUnread());
	EXPECT_EQ(letter->getPersistentState(), model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// getAttachments (MailService.java:215-254)
// ---------------------------------------------------------------------------------------------------------------------------------------------

TEST_F(MailServiceTest, TakingTheItemMovesItToTheCube) {
	runtime::Ref<Item> potions = itemtest::loadedItem(600001, MINOR_LIFE_POTION, 5, StorageType::MAILBOX);
	runtime::Ref<Letter> letter = putLetter(700001, potions, 0, 1000);

	MailService::getAttachments(partner(), 700002, 0); // no such letter
	MailService::getAttachments(partner(), 700001, 2); // no such attachment type
	EXPECT_TRUE(sentB().empty());

	MailService::getAttachments(partner(), 700001, 0);
	std::vector<std::vector<uint8_t>> packets = sentB();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(itemtest::javaOpcodeOf(packets.front()), itemtest::SM_INVENTORY_ADD_ITEM_OPCODE);
	EXPECT_EQ(packets.back(), letterState(700001, 0));
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 5);
	EXPECT_FALSE(letter->getAttachedItem());
	EXPECT_EQ(letter->getPersistentState(), model::gameobjects::Persistable_PersistentState::UPDATE_REQUIRED);

	clearSentB();
	MailService::getAttachments(partner(), 700001, 0); // taken already
	EXPECT_TRUE(sentB().empty());
	taskmanager::tasks::ExpireTimerTask::getInstance().unregisterExpirables(partner());
}

TEST_F(MailServiceTest, AFullCubeKeepsTheItemInTheLetter) {
	give(partner(), 600003, SPARKIE_CARAPACE_FRAGMENT, 1);
	partner().getInventory().setLimit(1);
	runtime::Ref<Item> potions = itemtest::loadedItem(600001, MINOR_LIFE_POTION, 5, StorageType::MAILBOX);
	runtime::Ref<Letter> letter = putLetter(700001, potions, 0, 1000);

	MailService::getAttachments(partner(), 700001, 0);
	EXPECT_EQ(sentB(), cp::exactly({serializedForB(SM_SYSTEM_MESSAGE::STR_MAIL_TAKE_ALL_CANCEL())}));
	EXPECT_EQ(letter->getAttachedItem().get(), potions.get());
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 0);
}

// Storage.add refuses an item whose object id the cube holds already (ItemStorage.putItem's putIfAbsent), and the take ends there
// (MailService.java:234-235): no packet, the item stays in the letter
TEST_F(MailServiceTest, AnItemTheCubeHoldsAlreadyStaysInTheLetter) {
	give(partner(), 600001, MINOR_LIFE_POTION, 2);
	runtime::Ref<Item> potions = itemtest::loadedItem(600001, MINOR_LIFE_POTION, 5, StorageType::MAILBOX);
	runtime::Ref<Letter> letter = putLetter(700001, potions, 0, 1000);

	MailService::getAttachments(partner(), 700001, 0);
	EXPECT_TRUE(sentB().empty());
	EXPECT_EQ(letter->getAttachedItem().get(), potions.get());
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 2);
	EXPECT_EQ(potions->getItemLocation(), MAILBOX_LOCATION);
}

// An item whose expire time passed is deleted (its row too) instead of taken; the letter lets it go all the same
TEST_F(MailServiceTest, AnExpiredItemIsDeletedInsteadOfTaken) {
	MAIL_REQUIRE_DATABASE();
	runtime::Ref<Item> expired = Item::create(600001, MINOR_LIFE_POTION, 5, std::nullopt, 0, "", /* expireTime */ 1, 0, false, false, 0,
		MAILBOX_LOCATION, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, false, 0, 0);
	execute("INSERT INTO inventory (item_unique_id, item_id, item_count, item_owner, item_location) VALUES (600001, 162000002, 5, 710202, " +
		std::to_string(MAILBOX_LOCATION) + ")");
	runtime::Ref<Letter> letter = putLetter(700001, expired, 0, 1000);

	MailService::getAttachments(partner(), 700001, 0);
	EXPECT_EQ(sentB(), cp::exactly({letterState(700001, 0)}));
	EXPECT_EQ(countOf(partner(), MINOR_LIFE_POTION), 0);
	EXPECT_FALSE(letter->getAttachedItem());
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM inventory WHERE item_unique_id = 600001"), 0);
}

// "fix for kinah dupe": the letter loses its kinah and is stored BEFORE the kinah is added. Without a database the store fails, so the kinah is
// audited and never added - it is gone from the letter all the same (MailService.java:244-249)
TEST_F(MailServiceTest, TheKinahIsStoredBeforeItIsAdded) {
	setKinah(partner(), B_KINAH_ITEM, 50);
	runtime::Ref<Letter> letter = putLetter(700001, nullptr, 200, 1000);
	LogCapture audit("AUDIT_LOG");

	MailService::getAttachments(partner(), 700001, 1);
	EXPECT_EQ(kinah(partner()), 50);
	EXPECT_EQ(letter->getAttachedKinah(), 0);
	EXPECT_TRUE(sentB().empty());
	// "Location: " + player.getPosition() (WorldPosition.java:218 toString: the fixture's position of B, not spawned)
	EXPECT_TRUE(audit.contains("tried to use kinah mail exploit. Location: WorldPosition [mapId=210010000, x=102.0, y=100.0, z=50.0, heading=0, "
							   "isSpawned=false], kinah count: 200"));
}

TEST_F(MailServiceTest, TakingTheKinahStoresTheLetterThenPays) {
	MAIL_REQUIRE_DATABASE();
	setKinah(partner(), B_KINAH_ITEM, 50);
	runtime::Ref<Letter> letter = Letter::create(700001, B_ID, nullptr, 200, "Kinah", "", "Sender",
		commons::database::Timestamp(std::chrono::milliseconds(1'700'000'000'000)), true, LetterType::NORMAL);
	ASSERT_TRUE(dao::MailDAO::storeLetter(*letter)); // the row, as MailService.sendMail stored it
	partner().getMailbox()->putLetterToMailbox(*letter);

	MailService::getAttachments(partner(), 700001, 1);
	EXPECT_EQ(kinah(partner()), 250);
	EXPECT_EQ(letter->getAttachedKinah(), 0);
	EXPECT_EQ(queryLong("SELECT attached_kinah_count FROM mail WHERE mail_unique_id = 700001"), 0);
	std::vector<std::vector<uint8_t>> packets = sentB();
	ASSERT_FALSE(packets.empty());
	EXPECT_EQ(packets.back(), letterState(700001, 1));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// deleteMail (MailService.java:256-263)
// ---------------------------------------------------------------------------------------------------------------------------------------------

TEST_F(MailServiceTest, DeletedLettersLeaveTheMailboxAndTheDatabase) {
	MAIL_REQUIRE_DATABASE();
	for (int32_t id : {700001, 700002, 700003}) {
		runtime::Ref<Letter> letter = Letter::create(id, B_ID, nullptr, 0, "L", "", "Sender",
			commons::database::Timestamp(std::chrono::milliseconds(1'700'000'000'000)), true, LetterType::NORMAL);
		ASSERT_TRUE(dao::MailDAO::storeLetter(*letter));
		partner().getMailbox()->putLetterToMailbox(*letter);
	}
	b.commonData->setMailboxLetters(3);

	const int32_t ids[] = {700001, 700003};
	MailService::deleteMail(partner(), ids);
	std::vector<runtime::Ptr<Letter>> left = partner().getMailbox()->getLetters();
	ASSERT_EQ(left.size(), 1u);
	EXPECT_EQ(left[0]->getObjectId(), 700002);
	EXPECT_EQ(b.commonData->getMailboxLetters(), 1);
	EXPECT_EQ(queryLong("SELECT COUNT(*) FROM mail"), 1);
	EXPECT_EQ(queryLong("SELECT mail_unique_id FROM mail"), 700002);
	// SM_MAIL_SERVICE(6): C(6), D(total + unread * 0x10000), D(unread express + black cloud), H(count), the ids
	EXPECT_EQ(sentB(), cp::exactly({itemtest::javaPacket(SM_MAIL_SERVICE_OPCODE, PacketWriter().C(6).D(1 + 0x10000).D(0).H(2).D(700001).D(700003))}));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------
// sendMailList (MailService.java:270-281)
// ---------------------------------------------------------------------------------------------------------------------------------------------

TEST_F(MailServiceTest, TheListIsNewestFirstAndTheExpressListOnlyUnreadExpress) {
	putLetter(700001, nullptr, 0, 1000, true, LetterType::NORMAL, "Oldest");
	putLetter(700002, nullptr, 0, 3000, true, LetterType::EXPRESS, "Newest");
	putLetter(700003, nullptr, 0, 2000, false, LetterType::EXPRESS, "Middle");
	putLetter(700004, nullptr, 0, 1500, true, LetterType::BLACKCLOUD, "Cloud");

	MailService::sendMailList(partner(), false, true);
	std::vector<std::vector<uint8_t>> packets = sentB();
	ASSERT_EQ(packets.size(), 2u);
	EXPECT_EQ(packets[0], mailboxState(4, 3, 1, 1));
	LetterList list = decodeLetterList(packets[1]);
	EXPECT_EQ(list.signedCount, -4);
	std::vector<int32_t> order;
	for (const ListedLetter& letter : list.letters)
		order.push_back(letter.letterId);
	EXPECT_EQ(order, (std::vector<int32_t>{700002, 700003, 700004, 700001}));

	// express: an unread letter whose isExpress() is set (EXPRESS and BLACKCLOUD, Letter.java), newest first; no refresh packet
	clearSentB();
	MailService::sendMailList(partner(), true, false);
	packets = sentB();
	ASSERT_EQ(packets.size(), 1u);
	list = decodeLetterList(packets[0]);
	order.clear();
	for (const ListedLetter& letter : list.letters)
		order.push_back(letter.letterId);
	EXPECT_EQ(order, (std::vector<int32_t>{700002, 700004}));
	EXPECT_EQ(list.signedCount, -2);
}

TEST_F(MailServiceTest, AnEmptyMailboxSendsOneEmptyList) {
	MailService::sendMailList(partner(), false, false);
	std::vector<std::vector<uint8_t>> packets = sentB();
	ASSERT_EQ(packets.size(), 1u);
	LetterList list = decodeLetterList(packets[0]);
	EXPECT_EQ(list.signedCount, 0);
	EXPECT_TRUE(list.letters.empty());
}

// A list larger than one packet body is split (DynamicServerPacketBodySplitList): only the last part's count is negative, the order runs on
TEST_F(MailServiceTest, ALongListIsSplitAcrossPackets) {
	// 100 letters (the mailbox's visible maximum) of 22 + 14 + 122 bytes each (DYNAMIC_BODY_PART_SIZE_CALCULATOR) exceed one body of 8,177
	const std::string title(60, 't');
	for (int32_t i = 0; i < 100; i++)
		putLetter(700001 + i, nullptr, 0, 1000 + i, true, LetterType::NORMAL, title);

	MailService::sendMailList(partner(), false, false);
	std::vector<std::vector<uint8_t>> packets = sentB();
	ASSERT_GE(packets.size(), 2u);
	std::vector<int32_t> order;
	for (size_t i = 0; i < packets.size(); i++) {
		LetterList list = decodeLetterList(packets[i]);
		if (i + 1 < packets.size())
			EXPECT_GT(list.signedCount, 0);
		else
			EXPECT_LT(list.signedCount, 0);
		for (const ListedLetter& letter : list.letters)
			order.push_back(letter.letterId);
	}
	ASSERT_EQ(order.size(), 100u);
	for (int32_t i = 0; i < 100; i++)
		EXPECT_EQ(order[static_cast<size_t>(i)], 700100 - i);
}

} // namespace
} // namespace aion::gameserver::economy::test::mail
