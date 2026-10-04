// The summon client packets (P5-16, M5e stage 3, m5e-plan.md M-05): CM_SUMMON_COMMAND (C_PET_ORDER), CM_SUMMON_MOVE (C_CLIENTSIDE_NPC_MOVE),
// CM_SUMMON_EMOTION (C_CLIENTSIDE_NPC_ACTION), CM_SUMMON_ATTACK (C_CLIENTSIDE_NPC_ATTACK) and CM_SUMMON_CASTSPELL (C_CLIENTSIDE_NPC_USE_SKILL).
//
// Java: game-server/src/com/aionemu/gameserver/network/aion/clientpackets/CM_SUMMON_*.java.
//
// The read cases check each body against Java's readImpl byte for byte. The run cases drive runImpl for a master whose summon is a real
// Summon (the object VisibleObjectSpawner.spawnSummon builds: its SummonController, SummonMoveController and stats) placed in a Poeta map
// instance, built from the earth spirit 833287 of npc_templates.xml (a SUMMON_PET); a copy of it as a GENERAL npc is the summon that is no pet.

#include "../cm_ak/InWorldPacketRunSupport.h"
#include "../skills/P5-02b/EffectTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/configs/main/LoggingConfig.h"
#include "aion/gameserver/configs/main/PunishmentConfig.h"
#include "aion/gameserver/controllers/SummonController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/controllers/movement/MovementMask.h"
#include "aion/gameserver/controllers/movement/SummonMoveController.h"
#include "aion/gameserver/dataholders/PetSkillData.bind.h"
#include "aion/gameserver/dataholders/PetSkillData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/EmotionTypeInfo.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/summons/SkillOrder.h"
#include "aion/gameserver/model/summons/SummonMode.h"
#include "aion/gameserver/model/summons/SummonModeInfo.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SUMMON_ATTACK.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SUMMON_CASTSPELL.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SUMMON_COMMAND.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SUMMON_EMOTION.h"
#include "aion/gameserver/network/aion/clientpackets/CM_SUMMON_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOVE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/skillengine/effect/AbnormalState.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing {
namespace {

namespace et = skillengine::effecttest;
using controllers::movement::MovementMask;
using model::EmotionType;
using model::gameobjects::Summon;
using model::summons::SummonMode;
using network::test::LogCapture;
using network::test::PacketWriter;
using serverpackets::SM_SYSTEM_MESSAGE;

/** the decoded opcodes of ClientPacketInfo.gen.inc:112, 173-176 (Java AionClientPacketFactory packets[121], [201]-[203], [205]) */
constexpr int32_t COMMAND_OPCODE = 121;
constexpr int32_t MOVE_OPCODE = 201;
constexpr int32_t EMOTION_OPCODE = 202;
constexpr int32_t ATTACK_OPCODE = 203;
constexpr int32_t CASTSPELL_OPCODE = 205;

const char* BASE_CLIENT_PACKET_LOGGER = "com.aionemu.commons.network.packet.BaseClientPacket";
const char* EMOTION_LOGGER = "com.aionemu.gameserver.network.aion.clientpackets.CM_SUMMON_EMOTION";
const char* CASTSPELL_LOGGER = "com.aionemu.gameserver.network.aion.clientpackets.CM_SUMMON_CASTSPELL";
const char* AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp:22

/** Java readImpl: UC mode, D 0, D 0, D target */
std::vector<uint8_t> commandBody(int32_t mode, int32_t targetObjId = 0) {
	return PacketWriter().C(mode).D(0).D(0).D(targetObjId).data;
}

/** Java readImpl: D object, F x, F y, F z, C heading, C type, then the type's optional parts */
PacketWriter moveHead(int32_t objectId, float x, float y, float z, int32_t heading, int8_t type) {
	return PacketWriter().D(objectId).F(x).F(y).F(z).C(heading).C(type);
}

/** Java readImpl: D object, UC emotion */
std::vector<uint8_t> emotionBody(int32_t objectId, int32_t emotionTypeId) {
	return PacketWriter().D(objectId).C(emotionTypeId).data;
}

/** Java readImpl: D summon, D target, C, UH time, C */
std::vector<uint8_t> attackBody(int32_t summonObjId, int32_t targetObjId, int32_t time) {
	return PacketWriter().D(summonObjId).D(targetObjId).C(0).H(time).C(0).data;
}

/** Java readImpl: D summon, UH skill, UC level, D target, D */
std::vector<uint8_t> castBody(int32_t summonObjId, int32_t skillId, int32_t skillLvl, int32_t targetObjId) {
	return PacketWriter().D(summonObjId).H(skillId).C(skillLvl).D(targetObjId).D(0).data;
}

template <class P>
void expectReadExactly(int32_t opcode, std::vector<uint8_t> data) {
	LogCapture capture({BASE_CLIENT_PACKET_LOGGER});
	auto packet = std::make_unique<P>(opcode, StateSet{AionConnection_State::IN_GAME});
	packet->setBuffer(commons::utils::ByteBuffer::wrap(data));
	ASSERT_TRUE(packet->read());
	EXPECT_EQ(packet->getRemainingBytes(), 0);
	EXPECT_FALSE(capture.contains("Missing")) << capture.dump();
}

TEST(SummonPacketsReadTest, EachBodyIsReadToItsEnd) {
	expectReadExactly<CM_SUMMON_COMMAND>(COMMAND_OPCODE, commandBody(1, 7));
	expectReadExactly<CM_SUMMON_EMOTION>(EMOTION_OPCODE, emotionBody(7, 3));
	expectReadExactly<CM_SUMMON_ATTACK>(ATTACK_OPCODE, attackBody(7, 8, 1500));
	expectReadExactly<CM_SUMMON_CASTSPELL>(CASTSPELL_OPCODE, castBody(7, 22107, 1, 8));
}

TEST(SummonPacketsReadTest, AMoveReadsWhatItsTypeAnnounces) {
	// IMMEDIATE: the head only
	expectReadExactly<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(7, 1, 2, 3, 4, MovementMask::IMMEDIATE).data);
	// POSITION | MANUAL | ABSOLUTE: the destination follows
	expectReadExactly<CM_SUMMON_MOVE>(MOVE_OPCODE,
		moveHead(7, 1, 2, 3, 4, MovementMask::POSITION | MovementMask::MANUAL | MovementMask::ABSOLUTE).F(5).F(6).F(7).data);
	// POSITION | MANUAL without ABSOLUTE: nothing follows (a stun or a resist while moving)
	expectReadExactly<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(7, 1, 2, 3, 4, MovementMask::POSITION | MovementMask::MANUAL).data);
	// GLIDE: one byte; VEHICLE: two ints and three floats
	expectReadExactly<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(7, 1, 2, 3, 4, MovementMask::GLIDE).C(1).data);
	expectReadExactly<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(7, 1, 2, 3, 4, MovementMask::VEHICLE).D(11).D(12).F(13).F(14).F(15).data);
}

