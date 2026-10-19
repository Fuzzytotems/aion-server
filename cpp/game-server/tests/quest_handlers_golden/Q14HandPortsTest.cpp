// P6 leftovers (lane C, 2026-10-18): what the golden traces of Q14's four hand ports cannot see (docs/deviations/Q14.md, "Hand ports:
// 13744, 23744, 30721 and 30771"), checked here by hand with the Java lines beside them, on QuestHandlerTest's fixture with the quests'
// real rows: the oracle replays levinshor/_13744AgentinNeed and _23744EffectElyosElimination with no siege in progress and models neither
// their kills in a zone nor the REWARD guard of their dialog, and it refuses the item-use zone of tiamat_stronghold/_30721 and _30771, so
// their ambush (two Mirror Images 1.5 s after the item use) has no case. A siege of Levinshor's fortress (8011) is put into SiegeService;
// the zones are small cylinders in the quester's region of the fixture's Poeta map, published by a gtest environment before any fixture of the executable
// publishes its empty zone list (ItemPacketTestSupport.h: only when none is published). Only the zones' names and types are the real ones.

#include "../quest_handlers/QuestHandlerTestSupport.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <tuple>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <pugixml.hpp>

#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/siege/Siege.h"
#include "aion/gameserver/world/zone/ZoneName.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

// the handlers' factories: GoldenQ14Handlers.cpp compiles the four files into this executable
namespace aion::gameserver::handlers::quest::levinshor {
::std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _13744AgentinNeed_questFactory();
::std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _23744EffectElyosElimination_questFactory();
} // namespace aion::gameserver::handlers::quest::levinshor
namespace aion::gameserver::handlers::quest::tiamat_stronghold {
::std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _30721OminousDebrisEnergy_questFactory();
::std::unique_ptr<::aion::gameserver::questEngine::handlers::AbstractQuestHandler> _30771ImpendingDebrisEnergy_questFactory();
} // namespace aion::gameserver::handlers::quest::tiamat_stronghold

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services {

/** The service's active sieges (SiegeService.h befriends this struct for the tests) */
struct SiegeServiceTestAccess {
	static runtime::ConcurrentHashMap<int32_t, runtime::Ref<siege::Siege>>& activeSieges(SiegeService& service) { return service.activeSieges; }
};

} // namespace aion::gameserver::services

namespace aion::gameserver::questEngine::handlers::test::q14 {
namespace {

namespace fs = std::filesystem;
namespace DA = ::aion::gameserver::model::DialogAction;
using gameserver::model::Race;

const fs::path STATIC_DATA = fs::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";
constexpr int32_t AGENT_IN_NEED = 13744;
constexpr int32_t ELYOS_ELIMINATION = 23744;
constexpr int32_t ELYOS_NPC = 832840;
constexpr int32_t ASMODIAN_NPC = 832841;
constexpr int32_t LEVINSHOR_FORTRESS = 8011;
constexpr int32_t OMINOUS = 30721;
constexpr int32_t IMPENDING = 30771;
constexpr int32_t SEDATIVE = 182215698;
constexpr int32_t RESTORATIVE = 182215699;
constexpr int32_t MIRROR_IMAGE = 217424;
// the cases' spots in the fixture's Poeta map, each the centre of one test zone (radius 10), all in the quester's first region (the cell
// (0, 0) of 128 m: a move by setXYZH keeps the position's region) and away from its spot (100, 100), which no zone holds
constexpr float SHRINE_X = 30.0f, SHRINE_Y = 30.0f;
constexpr float OMINOUS_X = 30.0f, OMINOUS_Y = 70.0f;
constexpr float IMPENDING_X = 70.0f, IMPENDING_Y = 30.0f;
constexpr float OUTSIDE = 25.0f; // a move this far along x leaves the spot's zone

/** Levinshor's two zones and the two item-use zones as cylinders of Poeta (only the names and types are the real ones) */
constexpr const char* Q14_ZONES_XML =
	R"(<zones><zone mapid="210010000" name="DRAGON_LORDS_SHRINE_600100000" area_type="CYLINDER" zone_type="SUB">)"
	R"(<cylinder x="30" y="30" r="10" bottom="0" top="100"/></zone>)"
	R"(<zone mapid="210010000" name="LF5_ITEMUSEAREA_Q30721" area_type="CYLINDER" zone_type="ITEM_USE">)"
	R"(<cylinder x="30" y="70" r="10" bottom="0" top="100"/></zone>)"
	R"(<zone mapid="210010000" name="DF5_ITEMUSEAREA_Q30771" area_type="CYLINDER" zone_type="ITEM_USE">)"
	R"(<cylinder x="70" y="30" r="10" bottom="0" top="100"/></zone></zones>)";

/** Publishes the zones once per process, before any fixture runs (ZONE_DATA stays published: the map instances cache their zones) */
class Q14ZonesEnvironment final : public ::testing::Environment {
public:
	void SetUp() override {
		if (!dataholders::DataManager::ZONE_DATA)
			dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(context, Q14_ZONES_XML));
	}

private:
	xml::LoadContext context;
};
[[maybe_unused]] ::testing::Environment* const q14Zones = ::testing::AddGlobalTestEnvironment(new Q14ZonesEnvironment);

/** A siege that is only in SiegeService's active sieges (isSiegeInProgress) */
class ActiveSiege final : public services::siege::Siege {
	AION_MAKE_REF_FRIEND

public:
	explicit ActiveSiege(gameserver::model::siege::SiegeLocation& location) : Siege(location) {}

