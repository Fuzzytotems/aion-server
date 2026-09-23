// m5b-1 E-01b, stage 1 (m5b-plan.md §4, P4-11b): the arithmetic half of the ControllerStandIns deletion, the companion of AiSeamTest.cpp's
// AI half. Five stand-ins are gone and the controller bodies call the ported classes the way Java writes them:
//
//   CreatureController.java:323  AttackUtil.calculatePhysAttackResult(getOwner(), target, calculationTypes)   (B-01)
//   NpcController.java:219-221   StatFunctions.calculateExperienceReward / calculateDPReward                  (B-03)
//   NpcController.java:236       StatFunctions.calculatePvEApGained                                           (B-03)
//   PlayerController.java:397    PlayerRestrictions.canAttack(getOwner(), target)                             (C-01)
//
// The plan scheduled these for stage 2 because "none of these is reachable before its owner lands". A-06 registered AggressiveNpcAI in the same
// stage, so a monster now aggroes and swings, and the first of them became live inside stage 1 - it is what turned gs.scenario.m5a red.
//
// Every case below reaches a call site that was an `AION_UNPORTED()` stand-in until this item, so each one fails with an UnportedException if a
// stand-in comes back. Each also asserts what the real callee answered - the damage the seeded AttackUtil hands back hit for hit, the experience
// and DP the reward chain awards, the AP the abyss arm computes before it reaches its own unported service, and the no PlayerRestrictions gives
// before anything else in attackTarget runs - so a call site emptied, or handed the wrong pair of creatures, fails too.
//
// The npcs follow AiSeamTest: a real Npc through VisibleObject::create with the parts VisibleObjectSpawner gives a spawned one (EffectController,
// NpcKnownList; VisibleObjectSpawner.java:60-61), and a real NpcAI leaf installed with replaceAi, because this executable registers no AI handler.
// The attack case deliberately leaves the two npcs out of each other's known lists, so AggroList.addDamage returns at its isAware check
// (AggroList.java:38) and the case stays about the damage arithmetic; the reward cases pair the npc with the character, because the aggro list is
// what doReward reads.
//
// One test double stands in for another chunk: SeamInstanceHandler answers getExpMultiplier, which is AION_UNPORTED in P5-13 and which
// StatFunctions.calculateExperienceReward calls - so the reward seam cannot complete on a server today, only in this test. That body is the one
// open blocker of the gate's R1.

#include "ControllersTestSupport.h"

#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/ai/NpcAI.h"
#include "aion/gameserver/ai/poll/AIQuestion.h"
#include "aion/gameserver/configs/main/GeoDataConfig.h"
#include "aion/gameserver/configs/main/RatesConfig.h"
#include "aion/gameserver/configs/main/WorldConfig.h"
#include "aion/gameserver/controllers/CreatureController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/attack/AttackResult.h"
#include "aion/gameserver/controllers/attack/AttackStatus.h"
#include "aion/gameserver/controllers/attack/AttackUtil.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/MaterialData.bind.h"
#include "aion/gameserver/dataholders/MaterialData.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/ShieldData.bind.h"
#include "aion/gameserver/dataholders/ShieldData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/dataholders/WorldMapsData.bind.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/ZoneData.bind.h"
#include "aion/gameserver/dataholders/ZoneData.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/PlayerClassInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/Persistable.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/Equipment.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/model/skill/PlayerSkillEntry.h"
#include "aion/gameserver/model/skill/PlayerSkillList.h"
#include "aion/gameserver/model/stats/container/CreatureLifeStats.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/templates/item/ItemAttackType.h"
#include "aion/gameserver/model/templates/item/ItemAttackTypeInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.bind.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.bind.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/HopType.h"
#include "aion/gameserver/utils/stats/CalculationType.h"
#include "aion/gameserver/utils/stats/StatFunctions.h"
#include "aion/gameserver/world/MapRegion.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

