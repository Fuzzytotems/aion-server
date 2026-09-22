#pragma once

// World test support of the AI framework chunk (P5-05, m5b-plan.md A-08): the AI framework's decision tables (CreatureEventHandler::checkAggro,
// AttackManager::checkGiveupDistance) read the npc's map region, its world map instance and its spawn, which the plain AiTest fixture of
// AiTestSupport.h does not give it. This header adds one test world map with one instance, npcs placed in it from real npc templates, and the
// knownlist pairing the aggro checks need. It is written here and not taken from tests/world/WorldTestSupport.h, which belongs to P4-10.
//
// The data holders (world maps, zones, shields, materials) are published once per process, because WorldMapInstance::regionSize() and the
// ZoneService read theirs once (the note of tests/stats/CombatDamageTest.cpp, which solves the same problem for P5-01).

#include <gtest/gtest.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/PlayerClass.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "AiTestSupport.h"

namespace aion::gameserver::ai::testing {

inline constexpr int32_t POETA = 210010000;

/** the gate's monster (m5b-plan.md D11): level 2, aggro range 8, short aggro range 4, aggro angle 270 */
inline constexpr int32_t SPARKIE_NPC_ID = 210663;
/** a level-2 GUARD-tribe npc, so the guard arms of the aggro and support tables have an owner */
inline constexpr int32_t GUARD_NPC_ID = 210664;
/** a level-40 monster, so validateAggro's `level difference < 10` arm has a failing case */
inline constexpr int32_t HIGH_LEVEL_NPC_ID = 210665;
/** a level-40 GUARD-tribe npc without a GUARD template type: aggressive to a MONSTER owner, but 38 levels above it */
inline constexpr int32_t HIGH_LEVEL_GUARD_NPC_ID = 210666;
/** rating HERO, so Npc::isBoss() is true and checkGiveupDistance takes its 50 / 150 arms */
inline constexpr int32_t BOSS_NPC_ID = 210667;
/** tribe DUMMY and ai="noaction": the training-dummy arm of NoActionAI::handleAttack */
inline constexpr int32_t DUMMY_NPC_ID = 210668;
/** tribe GENERAL and ai="noaction": the same AI without the training-dummy arm */
inline constexpr int32_t NO_ACTION_NPC_ID = 210669;
/** `<talk_info is_dialog="true"/>`, so NpcTemplate::isDialogNpc() is true and TalkEventHandler::onSimpleTalk has an owner */
inline constexpr int32_t DIALOG_NPC_ID = 210670;

inline const char* const AI_WORLD_MAPS_XML = R"(<world_maps>)"
											 R"(<map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
											 R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/>)"
											 R"(</world_maps>)";

/**
 * MONSTER and GUARD are aggressive and hostile to each other (`<aggro>`/`<hostile>` are @XmlList elements), so TribeRelationService::isAggressive
 * and Npc::isEnemyFrom answer true for that pair.
 * <p>
 * MONSTER is also aggressive to MONSTER. That is what makes checkAggro's `!isFriend` term observable: two npcs of the same tribe are friends
 * (TribeRelationService.java:175), so without the term an aggressive-to-its-own-tribe npc would aggro its own kind. With the term only
 * isAggressive would decide, and a test whose pair is not aggressive at all cannot tell the two apart.
 */
inline const char* const AI_TRIBE_RELATIONS_XML =
	R"(<tribe_relations>)"
	R"(<tribe name="PC"/><tribe name="PC_DARK"/>)"
	R"(<tribe name="GENERAL"><hostile>GUARD</hostile></tribe>)"
	R"(<tribe name="DUMMY"><hostile>GUARD</hostile></tribe>)"
	R"(<tribe name="GUARD"><aggro>MONSTER</aggro><hostile>MONSTER GENERAL DUMMY</hostile></tribe>)"
	R"(<tribe name="MONSTER"><aggro>PC PC_DARK GUARD MONSTER</aggro><hostile>PC PC_DARK GUARD</hostile><support>MONSTER</support></tribe>)"
	R"(</tribe_relations>)";

