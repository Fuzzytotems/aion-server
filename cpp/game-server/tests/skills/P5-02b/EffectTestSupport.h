#pragma once

// Test support of the effect engine (P5-02b, m5b2-plan.md K-07): a Poeta map instance with a real instance handler, spawned players and npcs
// placed in it (Effect.startEffect and endEffect end with `effected.getPosition().getWorldMapInstance().getInstanceHandler()`), a
// DeterministicExecutor on a ManualClock for the end and periodic tasks, and ProbeEffect - an EffectTemplate that records what the Effect
// asks of it.
//
// **Why probes and not real effect classes.** Almost every leaf effect class still has AION_UNPORTED calculate/start/end bodies (m5b2-plan.md
// part 3), so a skill bound from skill_templates.xml would stop in the leaf before Effect's own logic could be observed. A ProbeEffect answers
// EffectTemplate's own bodies (or a scripted success/failure) and records every call in a journal, which is what lets the cases assert Java's
// statement order - the order Effect calls its templates in, and the order of the random draws. The probes are injected into a bound
// <skill_template> through the explicit-instantiation access rule (PrivateAccess below, as tests/effects_al/EffectTemplateTest.cpp does), so no
// frozen header needs a test friend.
//
// Everything is in an unnamed namespace: each test translation unit gets its own PrivateAccess instantiations (an explicit instantiation may
// appear once per program).

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/NpcSkillData.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
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
#include "aion/gameserver/model/SkillElement.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/NpcGameStats.h"
#include "aion/gameserver/model/stats/container/NpcLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Reclaimer.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/runtime/sched/Clock.h"
#include "aion/gameserver/runtime/sched/DeterministicExecutor.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/EffectType.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.bind.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::skillengine::effecttest {
namespace {

using gameserver::model::gameobjects::Creature;
using gameserver::model::gameobjects::Npc;
using gameserver::model::gameobjects::player::Player;
using runtime::Ptr;
using runtime::Ref;
namespace Rnd = commons::utils::Rnd;

#define EFFECT_TEST_SCOPE runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST))

// ---- access ---------------------------------------------------------------------------------------------------------------------------------

/** The names in an explicit instantiation are not access-checked ([temp.spec.general]/6): the friend hands the member pointer out. */
template <class Tag, typename Tag::Type Member>
struct PrivateAccess {
	friend typename Tag::Type privateMember(Tag) { return Member; }
};

struct SkillEffectsTag {
	using Type = std::unique_ptr<effect::Effects> model::SkillTemplate::*;
	friend Type privateMember(SkillEffectsTag);
};
template struct PrivateAccess<SkillEffectsTag, &model::SkillTemplate::effects>;

struct EffectListTag {
	using Type = std::vector<std::unique_ptr<effect::EffectTemplate>> effect::Effects::*;
	friend Type privateMember(EffectListTag);
};
template struct PrivateAccess<EffectListTag, &effect::Effects::effects>;

struct EffectTypesTag {
	using Type = std::set<effect::EffectType> effect::Effects::*;
	friend Type privateMember(EffectTypesTag);
};
template struct PrivateAccess<EffectTypesTag, &effect::Effects::effectTypes>;

struct ConflictTypesTag {
	using Type = std::set<effect::EffectType> effect::Effects::*;
	friend Type privateMember(ConflictTypesTag);
};
template struct PrivateAccess<ConflictTypesTag, &effect::Effects::possibleConflictEffectTypes>;

// ---- probes ---------------------------------------------------------------------------------------------------------------------------------

/** The calls Effect makes on its templates, in order: "<name>.<call>" per line */
using Journal = std::vector<std::string>;

/**
 * An EffectTemplate whose behaviour hooks record into a journal. calculate() either runs EffectTemplate's own body (REAL), adds the template to
 * the success effects (SUCCEED) or does nothing (FAIL). The protected JAXB fields are public so a case can configure positions, durations,
 * effect ids, basic levels, elements and hate.
 */
class ProbeEffect : public effect::EffectTemplate {
public:
	enum class Calculate { REAL, SUCCEED, FAIL };