namespace aion::gameserver::controllers::testing {
namespace {

using attack::AttackResult;
using attack::AttackStatus;
using attack::AttackUtil;
using model::gameobjects::Npc;
using runtime::Ptr;
using runtime::Ref;
using utils::stats::CalculationType;

/** The calculation types CreatureController.attackTarget builds for an attacker without a dual weapon (CreatureController.java:319-321) */
const std::unordered_set<CalculationType>& meleeCalculationTypes() {
	static const std::unordered_set<CalculationType> types{CalculationType::APPLY_POWER_SHARD_DAMAGE, CalculationType::REMOVE_POWER_SHARD};
	return types;
}

/**
 * A real NpcAI leaf that answers every question false, as AITemplate<Npc> and AIEngine's DummyNpcAI do (AiSeamTest's ProbeNpcAI), except for the
 * questions a case opts into. It adds no hook: NpcAI inherits AITemplate's no-op handleAttack, so the onAddHate of a test that seeds the aggro
 * list starts no attack manager.
 */
class SeamNpcAI final : public ai::NpcAI {
public:
	std::unordered_set<ai::poll::AIQuestion> answeredTrue;
	/**
	 * Java AI.modifyAttackType, which Npc.getAttackType() calls with PHYSICAL (Npc.java:124). AbstractAI returns the argument, so an ordinary npc
	 * is physical; the world and instance handlers that answer something else are the npc half of CreatureController.attackTarget's magical arm.
	 */
	std::optional<model::templates::item::ItemAttackType> attackType;

	explicit SeamNpcAI(Npc& owner) : NpcAI(owner) {}

	bool ask(ai::poll::AIQuestion question) override { return answeredTrue.contains(question); }

	model::templates::item::ItemAttackType modifyAttackType(model::templates::item::ItemAttackType type) override {
		return attackType.value_or(type);
	}
};

/**
 * The open-world answer of GeneralInstanceHandler.getExpMultiplier (GeneralInstanceHandler.java:248-251), which is AION_UNPORTED in the port
 * (P5-13, `GeneralInstanceHandler.cpp:99-101`) and is the one value StatFunctions.calculateExperienceReward reads from outside this chunk. The
 * same double as tests/stats/CombatDamageTest.cpp's OpenWorldInstanceHandler, and the same reason.
 */
class SeamInstanceHandler final : public instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit SeamInstanceHandler(world::WorldMapInstance& instanceValue) : GeneralInstanceHandler(instanceValue) {}

	static Ref<SeamInstanceHandler> create(world::WorldMapInstance& instanceValue) { return runtime::makeRef<SeamInstanceHandler>(instanceValue); }

	float getExpMultiplier() override { return 1.25f; }

protected:
	~SeamInstanceHandler() override = default;
};

/** Exposes the protected KnownList::addPair, so two objects know each other without the World singleton (CombatDamageTest's KnownListPairing) */
struct KnownListPairing : world::knownlist::KnownList {
	static bool pair(model::gameobjects::VisibleObject& a, model::gameobjects::VisibleObject& b) { return addPair(a, b); }
};

/** world_maps.xml of Poeta: an open world map, so WorldMap.isInstanceType() is false and getExpMultiplier's open-world arm is the right one */
const char* const WORLD_MAPS_XML = R"(<world_maps>)"
								   R"(<map id="210010000" cName="LF1" name="Poeta" name_id="1" water_level="16" death_level="0")"
								   R"( world_type="ELYSEA" world_size="1024" flags="FLY GLIDE RECALL"/>)"
								   R"(</world_maps>)";

/** tribe_relations.xml, reduced to what AggroList.isAware asks: a MONSTER npc is hostile to the two player tribes */
const char* const TRIBE_RELATIONS_XML = R"(<tribe_relations>)"
										R"(<tribe name="PC"/><tribe name="PC_DARK"/>)"
										R"(<tribe name="MONSTER"><hostile>PC</hostile><hostile>PC_DARK</hostile></tribe>)"
										R"(</tribe_relations>)";

/** The first 16 levels of player_experience_table.xml, as tests/cm_ak/InWorldPacketRunSupport.h spells them (PlayerCommonData.setExp reads it) */
const char* const EXPERIENCE_TABLE_XML =
	"<player_experience_table><exp>0</exp><exp>400</exp><exp>1433</exp><exp>3820</exp><exp>9054</exp>"
	"<exp>17655</exp><exp>30978</exp><exp>52010</exp><exp>82982</exp><exp>126069</exp><exp>182252</exp><exp>260622</exp><exp>360825</exp>"
	"<exp>490331</exp><exp>649169</exp><exp>844378</exp></player_experience_table>";

/**
 * The holders a map region and the reward chain read. ZoneService and WorldMapInstance::regionSize() read theirs once per process, so these are
 * published once and never reset (the pattern of tests/stats/CombatDamageTest.cpp, whose support belongs to another chunk).
 */
void publishMapStaticDataOnce() {
	static const bool published = [] {
		configs::main::WorldConfig::WORLD_REGION_SIZE.store(128);
		// the unit tests never load geo data; with gameserver.geodata.cansee.enable off GeoService::canSee answers true (GeoService.cpp:117-119),
		// which AggroList::streamValidTargetInfo asks for every candidate target
		configs::main::GeoDataConfig::CANSEE_ENABLE.store(false);
		runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::TEST));
		static std::deque<xml::LoadContext> contexts;
		dataholders::DataManager::WORLD_MAPS_DATA.publish(xml::bindString<dataholders::WorldMapsData>(contexts.emplace_back(), WORLD_MAPS_XML));
		dataholders::DataManager::ZONE_DATA.publish(xml::bindString<dataholders::ZoneData>(contexts.emplace_back(), "<zones/>"));
		dataholders::DataManager::SHIELD_DATA.publish(xml::bindString<dataholders::ShieldData>(contexts.emplace_back(), "<shields/>"));
		dataholders::DataManager::MATERIAL_DATA.publish(
			xml::bindString<dataholders::MaterialData>(contexts.emplace_back(), "<material_templates/>"));
		return true;
	}();
	static_cast<void>(published);
}