TEST(SummonPacketsReadTest, TheModeIdsAreJavas) {
	// SummonMode.java: ATTACK(0), GUARD(1), REST(2), RELEASE(3), UNK(5); getSummonModeById answers null for any other id
	EXPECT_EQ(model::summons::getSummonModeById(0), SummonMode::ATTACK);
	EXPECT_EQ(model::summons::getSummonModeById(1), SummonMode::GUARD);
	EXPECT_EQ(model::summons::getSummonModeById(2), SummonMode::REST);
	EXPECT_EQ(model::summons::getSummonModeById(3), SummonMode::RELEASE);
	EXPECT_EQ(model::summons::getSummonModeById(5), SummonMode::UNK);
	EXPECT_EQ(model::summons::getSummonModeById(4), std::nullopt);
	EXPECT_EQ(model::summons::getSummonModeById(255), std::nullopt);
	EXPECT_EQ(model::summons::getId(SummonMode::UNK), 5);
}

/** AuditLogger only writes its line with gameserver.log.audit on, and must not reach AutoBan (PunishmentConfig off) */
class AuditScope {
public:
	AuditScope() {
		configs::main::PunishmentConfig::PUNISHMENT_ENABLE.store(false);
		configs::main::LoggingConfig::LOG_AUDIT.store(true);
	}
	~AuditScope() { configs::main::LoggingConfig::LOG_AUDIT.store(false); }
	AuditScope(const AuditScope&) = delete;
	AuditScope& operator=(const AuditScope&) = delete;
};

