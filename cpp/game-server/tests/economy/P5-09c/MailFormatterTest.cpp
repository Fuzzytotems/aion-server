// MailFormatter (MailFormatter.java:21-169) and the enums AbyssSiegeLevel / SiegeResult (services/mail): each sender's template, the values its
// anonymous MailPart gives the template's parameters, and the letter SystemMailService delivers to the online recipient B. The template rows
// are mail_templates.xml's (:39-54 $$ABYSS_REWARD_MAIL, :56-91 $$HS_OVERDUE_*, :93-121 $$HS_AUCTION_MAIL, :123-137 $$CASH_ITEM_MAIL, :233-260
// $$GD_REWARD_MAIL), verbatim. The fixture is MailTestSupport.h's; a delivered letter needs the economy test database.

#include "MailTestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/dataholders/HouseData.bind.h"
#include "aion/gameserver/dataholders/HouseData.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/model/templates/mail/Mails.bind.h"
#include "aion/gameserver/model/templates/mail/Mails.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/services/mail/AbyssSiegeLevelInfo.h"
#include "aion/gameserver/services/mail/AuctionResultInfo.h"
#include "aion/gameserver/services/mail/MailFormatter.h"
#include "aion/gameserver/services/mail/SiegeResultInfo.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::economy::test::mail {
namespace {

using services::mail::AbyssSiegeLevel;
using services::mail::AuctionResult;
using services::mail::MailFormatter;
using services::mail::SiegeResult;

constexpr std::string_view MAIL_TEMPLATES_XML = R"xml(<mails>
	<mail name="$$ABYSS_REWARD_MAIL">
		<template name="" race="PC_ALL">
			<sender id="0"/>
			<title id="0">
				<param id="siegelocid"/>
				<param id="resultid"/>
				<param id="raceid"/>
			</title>
			<header id="0"/>
			<body id="0">
				<param id="datetime"/>
				<param id="rankid"/>
			</body>
			<tail id="0"/>
		</template>
	</mail>
	<mail name="$$HS_OVERDUE_1ST">
		<template name="" race="PC_ALL">
			<sender id="904143"/>
			<title id="904144"/>
			<header id="904145"/>
			<body id="904146"/>
			<tail id="904147">
				<param id="datetime"/>
				<param id="address"/>
			</tail>
		</template>
	</mail>
	<mail name="$$HS_OVERDUE_2ND">
		<template name="" race="PC_ALL">
			<sender id="904148"/>
			<title id="904149"/>
			<header id="904150"/>
			<body id="904151"/>
			<tail id="904152">
				<param id="datetime"/>
				<param id="address"/>
			</tail>
		</template>
	</mail>
	<mail name="$$HS_OVERDUE_3RD">
		<template name="" race="PC_ALL">
			<sender id="904153"/>
			<title id="904154"/>
			<header id="904155"/>
			<body id="904156"/>
			<tail id="904157">
				<param id="datetime"/>
				<param id="address"/>
			</tail>
		</template>
	</mail>
	<mail name="$$HS_AUCTION_MAIL">
		<template name="" race="ASMODIANS">
			<sender id="903901"/>
			<title id="0">
				<param id="resultid"/>
				<param id="raceid"/>
			</title>
			<header id="0"/>
			<body id="0">
				<param id="datetime"/>
				<param id="address"/>
			</body>
			<tail id="0"/>
		</template>
		<template name="" race="ELYOS">
			<sender id="903401"/>
			<title id="0">
				<param id="resultid"/>
				<param id="raceid"/>
			</title>
			<header id="0"/>
			<body id="0">
				<param id="datetime"/>
				<param id="address"/>
				<param id="datetime"/>
			</body>
			<tail id="0"/>
		</template>
	</mail>
	<mail name="$$CASH_ITEM_MAIL">
		<template name="" race="PC_ALL">
			<sender id="901526"/>
			<title id="0">
				<param id="itemid"/>
				<param id="count"/>
			</title>
			<header id="0"/>
			<body id="0">
				<param id="unk1"/>
				<param id="purchasedate"/>
			</body>
			<tail id="0"/>
		</template>
	</mail>
	<mail name="$$GD_REWARD_MAIL">
		<template name="" race="ELYOS">
			<sender id="905975" />
			<title id="905976" />
			<header id="0" />
			<body id="905977">
				<param id="month" />
				<param id="day" />
				<param id="territorial" />
				<param id="legionName" />
			</body>
			<tail id="905978" />
		</template>
		<template name="" race="ASMODIANS">
			<sender id="905979" />
			<title id="905980" />
			<header id="0" />
			<body id="905981">
				<param id="month" />
				<param id="day" />
				<param id="territorial" />
				<param id="legionName" />
			</body>
			<tail id="905982" />
		</template>
	</mail>
</mails>)xml";

