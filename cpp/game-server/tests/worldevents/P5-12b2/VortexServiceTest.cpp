// M5i I-10 (P5-12b2): DimensionalVortex's start and stop guards, its generator lookup and location id (DimensionalVortex.java:43-101), and
// VortexService's bookkeeping without an invasion (VortexService.java:60-151): stop of nothing, despawn, the duration, the player queries over
// an invasion the test registers, and the vortex zone of a world without a vortex. Spawning a vortex with its npcs is the gate's (I-11).

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "../../playersvc/PlayerEventsTestSupport.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/model/templates/vortex/VortexTemplate.bind.h"
#include "aion/gameserver/model/templates/vortex/VortexTemplate.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/VortexService.h"
#include "aion/gameserver/services/vortex/DimensionalVortex.h"
#include "aion/gameserver/services/vortex/Invasion.h"
#include "aion/gameserver/world/WorldPosition.h"

namespace aion::gameserver::playerevents::test {
namespace {

using model::gameobjects::player::Player;
using model::vortex::VortexLocation;

/** A DimensionalVortex that counts its invasion hooks and holds the players the test gives it */
class CountingVortex final : public services::vortex::DimensionalVortex {
	AION_MAKE_REF_FRIEND
public:
	explicit CountingVortex(VortexLocation& location) : DimensionalVortex(location) {}

	void startInvasion() override { starts++; }
	void stopInvasion() override { stops++; }
	void addPlayer(Player&, bool) override {}
	void kickPlayer(Player& player, bool isInvader) override { kicked.emplace_back(player.getObjectId(), isInvader); }
	void updateDefenders(Player&) override {}
	void updateInvaders(Player&) override {}
	std::unordered_map<int32_t, runtime::Ptr<Player>> getDefenders() override { return defenders; }
	std::unordered_map<int32_t, runtime::Ptr<Player>> getInvaders() override { return invaders; }

	void lookForTheGenerator() { initRiftGenerator(); }

	int starts = 0;
	int stops = 0;
	std::vector<std::pair<int32_t, bool>> kicked;
	std::unordered_map<int32_t, runtime::Ptr<Player>> defenders, invaders;

protected:
	~CountingVortex() override = default;
};

class VortexServiceTest : public PlayerEventsTest {
protected:
	void SetUp() override {
		PlayerEventsTest::SetUp();
		vortexTemplate = bindXml<model::templates::vortex::VortexTemplate>(
			R"(<vortex_location id="0" defends_race="ELYOS" offence_race="ASMODIANS">)"
			R"(<home_point map="120080000" x="559.4" y="207.8" z="93.5" h="0"/>)"
			R"(<resurrection_point map="210060000" x="951.0" y="2433.0" z="107.0" h="0"/>)"
			R"(<start_point map="210060000" x="951.0" y="2433.0" z="107.0" h="0"/></vortex_location>)");
	}

	void TearDown() override {
		services::VortexService::getInstance().getActiveInvasions().clear();
		vortexTemplate.reset();
		PlayerEventsTest::TearDown();
	}