	ProbeEffect(std::string probeName, Journal* probeJournal, Calculate calculateMode = Calculate::SUCCEED)
		: name(std::move(probeName)), journal(probeJournal), mode(calculateMode) {}

	std::string_view javaClassName() const override { return "ProbeEffect"; }

	using EffectTemplate::calculate;
	void calculate(model::Effect& effect) const override {
		record("calculate");
		switch (mode) {
			case Calculate::REAL:
				EffectTemplate::calculate(effect);
				break;
			case Calculate::SUCCEED:
				effect.addSuccessEffect(this);
				break;
			case Calculate::FAIL:
				break;
		}
	}

	void applyEffect(model::Effect& effect) const override {
		record("apply");
		if (onApply)
			onApply(effect);
	}

	void startEffect(model::Effect& effect) const override {
		record("start");
		if (onStart)
			onStart(effect);
	}

	void endEffect(model::Effect& effect) const override {
		record("end");
		if (onEnd)
			onEnd(effect);
	}

	void record(std::string_view call) const {
		if (journal != nullptr)
			journal->push_back(name + "." + std::string(call));
	}

	using EffectTemplate::basicLvl;
	using EffectTemplate::change;
	using EffectTemplate::duration1;
	using EffectTemplate::duration2;
	using EffectTemplate::effectid;
	using EffectTemplate::element;
	using EffectTemplate::hopA;
	using EffectTemplate::hopB;
	using EffectTemplate::hopType;
	using EffectTemplate::noResist;
	using EffectTemplate::position;
	using EffectTemplate::randomTime;
	using EffectTemplate::subEffect;

	const std::string name;
	Journal* const journal;
	Calculate mode;
	std::function<void(model::Effect&)> onApply;
	std::function<void(model::Effect&)> onStart;
	std::function<void(model::Effect&)> onEnd;
};

/**
 * Binds a <skill_template> (no <effects> element) and gives it `probes` as its effect list, in the order given (Java keeps the XML order), with
 * the effect types `effectTypes` (Effects.effectTypes, which hasAnyEffect reads) and `conflictTypes` (possibleConflictEffectTypes). The skill
 * templates are immortal static data in the server; the caller keeps the returned owner for the test.
 */
inline std::unique_ptr<model::SkillTemplate> skillWithProbes(const std::string& skillXml, std::vector<std::unique_ptr<effect::EffectTemplate>> probes,
	std::set<effect::EffectType> effectTypes = {}, std::set<effect::EffectType> conflictTypes = {}) {
	xml::LoadContext context;
	std::unique_ptr<model::SkillTemplate> skill = xml::bindString<model::SkillTemplate>(context, skillXml);
	auto effects = std::make_unique<effect::Effects>();
	(*effects).*privateMember(EffectListTag{}) = std::move(probes);
	(*effects).*privateMember(EffectTypesTag{}) = std::move(effectTypes);
	(*effects).*privateMember(ConflictTypesTag{}) = std::move(conflictTypes);
	(*skill).*privateMember(SkillEffectsTag{}) = std::move(effects);
	return skill;
}

/** Gives an already published (SKILL_DATA) template the probes: test-only mutation of static data that TearDown forgets with the holder */
inline void injectProbes(const model::SkillTemplate* skill, std::vector<std::unique_ptr<effect::EffectTemplate>> probes,
	std::set<effect::EffectType> effectTypes = {}) {
	auto effects = std::make_unique<effect::Effects>();
	(*effects).*privateMember(EffectListTag{}) = std::move(probes);
	(*effects).*privateMember(EffectTypesTag{}) = std::move(effectTypes);
	(*const_cast<model::SkillTemplate*>(skill)).*privateMember(SkillEffectsTag{}) = std::move(effects);
}

/** A one-template probe list */
inline std::vector<std::unique_ptr<effect::EffectTemplate>> probeList(std::unique_ptr<ProbeEffect> probe) {
	std::vector<std::unique_ptr<effect::EffectTemplate>> list;
	list.push_back(std::move(probe));
	return list;
}

inline std::unique_ptr<ProbeEffect> probe(std::string name, Journal* journal, int32_t position,
	ProbeEffect::Calculate mode = ProbeEffect::Calculate::SUCCEED) {
	auto result = std::make_unique<ProbeEffect>(std::move(name), journal, mode);
	result->position = position;
	return result;
}

/** A <skill_template> element with the attributes a case needs; `extra` is appended verbatim (e.g. `tslot="BUFF" pvp_duration="50"`) */
inline std::string skillXml(int32_t skillId, std::string_view stack, std::string_view extra) {
	return R"(<skill_template skill_id=")" + std::to_string(skillId) + R"(" name="probe )" + std::to_string(skillId) + R"(" nameId="1" stack=")"
		+ std::string(stack) + R"(" lvl="1" skilltype="MAGICAL" skillsubtype="BUFF" activation="ACTIVE" duration="0" )" + std::string(extra) + "/>";
}