constexpr int64_t DAY = 24LL * 60 * 60 * 1000;

/** two addresses of house_lands.xml's land 325001 (an address binds only inside its land), as tests/legionhouse/HouseModelTest.cpp has them */
constexpr std::string_view HOUSE_LANDS = R"(<house_lands>
	<land id="325001" teleport_npc="810003" manager_npc="810017" sign_home="810007" sign_waiting="810006" sign_sale="810005" sign_nosale="810004">
		<addresses>
			<address id="10001" map="700010000" town="1001" x="696.159973" y="1999.969971" z="174.42577"/>
			<address id="10002" map="700010000" town="1001" x="609.904907" y="2057.186279" z="174.90712"/>
		</addresses>
		<buildings>
			<building id="350000" default="true" type="PERSONAL_FIELD" size="HOUSE"/>
		</buildings>
		<sale level="50" gold_price="1000000000" point_price="0"/>
		<fee>20000000</fee>
		<caps room="false" floor="false" emblemId="2" addon="true"/>
	</land>
</house_lands>)";

class MailFormatterTest : public MailTest {
protected:
	void SetUp() override {
		MailTest::SetUp();
		dataholders::DataManager::SYSTEM_MAIL_TEMPLATES.publish(xml::bindString<model::templates::mail::Mails>(context, MAIL_TEMPLATES_XML));
		if (!dataholders::DataManager::HOUSE_DATA) {
			dataholders::DataManager::HOUSE_DATA.publish(xml::bindString<dataholders::HouseData>(context, HOUSE_LANDS));
			publishedHouseData = true;
		}
	}

	void TearDown() override {
		houses.clear();
		MailTest::TearDown();
		dataholders::DataManager::SYSTEM_MAIL_TEMPLATES.resetForTests();
		if (publishedHouseData)
			dataholders::DataManager::HOUSE_DATA.resetForTests();
	}

	/** a House at the address <id> of HOUSE_LANDS (the formatter reads its address id only) */
	model::house::House& house(int32_t addressId) {
		houses.push_back(model::gameobjects::VisibleObject::create<model::house::House>(dataholders::DataManager::HOUSE_DATA->getAddress(addressId), 0));
		return *houses.back();
	}

	/** the only letter of B's mailbox */
	runtime::Ptr<Letter> onlyLetter() {
		std::vector<runtime::Ptr<Letter>> letters = partner().getMailbox()->getLetters();
		EXPECT_EQ(letters.size(), 1u);
		return letters.empty() ? nullptr : letters[0];
	}

	xml::LoadContext context;
	bool publishedHouseData = false;
	std::vector<runtime::Ref<model::house::House>> houses;
};

// ---- the enums (AbyssSiegeLevel.java, SiegeResult.java) -------------------------------------------------------------------------------------

