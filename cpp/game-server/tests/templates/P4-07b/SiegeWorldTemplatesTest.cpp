// P4-07b siege, static door, town, teleport, trade list, tribe, walker, world raid, npc shout, panel, recipe, ride and material templates, plus
// the chunk's C++ helpers (Java HashMap<Integer> order, FlatMap): hooks on small XML fixtures and the logic methods, with expected values derived
// by hand from the Java sources (AssaultData.java, AssaulterType.java, DoorRepairData.java, SiegeLocationTemplate.java, StaticDoorWorld.java,
// StaticDoorState.java, TownSpawnMap.java, TownSpawn.java, WalkerTemplate.java, Tribe.java, SkillPanel.java, RecipeTemplate.java, ...).

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/WalkerVersionsData.bind.h"
#include "aion/gameserver/dataholders/WalkerVersionsData.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataException.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/siege/Assaulter.h"
#include "aion/gameserver/model/templates/detail/FlatMap.h"
#include "aion/gameserver/model/templates/detail/JavaHashMap.h"
#include "aion/gameserver/model/templates/materials/MaterialSkill.bind.h"
#include "aion/gameserver/model/templates/npcshout/ShoutEventTypeInfo.h"
#include "aion/gameserver/model/templates/npcshout/ShoutGroup.bind.h"
#include "aion/gameserver/model/templates/panels/SkillPanel.bind.h"
#include "aion/gameserver/model/templates/recipe/RecipeTemplate.bind.h"
#include "aion/gameserver/model/templates/ride/RideInfo.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorStateInfo.h"
#include "aion/gameserver/model/templates/staticdoor/StaticDoorWorld.bind.h"
#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.bind.h"
#include "aion/gameserver/model/templates/towns/TownSpawnMap.bind.h"
#include "aion/gameserver/model/templates/tradelist/TradeListTemplate.bind.h"
#include "aion/gameserver/model/templates/tradelist/TradeNpcTypeInfo.h"
#include "aion/gameserver/model/templates/tribe/Tribe.bind.h"
#include "aion/gameserver/model/templates/walker/WalkerTemplate.bind.h"
#include "aion/gameserver/model/templates/worldraid/MarkerSpot.bind.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"

namespace aion::gameserver::model::templates {
namespace {

template <class T>
std::unique_ptr<T> bindXml(std::string_view text) {
	xml::LoadContext context;
	return xml::bindString<T>(context, text);
}

std::string failureOf(const std::function<void()>& action) {
	try {
		action();
	} catch (const xml::StaticDataException& e) {
		return e.what();
	}
	return "<no failure>";
}

// ---- C++ helpers -------------------------------------------------------------------------------------------------------------------------------

TEST(TemplateHelpersTest, JavaIntegerHashMapOrder) {
	using Puts = std::vector<std::pair<int32_t, int32_t>>;
	// capacity 16: HashMap.hash(k) = k ^ (k >>> 16); 17, 1 and 33 share bucket 1 in insertion order, 16 is in bucket 0
	EXPECT_EQ(detail::javaIntegerHashMapValues(Puts{{17, 17}, {1, 1}, {33, 33}, {16, 16}}), (std::vector<int32_t>{16, 17, 1, 33}));
	// a present key keeps its position and gets the new value
	EXPECT_EQ(detail::javaIntegerHashMapValues(Puts{{5, 1}, {3, 2}, {5, 3}}), (std::vector<int32_t>{2, 3}));
	// 20 and 4 share bucket 4 of 16; the 13th key exceeds the threshold 12, the table doubles to 32 and 20 moves to bucket 20
	Puts puts{{20, 20}, {4, 4}};
	for (int32_t key : {0, 1, 2, 3, 5, 6, 7, 8, 9, 10})
		puts.emplace_back(key, key);
	EXPECT_EQ(detail::javaIntegerHashMapValues(puts), (std::vector<int32_t>{0, 1, 2, 3, 20, 4, 5, 6, 7, 8, 9, 10})) << "12 keys: no resize";
	puts.emplace_back(11, 11);
	EXPECT_EQ(detail::javaIntegerHashMapValues(puts), (std::vector<int32_t>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 20})) << "13 keys: resized";
	// negative keys spread their high bits: hash(-1) = 0xFFFFFFFF ^ 0x0000FFFF = 0xFFFF0000, bucket 0 together with 0 (inserted later)
	EXPECT_EQ(detail::javaIntegerHashMapValues(Puts{{-1, -1}, {15, 15}, {0, 0}}), (std::vector<int32_t>{-1, 0, 15}));