	std::unique_ptr<model::templates::vortex::VortexTemplate> vortexTemplate;
};

TEST_F(VortexServiceTest, AVortexStartsAndStopsOnce) {
	runtime::Ref<VortexLocation> location = VortexLocation::create(vortexTemplate.get());
	runtime::Ref<CountingVortex> vortex = runtime::makeRef<CountingVortex>(*location);
	EXPECT_EQ(vortex->getVortexLocationId(), 0);
	EXPECT_FALSE(vortex->isFinished());
	vortex->start();
	vortex->start(); // started: returns silently (DimensionalVortex.java:54-56)
	EXPECT_EQ(vortex->starts, 1);
	vortex->stop();
	vortex->stop();
	EXPECT_EQ(vortex->stops, 1);
	EXPECT_TRUE(vortex->isFinished());
}

TEST_F(VortexServiceTest, AVortexWithoutItsGeneratorCannotWatchIt) {
	runtime::Ref<VortexLocation> location = VortexLocation::create(vortexTemplate.get());
	runtime::Ref<CountingVortex> vortex = runtime::makeRef<CountingVortex>(*location);
	try {
		vortex->lookForTheGenerator();
		FAIL() << "expected NullPointerException";
	} catch (const runtime::NullPointerException& e) {
		EXPECT_NE(std::string(e.what()).find("No generator was found in loc:0"), std::string::npos) << e.what();
	}
}

TEST_F(VortexServiceTest, TheServiceKicksAndAnswersForTheRegisteredInvasion) {
	services::VortexService& service = services::VortexService::getInstance();
	runtime::resetUnportedHitsForTests();
	AtomicConfigScope<int32_t> duration(configs::main::CustomConfig::VORTEX_DURATION, 3);
	EXPECT_EQ(service.getDuration(), 3);
	EXPECT_FALSE(service.isInvasionInProgress(0));
	service.stopInvasion(0); // nothing in progress

	runtime::Ref<VortexLocation> location = VortexLocation::create(vortexTemplate.get());
	runtime::Ref<CountingVortex> vortex = runtime::makeRef<CountingVortex>(*location);
	PlayerFixture invader = makePlayer(31, 3100, model::Race::ASMODIANS);
	PlayerFixture defender = makePlayer(32, 3200, model::Race::ELYOS);
	PlayerFixture stranger = makePlayer(33, 3300, model::Race::ELYOS);
	vortex->invaders[31] = invader.player;
	vortex->defenders[32] = defender.player;
	service.getActiveInvasions().put(0, vortex);
	EXPECT_TRUE(service.isInvasionInProgress(0));
	EXPECT_TRUE(service.isInvaderPlayer(*invader.player));
	EXPECT_FALSE(service.isInvaderPlayer(*defender.player));
	service.removeInvaderPlayer(*invader.player);
	service.removeDefenderPlayer(*defender.player);
	service.removeInvaderPlayer(*stranger.player);
	service.removeDefenderPlayer(*stranger.player);
	EXPECT_EQ(vortex->kicked, (std::vector<std::pair<int32_t, bool>>{{31, true}, {32, false}}));

	service.stopInvasion(0);
	EXPECT_FALSE(service.isInvasionInProgress(0));
	EXPECT_EQ(vortex->stops, 1);
	service.stopInvasion(0); // removed: nothing left
	EXPECT_EQ(vortex->stops, 1);
	stranger.player->setPosition(runtime::Ptr<world::WorldPosition>(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, 0)));
	EXPECT_FALSE(service.isInsideVortexZone(*stranger.player)) << "Poeta: a world without a vortex location";
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(VortexServiceTest, DespawnUnsetsTheControllerAndForgetsTheNpcs) {
	runtime::Ref<VortexLocation> location = VortexLocation::create(vortexTemplate.get());
	services::VortexService::getInstance().despawn(*location); // nothing spawned
	EXPECT_FALSE(location->getVortexController());
	EXPECT_TRUE(location->getSpawned().isEmpty());
}


TEST_F(VortexServiceTest, TheFirstInvaderJoinsAloneAndOnlyOnce) {
	// Invasion.java:62-88, 152-158: with no participant the invader is put without an alliance; an invader already in is left alone. The
	// second participant's alliance needs the team fixture (PlayerAllianceService.createAlliance): the gate's vortex (I-11) covers it
	runtime::Ref<VortexLocation> location = VortexLocation::create(vortexTemplate.get());
	runtime::Ref<services::vortex::Invasion> invasion = services::vortex::Invasion::create(*location);
	PlayerFixture first = makePlayer(41, 4100, model::Race::ASMODIANS);
	invasion->updateInvaders(*first.player);
	ASSERT_EQ(invasion->getInvaders().size(), 1u);
	EXPECT_EQ(invasion->getInvaders().at(41), first.player);
	EXPECT_FALSE(first.player->isInAlliance()) << "one invader: no alliance yet";
	invasion->updateInvaders(*first.player); // already an invader: nothing
	EXPECT_EQ(invasion->getInvaders().size(), 1u);
	EXPECT_TRUE(invasion->getDefenders().empty());
	invasion->addPlayer(*first.player, false); // a defender list of its own
	EXPECT_EQ(invasion->getDefenders().size(), 1u);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