/** PlayerService loads pets from the database; there is none here (StatsTestSupport.h uses the same hook) */
std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>> noPets(model::gameobjects::player::Player&) {
	return {};
}

/**
 * The real Player, with the real PlayerGameStats and PlayerLifeStats that Player::postConstruct installs. This chunk's older player fixture
 * (PlayerControllerTest.cpp) still puts a CreatureGameStats double in their place, from the days when the P5-01 stat calculation was missing; a
 * double is not an option here, because Player::getGameStats() narrows to PlayerGameStats and both the attack path
 * (PlayerController.java:400) and Rates.AP_PVE's apBoostRate read it through that narrowing.
 */
class SeamPlayer final : public model::gameobjects::player::Player {
	AION_MAKE_REF_FRIEND
public:
	SeamPlayer(CreateKey key, model::account::PlayerAccountData& playerAccountData, model::account::Account& account)
		: Player(key, playerAccountData, account) {}

protected:
	~SeamPlayer() override = default;
};

/** Npc templates are immortal static data, like the holder's own (ControllersTestSupport.h) */
const model::templates::npc::NpcTemplate* npcTemplateFrom(std::string_view xmlText) {
	xml::LoadContext context;
	return xml::bindString<model::templates::npc::NpcTemplate>(context, std::string(xmlText)).release();
}

