#pragma once

// Q10 (P6-Q slice 2, 2026-09-29): the fixture of the hand-ported quests of altgard and pandaemonium - 2208, 2230, 2252, 24013 and 2900 - and
// of the generated handler whose path kills its target (24012's cart), which the golden harness cannot pair (tests/quest_handlers_golden,
// knownNotReproducible), and, since the review of 2026-09-29, of the generated 2263, 2925 and 24016, whose hooks the golden harness does not
// observe (2223 and 24010, held back at the integration of slice 2, left the fixture's cases; docs/deviations/Q10.md, "Held back").
//
// - QuestHandlerTest (tests/quest_handlers/QuestHandlerTestSupport.h, included by relative path as the golden harness does): the quester stands
//   in the item fixture's Poeta instance on a DeterministicExecutor over a ManualClock, the npcs a case needs are spawned into the World's
//   Poeta instance (AbstractQuestHandler.spawn, SpawnEngine; poeta() below, another instance object than the quester's: the review of
//   2026-09-29), and every packet the quester's client receives is recorded.
// - The static data are the real rows of the quests, npcs and items the cases name, filtered out of the Java tree's static_data once per
//   process (the golden harness's way), and the whole player_experience_table.xml (the quests reach level 43).
// - One zone is published before the item fixture publishes its empty zone list: DF1A_ITEMUSEAREA_Q2016_220030000, 24013's item-use zone
//   (zones_220030000.xml:259, an ITEM_USE polygon of Altgard), as a cylinder of radius 20 around the quester's spot in the fixture's Poeta
//   map, so that Creature.isInsideZone answers true there and false 50 m away. Only the name and the zone type are Altgard's: 24013 checks
//   the name.
// - The handler under test is created through its AION_QUEST_HANDLER factory and handed to QuestEngine.addQuestHandler, as QuestEngine.init
//   does with the generated registry (Immortal, RT-11).

#include "../quest_handlers/QuestHandlerTestSupport.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <pugixml.hpp>

#include "aion/gameserver/controllers/VisibleObjectController.h"
#include "aion/gameserver/dataholders/SkillTreeData.bind.h"
#include "aion/gameserver/dataholders/SkillTreeData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/RecipeList.h"
#include "aion/gameserver/model/gameobjects/player/npcFaction/NpcFactions.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/quest/QuestNpc.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/services/GameTimeService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::questEngine::handlers::test::asmodae {

namespace fs = std::filesystem;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::VisibleObject;
using Player = gameserver::model::gameobjects::player::Player;

inline const fs::path STATIC_DATA = fs::path(AION_GAMESERVER_JAVA_DIR) / "data/static_data";

/**
 * The quests whose rows the fixture loads (their finished prerequisites are added). The review of 2026-09-29 added the generated 2263, 2925 and
 * 24016, whose hooks the oracle does not reach with an observable effect (docs/deviations/Q10.md, "Tests")
 */
inline constexpr int32_t QUESTS[] = {2208, 2223, 2230, 2252, 2263, 2900, 2925, 24010, 24012, 24013, 24016};
/** The npcs the cases talk to, kill or spawn (the handlers' registrations and spawns) */
inline constexpr int32_t NPCS[] = {203589, 203591, 203616, 700134, 211621, 203621, 203646, 700060, 210634, 210635, 203605, 700096, 203631, 210455,
	210456, 214039, 210458, 214032, 210457, 204182, 203550, 790003, 790002, 203546, 204264, 204061, 204263, 798036, 204261, 204235, 204127, 204193,
	203557};
/** Items the cases hold besides those the quest rows name */
inline constexpr int32_t ITEMS[] = {182203205, 182203223, 182203235, 182215359, 182203217, 140000001, 140000002, 140000003, 140000004, 182203242,
	110100288};

/** 24013's item-use zone (zones_220030000.xml:259) as a cylinder around the quester's spot (100, 100, 50) of the fixture's Poeta map */
inline constexpr const char* ASMODAE_ZONES_XML =
	R"(<zones><zone mapid="210010000" name="DF1A_ITEMUSEAREA_Q2016_220030000" area_type="CYLINDER" zone_type="ITEM_USE">)"
	R"(<cylinder x="100" y="100" r="20" bottom="0" top="100"/></zone></zones>)";

struct AsmodaeData {
	std::string questsXml;
	std::string npcsXml;
	std::string itemsXml;
	std::string experienceXml;
};

inline std::string printedRow(const pugi::xml_node& node) {
	std::ostringstream out;
	node.print(out, "", pugi::format_raw);
	return out.str();
}