	void onSiegeStart() override {}
	void onSiegeFinish() override {}
	bool isEndless() override { return true; }
	void onAbyssPointsAdded(gameserver::model::gameobjects::player::Player&, int32_t) override {}

protected:
	~ActiveSiege() override = default;
};

std::string printedRow(const pugi::xml_node& node) {
	std::ostringstream out;
	node.print(out, "", pugi::format_raw);
	return out.str();
}

struct LevinshorData {
	std::string questsXml;
	std::string npcsXml;
	std::string itemRows;
	std::string experienceXml;
};

/** The rows of quests 13744 and 23744 and of their npcs, read once per process; the whole experience table */
const LevinshorData& levinshorData() {
	static const LevinshorData data = [] {
		LevinshorData d;
		pugi::xml_document quests;
		if (!quests.load_file((STATIC_DATA / "quest_data/quest_data.xml").c_str()))
			throw std::runtime_error("cannot read quest_data.xml");
		for (pugi::xml_node quest : quests.document_element().children("quest")) {
			int32_t id = quest.attribute("id").as_int();
			if (id == AGENT_IN_NEED || id == ELYOS_ELIMINATION || id == OMINOUS || id == IMPENDING)
				d.questsXml += printedRow(quest);
		}
		d.questsXml = "<quests>" + d.questsXml + "</quests>";
		pugi::xml_document npcs;
		if (!npcs.load_file((STATIC_DATA / "npcs/npc_templates.xml").c_str()))
			throw std::runtime_error("cannot read npc_templates.xml");
		for (pugi::xml_node npc : npcs.document_element().children("npc_template")) {
			int32_t id = npc.attribute("npc_id").as_int();
			if (id == ELYOS_NPC || id == ASMODIAN_NPC || id == MIRROR_IMAGE)
				d.npcsXml += printedRow(npc);
		}
		d.npcsXml = "<npc_templates>" + d.npcsXml + "</npc_templates>";
		pugi::xml_document items;
		if (!items.load_file((STATIC_DATA / "items/item_templates.xml").c_str()))
			throw std::runtime_error("cannot read item_templates.xml");
		for (pugi::xml_node item : items.document_element().children("item_template")) {
			int32_t id = item.attribute("id").as_int();
			if (id == SEDATIVE || id == RESTORATIVE)
				d.itemRows += printedRow(item);
		}
		std::ifstream experience(STATIC_DATA / "player_experience_table.xml", std::ios::binary);
		if (!experience)
			throw std::runtime_error("cannot read player_experience_table.xml");
		d.experienceXml.assign(std::istreambuf_iterator<char>(experience), std::istreambuf_iterator<char>());
		return d;
	}();
	return data;
}

class Q14HandPortsTest : public QuestHandlerTest {
protected:
	void SetUp() override {
		QuestHandlerTest::SetUp();
		const LevinshorData& data = levinshorData();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), data.questsXml));
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), data.npcsXml));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), data.experienceXml));
		std::string itemsXml(items::ITEM_TEMPLATES_XML);
		itemsXml.erase(itemsXml.rfind("</item_templates>"));
		itemsXml += HANDLER_ITEMS_XML;
		itemsXml += data.itemRows;
		itemsXml += "</item_templates>";
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(), itemsXml));
		static const bool geoInitialised = [] {
			world::geo::GeoService::getInstance().init(); // empty geo maps: getZ answers NaN, getClosestCollision the point itself
			return true;
		}();
		static_cast<void>(geoInitialised);
		auto ominous = ::aion::gameserver::handlers::quest::tiamat_stronghold::_30721OminousDebrisEnergy_questFactory();
		ominousHandler = ominous.get();
		QuestEngine::getInstance().addQuestHandler(std::move(ominous));
		auto impending = ::aion::gameserver::handlers::quest::tiamat_stronghold::_30771ImpendingDebrisEnergy_questFactory();
		impendingHandler = impending.get();
		QuestEngine::getInstance().addQuestHandler(std::move(impending));
		auto agent = ::aion::gameserver::handlers::quest::levinshor::_13744AgentinNeed_questFactory();
		agentHandler = agent.get();
		QuestEngine::getInstance().addQuestHandler(std::move(agent));
		auto elimination = ::aion::gameserver::handlers::quest::levinshor::_23744EffectElyosElimination_questFactory();
		eliminationHandler = elimination.get();
		QuestEngine::getInstance().addQuestHandler(std::move(elimination));
		fortressTemplate = xml::bindString<gameserver::model::templates::siegelocation::SiegeLocationTemplate>(
			contexts.emplace_back(), R"(<siege_location id="8011" type="FORTRESS" world="600100000" siege_duration="3600"/>)");
		fortress = gameserver::model::siege::FortressLocation::create(fortressTemplate.get());
	}

	void TearDown() override {
		services::SiegeServiceTestAccess::activeSieges(services::SiegeService::getInstance()).remove(LEVINSHOR_FORTRESS);
		fortress = nullptr;
		for (const Ptr<gameserver::model::gameobjects::Npc>& npc : spawnedOf(MIRROR_IMAGE))
			npc->getController().delete_();
		QuestHandlerTest::TearDown();
	}

	void siegeInProgress() {
		services::SiegeServiceTestAccess::activeSieges(services::SiegeService::getInstance())
			.put(LEVINSHOR_FORTRESS, runtime::makeRef<ActiveSiege>(*fortress));
		ASSERT_TRUE(services::SiegeService::getInstance().isSiegeInProgress(LEVINSHOR_FORTRESS));
	}

	Quester* quester(Race race, int32_t level = 65) {
		static int32_t nextObjectId = 832501;
		int32_t objectId = nextObjectId++;
		Quester* q = makeQuester(objectId, "Levinshor" + std::to_string(objectId), race, level);
		q->clearSent();
		return q;
	}

	QuestEnv& env(Quester& q, int32_t questId, int32_t targetNpc = 0, int32_t dialogActionId = 0) {
		Ptr<gameserver::model::gameobjects::VisibleObject> target;
		if (targetNpc != 0) {
			target = npcOf(targetNpc);
			q.player().setTarget(target);
		}
		envs.push_back(QuestEnv::create(target, q.player(), questId, dialogActionId));
		return *envs.back();
	}

	static Ptr<QuestState> stateOf(const Quester& q, int32_t questId) { return q.player().getQuestStateList()->getQuestState(questId); }

	/** the zone update of a move (ZoneUpdateService: MapRegion.revalidateZones) */
	static void moveTo(Quester& q, float x, float y) {
		q.player().getPosition()->setXYZH(x, y, 50.0f, int8_t{0});
		q.player().getPosition()->getMapRegion()->revalidateZones(q.player());
	}

	static world::WorldMapInstance& poeta() { return *world::World::getInstance().getWorldMap(210010000)->getMainWorldMapInstance(); }

	static std::vector<Ptr<gameserver::model::gameobjects::Npc>> spawnedOf(int32_t npcId) { return poeta().getNpcs({npcId}); }

	/** a kill of a player of `level` by the quester, as QuestEngine.onKillInZone hands it to the handler */
	bool killPlayer(AbstractQuestHandler& handler, Quester& q, int32_t questId, int32_t level) {
		Quester* victim = quester(Race::ELYOS, level);
		envs.push_back(QuestEnv::create(Ptr<gameserver::model::gameobjects::VisibleObject>(victim->player()), q.player(), questId, 0));
		return handler.onKillInZoneEvent(*envs.back());
	}

	AbstractQuestHandler* ominousHandler = nullptr;
	AbstractQuestHandler* impendingHandler = nullptr;
	AbstractQuestHandler* agentHandler = nullptr;
	AbstractQuestHandler* eliminationHandler = nullptr;
	std::unique_ptr<gameserver::model::templates::siegelocation::SiegeLocationTemplate> fortressTemplate;
	Ptr<gameserver::model::siege::FortressLocation> fortress;
	std::vector<Ref<QuestEnv>> envs;
};

