// M5i I-1 (m5i-plan.md 15.3): //siege (data/handlers/admincommands/SiegeCommand.java) on a real Player with a real AionConnection
// (CommandTestSupport.h): the syntax info, the location argument, the location list, and the start/stop refusals and the stop of an active
// siege. Starting a FortressSiege, a capture (spawns, siege_locations) and an assault are driven by the gate (I-6, gs.scenario.m5i).

#include "CommandTestSupport.h"

#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/configs/administration/CommandsConfig.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/handlers/admincommands/SiegeCommand.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/siege/Siege.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

using model::templates::siegelocation::SiegeLocationTemplate;

/** A Siege that records its stop */
class StoppableSiege final : public services::siege::Siege {
	AION_MAKE_REF_FRIEND
public:
	explicit StoppableSiege(model::siege::SiegeLocation& location) : Siege(location) {}

	void onSiegeStart() override {}
	void onSiegeFinish() override { finishes++; }
	bool isEndless() override { return false; }
	void onAbyssPointsAdded(Player&, int32_t) override {}

	int32_t finishes = 0;

protected:
	~StoppableSiege() override = default;
};

class SiegeCommandTest : public CommandTest {
protected:
	void SetUp() override {
		CommandTest::SetUp();
		std::map<std::string, int8_t, std::less<>> levels(*configs::administration::CommandsConfig::ACCESS_LEVELS.get());
		levels["siege"] = 9; // commands.properties: siege = 9
		configs::administration::CommandsConfig::ACCESS_LEVELS.set(levels);
		savedAutoAssault = configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.load();
		configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.store(false);
	}

	void TearDown() override {
		services::SiegeService& service = services::SiegeService::getInstance();
		for (int32_t id : added) {
			service.stopSiege(id); // removes a StoppableSiege; a debug line otherwise
			service.getSiegeLocations().remove(id);
			service.getFortresses().remove(id);
			service.getArtifacts().remove(id);
		}
		configs::main::SiegeConfig::BALAUR_AUTO_ASSAULT.store(savedAutoAssault);
		CommandTest::TearDown();
	}

	Player& gm() { return connected(730100, "Warden", 9); }

	const SiegeLocationTemplate& bindTemplate(const std::string& row) {
		templates.push_back(xml::bindString<SiegeLocationTemplate>(context, row));
		return *templates.back();
	}

	runtime::Ref<model::siege::FortressLocation> addFortress(int32_t id) {
		const SiegeLocationTemplate& t =
			bindTemplate(R"(<siege_location id=")" + std::to_string(id) + R"(" type="FORTRESS" world="400010000" siege_duration="3600"/>)");
		runtime::Ref<model::siege::FortressLocation> fortress = model::siege::FortressLocation::create(&t);
		services::SiegeService::getInstance().getFortresses().put(id, fortress);
		services::SiegeService::getInstance().getSiegeLocations().put(id, fortress);
		added.push_back(id);
		return fortress;
	}

	void addArtifact(int32_t id) {
		const SiegeLocationTemplate& t = bindTemplate(R"(<siege_location id=")" + std::to_string(id) + R"(" type="ARTIFACT" world="400010000"/>)");
		runtime::Ref<model::siege::ArtifactLocation> artifact = model::siege::ArtifactLocation::create(&t);
		services::SiegeService::getInstance().getArtifacts().put(id, artifact);
		services::SiegeService::getInstance().getSiegeLocations().put(id, artifact);
		added.push_back(id);
	}

	runtime::Ref<StoppableSiege> activate(model::siege::SiegeLocation& location) {
		runtime::Ref<StoppableSiege> siege = runtime::makeRef<StoppableSiege>(location);
		siege->startSiege();
		// the service's map is private; a siege enters it through startSiege only, which builds a FortressSiege - stand in for that put
		siegeMap().put(location.getLocationId(), siege);
		return siege;
	}

	static runtime::ConcurrentHashMap<int32_t, runtime::Ref<services::siege::Siege>>& siegeMap();

	xml::LoadContext context;
	std::vector<std::unique_ptr<SiegeLocationTemplate>> templates;
	std::vector<int32_t> added;
	bool savedAutoAssault = false;
};

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing

namespace aion::gameserver::services {

/** The service's active sieges (SiegeService.h befriends this struct for tests/siege/SiegeCoreTest.cpp too) */
struct SiegeServiceTestAccess {
	static runtime::ConcurrentHashMap<int32_t, runtime::Ref<siege::Siege>>& activeSieges(SiegeService& service) { return service.activeSieges; }
};

} // namespace aion::gameserver::services

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

