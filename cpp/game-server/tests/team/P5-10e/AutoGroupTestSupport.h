#pragma once

// Shared fixture of the matchmaking tests (P5-10e, m5j-plan.md §18.5 CP1/CP2): TeamTestSupport.h's players plus the AUTO_GROUP rows of every
// AutoGroupType mask (auto_group.xml's for the five the cases read, a minimal level-1 row for the others) and test INSTANCE_COOLTIME_DATA rows.

#include "../P5-10b/TeamTestSupport.h"
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/AutoGroupData.bind.h"
#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.bind.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/autogroup/AGPlayer.h"
#include "aion/gameserver/model/autogroup/AGQuestion.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/autogroup/AutoHarmonyInstance.h"
#include "aion/gameserver/model/autogroup/AutoPvPFFAInstance.h"
#include "aion/gameserver/model/autogroup/AutoPvpInstance.h"
#include "aion/gameserver/model/autogroup/EntryRequestTypeInfo.h"
#include "aion/gameserver/model/autogroup/LookingForParty.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldown.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/network/aion/serverpackets/SM_AUTO_GROUP.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/autogroup/AutoGroupUtility.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::network::aion::clientpackets::testing::team {


namespace ag = model::autogroup;

using ag::AGQuestion;
using ag::AutoGroupType;
using ag::EntryRequestType;
using ag::LookingForParty;
using model::Race;
using services::autogroup::AutoGroupUtility;
using serverpackets::SM_AUTO_GROUP;

/**
 * auto_group.xml's rows of the masks the cases read (Baranath Dredgion 1, Chaos 1 21, Discipline 1 24, Harmony 1 33, Kamar 107) and a minimal
 * row (level 1, every entry allowed, no npcs, world 390000000 + mask) for every other mask of AutoGroupType's 33 constants: its lookups walk every constant and
 * dereference each template (Java's NullPointerException without one)
 */
inline std::string autoGroupXml(bool withIdgelDome = true) {
	std::string xml = R"(<auto_groups>)"
	R"(<auto_group register_new="true" register_quick="true" register_group="true" max_lvl="50" min_lvl="46" title_id="401197" name_id="401193" instanceId="300110000" id="1" npc_ids="279055 279054 279041 279057 279039 279056"/>)"
	R"(<auto_group register_new="true" register_quick="true" max_lvl="50" min_lvl="46" title_id="903479" name_id="402045" instanceId="300350000" id="21" npc_ids="730482 730481"/>)"
	R"(<auto_group register_new="true" max_lvl="50" min_lvl="46" title_id="903479" name_id="402046" instanceId="300360000" id="24" npc_ids="730484 730483"/>)"
	R"(<auto_group register_group="true" register_new="true" register_quick="true" max_lvl="50" min_lvl="46" title_id="904400" name_id="403505" instanceId="300450000" id="33" npc_ids="730576 730577"/>)"
	R"(<auto_group register_new="true" register_quick="true" register_group="true" max_lvl="65" min_lvl="61" title_id="904908" name_id="403696" instanceId="301120000" id="107" npc_ids="0"/>)";
	for (int32_t mask : {2, 3, 22, 23, 25, 26, 27, 28, 29, 30, 31, 32, 34, 35, 38, 39, 40, 41, 42, 43, 44, 45, 101, 102, 103, 108, 109, 111}) {
		if (mask == 111 && !withIdgelDome)
			continue;
		xml += R"(<auto_group id=")" + std::to_string(mask) + R"(" instanceId=")" + std::to_string(390000000 + mask) + R"(" min_lvl="1" max_lvl="1" register_new="true" register_quick="true" register_group="true"/>)";
	}
	return xml + "</auto_groups>";
}

/** instance_cooltimes.xml shape with test member counts: the dredgion 2 a side, the chaos arena 3, the harmony arena 6, Kamar 3 / 2, mask 22's test world 3 */
inline constexpr const char* COOLTIME_XML =
	R"(<instance_cooltimes>)"
	R"(<instance_cooltime id="81" worldId="300110000" race="PC_ALL" sync_id="81"><maxcount>3</maxcount><max_member_light>2</max_member_light><max_member_dark>2</max_member_dark></instance_cooltime>)"
	R"(<instance_cooltime id="82" worldId="300350000" race="PC_ALL" sync_id="82"><maxcount>3</maxcount><max_member_light>3</max_member_light><max_member_dark>3</max_member_dark></instance_cooltime>)"
	R"(<instance_cooltime id="83" worldId="300450000" race="PC_ALL" sync_id="83"><maxcount>3</maxcount><max_member_light>6</max_member_light><max_member_dark>6</max_member_dark></instance_cooltime>)"
	R"(<instance_cooltime id="84" worldId="301120000" race="PC_ALL" sync_id="84"><maxcount>3</maxcount><max_member_light>3</max_member_light><max_member_dark>2</max_member_dark></instance_cooltime>)"
	R"(<instance_cooltime id="85" worldId="390000022" race="PC_ALL" sync_id="85"><maxcount>3</maxcount><max_member_light>3</max_member_light><max_member_dark>3</max_member_dark></instance_cooltime>)"
	R"(<instance_cooltime id="86" worldId="390000002" race="PC_ALL" sync_id="86"><maxcount>3</maxcount><max_member_light>2</max_member_light><max_member_dark>2</max_member_dark></instance_cooltime>)"
	R"(</instance_cooltimes>)";

class AutoGroupTest : public TeamTest {
protected:
	void SetUp() override {
		TeamTest::SetUp();
		dataholders::DataManager::AUTO_GROUP.publish(xml::bindString<dataholders::AutoGroupData>(dataContext, autoGroupXml()));
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.publish(xml::bindString<dataholders::InstanceCooltimeData>(dataContext, COOLTIME_XML));
	}

	void TearDown() override {
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.resetForTests();
		dataholders::DataManager::AUTO_GROUP.resetForTests();
		TeamTest::TearDown();
	}

	Member& asmodian(std::string_view name) {
		Member& m = addMember(name);
		m.f.commonData->setRace(Race::ASMODIANS);
		return m;
	}

	runtime::Ref<LookingForParty> solo(Member& m, EntryRequestType ert = EntryRequestType::NEW_GROUP_ENTRY, int32_t maskId = 1) {
		return LookingForParty::create(m.player(), ert, maskId);
	}

	/** a used-up portal cooldown of the world, as PortalCooldownsDAO loads it (addPortalCooldown would store it in the database) */
	void cooldown(Member& m, int32_t worldId) {
		runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<model::gameobjects::player::PortalCooldown>>> cooldowns =
			runtime::RcHashMap<int32_t, runtime::Ref<model::gameobjects::player::PortalCooldown>>::create();
		cooldowns->put(worldId, model::gameobjects::player::PortalCooldown::create(worldId, commons::utils::currentTimeMillis() + 60000, 3));
		m.player().getPortalCooldownList().setPortalCoolDowns(runtime::Ptr<runtime::RcHashMap<int32_t, runtime::Ref<model::gameobjects::player::PortalCooldown>>>(cooldowns));
		cooldownMaps.push_back(cooldowns);
	}

	xml::LoadContext dataContext;
	std::vector<runtime::Ref<runtime::RcHashMap<int32_t, runtime::Ref<model::gameobjects::player::PortalCooldown>>>> cooldownMaps;
};

} // namespace aion::gameserver::network::aion::clientpackets::testing::team