TEST(MailEnumsTest, AbyssSiegeLevelIdsAndLookup) {
	using services::mail::getId;
	using services::mail::getLevelById;
	EXPECT_EQ(getId(AbyssSiegeLevel::NONE), 0);
	EXPECT_EQ(getId(AbyssSiegeLevel::HERO_DECORATION), 1);
	EXPECT_EQ(getId(AbyssSiegeLevel::MEDAL), 2);
	EXPECT_EQ(getId(AbyssSiegeLevel::ELITE_SOLDIER), 3);
	EXPECT_EQ(getId(AbyssSiegeLevel::VETERAN_SOLDIER), 4);
	for (int32_t id = 0; id <= 4; id++)
		EXPECT_EQ(getId(getLevelById(id)), id);
	try {
		getLevelById(5);
		ADD_FAILURE() << "an unknown id throws";
	} catch (const commons::utils::IllegalArgumentException& e) {
		EXPECT_STREQ(e.what(), "There is no AbyssSiegeLevel with ID 5");
	}
	EXPECT_THROW(getLevelById(-1), commons::utils::IllegalArgumentException);
}

TEST(MailEnumsTest, SiegeResultIds) {
	using services::mail::getId;
	EXPECT_EQ(getId(SiegeResult::DEFENCE), 0);
	EXPECT_EQ(getId(SiegeResult::OCCUPY), 1);
	EXPECT_EQ(getId(SiegeResult::PROTECT), 2);
	EXPECT_EQ(getId(SiegeResult::DEFENDER), 3);
	EXPECT_EQ(getId(SiegeResult::EMPTY), 4);
	EXPECT_EQ(getId(SiegeResult::FAIL), 5);
}

// ---- without a delivery --------------------------------------------------------------------------------------------------------------------

// MailFormatter.java:49-58: 15 whole days or more before the impoundment no template is read and no mail is sent (16 days ahead minus the
// milliseconds of the call are 15 whole days; 15 days minus a second would be 14, the 1st notice)
TEST_F(MailFormatterTest, MaintenanceMoreThanTwoWeeksAheadSendsNothing) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	model::house::House& home = house(10001);
	MailFormatter::sendHouseMaintenanceMail(home, B_NAME, commons::utils::currentTimeMillis() + 16 * DAY, 100);
	EXPECT_TRUE(partner().getMailbox()->getLetters().empty());
	EXPECT_TRUE(sentB().empty());
}

// MailFormatter.java:79-82: the template is read first; a null house returns before anything is formatted or sent
TEST_F(MailFormatterTest, AnAuctionMailWithoutAHouseIsNotSent) {
	goOnline(partner());
	MailFormatter::sendHouseAuctionMail(nullptr, *b.commonData, AuctionResult::WIN_BID, 1000, 5);
	EXPECT_TRUE(partner().getMailbox()->getLetters().empty());
}

// MailFormatter.java:139: participantDate.toLocalDateTime() on null
TEST_F(MailFormatterTest, AGuildDominionRewardWithoutADateIsJavasNullPointerException) {
	EXPECT_THROW(MailFormatter::sendGuildDominionRewardMail(partner(), 1, std::nullopt, MINOR_LIFE_POTION, 1), runtime::NullPointerException);
	EXPECT_TRUE(partner().getMailbox()->getLetters().empty());
}

// a missing system mail template: Java's NullPointerException at template.getFormattedTitle
TEST_F(MailFormatterTest, AMissingTemplateIsJavasNullPointerException) {
	dataholders::DataManager::SYSTEM_MAIL_TEMPLATES.resetForTests();
	dataholders::DataManager::SYSTEM_MAIL_TEMPLATES.publish(xml::bindString<model::templates::mail::Mails>(context, "<mails/>"));
	EXPECT_THROW(MailFormatter::sendBlackCloudMail(B_NAME, MINOR_LIFE_POTION, 1), runtime::NullPointerException);
}

// ---- delivered letters (the test database) -----------------------------------------------------------------------------------------------