runtime::ConcurrentHashMap<int32_t, runtime::Ref<services::siege::Siege>>& SiegeCommandTest::siegeMap() {
	return services::SiegeServiceTestAccess::activeSieges(services::SiegeService::getInstance());
}

TEST_F(SiegeCommandTest, WithoutArgumentsSendsTheSyntax) {
	Player& admin = gm();
	handlers::admincommands::SiegeCommand siege;
	EXPECT_TRUE(siege.process(admin, args({})));
	EXPECT_EQ(client()->sentBytes(), info(siege.getSyntaxInfo()));
	client()->clearSent();
	EXPECT_TRUE(siege.process(admin, args({"lift"})));
	EXPECT_EQ(client()->sentBytes(), info(siege.getSyntaxInfo())) << "an unknown arm";
}

TEST_F(SiegeCommandTest, ALocationIdIsRequiredAndMustExist) {
	Player& admin = gm();
	handlers::admincommands::SiegeCommand siege;
	EXPECT_TRUE(siege.process(admin, args({"start"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid location ID.")) << "params.length < 2";
	client()->clearSent();
	EXPECT_TRUE(siege.process(admin, args({"stop", "4242"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid location ID.")) << "no such location";
	client()->clearSent();
	EXPECT_TRUE(siege.process(admin, args({"capture", "fort"})));
	EXPECT_EQ(client()->sentBytes(), info("Invalid number: \"fort\"")) << "Integer.parseInt's NumberFormatException (ChatCommand.run)";
}

TEST_F(SiegeCommandTest, StartAndStopAnswerTheSiegeState) {
	Player& admin = gm();
	runtime::Ref<model::siege::FortressLocation> fortress = addFortress(1011);
	handlers::admincommands::SiegeCommand siege;
	EXPECT_TRUE(siege.process(admin, args({"stop", "1011"})));
	EXPECT_EQ(client()->sentBytes(), info("This location is not under siege."));

	runtime::Ref<StoppableSiege> active = activate(*fortress);
	client()->clearSent();
	EXPECT_TRUE(siege.process(admin, args({"start", "1011"})));
	EXPECT_EQ(client()->sentBytes(), info("This location is already under siege."));

	client()->clearSent();
	EXPECT_TRUE(siege.process(admin, args({"STOP", "1011"})));
	EXPECT_EQ(client()->sentBytes(), info("Stopped siege at FORTRESS 1011")) << "no name id: the type and the id";
	EXPECT_EQ(active->finishes, 1);
	EXPECT_FALSE(services::SiegeService::getInstance().isSiegeInProgress(1011));
}

TEST_F(SiegeCommandTest, LocationsAreListedByIdInGroupsOfTenWithTheirArtifactsIndented) {
	Player& admin = gm();
	addFortress(1021);
	addArtifact(1012);
	runtime::Ref<model::siege::FortressLocation> f1011 = addFortress(1011);
	addArtifact(1001);
	activate(*f1011);
	handlers::admincommands::SiegeCommand siege;
	const int32_t secondsBefore = services::SiegeService::getInstance().getRemainingSiegeTimeInSeconds(1011);
	EXPECT_TRUE(siege.process(admin, args({"locations"})));
	const int32_t secondsAfter = services::SiegeService::getInstance().getRemainingSiegeTimeInSeconds(1011);
	const auto remaining = [](int32_t secondsLeft) {
		return " (" + std::to_string(secondsLeft / 60) + "m " + std::to_string(secondsLeft % 60) + "s until siege ends)";
	};
	// (id - 1) / 10: 1001 -> 100; 1011, 1012 -> 101; 1021 -> 102. An artifact is indented unless it opens its group.
	const std::vector<std::vector<uint8_t>> sent = client()->sentBytes();
	ASSERT_EQ(sent.size(), 4u);
	EXPECT_EQ(sent[0], info("ARTIFACT (ID: 1001) in 400010000 belongs to BALAUR")[0]);
	// an active siege of 3600 s that just started (the wall clock may pass a second boundary meanwhile)
	EXPECT_TRUE(sent[1] == info("FORTRESS (ID: 1011) in 400010000 belongs to BALAUR" + remaining(secondsBefore))[0] ||
		sent[1] == info("FORTRESS (ID: 1011) in 400010000 belongs to BALAUR" + remaining(secondsAfter))[0]);
	EXPECT_GE(secondsBefore, 3599);
	EXPECT_EQ(sent[2], info("\tARTIFACT (ID: 1012) in 400010000 belongs to BALAUR")[0]);
	EXPECT_EQ(sent[3], info("FORTRESS (ID: 1021) in 400010000 belongs to BALAUR")[0]);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