class AttackSeamTest : public ControllersTest {
protected:
	void SetUp() override {
		ControllersTest::SetUp();
		publishMapStaticDataOnce();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(&noPets);
		// the M5b profile (m5b-plan.md D1); membership 0 means rate 1.0, so Rates.XP_HUNTING only applies its expNeed * 0.2f cap
		xpSoloRates = configs::main::RatesConfig::XP_SOLO_RATES.get();
		configs::main::RatesConfig::XP_SOLO_RATES.set({1.0f, 2.0f});
		xml::LoadContext context;
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(context, EXPERIENCE_TABLE_XML));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(context, TRIBE_RELATIONS_XML));

		CONTROLLERS_TEST_SCOPE;
		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(210010000));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return Ref<instance::handlers::InstanceHandler>(SeamInstanceHandler::create(instance));
		});
	}

	void TearDown() override {
		mapInstance = nullptr;
		map = nullptr;
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		if (xpSoloRates)
			configs::main::RatesConfig::XP_SOLO_RATES.set(*xpSoloRates);
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
		ControllersTest::TearDown();
	}

	/** Places the object in the test map instance, so getPosition()->getWorldMapInstance() answers (Java: World.setPosition) */
	void place(model::gameobjects::VisibleObject& object, float x, float y, float z) {
		object.setPosition(world::WorldPosition::create(210010000, x, y, z, int8_t{0}, mapInstance->getRegion(x, y, z)));
		object.getPosition()->setIsSpawned(true);
	}

	/** The npc of the controller tests plus the two parts VisibleObjectSpawner adds when it spawns one, spawned and with a real NpcAI */
	Ref<ControllersTestNpc> createFighter(const model::templates::npc::NpcTemplate* objectTemplate) {
		Ref<ControllersTestNpc> npc =
			model::gameobjects::VisibleObject::create<ControllersTestNpc>(std::make_unique<RecordingNpcController>(), *spawnTemplate, objectTemplate);
		npc->setEffectController(std::make_unique<effect::EffectController>(*npc));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->getPosition()->setIsSpawned(true);
		auto ai = std::make_unique<SeamNpcAI>(*npc);
		SeamNpcAI& installed = *ai;
		npc->replaceAi(std::move(ai));
		npcAi = &installed;
		return npc;
	}

	/** Java PlayerService.getPlayer, as PlayerControllerTest.cpp builds it */
	Ref<SeamPlayer> createPlayer(int32_t objectId, int32_t accountId, model::PlayerClass playerClass = model::PlayerClass::WARRIOR) {
		account = model::account::Account::create(accountId);
		commonData = model::gameobjects::player::PlayerCommonData::create(objectId);
		commonData->setName("Seam" + std::to_string(objectId));
		commonData->setRace(model::Race::ELYOS);
		// PlayerGameStats interns the stats template of this class and level, so both are set before create<Player>
		commonData->setPlayerClass(playerClass);
		commonData->setLevel(1);
		appearance = model::gameobjects::player::PlayerAppearance::create();
		account->addPlayerAccountData(std::make_unique<model::account::PlayerAccountData>(*account, *commonData, *appearance));
		account->setAccountWarehouse(
			std::make_unique<model::items::storage::PlayerStorage>(*account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE));
		Ref<SeamPlayer> player = model::gameobjects::VisibleObject::create<SeamPlayer>(*account->getPlayerAccountData(objectId), *account);
		player->setPosition(world::WorldPosition::create(210010000, 10.0f, 20.0f, 30.0f, int8_t{0}));
		player->setKnownlist(std::make_unique<world::knownlist::KnownList>(*player));
		player->setEffectController(std::make_unique<effect::PlayerEffectController>(*player));
		return player;
	}

	Ref<model::account::Account> account;
	Ref<model::gameobjects::player::PlayerCommonData> commonData;
	Ref<model::gameobjects::player::PlayerAppearance> appearance;
	Ref<world::WorldMap> map;
	Ref<world::WorldMapInstance> mapInstance;
	SeamNpcAI* npcAi = nullptr;
	std::shared_ptr<const std::vector<float>> xpSoloRates;

	/** A plain physical melee npc: no ai name (so AIEngine hands out Java's own DummyAI, which replaceAi then replaces) */
	static inline const model::templates::npc::NpcTemplate* fighterTemplate = npcTemplateFrom(
		R"(<npc_template npc_id="210012" name_id="1" level="12" name="fighter" attack_speed="2000" rating="NORMAL" rank="VETERAN" tribe="MONSTER")"
		R"(><stats maxHp="199" maxMp="0" attack="200" pdef="10" evasion="20" accuracy="60" pcrit="10")"
		R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)");

	/**
	 * A second, deliberately weaker melee npc. The two templates must differ enough that a call site which swapped the attacker and the attacked
	 * computes a different damage: StatFunctions.calculateAttackDamage reads the ATTACKER's main hand power and adjustDamageByStatModifiers
	 * subtracts the ATTACKED's pdef/10, and both arms floor a landed hit at 1 - so two templates that merely differ are not enough, they have to
	 * differ away from that floor. 200 against 4 does; 16 against 4 does not (both fold to 1).
	 */
	static inline const model::templates::npc::NpcTemplate* sandbagTemplate = npcTemplateFrom(
		R"(<npc_template npc_id="210013" name_id="1" level="12" name="sandbag" attack_speed="2000" rating="NORMAL" rank="VETERAN" tribe="MONSTER")"
		R"(><stats maxHp="199" maxMp="0" attack="4" pdef="20" evasion="20" accuracy="60" pcrit="10")"
		R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)");

	/**
	 * The target of the two magical cases. Every magical stat is written out because `NpcData::loadData` computes the ones an XML leaves at 0
	 * (NpcData.cpp:64-90) - and the two that matter are chosen so that the swing has no random outcome at all: `mresist="10"` is below any
	 * attacker's magical accuracy, so `calculateMagicalStatus` never rolls a RESIST, and `spell_resist="700"` is above it, so it never rolls a
	 * CRITICAL either. A CRITICAL would matter beyond its damage: `CreatureController::attackTarget` answers one with a `Rnd.chance() < 10` draw
	 * into `SkillEngine::createCriticalProcEffect` for a **Player** attacker (CreatureController.java:341-345), and that extra draw would put the
	 * replayed AttackUtil call below out of step with the controller's run.
	 */
	static inline const model::templates::npc::NpcTemplate* magicalTargetTemplate = npcTemplateFrom(
		R"(<npc_template npc_id="210014" name_id="1" level="2" name="spell sandbag" attack_speed="2000" rating="NORMAL" rank="NOVICE")"
		R"( tribe="MONSTER"><stats maxHp="199" maxMp="0" attack="4" pdef="20" mdef="40" evasion="20" accuracy="60" pcrit="10")"
		R"( mresist="10" macc="20" mcrit="40" spell_resist="700")"
		R"(><speeds walk="0.8" run="2.0" run_fight="3.0" fly="4.0"/></stats></npc_template>)");
};