inline std::string aiNpcTemplatesXml() {
	return std::string(R"(<npc_templates>)")
		+ R"(<npc_template npc_id="210663" name_id="1" level="2" name="juvenile sparkie" attack_speed="2142" tribe="MONSTER" rating="NORMAL")"
		  R"( rank="DISCIPLINED" ai="aggressive" srange="8" sangle="270" arange="2"><stats maxHp="199" maxMp="0" attack="10")"
		  R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
		+ R"(<npc_template npc_id="210664" name_id="1" level="2" name="guard" attack_speed="2000" tribe="GUARD" rating="NORMAL" rank="NOVICE")"
		  R"( ai="general" srange="8" sangle="270" arange="2" type="GUARD"><stats maxHp="199" maxMp="0" attack="10")"
		  R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
		+ R"(<npc_template npc_id="210665" name_id="1" level="40" name="elder sparkie" attack_speed="2142" tribe="MONSTER" rating="NORMAL")"
		  R"( rank="DISCIPLINED" ai="aggressive" srange="8" sangle="270" arange="2"><stats maxHp="199" maxMp="0" attack="10")"
		  R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
		+ R"(<npc_template npc_id="210666" name_id="1" level="40" name="elder guard" attack_speed="2000" tribe="GUARD" rating="NORMAL")"
		  R"( rank="NOVICE" ai="general" srange="8" sangle="270" arange="2"><stats maxHp="199" maxMp="0" attack="10")"
		  R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
		+ R"(<npc_template npc_id="210667" name_id="1" level="2" name="sparkie chief" attack_speed="2142" tribe="MONSTER" rating="HERO")"
		  R"( rank="DISCIPLINED" ai="aggressive" srange="8" sangle="270" arange="2"><stats maxHp="199" maxMp="0" attack="10")"
		  R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
		+ R"(<npc_template npc_id="210668" name_id="1" level="2" name="training dummy" attack_speed="2000" tribe="DUMMY" rating="NORMAL")"
		  R"( rank="NOVICE" ai="noaction" srange="8" sangle="270" arange="2"><stats maxHp="199" maxMp="0" attack="10")"
		  R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
		+ R"(<npc_template npc_id="210669" name_id="1" level="2" name="statue" attack_speed="2000" tribe="GENERAL" rating="NORMAL")"
		  R"( rank="NOVICE" ai="noaction" srange="8" sangle="270" arange="2"><stats maxHp="199" maxMp="0" attack="10")"
		  R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)"
		+ R"(<npc_template npc_id="210670" name_id="1" level="2" name="villager" attack_speed="2000" tribe="GENERAL" rating="NORMAL")"
		  R"( rank="NOVICE" ai="general" srange="8" sangle="270" arange="2"><stats maxHp="199" maxMp="0" attack="10")"
		  R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats><talk_info is_dialog="true"/></npc_template>)"
		+ R"(</npc_templates>)";
}

/** GeneralInstanceHandler's onAggro, onCreatureDetected and onBackHome are empty inline bodies; only the create() is missing. */
class AiWorldInstanceHandler final : public ::aion::gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit AiWorldInstanceHandler(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}

	static runtime::Ref<AiWorldInstanceHandler> create(world::WorldMapInstance& instance) {
		return runtime::makeRef<AiWorldInstanceHandler>(instance);
	}

protected:
	~AiWorldInstanceHandler() override = default;
};

/** Exposes the protected static KnownList::addPair, so two npcs know each other without the World singleton. */
struct AiKnownListPairing : world::knownlist::KnownList {
	static bool pair(model::gameobjects::VisibleObject& a, model::gameobjects::VisibleObject& b) { return addPair(a, b); }
};

/** A spawn template at the coordinates the test chooses (the spawn point an npc returns to) */
class AiWorldSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	AiWorldSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

inline void publishAiMapStaticDataOnce() {
	static const bool published = [] {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		AI_TEST_SCOPE;
		static std::deque<xml::LoadContext> contexts;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(
			xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), AI_WORLD_MAPS_XML));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(
			xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		return true;
	}();
	static_cast<void>(published);
}

