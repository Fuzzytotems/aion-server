// M5i I-6 (P5-12a lease on P4-16's SM_INFLUENCE_RATIO and SM_FORTRESS_STATUS): the bodies of both packets, byte for byte, against the
// expectations written field by field from SM_INFLUENCE_RATIO.java:14-29 and SM_FORTRESS_STATUS.java:14-36 - the same layout the scenario
// decoders (decoders/SiegeDecoders decodeInfluenceRatio/decodeFortressStatus) read and SiegeDecodersTest checks.
//
// Sieges stay disabled (getSecondsUntilNextFortressState() is 0); the locations are put into the service's maps by hand and
// Influence.recalculateInfluence() sums them like Influence.java:44-65.

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "../playersvc/PlayerEventsTestSupport.h"
#include "../sm_ak/SmAkTestSupport.h"
#include "aion/gameserver/configs/main/SiegeConfig.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/Influence.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_FORTRESS_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_INFLUENCE_RATIO.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/SiegeService.h"

namespace aion::gameserver::playerevents::test {
namespace {

using model::siege::SiegeRace;
using network::aion::serverpackets::SM_FORTRESS_STATUS;
using network::aion::serverpackets::SM_INFLUENCE_RATIO;
using network::aion::serverpackets::test::Bytes;
using network::aion::serverpackets::test::dataOf;

class SiegeStatusPacketsTest : public PlayerEventsTest {
protected:
	using SiegeLocationTemplate = model::templates::siegelocation::SiegeLocationTemplate;

	const SiegeLocationTemplate* bind(int32_t id, int32_t world, int32_t influence) {
		templates.push_back(xml::bindString<SiegeLocationTemplate>(context, "<siege_location id=\"" + std::to_string(id) +
			"\" type=\"FORTRESS\" world=\"" + std::to_string(world) + "\" influence=\"" + std::to_string(influence) + "\"/>"));
		return templates.back().get();
	}

	/** a fortress in both getSiegeLocations() (Influence's source) and getFortresses() (SM_FORTRESS_STATUS's list) */
	void addFortress(int32_t id, int32_t world, int32_t influence, SiegeRace race, int32_t nextState) {
		runtime::Ref<model::siege::FortressLocation> fortress = model::siege::FortressLocation::create(bind(id, world, influence));
		fortress->setRace(race);
		fortress->setNextState(nextState);
		services::SiegeService::getInstance().getSiegeLocations().put(id, fortress);
		services::SiegeService::getInstance().getFortresses().put(id, fortress);
	}

	void TearDown() override {
		services::SiegeService& service = services::SiegeService::getInstance();
		service.getSiegeLocations().clear();
		service.getFortresses().clear();
		model::siege::Influence::getInstance().recalculateInfluence(); // back to no influence for the other tests of the executable
		PlayerEventsTest::TearDown();
	}

	xml::LoadContext context;
	std::vector<std::unique_ptr<SiegeLocationTemplate>> templates;
};

TEST_F(SiegeStatusPacketsTest, NoInfluenceWritesZeroRatesAndNoWorlds) {
	AtomicConfigScope<bool> disabled(configs::main::SiegeConfig::SIEGE_ENABLED, false);
	runtime::resetUnportedHitsForTests();
	model::siege::Influence::getInstance().recalculateInfluence();
	EXPECT_EQ(dataOf(SM_INFLUENCE_RATIO()), Bytes().D(0).F(0.0f).F(0.0f).F(0.0f).H(0).data);
	EXPECT_EQ(dataOf(SM_FORTRESS_STATUS()), Bytes().C(1).D(0).F(0.0f).F(0.0f).F(0.0f).H(0).H(0).data);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

TEST_F(SiegeStatusPacketsTest, InfluenceAndFortressesAreWrittenInJavasOrder) {
	AtomicConfigScope<bool> disabled(configs::main::SiegeConfig::SIEGE_ENABLED, false);
	runtime::resetUnportedHitsForTests();
	ASSERT_EQ(services::SiegeService::getInstance().getSiegeLocations().size(), 0) << "sieges are off";
	// insertion order: Reshanta first (its first fortress comes first), then 210050000 - Influence's LinkedHashMap by world
	addFortress(1011, 400010000, 10, SiegeRace::ELYOS, 1);
	addFortress(1221, 210050000, 20, SiegeRace::ASMODIANS, 0);
	addFortress(9001, 400010000, 5, SiegeRace::BALAUR, 1);
	addFortress(9002, 400010000, 0, SiegeRace::ASMODIANS, 0); // influence 0: not counted (Influence.java:47-48), but still a fortress
	model::siege::Influence::getInstance().recalculateInfluence();

	// global: elyos 10, asmodians 20, balaur 5 -> the rates are getInfluence(race) / 100f
	const std::vector<uint8_t> worlds = Bytes()
		.H(2)
		.D(400010000).F(10.0f).F(0.0f).F(5.0f)
		.D(210050000).F(0.0f).F(20.0f).F(0.0f)
		.data;
	Bytes influence;
	influence.D(0).F(10 / 100.0f).F(20 / 100.0f).F(5 / 100.0f).B(worlds);
	EXPECT_EQ(dataOf(SM_INFLUENCE_RATIO()), influence.data);

	Bytes status;
	status.C(1).D(0).F(10 / 100.0f).F(20 / 100.0f).F(5 / 100.0f).B(worlds);
	status.H(4).D(1011).C(1).D(1221).C(0).D(9001).C(1).D(9002).C(0);
	EXPECT_EQ(dataOf(SM_FORTRESS_STATUS()), status.data);
	EXPECT_EQ(runtime::unportedHitCount(), 0u);
}

} // namespace
} // namespace aion::gameserver::playerevents::test