/** Npc templates are immortal static data: bound once per process */
const model::templates::npc::NpcTemplate* boundTemplate(const char* xmlText) {
	xml::LoadContext context;
	return xml::bindString<model::templates::npc::NpcTemplate>(context, xmlText).release();
}

/** the earth spirit 833287 of npc_templates.xml, verbatim */
const model::templates::npc::NpcTemplate* earthSpirit() {
	static const model::templates::npc::NpcTemplate* bound = boundTemplate(
		R"(<npc_template npc_id="833287" level="16" name="earth spirit" name_id="466282" height="1" group_drop="ELEMENTALEARTH1" rank="DISCIPLINED")"
		R"( rating="NORMAL" race="ELEMENTAL" tribe="PET" type="SUMMON_PET" srange="15" arange="2" attack_speed="2040" hpgauge="3" cancel_level="90">)"
		R"(<stats maxHp="1575" attack="85" pdef="575" mresist="340" accuracy="503" macc="291" pcrit="50" mcrit="18" evasion="503" parry="0">)"
		R"(<speeds walk="2" group_walk="1.2" run="8.4" run_fight="8.4" group_run_fight="8" /></stats>)"
		R"(<bound_radius front="0.25" side="0.25" upper="2.8" /></npc_template>)");
	return bound;
}

/** the earth spirit as a GENERAL npc: a Summon that is no pet (Summon.isPet: SUMMON_PET only) */
const model::templates::npc::NpcTemplate* notAPet() {
	static const model::templates::npc::NpcTemplate* bound = boundTemplate(
		R"(<npc_template npc_id="833999" level="16" name="no pet" name_id="1" rank="DISCIPLINED" rating="NORMAL" race="ELEMENTAL" tribe="PET")"
		R"( type="GENERAL" srange="15" arange="2" attack_speed="2040"><stats maxHp="1575" attack="85"><speeds walk="2" run="8.4" /></stats>)"
		R"(</npc_template>)");
	return bound;
}

/** The captured packets of type P (the header's encoded opcode, AionServerPacket::writeOP), as tests/skills/P5-02a/CastTestSupport.h finds them */
template <class P>
std::vector<std::vector<uint8_t>> packetsOf(const std::vector<std::vector<uint8_t>>& sent) {
	std::vector<std::vector<uint8_t>> result;
	for (const std::vector<uint8_t>& bytes : sent) {
		if (bytes.size() < 2)
			continue;
		const int32_t encoded = static_cast<int32_t>(static_cast<uint16_t>(bytes[0] | bytes[1] << 8));
		if (encoded == (network::Crypt::encodeServerPacketOpcode(network::aion::opcodeOf<P>) & 0xFFFF))
			result.push_back(bytes);
	}
	return result;
}

constexpr int32_t ORDERED_SKILL = 22107; // Command: Earth Detonation Claw, the spirit's skill of the order 3835