/** Reads the static data files once and keeps the rows of the quests, npcs and items above */
inline const AsmodaeData& asmodaeData() {
	static const AsmodaeData data = [] {
		AsmodaeData d;
		std::set<int32_t> questIds(std::begin(QUESTS), std::end(QUESTS)), npcIds(std::begin(NPCS), std::end(NPCS)),
			itemIds(std::begin(ITEMS), std::end(ITEMS));
		pugi::xml_document quests;
		if (!quests.load_file((STATIC_DATA / "quest_data/quest_data.xml").c_str()))
			throw std::runtime_error("cannot read quest_data.xml");
		for (pugi::xml_node quest : quests.document_element().children("quest")) {
			if (!questIds.contains(quest.attribute("id").as_int()))
				continue;
			for (pugi::xml_node conditions : quest.children("start_conditions")) {
				for (pugi::xml_node finished : conditions.children("finished"))
					questIds.insert(finished.attribute("quest_id").as_int());
			}
		}
		const std::regex itemIdAttribute(R"(item_id="(\d+)\")");
		for (pugi::xml_node quest : quests.document_element().children("quest")) {
			if (!questIds.contains(quest.attribute("id").as_int()))
				continue;
			std::string text = printedRow(quest);
			for (std::sregex_iterator it(text.begin(), text.end(), itemIdAttribute), end; it != end; ++it)
				itemIds.insert(std::stoi((*it)[1].str()));
			d.questsXml += text;
		}
		d.questsXml = "<quests>" + d.questsXml + "</quests>";

		pugi::xml_document npcs;
		if (!npcs.load_file((STATIC_DATA / "npcs/npc_templates.xml").c_str()))
			throw std::runtime_error("cannot read npc_templates.xml");
		for (pugi::xml_node npc : npcs.document_element().children("npc_template")) {
			if (npcIds.contains(npc.attribute("npc_id").as_int()))
				d.npcsXml += printedRow(npc);
		}
		d.npcsXml = "<npc_templates>" + d.npcsXml + "</npc_templates>";

		// the item fixture (ItemPacketTestSupport.h) and QuestHandlerTestSupport.h already hold some rows: never twice
		std::set<int32_t> fixtureItems;
		const std::regex templateId(R"(<item_template id="(\d+)\")");
		for (std::string_view fixture : {std::string_view(items::ITEM_TEMPLATES_XML), std::string_view(HANDLER_ITEMS_XML)}) {
			std::string text(fixture);
			for (std::sregex_iterator it(text.begin(), text.end(), templateId), end; it != end; ++it)
				fixtureItems.insert(std::stoi((*it)[1].str()));
		}
		pugi::xml_document itemsDoc;
		if (!itemsDoc.load_file((STATIC_DATA / "items/item_templates.xml").c_str()))
			throw std::runtime_error("cannot read item_templates.xml");
		for (pugi::xml_node item : itemsDoc.document_element().children("item_template")) {
			int32_t id = item.attribute("id").as_int();
			if (itemIds.contains(id) && !fixtureItems.contains(id))
				d.itemsXml += printedRow(item);
		}
		std::ifstream experience(STATIC_DATA / "player_experience_table.xml", std::ios::binary);
		if (!experience)
			throw std::runtime_error("cannot read player_experience_table.xml");
		d.experienceXml.assign(std::istreambuf_iterator<char>(experience), std::istreambuf_iterator<char>());
		return d;
	}();
	return data;
}