/**
 * CreatureController.attackTarget -> AttackUtil.calculatePhysAttackResult (CreatureController.java:323). An npc attacker takes the physical arm
 * unconditionally: Npc.getAttackType() is `getAi().modifyAttackType(PHYSICAL)` (Npc.java:124) and no AI M5b-1 registers overrides it.
 *
 * The assertion is an equivalence, not a bound: Rnd is seeded, the controller's attack is run, then the same seed is replayed through a direct
 * AttackUtil.calculatePhysAttackResult with the same two creatures and the same calculation types, and the HP the controller took off the target
 * must equal the sum of that list's damages. An npc attacker draws no other random number on this path (the 10 % critical proc of
 * CreatureController.java:341-345 is guarded by `getOwner() instanceof Player`), so the two runs see the same sequence. That pins the function,
 * its arguments and their order - a call site that passed the wrong pair, or an empty calculation-type set, moves the numbers.
 */
TEST_F(AttackSeamTest, AttackTargetTakesItsDamageFromTheRealAttackUtil) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> attacker = createFighter(fighterTemplate);
	Ref<ControllersTestNpc> target = createFighter(sandbagTemplate);
	const int32_t hpBefore = target->model::gameobjects::Creature::getLifeStats()->getCurrentHp();
	ASSERT_EQ(hpBefore, 1000) << "FixedLifeStats";
	ASSERT_EQ(attacker->getGameStats()->getAttackCounter(), 0);

	commons::utils::Rnd::seedCurrentThreadForTests(20260922);
	attacker->recordingController().attackTarget(Ptr<model::gameobjects::Creature>(*target), 0, true);
	const int32_t applied = hpBefore - target->model::gameobjects::Creature::getLifeStats()->getCurrentHp();

	commons::utils::Rnd::seedCurrentThreadForTests(20260922);
	std::vector<Ref<AttackResult>> replay = AttackUtil::calculatePhysAttackResult(*attacker, *target, meleeCalculationTypes());
	ASSERT_FALSE(replay.empty()) << "AttackStatus.getBaseStatus(attackResult.getFirst()) would throw on an empty list";
	int32_t expected = 0;
	for (const Ref<AttackResult>& result : replay)
		expected += result->getDamage();

	EXPECT_EQ(applied, expected) << "the damage reduceHp took off is the sum of the AttackUtil result list (CreatureController.java:332-336, 248)";
	EXPECT_GT(expected, 0) << "this seed lands the hit, so the case is not vacuously equal at 0";
	EXPECT_EQ(attacker->getGameStats()->getAttackCounter(), 1) << "increaseAttackCounter ran after the broadcast (CreatureController.java:351)";
	EXPECT_EQ(target->getAttackedCount(), 1) << "CreatureController.onAttack reached incrementAttackedCount";
}

/**
 * m5b-2 stage 0, M-03: `CreatureController.attackTarget`'s **else** arm ->
 * `AttackUtil.calculateMagAttackResult(getOwner(), target, getOwner().getAttackType().getMagicalElement(), calculationTypes)`
 * (CreatureController.java:325). Until this item the call site was `standins::attackUtilCalculateMagAttackResult`, an `AION_UNPORTED()` stub, so
 * every swing of a creature whose attack type is not PHYSICAL threw - m5b-client-session.md S-3.
 *
 * This case takes the arm through an **npc**, whose `getAttackType()` is `getAi().modifyAttackType(PHYSICAL)` (Npc.java:124): the fixture's AI
 * answers MAGICAL_FIRE, which is what a world or instance handler does on a caster npc. The equivalence is the same one the physical sibling
 * asserts - seed, run the controller, replay the same seed through a direct `AttackUtil::calculateMagAttackResult` with the same pair, the same
 * element and the same calculation types, and the HP the controller took off must be that list's sum. A call site that passed the wrong pair, the
 * wrong element (SkillElement::NONE reads MAGICAL_ATTACK's physical sibling and MAGICAL_DEFEND's) or an empty calculation-type set moves it.
 */