class SummonPacketsRunTest : public InWorldPacketTest {
protected:
	void SetUp() override {
		InWorldPacketTest::SetUp();
		et::publishWorldStaticDataOnce();
		// the spirit has a pet skill row (PetSkillData.petHasSkill throws for a pet without any, as Java's NPE) for another skill than the ordered
		// ones: SummonController.useSkill(order) then returns at its pet-skill check, and the cases see the order taken from the queue
		dataholders::DataManager::PET_SKILL_DATA.publish(xml::bindString<dataholders::PetSkillData>(petSkillContext,
			R"(<pet_skill_templates><pet_skill skill_id="22999" pet_id="833287" order_skill="3999"/></pet_skill_templates>)"));
		map = world::WorldMap::create(dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(et::POETA));
		mapInstance = world::WorldMap2DInstance::create(*map, 1, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
				::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
		});
		master = makePlayer(320021, 9421, "Master");
		place(*master.player, 500, 500, 100);
		other = makePlayer(320022, 9422, "Bystander");
		place(*other.player, 504, 500, 100);
		client = std::make_unique<TestClient>();
		client->enterWorld(master);
		(*client)->clearSent();
	}

	void TearDown() override {
		if (master.player) {
			master.player->setSummon(nullptr);
			master.player->setClientConnection(nullptr);
		}
		summon = nullptr;
		client.reset();
		master = {};
		other = {};
		mapInstance = nullptr;
		map = nullptr;
		spawnGroups.clear();
		dataholders::DataManager::PET_SKILL_DATA.resetForTests();
		InWorldPacketTest::TearDown();
	}

	void place(model::gameobjects::VisibleObject& object, float x, float y, float z) {
		object.setPosition(world::WorldPosition::create(et::POETA, x, y, z, int8_t{0}, mapInstance->getRegion(x, y, z)));
		object.getPosition()->setIsSpawned(true);
	}

	/** VisibleObjectSpawner.spawnSummon's object, placed next to the master and set as his summon; both know each other */
	Summon& summonOf(const model::templates::npc::NpcTemplate* npcTemplate = earthSpirit()) {
		runtime::Ref<model::templates::spawns::SpawnGroup> group =
			model::templates::spawns::SpawnGroup::create(et::POETA, npcTemplate->getTemplateId(), 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<et::EffectTestSpawnTemplate>(*group, 502, 500, 100));
		spawnGroups.push_back(group);
		summon = model::gameobjects::VisibleObject::create<Summon>(utils::idfactory::IDFactory::getInstance().nextId(),
			std::make_unique<controllers::SummonController>(), spawn, npcTemplate, *master.player, 0);
		summon->setKnownlist(std::make_unique<world::knownlist::KnownList>(*summon));
		summon->setEffectController(std::make_unique<controllers::effect::EffectController>(*summon));
		place(*summon, 502, 500, 100);
		master.player->setSummon(runtime::Ptr<Summon>(summon));
		EXPECT_TRUE(et::KnownListPairing::pair(*summon, *master.player));
		(*client)->clearSent();
		return *summon;
	}

	template <class P>
	void run(int32_t opcode, const std::vector<uint8_t>& body) {
		Driver<P> packet(opcode);
		packet.readAndRun(body, client->get());
	}

	int64_t countSent(SM_SYSTEM_MESSAGE&& message) {
		const std::vector<uint8_t> expected = serialized(std::move(message), client->con());
		const std::vector<std::vector<uint8_t>> sent = (*client)->sentBytes();
		return std::count(sent.begin(), sent.end(), expected);
	}

	template <class P>
	int64_t countSentOf() {
		return static_cast<int64_t>(packetsOf<P>((*client)->sentBytes()).size());
	}

	xml::LoadContext petSkillContext;
	runtime::Ref<world::WorldMap> map;
	runtime::Ref<world::WorldMapInstance> mapInstance;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	PlayerFixture master;
	PlayerFixture other;
	runtime::Ref<Summon> summon;
	std::unique_ptr<TestClient> client;
};

// ---- CM_SUMMON_COMMAND ------------------------------------------------------------------------------------------------------------------------

