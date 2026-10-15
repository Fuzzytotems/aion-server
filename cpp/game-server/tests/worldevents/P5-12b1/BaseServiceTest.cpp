// P5-12b1 bases (M5i I-8): BaseService over a small base_locations table - one location class per type, one base class per type at start,
// a base that fails to spawn staying active (Java catches the exception in start), the guards of start/stop/capture, and the BaseOccupier
// companion. No spawn data is published, so every base start ends in spawnBySpawnHandler's NullPointerException, logged by BaseService.start.
//
// Expectations are derived by hand from BaseService.java, Base.java and the location classes.

#include <gtest/gtest.h>

#include <string>

#include "../../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/dataholders/BaseData.bind.h"
#include "aion/gameserver/dataholders/BaseData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/model/base/Base.h"
#include "aion/gameserver/model/base/BaseException.h"
#include "aion/gameserver/model/base/BaseLocation.h"
#include "aion/gameserver/model/base/BaseOccupier.h"
#include "aion/gameserver/model/base/BaseOccupierInfo.h"
#include "aion/gameserver/model/base/BaseType.h"
#include "aion/gameserver/model/base/CasualBase.h"
#include "aion/gameserver/model/base/PanesterraArtifact.h"
#include "aion/gameserver/model/base/PanesterraBase.h"
#include "aion/gameserver/model/base/PanesterraBaseLocation.h"
#include "aion/gameserver/model/base/PanesterraFactionCamp.h"
#include "aion/gameserver/model/base/SiegeBase.h"
#include "aion/gameserver/model/base/SiegeBaseLocation.h"
#include "aion/gameserver/model/base/StainedBase.h"
#include "aion/gameserver/model/base/StainedBaseLocation.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/BaseService.h"