TEST_F(AttackSeamTest, AttackTargetOfAMagicalAttackTypeTakesItsDamageFromTheRealAttackUtil) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> attacker = createFighter(fighterTemplate);
	SeamNpcAI& attackerAi = *npcAi;
	Ref<ControllersTestNpc> target = createFighter(magicalTargetTemplate);
	ASSERT_EQ(attacker->getAttackType(), model::templates::item::ItemAttackType::PHYSICAL) << "AbstractAI.modifyAttackType returns its argument";
	attackerAi.attackType = model::templates::item::ItemAttackType::MAGICAL_FIRE;
	ASSERT_EQ(attacker->getAttackType(), model::templates::item::ItemAttackType::MAGICAL_FIRE) << "so the else arm of attackTarget is taken";

	const int32_t hpBefore = target->model::gameobjects::Creature::getLifeStats()->getCurrentHp();
	ASSERT_EQ(hpBefore, 1000) << "FixedLifeStats";

	commons::utils::Rnd::seedCurrentThreadForTests(20260923);
	attacker->recordingController().attackTarget(Ptr<model::gameobjects::Creature>(*target), 0, true);
	const int32_t applied = hpBefore - target->model::gameobjects::Creature::getLifeStats()->getCurrentHp();

	commons::utils::Rnd::seedCurrentThreadForTests(20260923);
	std::vector<Ref<AttackResult>> replay = AttackUtil::calculateMagAttackResult(*attacker, *target,
		model::templates::item::getMagicalElement(model::templates::item::ItemAttackType::MAGICAL_FIRE), meleeCalculationTypes());
	ASSERT_FALSE(replay.empty());
	int32_t expected = 0;
	for (const Ref<AttackResult>& result : replay)
		expected += result->getDamage();

	EXPECT_EQ(applied, expected) << "the damage reduceHp took off is the sum of the AttackUtil result list";
	EXPECT_GT(expected, 0) << "the target resists nothing at mresist 10, so the case is not vacuously equal at 0";
	EXPECT_EQ(replay[0]->getAttackStatus(), AttackStatus::NORMALHIT) << "and it neither resisted nor critted, as the template promises";
	EXPECT_EQ(attacker->getGameStats()->getAttackCounter(), 1) << "increaseAttackCounter ran after the broadcast";
	EXPECT_EQ(target->getAttackedCount(), 1) << "CreatureController.onAttack reached incrementAttackedCount";

	// and the same attacker back on PHYSICAL takes the other arm, so the case really is about the branch and not about the seed
	attackerAi.attackType.reset();
	commons::utils::Rnd::seedCurrentThreadForTests(20260923);
	std::vector<Ref<AttackResult>> physical = AttackUtil::calculatePhysAttackResult(*attacker, *target, meleeCalculationTypes());
	EXPECT_NE(physical[0]->getDamage(), expected) << "the two halves read different stats, so they must not agree by accident";
}

/**
 * The same arm, reached the way the user reaches it: a **character whose main-hand weapon is magical**. `Player.getAttackType()` returns the main
 * hand item template's attack type (Player.java:937-941), so equipping an orb is the whole difference between a Mage that can swing and the
 * `UnportedException` of m5b-client-session.md S-3.
 *
 * This goes in through `PlayerController::attackTarget`, i.e. through PlayerRestrictions.canAttack, the attack-range check and GeoService - the
 * real entry point of a left click - and then into CreatureController::attackTarget's magical arm.
 */