TEST_F(SummonPacketsRunTest, ACommandWithoutASummonOrWithAnUnknownModeDoesNothing) {
	EXPECT_NO_THROW(run<CM_SUMMON_COMMAND>(COMMAND_OPCODE, commandBody(1)));
	EXPECT_TRUE((*client)->sentBytes().empty());

	Summon& spirit = summonOf();
	run<CM_SUMMON_COMMAND>(COMMAND_OPCODE, commandBody(4)); // SummonMode ids are 0, 1, 2, 3 and 5
	EXPECT_EQ(spirit.getMode(), SummonMode::GUARD);
	EXPECT_TRUE((*client)->sentBytes().empty());
}

TEST_F(SummonPacketsRunTest, ARestOrderPutsTheSummonToRest) {
	Summon& spirit = summonOf();
	run<CM_SUMMON_COMMAND>(COMMAND_OPCODE, commandBody(2));
	EXPECT_EQ(spirit.getMode(), SummonMode::REST);
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_REST_MODE(spirit.getL10n())), 1);
}

TEST_F(SummonPacketsRunTest, AReleaseOrderIsACommandReleaseTheMasterMayTakeBack) {
	Summon& spirit = summonOf();
	run<CM_SUMMON_COMMAND>(COMMAND_OPCODE, commandBody(3));
	EXPECT_TRUE(spirit.isBeingReleased());
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_SUMMON_UNSUMMON_FOLLOWER(spirit.getL10n())), 1) << "UnsummonType.COMMAND";

	run<CM_SUMMON_COMMAND>(COMMAND_OPCODE, commandBody(5)); // UNK: not taken back
	EXPECT_TRUE(spirit.isBeingReleased());
	run<CM_SUMMON_COMMAND>(COMMAND_OPCODE, commandBody(1)); // GUARD: taken back
	EXPECT_FALSE(spirit.isBeingReleased());
	EXPECT_EQ(spirit.getMode(), SummonMode::GUARD);
}

// ---- CM_SUMMON_EMOTION ------------------------------------------------------------------------------------------------------------------------

TEST_F(SummonPacketsRunTest, AttackModeOnAndOffSetTheWeaponState) {
	Summon& spirit = summonOf();
	run<CM_SUMMON_EMOTION>(EMOTION_OPCODE, emotionBody(spirit.getObjectId(), model::getTypeId(EmotionType::ATTACKMODE_IN_MOVE)));
	EXPECT_TRUE(spirit.isInState(model::gameobjects::state::CreatureState::WEAPON_EQUIPPED));
	EXPECT_EQ(countSentOf<serverpackets::SM_EMOTION>(), 1);

	run<CM_SUMMON_EMOTION>(EMOTION_OPCODE, emotionBody(spirit.getObjectId(), model::getTypeId(EmotionType::NEUTRALMODE_IN_MOVE)));
	EXPECT_FALSE(spirit.isInState(model::gameobjects::state::CreatureState::WEAPON_EQUIPPED));
	EXPECT_EQ(countSentOf<serverpackets::SM_EMOTION>(), 2);
}

TEST_F(SummonPacketsRunTest, FlyAndLandChangeTheSpeedFirstAndJumpsAreOnlyShown) {
	Summon& spirit = summonOf();
	run<CM_SUMMON_EMOTION>(EMOTION_OPCODE, emotionBody(spirit.getObjectId(), model::getTypeId(EmotionType::FLY)));
	const std::vector<std::vector<uint8_t>> emotions = packetsOf<serverpackets::SM_EMOTION>((*client)->sentBytes());
	ASSERT_EQ(emotions.size(), 2u);
	EXPECT_EQ(emotions[0], serialized(serverpackets::SM_EMOTION(spirit, EmotionType::CHANGE_SPEED), client->con()));
	EXPECT_EQ(emotions[1], serialized(serverpackets::SM_EMOTION(spirit, EmotionType::FLY), client->con()));

	(*client)->clearSent();
	run<CM_SUMMON_EMOTION>(EMOTION_OPCODE, emotionBody(spirit.getObjectId(), model::getTypeId(EmotionType::JUMP)));
	EXPECT_EQ(countSentOf<serverpackets::SM_EMOTION>(), 1);
	EXPECT_FALSE(spirit.isInState(model::gameobjects::state::CreatureState::WEAPON_EQUIPPED));
}