namespace aion::gameserver::playerevents::test {
namespace {

using model::base::BaseOccupier;
using model::base::BaseType;
using services::panesterra::ahserion::PanesterraFaction;

constexpr const char* BASE_LOCATIONS_XML = R"(<base_locations>
	<base_location id="9101" type="CASUAL" world="210020000" />
	<base_location id="9102" type="SIEGE" world="400010000" default_occupier="ELYOS" />
	<base_location id="9103" type="STAINED" color="GREEN" world="600100000" />
	<base_location id="9104" type="PANESTERRA" world="400020000" />
	<base_location id="9105" type="PANESTERRA_ARTIFACT" world="400020000" />
	<base_location id="9106" type="PANESTERRA_FACTION_CAMP" world="400020000" default_occupier="IVY_TEMPLE" />
</base_locations>)";

class BaseServiceTest : public PlayerEventsTest {
protected:
	void SetUp() override {
		PlayerEventsTest::SetUp();
		// BaseService reads BASE_DATA once, in its constructor: published once for the whole executable
		static const bool published = [] {
			static xml::LoadContext context;
			dataholders::DataManager::BASE_DATA.publish(xml::bindString<dataholders::BaseData>(context, BASE_LOCATIONS_XML));
			return true;
		}();
		static_cast<void>(published);
	}
};

TEST_F(BaseServiceTest, EveryTemplateGetsTheLocationClassOfItsType) {
	runtime::resetPartialHitsForTests();
	services::BaseService& service = services::BaseService::getInstance();
	EXPECT_EQ(service.getBaseLocations().size(), 6u);
	EXPECT_FALSE(service.getBaseLocation(1));

	runtime::Ptr<model::base::BaseLocation> casual = service.getBaseLocation(9101);
	ASSERT_TRUE(casual);
	EXPECT_EQ(casual->getType(), BaseType::CASUAL);
	EXPECT_EQ(casual->getOccupier(), BaseOccupier::BALAUR) << "BaseTemplate's default occupier";
	EXPECT_EQ(casual->getWorldId(), 210020000);
	EXPECT_FALSE(runtime::as<model::base::SiegeBaseLocation>(casual));
	EXPECT_TRUE(runtime::as<model::base::SiegeBaseLocation>(service.getBaseLocation(9102)));
	EXPECT_EQ(service.getBaseLocation(9102)->getOccupier(), BaseOccupier::ELYOS);
	runtime::Ptr<model::base::StainedBaseLocation> stained = runtime::as<model::base::StainedBaseLocation>(service.getBaseLocation(9103));
	ASSERT_TRUE(stained);
	EXPECT_EQ(stained->getColor(), model::base::BaseColorType::GREEN);
	for (int32_t id : {9104, 9105, 9106})
		EXPECT_TRUE(runtime::as<model::base::PanesterraBaseLocation>(service.getBaseLocation(id))) << id;
	EXPECT_EQ(service.getBaseLocation(9104)->getOccupier(), BaseOccupier::BALAUR);
	EXPECT_EQ(service.getBaseLocation(9106)->getOccupier(), BaseOccupier::PEACE) << "a faction camp starts at peace";
	EXPECT_EQ(service.getBaseLocation(9106)->getTemplate()->getDefaultOccupier(), BaseOccupier::IVY_TEMPLE);
}

TEST_F(BaseServiceTest, AStartedBaseIsOfItsTypesClassAndStaysActiveWhenItCannotSpawn) {
	services::BaseService& service = services::BaseService::getInstance();
	runtime::resetUnportedHitsForTests();
	const struct {
		int32_t id;
		bool (*isRightClass)(runtime::Ptr<model::base::Base>);
	} cases[] = {
		{9101, [](runtime::Ptr<model::base::Base> b) { return static_cast<bool>(runtime::as<model::base::CasualBase>(b)); }},
		{9102, [](runtime::Ptr<model::base::Base> b) { return static_cast<bool>(runtime::as<model::base::SiegeBase>(b)); }},
		{9103, [](runtime::Ptr<model::base::Base> b) { return static_cast<bool>(runtime::as<model::base::StainedBase>(b)); }},
		{9104, [](runtime::Ptr<model::base::Base> b) {
			return runtime::as<model::base::PanesterraBase>(b) && !runtime::as<model::base::PanesterraArtifact>(b) && !runtime::as<model::base::PanesterraFactionCamp>(b);
		}},
		{9105, [](runtime::Ptr<model::base::Base> b) { return static_cast<bool>(runtime::as<model::base::PanesterraArtifact>(b)); }},
		{9106, [](runtime::Ptr<model::base::Base> b) { return static_cast<bool>(runtime::as<model::base::PanesterraFactionCamp>(b)); }},
	};
	for (const auto& c : cases) {
		EXPECT_FALSE(service.isActive(c.id));
		service.start(c.id); // no base spawns: spawnBySpawnHandler(FLAG) throws, BaseService.start logs "Base could not be started!"
		ASSERT_TRUE(service.isActive(c.id)) << c.id << ": put into activeBases before base.start(), as in Java";
		runtime::Ptr<model::base::Base> base = service.getActiveBase(c.id);
		EXPECT_TRUE(c.isRightClass(base)) << c.id;
		EXPECT_EQ(base->getId(), c.id);
		EXPECT_TRUE(base->isStarted());
		EXPECT_FALSE(base->isStopped());
		EXPECT_FALSE(base->isUnderAssault());
		service.start(c.id); // already active: nothing happens
		EXPECT_EQ(service.getActiveBase(c.id), base);
		try {
			base->start();
			FAIL() << "expected BaseException";
		} catch (const model::base::BaseException& e) {
			EXPECT_EQ(std::string(e.what()), "Attempt to start Base twice! ID:" + std::to_string(c.id));
		}
	}
	EXPECT_EQ(runtime::unportedHitCount(), 0u);

	// the boss killer decides the occupier (Base.getOccupier(Creature)); the faction camp is always eliminated to PEACE
	EXPECT_EQ(service.getActiveBase(9101)->getOccupier(nullptr), BaseOccupier::BALAUR) << "no killer: the default occupier";
	EXPECT_EQ(service.getActiveBase(9102)->getOccupier(nullptr), BaseOccupier::ELYOS);
	EXPECT_EQ(service.getActiveBase(9104)->getOccupier(nullptr), BaseOccupier::BALAUR);
	EXPECT_EQ(service.getActiveBase(9106)->getOccupier(nullptr), BaseOccupier::PEACE);
	PlayerFixture f = makePlayer(91, 9100);
	EXPECT_EQ(service.getActiveBase(9101)->getOccupier(runtime::Ptr<model::gameobjects::Creature>(f.player)), BaseOccupier::ELYOS);
	EXPECT_EQ(service.getActiveBase(9104)->getOccupier(runtime::Ptr<model::gameobjects::Creature>(f.player)), BaseOccupier::BALAUR)
		<< "a player without a Panesterra faction: the default occupier";
}

TEST_F(BaseServiceTest, CaptureAndStopOfAnInactiveBaseDoNothing) {
	services::BaseService& service = services::BaseService::getInstance();
	ASSERT_FALSE(service.isActive(1));
	service.stop(1);
	service.capture(1, BaseOccupier::ELYOS);
	EXPECT_FALSE(service.isActive(1));
}

TEST(BaseOccupierInfoTest, FactionsAndRacesMapToOccupiers) {
	EXPECT_EQ(model::base::getPanesterraFaction(BaseOccupier::ELYOS), std::nullopt);
	EXPECT_EQ(model::base::getPanesterraFaction(BaseOccupier::ASMODIANS), std::nullopt);
	EXPECT_EQ(model::base::getPanesterraFaction(BaseOccupier::BALAUR), PanesterraFaction::BALAUR);
	EXPECT_EQ(model::base::getPanesterraFaction(BaseOccupier::IVY_TEMPLE), PanesterraFaction::IVY_TEMPLE);
	EXPECT_EQ(model::base::getPanesterraFaction(BaseOccupier::PEACE), PanesterraFaction::PEACE);
	EXPECT_EQ(model::base::findBy(PanesterraFaction::ASPIDA), BaseOccupier::ASPIDA);
	EXPECT_EQ(model::base::findBy(model::Race::ELYOS), BaseOccupier::ELYOS);
	EXPECT_EQ(model::base::findBy(model::Race::ASMODIANS), BaseOccupier::ASMODIANS);
	EXPECT_EQ(model::base::findBy(model::Race::NPC), BaseOccupier::BALAUR);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