TEST_F(AttackSeamTest, AMageWhoseMainHandWeaponIsMagicalCanAutoAttack) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> target = createFighter(magicalTargetTemplate);
	place(*target, 500.0f, 500.0f, 10.0f);
	Ref<SeamPlayer> mage = createPlayer(100031, 9031, model::PlayerClass::MAGE);
	place(*mage, 501.0f, 500.0f, 10.0f);
	ASSERT_TRUE(KnownListPairing::pair(*target, *mage));

	// Equipment.checkAvailableEquipSkills wants the spellbook mastery (ItemGroupInfo.h: SPELLBOOK {100}) or the item goes back into the inventory
	std::vector<Ref<model::skill::PlayerSkillEntry>> owned;
	std::vector<Ptr<model::skill::PlayerSkillEntry>> entries;
	for (int32_t skillId : {100, 111}) {
		owned.push_back(model::skill::PlayerSkillEntry::create(skillId, 1, 0, model::gameobjects::Persistable::PersistentState::NOACTION));
		entries.emplace_back(*owned.back());
	}
	mage->setSkillList(model::skill::PlayerSkillList::create(entries));
	// **Item 100600034, "Training Spellbook", verbatim from data/static_data/items/item_templates.xml.** It is not an example: it is the weapon a
	// level-1 MAGE is handed at character creation (player_initial_data.xml, `<player_data class="MAGE">` item 100600034), and its
	// `attack_type="MAGICAL_WATER"` is the whole of m5b-client-session.md S-3 - every left click of a fresh Mage went through the stand-in.
	// The Warrior's own starting weapon, 100000094 "Training Sword", is `attack_type="PHYSICAL"`, which is why the M5b-1 gate never saw this.
	xml::LoadContext itemContext;
	const model::templates::item::ItemTemplate* spellbookTemplate = xml::bindString<model::templates::item::ItemTemplate>(itemContext,
		R"(<item_template id="100600034" name="Training Spellbook" level="1" item_group="SPELLBOOK" quality="COMMON" attack_type="MAGICAL_WATER">)"
		R"(<weapon_stats hit_count="1" attack_range="15000" boost_magical_skill="80" magical_accuracy="85" attack_speed="2200")"
		R"( max_damage="23" min_damage="20"/></item_template>)")
		.release();
	Ref<model::gameobjects::Item> spellbook = model::gameobjects::Item::create(9032, spellbookTemplate, 1, true,
		model::items::getSlotIdMask(model::items::ItemSlot::MAIN_HAND));
	mage->getEquipment().onLoadHandler(*spellbook);
	ASSERT_TRUE(mage->getEquipment().getMainHandWeapon()) << "the spellbook was not equipped, so the case would test the unarmed physical arm";
	ASSERT_EQ(mage->getAttackType(), model::templates::item::ItemAttackType::MAGICAL_WATER)
		<< "Player.getAttackType() is the main hand template's - this is the whole of m5b-client-session.md S-3";

	const int32_t hpBefore = target->model::gameobjects::Creature::getLifeStats()->getCurrentHp();
	commons::utils::Rnd::seedCurrentThreadForTests(20260923);
	// with the stand-in still in place this threw UnportedException out of PlayerController::attackTarget
	ASSERT_NO_THROW(mage->getController().attackTarget(Ptr<model::gameobjects::Creature>(*target), 0, false));
	const int32_t applied = hpBefore - target->model::gameobjects::Creature::getLifeStats()->getCurrentHp();

	commons::utils::Rnd::seedCurrentThreadForTests(20260923);
	std::vector<Ref<AttackResult>> replay = AttackUtil::calculateMagAttackResult(*mage, *target,
		model::templates::item::getMagicalElement(model::templates::item::ItemAttackType::MAGICAL_WATER), meleeCalculationTypes());
	int32_t expected = 0;
	for (const Ref<AttackResult>& result : replay)
		expected += result->getDamage();

	EXPECT_GT(applied, 0) << "the monster lost hit points: a Mage can swing";
	EXPECT_EQ(applied, expected) << "and exactly what AttackUtil::calculateMagAttackResult answers for the same pair and element";
	EXPECT_EQ(target->getAttackedCount(), 1) << "the whole chain ran, down to CreatureController.onAttack";
	EXPECT_EQ(mage->getGameStats()->getAttackCounter(), 1);
}

/**
 * PlayerController.attackTarget -> PlayerRestrictions.canAttack (PlayerController.java:397-398), the first statement of the method.
 *
 * The character of this fixture is not spawned, so canAttack answers false at PlayerRestrictions.java:212 - before the two branches that read
 * PlayerGameStats.getAttackCounter(), which this chunk's player double does not have - and attackTarget returns without touching the range check,
 * GeoService or the target. The positive arms (SM_ATTACK_RESPONSE.TARGET_TOO_FAR_AWAY and SM_ATTACK) are asserted where a character has real stat
 * containers and a connection: tests/cm_ak/AttackPacketTest.cpp.
 */
TEST_F(AttackSeamTest, AttackTargetAsksPlayerRestrictionsFirstAndStopsOnItsNo) {
	CONTROLLERS_TEST_SCOPE;
	Ref<SeamPlayer> player = createPlayer(100011, 9011);
	Ref<ControllersTestNpc> target = createFighter(fighterTemplate);
	ASSERT_FALSE(player->isSpawned()) << "PlayerRestrictions.canAttack:212 answers false for an unspawned character";

	EXPECT_NO_THROW(player->getController().attackTarget(Ptr<model::gameobjects::Creature>(*target), 0, false));

	EXPECT_EQ(target->getAttackedCount(), 0) << "attackTarget returned on canAttack's no, so nothing reached the target";
	EXPECT_EQ(target->model::gameobjects::Creature::getLifeStats()->getCurrentHp(), 1000);
}

/**
 * NpcController.doReward -> StatFunctions.calculateExperienceReward and calculateDPReward (NpcController.java:219-221). The npc is killed by
 * hand - the aggro list is seeded with one player's damage, which is all doReward reads (getFinalDamageList().toTeamDamages()) - and the
 * experience the character ends up with is compared against a direct StatFunctions.calculateExperienceReward for the same pair.
 *
 * The character is level 10 against a level-12 npc on purpose: at level 1 the reward would exceed Rates.XP_HUNTING's `expNeed * 0.2f` cap and the
 * cap, not the reward, would decide the number - which would hide a call site that passed the wrong level. The case asserts that the cap is not
 * binding, so the comparison really is against calculateExperienceReward's own answer.
 */