/** AiTest plus one Poeta map instance, npc templates from NPC_DATA and npcs placed in the map. */
class AiWorldTest : public AiTest {
protected:
	void SetUp() override {
		AiTest::SetUp();
		// the templates carry their real ai names and this executable links the empty AI registry, so every npc starts with the warn-mode
		// substitute of A-00 (AIEngine::DummyNpcAI); the cases that need a real AI replace it with their own leaf.
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		publishAiMapStaticDataOnce();
		// the unit tests never load geo data; with gameserver.geodata.cansee.enable off GeoService::canSee answers true (GeoService.cpp:118-119)
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
		// Player::postConstruct loads the toy pets from the database; the tests have none (PetList::setPlayerPetsLoaderForTests)
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(contexts.emplace_back(), aiNpcTemplatesXml()));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(contexts.emplace_back(), AI_TRIBE_RELATIONS_XML));
		// AiTest installs one too, but keeps no pointer; the AI cases have to drive the scheduled tasks (the 500 ms AggroNotifier, the attack
		// interval) by hand, so this fixture installs its own and keeps it. Installing retires the previous one, which has nothing pending yet.
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 5);
		executor = backend.get();
		utils::ThreadPoolManager::installBackend(std::move(backend));
		AI_TEST_SCOPE;
		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(POETA));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(AiWorldInstanceHandler::create(instance));
		});
	}

	void TearDown() override {
		executor = nullptr;
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		npcs.clear();
		players.clear();
		commonDatas.clear();
		appearances.clear();
		accounts.clear();
		mapInstance = nullptr;
		map = nullptr;
		spawnGroups.clear();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		AiTest::TearDown();
	}

	model::templates::spawns::SpawnTemplate& makeSpawn(int32_t npcId, float x, float y, float z) {
		runtime::Ref<model::templates::spawns::SpawnGroup> spawnGroup = model::templates::spawns::SpawnGroup::create(POETA, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& value =
			spawnGroup->addSpawnTemplate(std::make_unique<AiWorldSpawnTemplate>(*spawnGroup, x, y, z));
		spawnGroups.push_back(spawnGroup);
		return value;
	}

	/** places the object in the test map instance, so getPosition()->getWorldMapInstance() answers (Java: World.setPosition) */
	void place(model::gameobjects::VisibleObject& object, float x, float y, float z) {
		object.setPosition(world::WorldPosition::create(POETA, x, y, z, int8_t{0}, mapInstance->getRegion(x, y, z)));
		object.getPosition()->setIsSpawned(true); // Java: World.spawn sets the flag
	}

	/**
	 * An npc of the given template id, spawned at its spawn point, with the knownlist and effect controller a real spawn gives it. It is a
	 * plain Npc (not AiTestSupport's AiTestNpc), so setupStatContainers builds the real NpcGameStats and NpcLifeStats that NpcAI's narrowing
	 * accessors, the movement speed and the aggro checks read.
	 */
	runtime::Ref<model::gameobjects::Npc> makeWorldNpc(int32_t npcId, float x = 500, float y = 500, float z = 100) {
		runtime::Ref<model::gameobjects::Npc> npc =
			model::gameobjects::VisibleObject::create<model::gameobjects::Npc>(std::make_unique<controllers::NpcController>(),
				makeSpawn(npcId, x, y, z), dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, x, y, z);
		npcs.push_back(npc);
		return npc;
	}

	/**
	 * A Player with the parts VisibleObject::create needs, following the fixture of tests/skills/CriticalProcEffectTest.cpp. The AI tests use
	 * it for two things Java gives no other way to: a map region only becomes active when a Player is added to it (MapRegion.java:89-91), and
	 * an aggro target of the race an npc really aggroes.
	 */
	runtime::Ref<model::gameobjects::player::Player> makeWorldPlayer(int32_t objectId, float x = 500, float y = 500, float z = 100) {
		runtime::Ref<model::account::Account> account = model::account::Account::create(9000 + objectId);
		runtime::Ref<model::gameobjects::player::PlayerCommonData> commonData =
			model::gameobjects::player::PlayerCommonData::create(objectId);
		commonData->setName("AiTest" + std::to_string(objectId));
		commonData->setRace(model::Race::ELYOS);
		commonData->setPlayerClass(model::PlayerClass::WARRIOR);
		runtime::Ref<model::gameobjects::player::PlayerAppearance> appearance = model::gameobjects::player::PlayerAppearance::create();
		account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*account, *commonData, *appearance));
		account->setAccountWarehouse(
			std::make_unique<model::items::storage::PlayerStorage>(*account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		runtime::Ref<model::gameobjects::player::Player> player = model::gameobjects::VisibleObject::create<model::gameobjects::player::Player>(
			*account->getPlayerAccountData(objectId), *account);
		player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*player));
		player->setSkillList(model::skill::PlayerSkillList::create());
		place(*player, x, y, z);
		accounts.push_back(account);
		commonDatas.push_back(commonData);
		appearances.push_back(appearance);
		players.push_back(player);
		return player;
	}

	/**
	 * Makes the two objects know each other, which Java's knownlist update does when they come into range. AggroList::isAware refuses hate for
	 * a creature the owner does not know (AggroList.java:200), so every case that adds hate pairs its objects first.
	 */
	void know(model::gameobjects::VisibleObject& a, model::gameobjects::VisibleObject& b) { AiKnownListPairing::pair(a, b); }

	/**
	 * Makes the map region around (x, y, z) active, which Java only does when a player enters it (MapRegion.java:89-91, activate() is private).
	 * checkAggro returns early while the owner's region is inactive (CreatureEventHandler.java:82-83).
	 */
	runtime::Ref<model::gameobjects::player::Player> activateRegionAt(float x, float y, float z) {
		runtime::Ref<model::gameobjects::player::Player> activator = makeWorldPlayer(nextActivatorId--, x, y, z);
		mapInstance->getRegion(x, y, z)->add(*activator);
		return activator;
	}

	runtime::DeterministicExecutor* executor = nullptr;
	runtime::Ref<world::WorldMap> map;
	runtime::Ref<world::WorldMapInstance> mapInstance;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<model::gameobjects::Npc>> npcs;
	std::vector<runtime::Ref<model::gameobjects::player::Player>> players;
	std::vector<runtime::Ref<model::account::Account>> accounts;
	std::vector<runtime::Ref<model::gameobjects::player::PlayerCommonData>> commonDatas;
	std::vector<runtime::Ref<model::gameobjects::player::PlayerAppearance>> appearances;
	std::deque<xml::LoadContext> contexts;
	int32_t nextActivatorId = 899999;
};

} // namespace aion::gameserver::ai::testing