/** _13744AgentinNeed.java:33-43: entering either zone starts the quest only while the fortress 8011 is besieged */
TEST_F(Q14HandPortsTest, TheZonesStartTheQuestOnlyDuringTheSiege) {
	for (auto [handler, questId, race] : {std::tuple{agentHandler, AGENT_IN_NEED, Race::ELYOS}, std::tuple{eliminationHandler, ELYOS_ELIMINATION, Race::ASMODIANS}}) {
		services::SiegeServiceTestAccess::activeSieges(services::SiegeService::getInstance()).remove(LEVINSHOR_FORTRESS);
		Quester* q = quester(race);
		const world::zone::ZoneName* shrine = world::zone::ZoneName::createOrGet("DRAGON_LORDS_SHRINE_600100000");
		const world::zone::ZoneName* downs = world::zone::ZoneName::createOrGet("FLAMEBERTH_DOWNS_600100000");
		const world::zone::ZoneName* other = world::zone::ZoneName::createOrGet("LEVINSHOR_TEST_OTHER_ZONE");
		EXPECT_FALSE(handler->onEnterZoneEvent(env(*q, questId), shrine)) << questId << ": no siege";
		EXPECT_EQ(stateOf(*q, questId), nullptr) << questId;
		siegeInProgress();
		EXPECT_FALSE(handler->onEnterZoneEvent(env(*q, questId), other)) << questId << ": another zone";
		EXPECT_EQ(stateOf(*q, questId), nullptr) << questId;
		EXPECT_TRUE(handler->onEnterZoneEvent(env(*q, questId), downs)) << questId << ": the siege";
		ASSERT_NE(stateOf(*q, questId), nullptr) << questId;
		EXPECT_EQ(stateOf(*q, questId)->getStatus(), QuestStatus::START) << questId;
		EXPECT_FALSE(handler->onEnterZoneEvent(env(*q, questId), shrine)) << questId << ": started already";
	}
}