TEST_F(AttackSeamTest, DoRewardTakesItsExperienceFromTheRealStatFunctions) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createFighter(fighterTemplate);
	place(*npc, 500.0f, 500.0f, 10.0f);
	// GLADIATOR, not WARRIOR: PlayerCommonData.setDp returns at once for a starting class (PlayerCommonData.java:228-230), so a Warrior could
	// never show that calculateDPReward's answer arrived
	Ref<SeamPlayer> player = createPlayer(100021, 9021, model::PlayerClass::GLADIATOR);
	player->getCommonData()->setLevel(10);
	place(*player, 502.0f, 500.0f, 10.0f);
	ASSERT_TRUE(KnownListPairing::pair(*npc, *player)) << "AggroList.isAware reads the known list (AggroList.java:199-216)";

	// Java: the damage a hit would have added. addDamage is the write half of B-04, and getFinalDamageList is what doReward reads.
	npc->getAggroList().addDamage(*player, 150, true, skillengine::model::HopType::DAMAGE);
	ASSERT_TRUE(npc->getAggroList().isHating(*player));

	const int64_t expBefore = player->getCommonData()->getExp();
	const int32_t dpBefore = player->getCommonData()->getDp();
	const int64_t reward = utils::stats::StatFunctions::calculateExperienceReward(player->getCommonData()->getLevel(), *npc);
	const int32_t dpReward = utils::stats::StatFunctions::calculateDPReward(*player, *npc);
	ASSERT_GT(dpReward, 0) << "the second StatFunctions call of doReward must award something too";
	ASSERT_GT(reward, 0) << "the npc template must award something, or the case is vacuous";
	ASSERT_LT(static_cast<float>(reward), static_cast<float>(player->getCommonData()->getExpNeed()) * 0.2f)
		<< "Rates.XP_HUNTING's cap must not bind here (Rates.java:13-18)";

	npc->recordingController().doReward();

	// the single attacker's damage share is 1.0, so rewardXp reaches addExp unscaled (NpcController.java:224)
	EXPECT_EQ(player->getCommonData()->getExp() - expBefore, reward)
		<< "doReward awarded exactly StatFunctions.calculateExperienceReward(player.getLevel(), npc)";
	EXPECT_EQ(player->getCommonData()->getDp() - dpBefore, dpReward) << "and exactly StatFunctions.calculateDPReward(player, npc)";
}

/**
 * NpcController.doReward -> StatFunctions.calculatePvEApGained (NpcController.java:236), the third StatFunctions call and the only one behind a
 * question: `if (getOwner().getAi().ask(AIQuestion.REWARD_AP))`. m5b-plan.md D16 leaves AbyssPointsService.addAp AION_UNPORTED on purpose, so a
 * `true` answer throws - and *which* function the throw names is the assertion: `addAp` means calculatePvEApGained ran and returned a positive
 * number, `statFunctionsCalculatePvEApGained` would mean the stand-in is back one line earlier.
 */
TEST_F(AttackSeamTest, DoRewardReachesTheRealPvEApGainedBeforeTheUnportedAbyssPointsService) {
	CONTROLLERS_TEST_SCOPE;
	Ref<ControllersTestNpc> npc = createFighter(fighterTemplate);
	place(*npc, 500.0f, 500.0f, 10.0f);
	npcAi->answeredTrue.insert(ai::poll::AIQuestion::REWARD_AP); // Java NpcAI.ask answers this false on an ELYSEA map (D16); forced here
	Ref<SeamPlayer> player = createPlayer(100022, 9022);
	player->getCommonData()->setLevel(10);
	place(*player, 502.0f, 500.0f, 10.0f);
	ASSERT_TRUE(KnownListPairing::pair(*npc, *player));
	npc->getAggroList().addDamage(*player, 150, true, skillengine::model::HopType::DAMAGE);

	ASSERT_GT(utils::stats::StatFunctions::calculatePvEApGained(*player, *npc), 0) << "so `rewardAp >= 1` and addAp is reached";
	try {
		npc->recordingController().doReward();
		FAIL() << "AbyssPointsService::addAp is AION_UNPORTED (m5b-plan.md C-04/D16) and must throw";
	} catch (const runtime::UnportedException& unported) {
		EXPECT_NE(std::string(unported.what()).find("addAp"), std::string::npos)
			<< "the throw must come from AbyssPointsService, i.e. after the real calculatePvEApGained answered: " << unported.what();
	}
}

} // namespace
} // namespace aion::gameserver::controllers::testing