// MailFormatter.java:21-45: title "itemid,count", body "unk1,purchasedate" (seconds), a BLACKCLOUD letter with the item
TEST_F(MailFormatterTest, TheBlackCloudMailCarriesTheItemAndThePurchaseSeconds) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	const int64_t before = commons::utils::currentTimeMillis() / 1000;
	MailFormatter::sendBlackCloudMail(B_NAME, MINOR_LIFE_POTION, 3);
	const int64_t after = commons::utils::currentTimeMillis() / 1000;
	runtime::Ptr<Letter> letter = onlyLetter();
	ASSERT_TRUE(letter);
	EXPECT_EQ(letter->getSenderName(), "$$CASH_ITEM_MAIL");
	EXPECT_EQ(letter->getTitle(), std::to_string(MINOR_LIFE_POTION) + ",3");
	ASSERT_TRUE(letter->getMessage().starts_with("0,"));
	const int64_t seconds = std::stoll(letter->getMessage().substr(2));
	EXPECT_GE(seconds, before);
	EXPECT_LE(seconds, after);
	EXPECT_EQ(letter->getLetterType(), LetterType::BLACKCLOUD);
	ASSERT_TRUE(letter->getAttachedItem());
	EXPECT_EQ(letter->getAttachedItem()->getItemId(), MINOR_LIFE_POTION);
	EXPECT_EQ(letter->getAttachedItem()->getItemCount(), 3);
}

// MailFormatter.java:47-77: 5 days ahead (4 whole days: Duration.toDays truncates) is the 2nd notice; the title has no parameter (formatted without the formatter), the tail gets
// "datetime" (minutes) and "address"; the kinah is attached
TEST_F(MailFormatterTest, TheMaintenanceNoticeDependsOnTheDaysLeft) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	model::house::House& home = house(10001);
	const int64_t impound = commons::utils::currentTimeMillis() + 5 * DAY;
	MailFormatter::sendHouseMaintenanceMail(home, B_NAME, impound, 250);
	runtime::Ptr<Letter> letter = onlyLetter();
	ASSERT_TRUE(letter);
	EXPECT_EQ(letter->getSenderName(), "$$HS_OVERDUE_2ND");
	EXPECT_EQ(letter->getTitle(), "904149");
	EXPECT_EQ(letter->getMessage(), "904150,904151,904152," + std::to_string(impound / 60000) + ",10001");
	EXPECT_EQ(letter->getAttachedKinah(), 250);
	EXPECT_EQ(letter->getLetterType(), LetterType::NORMAL);
}

// a past impoundment is the 3rd notice; 15 days minus a second ahead is 14 whole days, still the 1st notice
TEST_F(MailFormatterTest, AnOverdueImpoundmentIsTheThirdNoticeAndTwoWeeksTheFirst) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	model::house::House& home = house(10002);
	MailFormatter::sendHouseMaintenanceMail(home, B_NAME, commons::utils::currentTimeMillis() - 1000, 0);
	MailFormatter::sendHouseMaintenanceMail(home, B_NAME, commons::utils::currentTimeMillis() + 15 * DAY - 1000, 0);
	std::vector<runtime::Ptr<Letter>> letters = partner().getMailbox()->getLetters();
	ASSERT_EQ(letters.size(), 2u);
	std::vector<std::string> senders{letters[0]->getSenderName(), letters[1]->getSenderName()};
	std::ranges::sort(senders);
	EXPECT_EQ(senders, (std::vector<std::string>{"$$HS_OVERDUE_1ST", "$$HS_OVERDUE_3RD"}));
}

// MailFormatter.java:79-104: the recipient's race picks the template; ELYOS' body repeats "datetime" (seconds)
TEST_F(MailFormatterTest, TheAuctionMailCarriesTheResultAndTheRace) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	model::house::House& home = house(10001);
	MailFormatter::sendHouseAuctionMail(runtime::Ptr<model::house::House>(&home), *b.commonData, AuctionResult::GRACE_SUCCESS, 1'700'000'123'456, 77);
	runtime::Ptr<Letter> letter = onlyLetter();
	ASSERT_TRUE(letter);
	EXPECT_EQ(letter->getSenderName(), "$$HS_AUCTION_MAIL");
	EXPECT_EQ(letter->getTitle(),
		std::to_string(services::mail::getId(AuctionResult::GRACE_SUCCESS)) + "," + std::to_string(model::getRaceId(model::Race::ELYOS)));
	EXPECT_EQ(letter->getMessage(), "1700000123,10001,1700000123");
	EXPECT_EQ(letter->getAttachedKinah(), 77);
}