class AsmodaeQuestTest : public QuestHandlerTest {
protected:
	void SetUp() override {
		// before the item fixture publishes its empty zone list (ItemPacketTestSupport.h: only when none is published)
		if (!dataholders::DataManager::ZONE_DATA)
			dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(zoneContext(), ASMODAE_ZONES_XML));
		QuestHandlerTest::SetUp();
		static const bool geoInitialised = [] {
			world::geo::GeoService::getInstance().init(); // empty geo maps: getZ answers NaN, the spawns keep their z
			return true;
		}();
		static_cast<void>(geoInitialised);
		const AsmodaeData& data = asmodaeData();
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(contexts.emplace_back(), data.questsXml));
		dataholders::DataManager::NPC_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), data.npcsXml));
		std::string itemsXml(items::ITEM_TEMPLATES_XML);
		itemsXml.erase(itemsXml.rfind("</item_templates>"));
		itemsXml += HANDLER_ITEMS_XML;
		itemsXml += data.itemsXml;
		itemsXml += "</item_templates>";
		dataholders::DataManager::ITEM_DATA.resetForTests();
		dataholders::DataManager::ITEM_DATA.publish(xml::bindString<dataholders::ItemData>(contexts.emplace_back(), itemsXml));
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(contexts.emplace_back(), data.experienceXml));
		services::GameTimeService::getInstance(); // a finish's level up reads the game time (the golden harness's reason)
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		dataholders::DataManager::SKILL_TREE_DATA.publish(xml::bindString<dataholders::SkillTreeData>(contexts.emplace_back(), "<skill_tree/>"));
	}

	void TearDown() override {
		for (const Ref<VisibleObject>& object : spawned) {
			if (object->isSpawned())
				object->getController().delete_();
		}
		spawned.clear();
		dataholders::DataManager::SKILL_TREE_DATA.resetForTests();
		QuestHandlerTest::TearDown();
	}

	static xml::LoadContext& zoneContext() {
		static xml::LoadContext context; // ZONE_DATA stays published for the process (the World's zones cache it)
		return context;
	}

	/** A quester of the race and level with the parts a loaded player has (the golden harness's run: npc factions, skills, recipes) */
	Quester* quester(gameserver::model::Race race, int32_t level, gameserver::model::PlayerClass playerClass = gameserver::model::PlayerClass::WARRIOR) {
		static int32_t nextObjectId = 830101; // unique in the process: the World keeps its players by id and by name
		int32_t objectId = nextObjectId++;
		Quester* q = makeQuester(objectId, "Asmodae" + std::to_string(objectId), race, level);
		Player& p = q->player();
		p.setNpcFactions(std::make_unique<gameserver::model::gameobjects::player::npcFaction::NpcFactions>(p));
		p.setSkillList(gameserver::model::skill::PlayerSkillList::create());
		p.setRecipeList(gameserver::model::gameobjects::player::RecipeList::create());
		q->f.commonData->setPlayerClass(playerClass);
		q->clearSent();
		return q;
	}

	static world::WorldMapInstance& poeta() { return *world::World::getInstance().getWorldMap(210010000)->getMainWorldMapInstance(); }

	/**
	 * The review of 2026-09-29: the quester on another map id at the same spot, in the fixture's Poeta region (the unit world holds no other
	 * map): what a hook reads through Player.getWorldId (24010's Altgard, 2900's Space of Destiny). Returns the position to restore
	 */
	static Ref<world::WorldPosition> onMap(Quester& q, int32_t mapId) {
		Ref<world::WorldPosition> old(*q.player().getPosition());
		Ref<world::WorldPosition> moved =
			world::WorldPosition::create(mapId, old->getX(), old->getY(), old->getZ(), old->getHeading(), old->getMapRegion());
		moved->setIsSpawned(true);
		q.player().setPosition(moved);
		return old;
	}

	/** An npc of the template spawned into the fixture's instance at the spot (AbstractQuestHandler.spawn), kept for the TearDown */
	Npc& spawnNpc(int32_t npcId, float x = 102.0f, float y = 100.0f, float z = 50.0f, int8_t heading = 0) {
		Ptr<VisibleObject> object = AbstractQuestHandler::spawn(npcId, poeta(), x, y, z, heading);
		EXPECT_TRUE(object) << npcId;
		spawned.emplace_back(*object);
		return *runtime::cast<Npc>(object);
	}

	/** The npcs of the template standing spawned in the fixture's instance */
	static std::vector<Ptr<Npc>> spawnedOf(int32_t npcId) { return poeta().getNpcs({npcId}); }

	/** Hands the handler of the factory to the engine (addQuestHandler calls its register_); returns it for the direct hook calls */
	AbstractQuestHandler& install(std::unique_ptr<AbstractQuestHandler> created) {
		AbstractQuestHandler& raw = *created;
		QuestEngine::getInstance().addQuestHandler(std::move(created));
		return raw;
	}

	/** QuestEngine.onDialog with a fresh env (CM_DIALOG_SELECT's way): target, quest and dialog action; the quester's target is set too */
	bool dialog(Quester& q, Ptr<VisibleObject> target, int32_t questId, int32_t dialogActionId) {
		q.player().setTarget(target);
		Ref<QuestEnv> env = QuestEnv::create(target, q.player(), questId, dialogActionId);
		envs.push_back(env);
		return QuestEngine::getInstance().onDialog(*env);
	}

	/** A fresh env of the quester, kept until the TearDown (a scheduled quest task pins its env) */
	QuestEnv& envFor(Quester& q, Ptr<VisibleObject> target, int32_t questId, int32_t dialogActionId = 0) {
		envs.push_back(QuestEnv::create(target, q.player(), questId, dialogActionId));
		return *envs.back();
	}

	static bool sentTo(const Quester& q, const std::vector<uint8_t>& packet) {
		std::vector<std::vector<uint8_t>> bytes = q.sent();
		return std::find(bytes.begin(), bytes.end(), packet) != bytes.end();
	}

	static int32_t varOf(const Quester& q, int32_t questId) {
		Ptr<QuestState> qs = q.player().getQuestStateList()->getQuestState(questId);
		return qs ? qs->getQuestVars()->getQuestVars() : -1;
	}

	static std::optional<QuestStatus> statusOf(const Quester& q, int32_t questId) {
		Ptr<QuestState> qs = q.player().getQuestStateList()->getQuestState(questId);
		return qs ? std::optional<QuestStatus>(qs->getStatus()) : std::nullopt;
	}

	std::vector<Ref<VisibleObject>> spawned;
	std::vector<Ref<QuestEnv>> envs;
};

} // namespace aion::gameserver::questEngine::handlers::test::asmodae