TEST_F(SummonPacketsRunTest, AnUnknownEmotionIsLogged) {
	Summon& spirit = summonOf();
	LogCapture log({EMOTION_LOGGER});
	run<CM_SUMMON_EMOTION>(EMOTION_OPCODE, emotionBody(spirit.getObjectId(), 250));
	EXPECT_TRUE(log.contains("Unknown emotion type 250 from ")) << log.dump();
	EXPECT_TRUE((*client)->sentBytes().empty());

	// NONE's id is -1 (EmotionType.java), which readUC never answers: the byte 0xFF is 255, unknown and logged like any other
	ASSERT_EQ(model::getTypeId(EmotionType::NONE), -1);
	run<CM_SUMMON_EMOTION>(EMOTION_OPCODE, emotionBody(spirit.getObjectId(), 0xFF));
	EXPECT_TRUE(log.contains("Unknown emotion type 255 from ")) << log.dump();
}

TEST_F(SummonPacketsRunTest, AnEmotionOfAnotherObjectDoesNothing) {
	Summon& spirit = summonOf();
	EXPECT_NO_THROW(run<CM_SUMMON_EMOTION>(EMOTION_OPCODE, emotionBody(spirit.getObjectId() + 1, model::getTypeId(EmotionType::FLY))));
	EXPECT_TRUE((*client)->sentBytes().empty());
}

// ---- CM_SUMMON_ATTACK -------------------------------------------------------------------------------------------------------------------------

TEST_F(SummonPacketsRunTest, AnAttackOnAKnownCreatureReachesTheSummonsController) {
	Summon& spirit = summonOf();
	ASSERT_TRUE(et::KnownListPairing::pair(spirit, *other.player));
	// SummonController.attackTarget refuses a target that is no enemy (the bystander is of the master's race) with STR_INVALID_TARGET
	run<CM_SUMMON_ATTACK>(ATTACK_OPCODE, attackBody(spirit.getObjectId(), other.player->getObjectId(), 0));
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_INVALID_TARGET()), 1);
}

TEST_F(SummonPacketsRunTest, AnAttackOnAnUnknownTargetOrWithoutASummonDoesNothing) {
	EXPECT_NO_THROW(run<CM_SUMMON_ATTACK>(ATTACK_OPCODE, attackBody(123, other.player->getObjectId(), 0)));
	Summon& spirit = summonOf();
	AuditScope auditOn;
	LogCapture audit({AUDIT_LOGGER});
	run<CM_SUMMON_ATTACK>(ATTACK_OPCODE, attackBody(spirit.getObjectId(), other.player->getObjectId(), 0)); // not in the summon's known list
	EXPECT_TRUE((*client)->sentBytes().empty());
	EXPECT_EQ(audit.count("tried to use summon attack on a wrong target"), 0);
}

// ---- CM_SUMMON_CASTSPELL ----------------------------------------------------------------------------------------------------------------------

TEST_F(SummonPacketsRunTest, ASpellWithoutAPetSaysThereIsNone) {
	run<CM_SUMMON_CASTSPELL>(CASTSPELL_OPCODE, castBody(123, ORDERED_SKILL, 1, 0));
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_NEED_PET()), 1);

	(*client)->clearSent();
	Summon& noPet = summonOf(notAPet());
	noPet.addSkillOrder(ORDERED_SKILL, 1, noPet, 0, false);
	run<CM_SUMMON_CASTSPELL>(CASTSPELL_OPCODE, castBody(noPet.getObjectId(), ORDERED_SKILL, 1, noPet.getObjectId()));
	EXPECT_EQ(countSent(SM_SYSTEM_MESSAGE::STR_SKILL_NOT_NEED_PET()), 1);
	EXPECT_TRUE(noPet.getNextSkillOrder()) << "the order stays";
}