/** The values Rnd.chance() gives after seeding the calling thread with `seed` (leaves the thread seeded and advanced) */
inline std::vector<float> chanceStream(uint64_t seed, int count) {
	Rnd::seedCurrentThreadForTests(seed);
	std::vector<float> stream;
	for (int i = 0; i < count; ++i)
		stream.push_back(Rnd::chance());
	return stream;
}

inline std::vector<Ref<gameserver::model::gameobjects::player::PetCommonData>> noPets(Player&) {
	return {};
}

// ---- world ----------------------------------------------------------------------------------------------------------------------------------

inline constexpr int32_t POETA = 210010000;

/**
 * The map data WorldMapInstance and the region lookup read once per process (tests/ai/AiWorldTestSupport.h, tests/stats/CombatDamageTest.cpp).
 * Published only when absent, so another fixture of this executable that published them first is not disturbed.
 */
inline void publishWorldStaticDataOnce() {
	static const bool published = [] {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		EFFECT_TEST_SCOPE;
		static std::deque<xml::LoadContext> contexts;
		if (!dataholders::DataManager::WORLD_MAPS_DATA)
			dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(),
				R"(<world_maps><map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
				R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/></world_maps>)"));
		if (!dataholders::DataManager::ZONE_DATA)
			dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		if (!dataholders::DataManager::SHIELD_DATA)
			dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		if (!dataholders::DataManager::MATERIAL_DATA)
			dataholders::DataManager::MATERIAL_DATA.publish(
				xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		return true;
	}();
	static_cast<void>(published);
}