/** _13744AgentinNeed.java:76-90: the npc talks only to a quest at REWARD */
TEST_F(Q14HandPortsTest, TheNpcTalksOnlyAtReward) {
	for (auto [handler, questId, race, npc] : {std::tuple{agentHandler, AGENT_IN_NEED, Race::ELYOS, ELYOS_NPC},
			 std::tuple{eliminationHandler, ELYOS_ELIMINATION, Race::ASMODIANS, ASMODIAN_NPC}}) {
		Quester* q = quester(race);
		EXPECT_FALSE(handler->onDialogEvent(env(*q, questId, npc, DA::USE_OBJECT))) << questId << ": no state";
		hold(*q, questId, QuestStatus::START);
		EXPECT_FALSE(handler->onDialogEvent(env(*q, questId, npc, DA::USE_OBJECT))) << questId << ": at START";
		stateOf(*q, questId)->setStatus(QuestStatus::REWARD);
		EXPECT_TRUE(handler->onDialogEvent(env(*q, questId, npc, DA::USE_OBJECT))) << questId << ": page 10002";
	}
}

/** _13744AgentinNeed.java:46-73: a kill of a player at most 5 levels below and 9 above, inside a zone, during the siege counts in var 1;
 * the twelfth makes the quest REWARD */
TEST_F(Q14HandPortsTest, TheKillsInTheZonesCountDuringTheSiege) {
	Quester* q = quester(Race::ELYOS, 55);
	hold(*q, AGENT_IN_NEED, QuestStatus::START);
	moveTo(*q, SHRINE_X + OUTSIDE, SHRINE_Y);
	siegeInProgress();
	EXPECT_FALSE(killPlayer(*agentHandler, *q, AGENT_IN_NEED, 55)) << "outside the zones";
	moveTo(*q, SHRINE_X, SHRINE_Y);
	ASSERT_TRUE(q->player().isInsideZone(world::zone::ZoneName::get("DRAGON_LORDS_SHRINE_600100000")));
	services::SiegeServiceTestAccess::activeSieges(services::SiegeService::getInstance()).remove(LEVINSHOR_FORTRESS);
	EXPECT_FALSE(killPlayer(*agentHandler, *q, AGENT_IN_NEED, 55)) << "no siege";
	siegeInProgress();
	EXPECT_FALSE(killPlayer(*agentHandler, *q, AGENT_IN_NEED, 61)) << "6 above";
	EXPECT_FALSE(killPlayer(*agentHandler, *q, AGENT_IN_NEED, 45)) << "10 below";
	EXPECT_EQ(stateOf(*q, AGENT_IN_NEED)->getQuestVarById(1), 0);
	EXPECT_TRUE(killPlayer(*agentHandler, *q, AGENT_IN_NEED, 60)) << "5 above (player >= target - 5)";
	EXPECT_TRUE(killPlayer(*agentHandler, *q, AGENT_IN_NEED, 46)) << "9 below (player <= target + 9)";
	EXPECT_EQ(stateOf(*q, AGENT_IN_NEED)->getQuestVarById(1), 2);
	for (int32_t kill = 2; kill < 11; kill++)
		EXPECT_TRUE(killPlayer(*agentHandler, *q, AGENT_IN_NEED, 55));
	EXPECT_EQ(stateOf(*q, AGENT_IN_NEED)->getQuestVarById(1), 11);
	EXPECT_EQ(stateOf(*q, AGENT_IN_NEED)->getStatus(), QuestStatus::START);
	EXPECT_TRUE(killPlayer(*agentHandler, *q, AGENT_IN_NEED, 55)) << "the twelfth";
	EXPECT_EQ(stateOf(*q, AGENT_IN_NEED)->getStatus(), QuestStatus::REWARD);
	EXPECT_EQ(stateOf(*q, AGENT_IN_NEED)->getQuestVarById(0), 1);
	EXPECT_EQ(stateOf(*q, AGENT_IN_NEED)->getQuestVarById(1), 0);
}