// MailFormatter.java:106-135
TEST_F(MailFormatterTest, TheAbyssRewardMailCarriesTheLocationResultRankAndAttachments) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	const std::unique_ptr<model::templates::siegelocation::SiegeLocationTemplate> template_ =
		xml::bindString<model::templates::siegelocation::SiegeLocationTemplate>(context, R"(<siege_location id="9001" type="FORTRESS" world="210010000"/>)");
	runtime::Ref<model::siege::SiegeLocation> location = model::siege::SiegeLocation::create(template_.get());
	MailFormatter::sendAbyssRewardMail(*location, *b.commonData, AbyssSiegeLevel::ELITE_SOLDIER, SiegeResult::PROTECT, 5'000'999, MINOR_LIFE_POTION,
		2, 40);
	runtime::Ptr<Letter> letter = onlyLetter();
	ASSERT_TRUE(letter);
	EXPECT_EQ(letter->getSenderName(), "$$ABYSS_REWARD_MAIL");
	EXPECT_EQ(letter->getTitle(), "9001,2," + std::to_string(model::getRaceId(model::Race::ELYOS)));
	EXPECT_EQ(letter->getMessage(), "5000,3");
	EXPECT_EQ(letter->getAttachedKinah(), 40);
	ASSERT_TRUE(letter->getAttachedItem());
	EXPECT_EQ(letter->getAttachedItem()->getItemCount(), 2);
}

// MailFormatter.java:137-163: month and day of the participation in the server's zone, the territory, no legion: ""
TEST_F(MailFormatterTest, TheGuildDominionRewardMailCarriesTheDateAndTerritory) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	// the server's zone (gameserver.timezone; the unit tests load no properties): the Java server makes it the JVM default zone
	const std::chrono::time_zone* previousZone = configs::main::GSConfig::TIME_ZONE_ID.load();
	configs::main::GSConfig::TIME_ZONE_ID.store(std::chrono::locate_zone("Europe/Berlin"));
	struct RestoreZone {
		const std::chrono::time_zone* zone;
		~RestoreZone() { configs::main::GSConfig::TIME_ZONE_ID.store(zone); }
	} restoreZone{previousZone};
	// noon of 2025-03-07 in the server's zone: the local date is March 7 whatever the zone's offset
	const commons::database::Timestamp participant = std::chrono::time_point_cast<std::chrono::milliseconds>(
		utils::time::ServerTime::of(std::chrono::local_time<std::chrono::milliseconds>(std::chrono::local_days{std::chrono::year{2025} / 3 / 7} + std::chrono::hours{12})).get_sys_time());
	MailFormatter::sendGuildDominionRewardMail(partner(), 4, participant, MINOR_LIFE_POTION, 5);
	runtime::Ptr<Letter> letter = onlyLetter();
	ASSERT_TRUE(letter);
	EXPECT_EQ(letter->getSenderName(), "$$GD_REWARD_MAIL");
	EXPECT_EQ(letter->getTitle(), "905976") << "the ELYOS template";
	EXPECT_EQ(letter->getMessage(), "905977,3,7,4,,905978");
	ASSERT_TRUE(letter->getAttachedItem());
	EXPECT_EQ(letter->getAttachedItem()->getItemCount(), 5);
}

// MailFormatter.java:165-169: no template; the sender is the race's npc, the title the client string 901513
TEST_F(MailFormatterTest, TheCustomAbyssDefeatRewardComesFromTheRacesNpc) {
	MAIL_REQUIRE_DATABASE();
	goOnline(partner());
	MailFormatter::sendCustomAbyssDefeatRewardMail(*b.commonData, MINOR_LIFE_POTION, 1);
	runtime::Ptr<Letter> letter = onlyLetter();
	ASSERT_TRUE(letter);
	EXPECT_EQ(letter->getSenderName(), "%NPC:203700") << "Fasimedes for an Elyos";
	EXPECT_EQ(letter->getTitle(), "$901513");
	EXPECT_EQ(letter->getMessage(), "");
}

} // namespace
} // namespace aion::gameserver::economy::test::mail