	detail::FlatMap<int32_t, std::string> map;
	EXPECT_TRUE(map.insertOrAssign(3, "c"));
	EXPECT_TRUE(map.insertOrAssign(1, "a"));
	EXPECT_FALSE(map.insertOrAssign(3, "C"));
	EXPECT_FALSE(map.tryEmplace(1, "x"));
	ASSERT_NE(map.find(3), nullptr);
	EXPECT_EQ(*map.find(3), "C");
	EXPECT_EQ(*map.find(1), "a");
	EXPECT_EQ(map.find(2), nullptr);
	map[2] += "b";
	EXPECT_EQ(map.size(), 3u);
	EXPECT_EQ(map.begin()->first, 1);
	EXPECT_TRUE((std::is_nothrow_move_constructible_v<detail::FlatMap<int32_t, std::string>>));
}

// ---- siege locations -------------------------------------------------------------------------------------------------------------------------

TEST(SiegeTemplatesTest, AssaultAndDoorRepairHooks) {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
	std::unique_ptr<siegelocation::SiegeLocationTemplate> location = bindXml<siegelocation::SiegeLocationTemplate>(
	  R"(<siege_location id="1011" type="FORTRESS" world="400010000" name_id="5" kinah_rewards="100 200" fortress_dependency="1 2">)"
	  R"(<artifact_activation item_id="1" count="2" cd="30" repeat_count="3"/>)"
	  R"(<door_repair_data item_id="3" count="1" cd="600"><door_repair_stone static_id="20" door_id="1"/>)"
	  R"(<door_repair_stone static_id="4" door_id="2"/></door_repair_data>)"
	  R"(<assault_data dredgion_id="9" base_budget="10" base_delay="11">)"
	  R"(<assaulter type="TELEPORT" npc_ids="1 2 3" heading_offset="10" distance_offset="5"/>)"
	  R"(<assaulter type="FIGHTER" npc_ids="11 12 13 14 15"/><assaulter type="COMMANDER" npc_ids="21"/></assault_data>)"
	  R"(<merc_zone id="1" cooldown="45"/><siege_reward top="1" item_id="5" item_count="1"/></siege_location>)");
	EXPECT_EQ(location->getActivation()->getCd(), 30000);
	EXPECT_EQ(location->getRepeatCount(), 3);
	EXPECT_EQ(location->getRepeatInterval(), 1);
	EXPECT_EQ(location->getKinahRewardByRewardLevel(1), 200);
	EXPECT_EQ(location->getKinahRewardByRewardLevel(2), 0);
	EXPECT_EQ(location->getFortressDependency(), (std::vector<int32_t>{1, 2}));
	EXPECT_EQ(location->getSiegeMercenaryZones().at(0).getCooldown(), 45000);
	EXPECT_TRUE(location->getSiegeRewards().at(0).hasItemRewardsForWin());
	EXPECT_FALSE(location->getSiegeRewards().at(0).hasItemRewardsForDefeat());

	const siegelocation::DoorRepairData& doors = *location->getDoorRepairData();
	EXPECT_EQ(doors.getCd(), 600000);
	EXPECT_EQ(doors.getRepairStone(4)->getDoorId(), 2);
	EXPECT_EQ(doors.getRepairStone(5), nullptr);
	ASSERT_EQ(doors.getRepairStones().size(), 2u);

	const auto& assaulters = location->getAssaultData()->getProcessedAssaulters();
	using siege::AssaulterType;
	const auto& teleport = assaulters[static_cast<size_t>(AssaulterType::TELEPORT)];
	ASSERT_TRUE(teleport.has_value());
	ASSERT_EQ(teleport->size(), 3u) << "every npc of a teleport wave, spawn cost 0";
	EXPECT_EQ((*teleport)[2]->getNpcId(), 3);
	EXPECT_FLOAT_EQ((*teleport)[2]->getSpawnCost(), 0.0f);
	EXPECT_EQ((*teleport)[0]->getHeadingOffset(), 10);
	EXPECT_EQ((*teleport)[0]->getDistanceOffset(), 5);
	const auto& fighters = assaulters[static_cast<size_t>(AssaulterType::FIGHTER)];
	ASSERT_EQ(fighters->size(), 4u) << "FIGHTER has 4 spawn costs, the fifth npc is dropped";
	EXPECT_FLOAT_EQ((*fighters)[3]->getSpawnCost(), 1.0f);
	EXPECT_EQ((*fighters)[0]->getHeadingOffset(), 60) << "heading_offset defaults to 60";
	EXPECT_FLOAT_EQ(assaulters[static_cast<size_t>(AssaulterType::COMMANDER)]->at(0)->getSpawnCost(), 1.0f);
	EXPECT_FALSE(assaulters[static_cast<size_t>(AssaulterType::GUNNER)].has_value()) << "absent from the EnumMap";

	std::unique_ptr<siegelocation::SiegeLocationTemplate> bare = bindXml<siegelocation::SiegeLocationTemplate>(R"(<siege_location id="1"/>)");
	EXPECT_THROW(bare->getRepeatCount(), runtime::NullPointerException);
	EXPECT_EQ(bare->getKinahRewardByRewardLevel(0), 0);
	EXPECT_TRUE(bare->getFortressDependency().empty());
}

// ---- static doors, towns, teleporters, trade lists, tribes -------------------------------------------------------------------------------

TEST(WorldTemplatesTest, StaticDoorsAndTowns) {
	std::unique_ptr<staticdoor::StaticDoorWorld> world = bindXml<staticdoor::StaticDoorWorld>(
	  R"(<world world="210010000"><staticdoor id="20" state="5"/><staticdoor id="4"/><staticdoor id="1"/></world>)");
	EXPECT_EQ(world->getStaticDoor(4)->getId(), 4);
	EXPECT_EQ(world->getStaticDoor(3), nullptr);
	std::vector<int32_t> ids;
	for (const staticdoor::StaticDoorTemplate* door : world->getStaticDoors())
		ids.push_back(door->getId());
	EXPECT_EQ(ids, (std::vector<int32_t>{1, 20, 4})) << "HashMap order: bucket 1, then bucket 4 with 20 before 4";
	EXPECT_NE(failureOf([] {
		          bindXml<staticdoor::StaticDoorWorld>(R"(<world world="7"><staticdoor id="2"/><staticdoor id="2"/></world>)");
	          }).find("Duplicate door template for world 7, id: 2"),
	          std::string::npos);

	EXPECT_EQ(staticdoor::getFlag(staticdoor::StaticDoorState::CLOSEABLE), 4);
	std::set<staticdoor::StaticDoorState> stateSet{staticdoor::StaticDoorState::NONE, staticdoor::StaticDoorState::ONEWAY};
	struct Adapter {
		std::set<staticdoor::StaticDoorState>& set;
		void add(staticdoor::StaticDoorState state) { set.insert(state); }
		void remove(staticdoor::StaticDoorState state) { set.erase(state); }
		bool contains(staticdoor::StaticDoorState state) const { return set.contains(state); }
	} states{stateSet};
	staticdoor::setStates(5, states);
	EXPECT_EQ(stateSet, (std::set<staticdoor::StaticDoorState>{staticdoor::StaticDoorState::NONE, staticdoor::StaticDoorState::OPENED,
	                                                           staticdoor::StaticDoorState::CLOSEABLE}))
	  << "NONE is left alone, ONEWAY removed";
	EXPECT_EQ(staticdoor::getFlags(states), 5);

	std::unique_ptr<towns::TownSpawnMap> map = bindXml<towns::TownSpawnMap>(
	  R"(<spawn_map map_id="700010000">)"
	  R"(<town_spawn town_id="17"><town_level level="1"><spawn npc_id="1"><spot x="1" y="2" z="3"/></spawn></town_level>)"
	  R"(<town_level level="2"><spawn npc_id="2"><spot x="1" y="2" z="3"/></spawn><spawn npc_id="3"><spot x="1" y="2" z="3"/></spawn>)"
	  R"(</town_level><town_level level="1"><spawn npc_id="4"><spot x="1" y="2" z="3"/></spawn></town_level></town_spawn>)"
	  R"(<town_spawn town_id="1"><town_level level="5"/></town_spawn></spawn_map>)");
	ASSERT_NE(map->getTownSpawn(17), nullptr);
	EXPECT_EQ(map->getTownSpawn(2), nullptr);
	const towns::TownSpawn& town = *map->getTownSpawn(17);
	ASSERT_EQ(town.getTownLevels().size(), 2u) << "level 1 twice: the later one replaces the earlier";
	EXPECT_EQ(town.getSpawnsForLevel(1)->getSpawns().at(0)->getNpcId(), 4);
	EXPECT_EQ(town.getSpawnsForLevel(2)->getSpawns().size(), 2u);
	ASSERT_EQ(map->getTownSpawns().size(), 2u);
	EXPECT_EQ(map->getTownSpawns()[0]->getTownId(), 17) << "HashMap order: 17 and 1 share bucket 1 in insertion order";
}

TEST(WorldTemplatesTest, TeleportTradeTribeAndMarkers) {
	std::unique_ptr<teleport::TeleporterTemplate> teleporter = bindXml<teleport::TeleporterTemplate>(
	  R"(<teleporter_template npc_ids="5 6")"
	  R"( teleportId="1"><locations><telelocation loc_id="3" price="1" type="REGULAR"/><telelocation loc_id="4" price="2" type="FLIGHT"/>)"
	  R"(</locations></teleporter_template>)");
	EXPECT_TRUE(teleporter->containNpc(6));
	EXPECT_FALSE(teleporter->containNpc(7));
	EXPECT_EQ(teleporter->getTeleLocIdData()->getTeleportLocation(4)->getPrice(), 2);
	EXPECT_EQ(teleporter->getTeleLocIdData()->getTeleportLocation(5), nullptr);
	EXPECT_THROW(bindXml<teleport::TeleporterTemplate>(R"(<teleporter_template teleportId="2"/>)")->containNpc(1), runtime::NullPointerException);

	std::unique_ptr<tradelist::TradeListTemplate> tradeList = bindXml<tradelist::TradeListTemplate>(
	  R"(<tradelist_template npc_id="1" npc_type="ABYSS"><tradelist id="1"/><tradelist id="2"/></tradelist_template>)");
	EXPECT_EQ(tradeList->getCount(), 2);
	EXPECT_EQ(tradeList->getTradeTablist()[1].getId(), 2);
	EXPECT_EQ(tradelist::index(tradeList->getTradeNpcType()), 2);
	EXPECT_EQ(tradelist::index(tradelist::TradeNpcType::ABYSS_KINAH), 5);

	std::unique_ptr<tribe::Tribe> guard = bindXml<tribe::Tribe>(R"(<tribe name="GUARD_DARK"><aggro>PC</aggro><friend>GUARD_DARK</friend></tribe>)");
	EXPECT_EQ(guard->getBase(), TribeClass::GUARD_DARK) << "no base: the tribe itself";
	EXPECT_TRUE(guard->isGuard());
	EXPECT_EQ(guard->getAggro(), (std::vector<TribeClass>{TribeClass::PC}));
	EXPECT_TRUE(guard->getHostile().empty());
	EXPECT_EQ(guard->toString(), "GUARD_DARK (NONE)");
	std::unique_ptr<tribe::Tribe> based = bindXml<tribe::Tribe>(R"(<tribe name="PC_DARK" base="PC"/>)");
	EXPECT_EQ(based->getBase(), TribeClass::PC);
	EXPECT_FALSE(based->isGuard());

	EXPECT_EQ(bindXml<worldraid::MarkerSpot>(R"(<marker x="1" y="2.5" z="1e7" h="-3"/>)")->toString(), "MarkerSpot[x=1.0, y=2.5, z=1.0E7, h=-3]");
}

// ---- walkers -------------------------------------------------------------------------------------------------------------------------------------

TEST(WalkerTemplatesTest, StepsLoopTypesAndRows) {
	constexpr std::string_view steps = R"(<routestep x="1" y="1" z="1" rest_time="0"/><routestep x="2" y="2" z="2" rest_time="1"/>)"
	                                   R"(<routestep x="3" y="3" z="3" rest_time="2"/><routestep x="4" y="4" z="4" rest_time="3"/>)";
	std::unique_ptr<walker::WalkerTemplate> normal =
	  bindXml<walker::WalkerTemplate>(std::string(R"(<walker_template route_id="R1">)") + std::string(steps) + "</walker_template>");
	ASSERT_EQ(normal->getRouteSteps().size(), 4u);
	for (int32_t i = 0; i < 4; ++i) {
		EXPECT_EQ(normal->getRouteStep(i)->getStepIndex(), i);
		EXPECT_EQ(normal->getRouteStep(i)->isLastStep(), i == 3);
	}
	EXPECT_THROW(normal->getRouteStep(4), commons::utils::IndexOutOfBoundsException);
	EXPECT_EQ(normal->getType(), spawnengine::WalkerGroupType::POINT);
	EXPECT_FALSE(normal->getRows().has_value());

	std::unique_ptr<walker::WalkerTemplate> back = bindXml<walker::WalkerTemplate>(
	  std::string(R"(<walker_template route_id="R2" loop_type="WALK_BACK">)") + std::string(steps) + "</walker_template>");
	ASSERT_EQ(back->getRouteSteps().size(), 6u) << "steps 3 and 2 appended";
	EXPECT_FLOAT_EQ(back->getRouteStep(4)->getX(), 3.0f);
	EXPECT_FLOAT_EQ(back->getRouteStep(5)->getX(), 2.0f);
	EXPECT_EQ(back->getRouteStep(5)->getRestTime(), 1);
	EXPECT_TRUE(back->getRouteStep(5)->isLastStep());
	EXPECT_FALSE(back->getRouteStep(3)->isLastStep());
	EXPECT_EQ(back->getRouteStep(5)->getStepIndex(), 5);

	std::unique_ptr<walker::WalkerTemplate> pair =
	  bindXml<walker::WalkerTemplate>(std::string(R"(<walker_template route_id="R3" pool="2" rows="7">)") + std::string(steps) + "</walker_template>");
	EXPECT_EQ(pair->getType(), spawnengine::WalkerGroupType::SQUARE);
	EXPECT_EQ(pair->getRows(), (std::vector<int32_t>{2})) << "pool 2 ignores rows";
	std::unique_ptr<walker::WalkerTemplate> square = bindXml<walker::WalkerTemplate>(
	  std::string(R"(<walker_template route_id="R4" formation="SQUARE" rows="3,2,">)") + std::string(steps) + "</walker_template>");
	EXPECT_EQ(square->getRows(), (std::vector<int32_t>{3, 2})) << "String.split drops the trailing empty part";
	std::unique_ptr<walker::WalkerTemplate> noRows =
	  bindXml<walker::WalkerTemplate>(std::string(R"(<walker_template route_id="R5" formation="SQUARE">)") + std::string(steps) + "</walker_template>");
	EXPECT_EQ(noRows->getType(), spawnengine::WalkerGroupType::POINT);
	std::unique_ptr<walker::WalkerTemplate> commasOnly = bindXml<walker::WalkerTemplate>(
	  std::string(R"(<walker_template route_id="R8" formation="SQUARE" rows=",,">)") + std::string(steps) + "</walker_template>");
	EXPECT_EQ(commasOnly->getType(), spawnengine::WalkerGroupType::SQUARE);
	EXPECT_EQ(commasOnly->getRows(), (std::vector<int32_t>{})) << "\",,\".split(\",\") is a zero-length array: rows = new int[0]";
	std::unique_ptr<walker::WalkerTemplate> emptyRows = bindXml<walker::WalkerTemplate>(
	  std::string(R"(<walker_template route_id="R9" formation="SQUARE" rows="">)") + std::string(steps) + "</walker_template>");
	EXPECT_EQ(emptyRows->getType(), spawnengine::WalkerGroupType::POINT)
	  << "Deviation (P4-07b.md): Java fails on Integer.parseInt(\"\"); the binder cannot tell an empty attribute from an absent one";
	EXPECT_NE(failureOf([&] {
		          bindXml<walker::WalkerTemplate>(std::string(R"(<walker_template route_id="R10" formation="SQUARE" rows=",2">)") + std::string(steps) +
		                                          "</walker_template>");
	          }).find("For input string: \"\""),
	          std::string::npos)
	  << "a leading empty part stays";
	EXPECT_NE(failureOf([&] {
		          bindXml<walker::WalkerTemplate>(std::string(R"(<walker_template route_id="R6" formation="SQUARE" rows="3, 2">)") + std::string(steps) +
		                                          "</walker_template>");
	          }).find("For input string: \" 2\""),
	          std::string::npos);

	walker::WalkerTemplate created("R7");
	EXPECT_EQ(created.getRouteId(), "R7");
	// Java getVersionId: DataManager.WALKER_VERSIONS_DATA.getRouteVersionId(routeId)
	EXPECT_THROW(static_cast<void>(created.getVersionId()), runtime::NullPointerException) << "WALKER_VERSIONS_DATA is not published";
	{
		struct Unpublish {
			~Unpublish() { dataholders::DataManager::WALKER_VERSIONS_DATA.resetForTests(); }
		} unpublish;
		dataholders::DataManager::WALKER_VERSIONS_DATA.publish(bindXml<dataholders::WalkerVersionsData>(
		  R"(<walker_versions><walk_parent id="GROUP_R"><version id="R7"/><version id="R8"/></walk_parent></walker_versions>)"));
		EXPECT_EQ(created.getVersionId(), std::optional<std::string>("GROUP_R"));
		EXPECT_EQ(normal->getVersionId(), std::nullopt) << "R1 is not versioned: Java null";
	}
	walker::RouteStep step(1.0f, 2.0f, 3.0f, 4);
	step.setZ(5.0f);
	EXPECT_FLOAT_EQ(step.getZ(), 5.0f);
	EXPECT_EQ(step.getRestTime(), 4);
}

// ---- shouts, panels, recipes, rides, materials -------------------------------------------------------------------------------------------------

TEST(MiscWorldTemplatesTest, ShoutsPanelsRecipesRidesMaterials) {
	std::unique_ptr<npcshout::ShoutGroup> group = bindXml<npcshout::ShoutGroup>(
	  R"(<shout_group client_ai="x"><shout_npcs npc_ids="1 2")"
	  R"( restrict_world="3"><shout string_id="1" when="IDLE" skill_no="4"/><shout string_id="2" when="DIED"/></shout_npcs></shout_group>)");
	npcshout::ShoutList& list = const_cast<npcshout::ShoutList&>(group->getShoutNpcs().at(0));
	EXPECT_EQ(list.getNpcIds(), (std::vector<int32_t>{1, 2}));
	EXPECT_EQ(list.getRestrictWorld(), 3);
	EXPECT_EQ(list.getNpcShouts()[0].getSkillNo(), 4);
	EXPECT_EQ(list.getNpcShouts()[1].getSkillNo(), 0);
	EXPECT_EQ(list.getNpcShouts()[1].getPollDelay(), 0);
	list.makeNull();
	EXPECT_TRUE(list.getNpcIds().empty());
	EXPECT_TRUE(list.getNpcShouts().empty());
	EXPECT_EQ(list.getRestrictWorld(), 0);
	group->makeNull();
	EXPECT_TRUE(group->getShoutNpcs().empty());
	EXPECT_EQ(group->getClientAi(), "");
	EXPECT_EQ(npcshout::fromValue("ATTACK_HITPOINT"), npcshout::ShoutEventType::ATTACK_HITPOINT);

	std::unique_ptr<panels::SkillPanel> panel = bindXml<panels::SkillPanel>(R"(<panel panel_id="2" panel_skills="4353537 4353538"/>)");
	EXPECT_EQ(panel->getPanelId(), 2);
	EXPECT_TRUE(panel->canUseSkill(17006, 1)) << "4353537 = 17006 << 8 | 1";
	EXPECT_FALSE(panel->canUseSkill(17006, 3));
	EXPECT_TRUE(panel->isSkillPresent(17006));
	EXPECT_FALSE(panel->isSkillPresent(17007));
	EXPECT_EQ(panel->getSkills(), nullptr) << "Java returns null";
	EXPECT_THROW(bindXml<panels::SkillPanel>(R"(<panel panel_id="3"/>)")->isSkillPresent(1), runtime::NullPointerException);

	std::unique_ptr<recipe::RecipeTemplate> recipe = bindXml<recipe::RecipeTemplate>(
	  R"(<recipe_template id="1" nameid="2" quantity="3"><comboproduct itemid="10"/><comboproduct itemid="11"/></recipe_template>)");
	EXPECT_EQ(recipe->getComboProductSize(), 2);
	EXPECT_EQ(recipe->getComboProduct(2), 11);
	EXPECT_THROW(recipe->getComboProduct(3), commons::utils::IndexOutOfBoundsException);
	EXPECT_EQ(bindXml<recipe::RecipeTemplate>(R"(<recipe_template id="2"/>)")->getComboProduct(1), std::nullopt);
	EXPECT_TRUE(recipe->getComponents().empty());

	EXPECT_TRUE(bindXml<ride::RideInfo>(R"(<ride_info id="1" sprint_speed="2"/>)")->canSprint());
	EXPECT_FALSE(bindXml<ride::RideInfo>(R"(<ride_info id="1"/>)")->canSprint());

	std::unique_ptr<materials::MaterialSkill> skill =
	  bindXml<materials::MaterialSkill>(R"(<skill id="1" level="2" frequency="3" conditions="NIGHT"/>)");
	EXPECT_EQ(skill->getTarget(), materials::MaterialTarget::ALL);
	EXPECT_EQ(skill->getConditions(), (std::vector<materials::MaterialActCondition>{materials::MaterialActCondition::NIGHT}));
	EXPECT_TRUE(bindXml<materials::MaterialSkill>(R"(<skill id="1" level="2" frequency="3" target="NPC"/>)")->getConditions().empty());
}

} // namespace
} // namespace aion::gameserver::model::templates
