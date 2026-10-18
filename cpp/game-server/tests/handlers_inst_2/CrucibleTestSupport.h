#pragma once

// The static data the crucible tests add to the instance fixture: the two Crucible Challenge items the handlers grant or check, the item
// clean-up holder ItemService.addItem asks, and Poppy's walker route (the bonus stage 2 spawn sets it).

#include <memory>

#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.bind.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/dataholders/WalkerData.bind.h"
#include "aion/gameserver/dataholders/WalkerData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"

namespace aion::gameserver::handlers::instance::test {

/** item_templates.xml:896506-896508 and :896518-896520 (the insignia, the worthiness ticket), :875778-875780 (Poppy's snacks),
 * :908897-908903 (the hidden boss's bundle); :896488-896490 (Empyrean Crucible's ticket), :831380-831385 and :831386-831391 (its potions); :834025-834027
 * (the Illuminary Obelisk's Crystalline Idium Piece) */
inline constexpr const char* CRUCIBLE_ITEMS_XML = R"x(<item_templates>
	<item_template id="186000130" name="Crucible Insignia" level="60" cName="coin_arena_01" mask="12414" max_stack_count="50000" item_group="COINS" quality="RARE" price="1" desc="773779">
		<inventory id="1"/>
	</item_template>
	<item_template id="186000134" name="Worthiness Ticket" level="50" cName="material_idarena_solo_01" mask="12352" max_stack_count="100" quality="COMMON" price="5" desc="780236">
		<uselimits ownership_worlds="300320000"/>
	</item_template>
	<item_template id="182006429" name="Poppy Snack" level="1" cName="junk_feed_bbobi_pet_01" mask="12360" max_stack_count="1000" quality="COMMON" price="1" desc="780269"/>
	<item_template id="182006430" name="Tasty Poppy Snack" level="1" cName="junk_feed_bbobi_pet_02" mask="12360" max_stack_count="1000" quality="COMMON" price="1" desc="780270"/>
	<item_template id="182006431" name="Nutritious Poppy Snack" level="1" cName="junk_feed_bbobi_pet_03" mask="12360" max_stack_count="1000" quality="COMMON" price="1" desc="780271"/>
	<item_template id="188051349" name="Superior Manastone Bundle" level="50" cName="wrap_r_matter_option_60a" casting_delay="1500" mask="12360" max_stack_count="1000" quality="RARE" price="1" desc="775442" activate_target="STANDALONE" activate_count="1">
		<actions>
			<decompose/>
		</actions>
		<acquisition type="REWARD" item="186000130" count="700"/>
		<uselimits usedelayid="85"/>
	</item_template>
	<item_template id="186000124" name="Worthiness Ticket" level="50" cName="material_idarena_01" mask="12378" max_stack_count="100" quality="COMMON" price="5" desc="770151">
		<uselimits ownership_worlds="300300000"/>
	</item_template>
	<item_template id="162000108" name="Spirit Rune" level="50" cName="potion_arena_hp_mp_50a" mask="12414" max_stack_count="1000" quality="LEGEND" price="100" desc="778801" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9922"/>
		</actions>
		<uselimits usedelay="1800000" usedelayid="14"/>
	</item_template>
	<item_template id="162000109" name="Soul Crystal" level="50" cName="potion_arena_hp_mp_60a" mask="12414" max_stack_count="1000" quality="LEGEND" price="100" desc="778803" activate_target="STANDALONE" activate_count="1">
		<actions>
			<skilluse level="1" skillid="9923"/>
		</actions>
		<uselimits usedelay="1800000" usedelayid="14"/>
	</item_template>
	<item_template id="164000289" name="Crystalline Idium Piece" level="1" cName="idf5_u3_activate01" mask="12352" max_stack_count="100" quality="RARE" price="5" desc="829362" temp_exchange_time="60">
		<uselimits ownership_worlds="301230000 301370000"/>
	</item_template>
</item_templates>)x";

/** custom_npc_walker.xml:40-56, Poppy's route */
inline constexpr const char* CRUCIBLE_WALKERS_XML = R"x(<npc_walker>
	<walker_template route_id="3003200001">
		<routestep x="1780.5665" y="307.40463" z="469.25"/>
		<routestep x="1782.2998" y="303.006" z="469.25"/>
		<routestep x="1790.9388" y="303.17572" z="469.25"/>
		<routestep x="1791.3456" y="312.68945" z="469.25"/>
		<routestep x="1779.4882" y="315.517" z="469.25"/>
		<routestep x="1776.9799" y="302.363" z="469.25"/>
		<routestep x="1791.8619" y="299.96545" z="469.25"/>
		<routestep x="1794.2048" y="314.88068" z="469.25"/>
		<routestep x="1778.9758" y="317.75433" z="469.25"/>
		<routestep x="1773.8234" y="303.15656" z="469.25"/>
		<routestep x="1786.6578" y="291.93002" z="469.25"/>
		<routestep x="1798.6467" y="310.38916" z="469.25"/>
		<routestep x="1784.9834" y="320.57855" z="469.25"/>
		<routestep x="1772.524" y="308.9545" z="469.25"/>
		<routestep x="1785.1372" y="294.636" z="469.25"/>
	</walker_template>
</npc_walker>)x";

inline void publishCrucibleData(xml::LoadContext& context) {
	dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(context, CRUCIBLE_ITEMS_XML));
	dataholders::DataManager::ITEM_CLEAN_UP.publish(std::make_unique<dataholders::ItemRestrictionCleanupData>());
	dataholders::DataManager::WALKER_DATA.publish(xml::bindString<dataholders::WalkerData>(context, CRUCIBLE_WALKERS_XML));
}

inline void resetCrucibleData() {
	dataholders::DataManager::WALKER_DATA.resetForTests();
	dataholders::DataManager::ITEM_CLEAN_UP.resetForTests();
	dataholders::DataManager::ITEM_DATA.resetForTests();
}

} // namespace aion::gameserver::handlers::instance::test