/** A spawn template at the coordinates the test chooses */
class EffectTestSpawnTemplate final : public gameserver::model::templates::spawns::SpawnTemplate {
public:
	EffectTestSpawnTemplate(gameserver::model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/**
 * The two-sided known-list insert of the server's pair sites (KnownList::addPair is protected), which AggroList.isAware and the broadcasts read
 * (the pattern of tests/controllers/AttackSeamTest.cpp)
 */
struct KnownListPairing : world::knownlist::KnownList {
	static bool pair(gameserver::model::gameobjects::VisibleObject& a, gameserver::model::gameobjects::VisibleObject& b) { return addPair(a, b); }
};

/** tribe_relations.xml, reduced to what AggroList.isAware asks: a MONSTER npc is hostile to the two player tribes */
inline constexpr std::string_view EFFECT_TEST_TRIBE_RELATIONS_XML =
	R"(<tribe_relations><tribe name="PC"/><tribe name="PC_DARK"/><tribe name="MONSTER"><hostile>PC</hostile><hostile>PC_DARK</hostile></tribe>)"
	R"(</tribe_relations>)";

/** Npc templates are immortal static data: kept for the process like the DataManager holder keeps them. */
inline const gameserver::model::templates::npc::NpcTemplate* npcTemplate(int32_t npcId, std::string_view race = {},
	std::string_view tribe = "GENERAL") {
	xml::LoadContext context;
	const std::string raceAttribute = race.empty() ? std::string() : R"( race=")" + std::string(race) + R"(")";
	return xml::bindString<gameserver::model::templates::npc::NpcTemplate>(context,
		R"(<npc_template name_id="1" npc_id=")" + std::to_string(npcId) + R"(" level="4" name="Effect test" attack_speed="2000" arange="2")"
			R"( rating="NORMAL" tribe=")" + std::string(tribe) + R"(")" + raceAttribute + R"(>)"
			R"(<stats maxHp="2522" maxMp="100" pdef="130" mdef="70" attack="16" evasion="45" parry="30" block="25" accuracy="200" macc="60")"
			R"( pcrit="10" mcrit="20"><speeds walk="0.8" run="2.0" run_fight="3.0" group_walk="0.5" group_run_fight="2.5" fly="4.0"/></stats>)"
			R"(</npc_template>)")
		.release();
}

/**
 * One Poeta map instance with the real GeneralInstanceHandler, a DeterministicExecutor on a ManualClock, and players and npcs placed in the
 * map (spawned, with the known list and effect controller their real spawn gives them).
 */
class EffectWorldTest : public ::testing::Test {
protected:
	void SetUp() override {
		utils::ThreadPoolManager::installBackend(nullptr);
		auto backend = std::make_unique<runtime::DeterministicExecutor>(clock, 29);
		executor = backend.get();
		utils::ThreadPoolManager::installBackend(std::move(backend));
		utils::idfactory::IDFactory::getInstance().resetForTests();
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
		savedRnd.emplace(Rnd::generator());
		publishWorldStaticDataOnce();
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
		// Npc's constructor builds its NpcSkillList from NPC_SKILL_DATA (none of these npcs has skills)
		dataholders::DataManager::NPC_SKILL_DATA.publish(std::make_unique<dataholders::NpcSkillData>());
		EFFECT_TEST_SCOPE;
		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(POETA));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return Ref<instance::handlers::InstanceHandler>(instance::handlers::GeneralInstanceHandler::create(instance));
		});
	}

	void TearDown() override {
		Rnd::generator() = *savedRnd;
		gameserver::model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		npcs.clear();
		players.clear();
		commonDatas.clear();
		appearances.clear();
		accounts.clear();
		spawnGroups.clear();
		mapInstance = nullptr;
		map = nullptr;
		if (skillDataPublished)
			dataholders::DataManager::SKILL_DATA.resetForTests();
		if (tribeRelationsPublished)
			dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::NPC_SKILL_DATA.resetForTests();
		runtime::Reclaimer::getInstance().drain();
		executor = nullptr;
		utils::ThreadPoolManager::installBackend(nullptr);
		runtime::Reclaimer::getInstance().drain();
	}

	/** places the object in the test map instance and marks it spawned (Java: World.setPosition + World.spawn) */
	void place(gameserver::model::gameobjects::VisibleObject& object, float x, float y, float z) {
		object.setPosition(world::WorldPosition::create(POETA, x, y, z, int8_t{0}, mapInstance->getRegion(x, y, z)));
		object.getPosition()->setIsSpawned(true);
	}

	/** A spawned Elyos player of the given class (Java PlayerService.getPlayer + World.spawn) with its skill list and effect controller */
	Ref<Player> makePlayer(int32_t objectId, gameserver::model::PlayerClass playerClass = gameserver::model::PlayerClass::MAGE,
		gameserver::model::Race race = gameserver::model::Race::ELYOS, float x = 500, float y = 500, float z = 100) {
		namespace m = gameserver::model;
		Ref<m::account::Account> account = m::account::Account::create(9000 + objectId);
		Ref<m::gameobjects::player::PlayerCommonData> commonData = m::gameobjects::player::PlayerCommonData::create(objectId);
		commonData->setName("Effect" + std::to_string(objectId));
		commonData->setRace(race);
		commonData->setPlayerClass(playerClass);
		Ref<m::gameobjects::player::PlayerAppearance> appearance = m::gameobjects::player::PlayerAppearance::create();
		account->addPlayerAccountData(std::make_unique<m::account::PlayerAccountData>(*account, *commonData, *appearance));
		account->setAccountWarehouse(std::make_unique<m::items::storage::PlayerStorage>(*account, m::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		Ref<Player> player = m::gameobjects::VisibleObject::create<Player>(*account->getPlayerAccountData(objectId), *account);
		player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*player));
		player->setSkillList(m::skill::PlayerSkillList::create());
		player->setEffectController(std::make_unique<controllers::effect::PlayerEffectController>(*player));
		place(*player, x, y, z);
		accounts.push_back(account);
		commonDatas.push_back(commonData);
		appearances.push_back(appearance);
		players.push_back(player);
		return player;
	}

	/** A spawned npc (Java VisibleObjectSpawner.spawnNpc: the known list and the plain EffectController); no ai attribute: Java's DummyAI */
	Ref<Npc> makeNpc(int32_t npcId, std::string_view race = {}, float x = 505, float y = 500, float z = 100, std::string_view tribe = "GENERAL") {
		Ref<gameserver::model::templates::spawns::SpawnGroup> group = gameserver::model::templates::spawns::SpawnGroup::create(POETA, npcId, 0, nullptr);
		gameserver::model::templates::spawns::SpawnTemplate& spawnTemplate =
			group->addSpawnTemplate(std::make_unique<EffectTestSpawnTemplate>(*group, x, y, z));
		Ref<Npc> npc = gameserver::model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawnTemplate,
			npcTemplate(npcId, race, tribe));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		place(*npc, x, y, z);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return npc;
	}

	/** Publishes DataManager::SKILL_DATA (the lookups of EffectController.findBySkillId and addSavedEffect); TearDown forgets it again */
	void publishSkillData(const std::string& templatesXml) {
		dataholders::DataManager::SKILL_DATA.publish(
			xml::bindString<dataholders::SkillData>(skillDataContext, "<skill_data>" + templatesXml + "</skill_data>"));
		skillDataPublished = true;
	}

	/** Publishes DataManager::TRIBE_RELATIONS_DATA (EFFECT_TEST_TRIBE_RELATIONS_XML) for AggroList.isAware; TearDown forgets it again */
	void publishTribeRelations() {
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(tribeRelationsContext, std::string(EFFECT_TEST_TRIBE_RELATIONS_XML)));
		tribeRelationsPublished = true;
	}

	/** Keeps a bound skill template for the test and answers it */
	const model::SkillTemplate* keep(std::unique_ptr<model::SkillTemplate> skill) {
		skills.push_back(std::move(skill));
		return skills.back().get();
	}

	/** Advances the manual clock and runs every task that became due */
	void advance(int64_t millis) { executor->advance(std::chrono::milliseconds(millis)); }

	runtime::ManualClock clock{};
	runtime::DeterministicExecutor* executor = nullptr;
	std::optional<Rnd::Xoshiro256PlusPlus> savedRnd;
	Ref<world::WorldMap> map;
	Ref<world::WorldMapInstance> mapInstance;
	std::vector<Ref<gameserver::model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<Ref<Npc>> npcs;
	std::vector<Ref<Player>> players;
	std::vector<Ref<gameserver::model::account::Account>> accounts;
	std::vector<Ref<gameserver::model::gameobjects::player::PlayerCommonData>> commonDatas;
	std::vector<Ref<gameserver::model::gameobjects::player::PlayerAppearance>> appearances;
	std::vector<std::unique_ptr<model::SkillTemplate>> skills;
	xml::LoadContext skillDataContext;
	bool skillDataPublished = false;
	xml::LoadContext tribeRelationsContext;
	bool tribeRelationsPublished = false;
};

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