TEST_F(SummonPacketsRunTest, ASpellOnTheOrderedTargetCarriesTheOrderOut) {
	Summon& spirit = summonOf();
	ASSERT_TRUE(et::KnownListPairing::pair(spirit, *other.player));
	spirit.addSkillOrder(ORDERED_SKILL, 1, *other.player, 0, false);
	LogCapture log({CASTSPELL_LOGGER});

	run<CM_SUMMON_CASTSPELL>(CASTSPELL_OPCODE, castBody(spirit.getObjectId(), ORDERED_SKILL, 1, other.player->getObjectId()));

	EXPECT_FALSE(spirit.getNextSkillOrder()) << "retrieveNextSkillOrder took it; SummonController.useSkill ran it";
	EXPECT_FALSE(log.contains("used summon order with a different skill")) << log.dump();
}

TEST_F(SummonPacketsRunTest, ASpellWithAnotherSkillIsLoggedAndTheOrderIsCarriedOutAllTheSame) {
	Summon& spirit = summonOf();
	spirit.addSkillOrder(ORDERED_SKILL, 1, spirit, 0, false);
	LogCapture log({CASTSPELL_LOGGER});

	run<CM_SUMMON_CASTSPELL>(CASTSPELL_OPCODE, castBody(spirit.getObjectId(), 22108, 2, spirit.getObjectId())); // on itself

	EXPECT_FALSE(spirit.getNextSkillOrder());
	EXPECT_TRUE(log.contains("used summon order with a different skill: skillId 22108->22107; skillLvl 2->1.")) << log.dump();
}

TEST_F(SummonPacketsRunTest, AnOrderForAnotherTargetIsDropped) {
	Summon& spirit = summonOf();
	ASSERT_TRUE(et::KnownListPairing::pair(spirit, *other.player));
	spirit.addSkillOrder(ORDERED_SKILL, 1, spirit, 0, false);
	LogCapture log({CASTSPELL_LOGGER});

	run<CM_SUMMON_CASTSPELL>(CASTSPELL_OPCODE, castBody(spirit.getObjectId(), 22108, 1, other.player->getObjectId()));

	EXPECT_FALSE(spirit.getNextSkillOrder()) << "retrieved, then not used";
	EXPECT_FALSE(log.contains("used summon order with a different skill")) << log.dump();
}

TEST_F(SummonPacketsRunTest, ASpellOnATargetTheSummonDoesNotKnowKeepsTheOrder) {
	Summon& spirit = summonOf();
	spirit.addSkillOrder(ORDERED_SKILL, 1, *other.player, 0, false);
	run<CM_SUMMON_CASTSPELL>(CASTSPELL_OPCODE, castBody(spirit.getObjectId(), ORDERED_SKILL, 1, other.player->getObjectId()));
	EXPECT_TRUE(spirit.getNextSkillOrder());
	EXPECT_TRUE((*client)->sentBytes().empty());
}

// ---- CM_SUMMON_MOVE ---------------------------------------------------------------------------------------------------------------------------

TEST_F(SummonPacketsRunTest, AStopMovesTheSummonAndShowsIt) {
	Summon& spirit = summonOf();
	const int8_t start = MovementMask::POSITION | MovementMask::MANUAL | MovementMask::ABSOLUTE;
	run<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(spirit.getObjectId(), 502, 500, 100, 0, start).F(510).F(500).F(100).data);
	ASSERT_TRUE(spirit.getMoveController()->isInMove()) << "onStartMove";
	(*client)->clearSent();

	run<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(spirit.getObjectId(), 503, 501, 100, 30, MovementMask::IMMEDIATE).data);

	EXPECT_FALSE(spirit.getMoveController()->isInMove()) << "onStopMove";

	EXPECT_FLOAT_EQ(spirit.getX(), 503);
	EXPECT_FLOAT_EQ(spirit.getY(), 501);
	EXPECT_EQ(spirit.getHeading(), 30);
	EXPECT_EQ(spirit.getMoveController()->getMovementMask(), MovementMask::IMMEDIATE);
	EXPECT_EQ(countSentOf<serverpackets::SM_MOVE>(), 1) << "broadcastToSightedPlayers";
}

