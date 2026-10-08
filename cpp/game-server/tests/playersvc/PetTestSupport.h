#pragma once

// The toy pet static data of the pet tests (m5j-plan.md §18.3 stage 2 CP1): a pet_feed document whose two flavours have the full counts 10
// and 100, and a pets document with three pets of the real pets.xml (900001 looting and eating, 900002 eating with a warehouse, 900010 a
// merchant) whose FOOD function points at the test flavour 9. The feed data is published once per process: PetFeedCalculator's static
// initializer reads it on first use and keeps what it read, as Java's does.

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PetData.bind.h"
#include "aion/gameserver/dataholders/PetData.h"
#include "aion/gameserver/dataholders/PetFeedData.bind.h"
#include "aion/gameserver/dataholders/PetFeedData.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"

namespace aion::gameserver::testing::pets {

inline constexpr const char* FEED_XML = R"XML(<pet_feed>
	<flavour id="7" full_count="100" cd="10">
		<food group="FLUIDS">
			<result item="188050758"/><result item="188050759"/><result item="188050760"/><result item="188050761"/><result item="188050762"/>
		</food>
	</flavour>
	<flavour id="9" full_count="10" cd="10">
		<food group="THORNS">
			<result item="188050988"/><result item="188050989"/><result item="188050990"/><result item="188050991"/><result item="188050992"/>
		</food>
	</flavour>
</pet_feed>)XML";

inline constexpr const char* PETS_XML = R"XML(<pets>
	<pet id="900001" name="siberian wild tiger (companion)" nameid="1600021" condition_reward="188051378">
		<petfunction id="1" type="LOOT"/>
		<petfunction id="9" type="FOOD"/>
		<petstats reaction="brave" run_speed="6.0" walk_speed="1.132" height="1.0" altitude="1.5"/>
	</pet>
	<pet id="900002" name="siberian wild tiger (signal)" nameid="1600023" condition_reward="188051378">
		<petfunction id="9" type="FOOD"/>
		<petfunction id="1" type="WAREHOUSE" slots="6"/>
		<petstats reaction="brave" run_speed="6.0" walk_speed="1.132" height="1.2"/>
	</pet>
	<pet id="900010" name="merchant test pet" nameid="1600041" condition_reward="0">
		<petfunction id="1" type="MERCHANT" rate_price="20"/>
		<petstats reaction="brave" run_speed="6.0" walk_speed="1.132" height="1.0"/>
	</pet>
</pets>)XML";

/** PET_FEED_DATA, once per process */
inline void publishFeedDataOnce() {
	static xml::LoadContext context;
	if (!dataholders::DataManager::PET_FEED_DATA)
		dataholders::DataManager::PET_FEED_DATA.publish(xml::bindString<dataholders::PetFeedData>(context, FEED_XML));
}

/** PET_DATA for one test; resetPetData() at its end */
inline void publishPetData() {
	static xml::LoadContext context;
	publishFeedDataOnce();
	dataholders::DataManager::PET_DATA.publish(xml::bindString<dataholders::PetData>(context, PETS_XML));
}

inline void resetPetData() {
	dataholders::DataManager::PET_DATA.resetForTests();
}

} // namespace aion::gameserver::testing::pets