/** _30721OminousDebrisEnergy.java:96-128 (and 30771's): inside the item-use zone at step 2 the quest item calls two Mirror Images within
 * 4 m of the player 1.5 s after the use. Their hate is not seen here: SpawnEngine puts them into the World's Poeta instance, another than
 * the fixture's quester's, so their known list never holds him and AggroList.addHate drops it (isAware); left to the retest */
TEST_F(Q14HandPortsTest, TheDebrisItemCallsTwoMirrorImages) {
	for (auto [handler, questId, item, x, y] : {std::tuple{ominousHandler, OMINOUS, SEDATIVE, OMINOUS_X, OMINOUS_Y},
			 std::tuple{impendingHandler, IMPENDING, RESTORATIVE, IMPENDING_X, IMPENDING_Y}}) {
		Quester* q = quester(questId == OMINOUS ? Race::ELYOS : Race::ASMODIANS);
		hold(*q, questId, QuestStatus::START, 2);
		gameserver::model::gameobjects::Item& use = holdItem(*q, 840101 + questId % 100, item, 1);
		moveTo(*q, x + OUTSIDE, y);
		EXPECT_EQ(handler->onItemUseEvent(env(*q, questId), use), HandlerResult::FAILED) << questId << ": outside the zone";
		moveTo(*q, x, y);
		EXPECT_EQ(handler->onItemUseEvent(env(*q, questId), use), HandlerResult::SUCCESS) << questId;
		executor->advance(std::chrono::milliseconds(1499));
		EXPECT_TRUE(spawnedOf(MIRROR_IMAGE).empty()) << questId;
		executor->advance(std::chrono::milliseconds(1));
		std::vector<Ptr<gameserver::model::gameobjects::Npc>> images = spawnedOf(MIRROR_IMAGE);
		ASSERT_EQ(images.size(), 2u) << questId << ": 1.5 s after the use";
		executor->advance(std::chrono::milliseconds(1500));
		EXPECT_EQ(stateOf(*q, questId)->getQuestVarById(0), 3) << questId << ": useQuestItem's 3 s, 2 -> 3";
		for (const Ptr<gameserver::model::gameobjects::Npc>& image : images) {
			EXPECT_LE(std::hypot(image->getX() - x, image->getY() - y), 4.0f + 0.01f) << questId;
			EXPECT_FLOAT_EQ(image->getZ(), 50.0f) << questId << ": the player's z (no geo)";
			image->getController().delete_();
		}
	}
}

} // namespace
} // namespace aion::gameserver::questEngine::handlers::test::q14