TEST_F(SummonPacketsRunTest, AMoveWithAMouseDestinationStartsTheMove) {
	Summon& spirit = summonOf();
	const int8_t type = MovementMask::POSITION | MovementMask::MANUAL | MovementMask::ABSOLUTE;
	run<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(spirit.getObjectId(), 502.5f, 500, 100, 0, type).F(510).F(500).F(100).data);

	EXPECT_FLOAT_EQ(spirit.getX(), 502.5f);
	EXPECT_FLOAT_EQ(spirit.getMoveController()->getTargetX2(), 510) << "setNewDirection";
	EXPECT_EQ(countSentOf<serverpackets::SM_MOVE>(), 1);
}

TEST_F(SummonPacketsRunTest, AManualPositionWithoutADestinationIsLeftToTheServer) {
	Summon& spirit = summonOf();
	run<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(spirit.getObjectId(), 509, 509, 100, 0, MovementMask::POSITION | MovementMask::MANUAL).data);
	EXPECT_FLOAT_EQ(spirit.getX(), 502) << "the stun or resist already placed it";
	EXPECT_EQ(countSentOf<serverpackets::SM_MOVE>(), 0);
}

TEST_F(SummonPacketsRunTest, AMoveIsStoredWithItsGlideAndVehicleParts) {
	Summon& spirit = summonOf();
	run<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(spirit.getObjectId(), 503, 500, 100, 0, MovementMask::GLIDE).C(3).data);
	auto& smc = static_cast<controllers::movement::SummonMoveController&>(*spirit.getMoveController());
	EXPECT_EQ(smc.glideFlag.get(), 3);
	EXPECT_FLOAT_EQ(spirit.getX(), 503) << "a move without POSITION: onMove, then the position";
	EXPECT_EQ(countSentOf<serverpackets::SM_MOVE>(), 0) << "no POSITION, not IMMEDIATE: not shown";

	run<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(spirit.getObjectId(), 504, 500, 100, 0, MovementMask::VEHICLE).D(11).D(12).F(13).F(14).F(15).data);
	EXPECT_EQ(smc.unk1.get(), 11);
	EXPECT_EQ(smc.unk2.get(), 12);
	EXPECT_FLOAT_EQ(smc.vehicleX.get(), 13);
	EXPECT_FLOAT_EQ(smc.vehicleZ.get(), 15);
}

TEST_F(SummonPacketsRunTest, AStunnedOrUnspawnedSummonDoesNotMove) {
	Summon& spirit = summonOf();
	spirit.getEffectController()->setAbnormal(skillengine::effect::AbnormalState::STUN);
	run<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(spirit.getObjectId(), 509, 509, 100, 0, MovementMask::IMMEDIATE).data);
	EXPECT_FLOAT_EQ(spirit.getX(), 502) << "CANT_MOVE_STATE";

	spirit.getEffectController()->unsetAbnormal(skillengine::effect::AbnormalState::STUN);
	spirit.getPosition()->setIsSpawned(false);
	run<CM_SUMMON_MOVE>(MOVE_OPCODE, moveHead(spirit.getObjectId(), 509, 509, 100, 0, MovementMask::IMMEDIATE).data);
	EXPECT_FLOAT_EQ(spirit.getX(), 502) << "not spawned";
	EXPECT_EQ(countSentOf<serverpackets::SM_MOVE>(), 0);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing
