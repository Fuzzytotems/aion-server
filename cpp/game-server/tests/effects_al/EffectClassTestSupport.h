#pragma once

// Test support of the effect classes A-L (P5-03, M5b-2 stage 1 part 3, item F-05): real Effects of templates bound from skill_templates.xml text,
// driven through Effect.initialize (calculate), Effect.applyEffect (the template's applyEffect, EffectController.addEffect, Effect.startEffect)
// and the end task or Effect.endEffect, on the effect lane's world (tests/skills/P5-02b/EffectTestSupport.h: a Poeta map instance with a real
// instance handler, a DeterministicExecutor on a ManualClock, real players and npcs) with the packet capture of tests/cm_ak
// (InWorldPacketRunSupport.h: a real AionConnection whose send queue the test reads). Both are included by relative path, the way the P5-02b
// tests include them: the chunks share no test support directory.
//
// The world data: every fixture here publishes the world chunk's test holders (tests/world/WorldTestSupport.h) before EffectWorldTest's own,
// which then finds them present and publishes nothing (EffectTestSupport.h publishes only absent holders). The two define the same test Poeta,
// and EscapeEffectTest needs the world chunk's holders because it teleports through World::getInstance(); a holder can be published once per
// process, so whichever fixture runs first must publish the same set.
//
// The packets are decoded field by field as the Java writeImpl writes them (SM_ABNORMAL_STATE.java:35-49, SM_ABNORMAL_EFFECT.java:46-70,
// SM_ATTACK_STATUS.java:120-160, SM_PLAYER_STATE.java, SM_DELETE.java), so a case can assert what the client would read.
//
// A see/notSee notification that throws is caught and logged by KnownList (KnownList.java: notifySee/notifyNotSee), so a case would stay green
// while the client never learns an npc reappeared. Every fixture here therefore resets KnownList::notifyFailureCount in SetUp and expects it to be
// 0 in TearDown. The one lookup that threw here is SM_NPC_INFO's town id (TownService, whose constructor loads the towns from the database and then
// reads HOUSE_DATA); it goes through the test lookup of the serverpackets (tests/sm_lz/NpcInfoPacketsTest.cpp) and answers 0, which is what
// TownService.getTownIdByPosition answers for these npcs: their spawn is no TownSpawnTemplate and no zone of the test Poeta has a town id.

#include "../cm_ak/InWorldPacketRunSupport.h"
#include "../skills/P5-02b/EffectTestSupport.h"
#include "../world/WorldTestSupport.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"
#include "aion/gameserver/model/gameobjects/Creature.h"
#include "aion/gameserver/model/stats/calc/Stat2.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/network/Crypt.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_EFFECT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ABNORMAL_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATTACK_STATUS.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DELETE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_STATE.h"
#include "aion/gameserver/network/aion/serverpackets/detail/PacketLookups.h"
#include "aion/gameserver/skillengine/effect/EffectTemplate.h"
#include "aion/gameserver/skillengine/effect/Effects.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.bind.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

namespace cp = network::aion::clientpackets::testing;

// ---- packets --------------------------------------------------------------------------------------------------------------------------------

/** Whether a captured packet is a P: its header is [H encodeServerPacketOpcode(opcode)][C 0x44][H ~encoded] (AionServerPacket::writeOP) */
template <class P>
bool isPacket(const std::vector<uint8_t>& bytes) {
	if (bytes.size() < 2)
		return false;
	int32_t encoded = static_cast<int32_t>(static_cast<uint16_t>(bytes[0] | bytes[1] << 8));
	return encoded == (network::Crypt::encodeServerPacketOpcode(network::aion::opcodeOf<P>) & 0xFFFF);
}

/** The captured packets of type P since the connection was last cleared, in order */
template <class P>
std::vector<std::vector<uint8_t>> packetsOf(cp::RecordingAionConnection& connection) {
	std::vector<std::vector<uint8_t>> result;
	for (const std::vector<uint8_t>& bytes : connection.sentBytes())
		if (isPacket<P>(bytes))
			result.push_back(bytes);
	return result;
}

/** One effect of SM_ABNORMAL_STATE / SM_ABNORMAL_EFFECT (the effector id is written for a player effected only in SM_ABNORMAL_EFFECT) */
struct ShownEffect {
	int32_t effectorId = 0;
	int32_t skillId = 0;
	int32_t level = 0;
	int32_t targetSlot = 0;
	int32_t remainingTime = 0;
};

/** SM_ABNORMAL_STATE.java:35-49: abnormals, 0, 0, slot, count, then effector, skill, level, target slot ordinal, remaining time per effect */
struct AbnormalStateFields {
	int32_t abnormals = 0;
	int32_t slot = 0;
	std::vector<ShownEffect> effects;
};

inline AbnormalStateFields decodeAbnormalState(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	AbnormalStateFields f;
	f.abnormals = reader.D();
	reader.D();
	reader.D();
	f.slot = reader.C();
	int32_t count = static_cast<uint16_t>(reader.H());
	for (int32_t i = 0; i < count; ++i) {
		ShownEffect e;
		e.effectorId = reader.D();
		e.skillId = static_cast<uint16_t>(reader.H());
		e.level = reader.C();
		e.targetSlot = reader.C();
		e.remainingTime = reader.D();
		f.effects.push_back(e);
	}
	return f;
}

/** SM_ABNORMAL_EFFECT.java:46-70: effected, effect type (2 player, 1 other), 0, abnormals, 0, slots, count, the effects */
struct AbnormalEffectFields {
	int32_t effectedId = 0;
	int32_t effectType = 0;
	int32_t abnormals = 0;
	int32_t slots = 0;
	std::vector<ShownEffect> effects;
};

inline AbnormalEffectFields decodeAbnormalEffect(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	AbnormalEffectFields f;
	f.effectedId = reader.D();
	f.effectType = reader.C();
	reader.D();
	f.abnormals = reader.D();
	reader.D();
	f.slots = reader.C();
	int32_t count = static_cast<uint16_t>(reader.H());
	for (int32_t i = 0; i < count; ++i) {
		ShownEffect e;
		if (f.effectType == 2)
			e.effectorId = reader.D();
		e.skillId = static_cast<uint16_t>(reader.H());
		e.level = reader.C();
		e.targetSlot = reader.C();
		e.remainingTime = reader.D();
		f.effects.push_back(e);
	}
	return f;
}

/** SM_ATTACK_STATUS.java:120-160: creature, the signed value, TYPE.getValue(), hp or mp percentage, skill, LOG.getValue(), critical flag */
struct AttackStatusFields {
	int32_t objectId = 0;
	int32_t value = 0;
	int32_t type = 0;
	int32_t percentage = 0;
	int32_t skillId = 0;
	int32_t log = 0;
	int32_t critical = 0;
};

inline AttackStatusFields decodeAttackStatus(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	AttackStatusFields f;
	f.objectId = reader.D();
	f.value = reader.D();
	f.type = reader.C();
	f.percentage = reader.C();
	f.skillId = static_cast<uint16_t>(reader.H());
	f.log = reader.C();
	f.critical = reader.C();
	return f;
}

/** The SM_ATTACK_STATUS packets of one creature, decoded, in order */
inline std::vector<AttackStatusFields> attackStatusesOf(cp::RecordingAionConnection& connection, int32_t objectId) {
	std::vector<AttackStatusFields> result;
	for (const std::vector<uint8_t>& bytes : packetsOf<network::aion::serverpackets::SM_ATTACK_STATUS>(connection)) {
		AttackStatusFields fields = decodeAttackStatus(bytes);
		if (fields.objectId == objectId)
			result.push_back(fields);
	}
	return result;
}

/** SM_PLAYER_STATE: the object, its visual state, its see state, the blinking flag */
struct PlayerStateFields {
	int32_t objectId = 0;
	int32_t visualState = 0;
	int32_t seeState = 0;
};

inline PlayerStateFields decodePlayerState(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	PlayerStateFields f;
	f.objectId = reader.D();
	f.visualState = reader.C();
	f.seeState = reader.C();
	return f;
}

/** SM_DELETE.java: the object, then the delete animation */
inline int32_t deletedObjectOf(const std::vector<uint8_t>& bytes) {
	network::test::PacketReader reader(cp::bodyOf(bytes));
	return reader.D();
}

// The wire values of SM_ATTACK_STATUS.TYPE and .LOG the classes of this chunk send (SM_ATTACK_STATUS.java:20-75)
inline constexpr int32_t TYPE_REGULAR = 5;
inline constexpr int32_t TYPE_DAMAGE_OR_HP = 7; // TYPE.DAMAGE and TYPE.HP share the value 7
inline constexpr int32_t TYPE_MP = 21;
inline constexpr int32_t LOG_HEAL = 3;
inline constexpr int32_t LOG_MPHEAL = 4;
inline constexpr int32_t CRITICAL_DISPLAY_CODE = 12; // SM_ATTACK_STATUS.java:12, written for a critical hit
inline constexpr int32_t LOG_BLEED = 26;
inline constexpr int32_t LOG_PROCATKINSTANT = 93;
inline constexpr int32_t LOG_REGULAR = 191;

// ---- stats ----------------------------------------------------------------------------------------------------------------------------------

/** Java `creature.getGameStats().getStat(stat, base).getCurrent()`: the base run through every stat function of the stat */
inline int32_t statOf(gameserver::model::gameobjects::Creature& creature, gameserver::model::stats::container::StatEnum stat, float base) {
	return creature.getGameStats()->getStat(stat, base)->getCurrent();
}

/**
 * The two halves of statOf that the client is sent apart (SM_STATS_INFO writes a stat's base and its bonus) and that decide the order of the
 * functions (StatFunction.java:54: a base RATE has priority 20 and a base ADD 30, before the bonus RATE 50 and the bonus ADD 60)
 */
inline int32_t statBaseOf(gameserver::model::gameobjects::Creature& creature, gameserver::model::stats::container::StatEnum stat, float base) {
	return creature.getGameStats()->getStat(stat, base)->getBase();
}

inline int32_t statBonusOf(gameserver::model::gameobjects::Creature& creature, gameserver::model::stats::container::StatEnum stat, float base) {
	return creature.getGameStats()->getStat(stat, base)->getBonus();
}

/**
 * Adds `value` to one stat of the creature as a bonus, through a stat function without an owner (RoahCustomInstanceHandler adds its functions the
 * same way; tests/effects_al/EffectTemplateTest.cpp's addStat)
 */
inline void addStat(gameserver::model::gameobjects::Creature& creature, gameserver::model::stats::container::StatEnum stat, int32_t value) {
	namespace functions = gameserver::model::stats::calc::functions;
	creature.getGameStats()->addEffect(nullptr,
		{Ptr<functions::IStatFunction>(functions::RcStatFunction<functions::StatAddFunction>::create(stat, value, true))});
}

// ---- fixture --------------------------------------------------------------------------------------------------------------------------------

/**
 * EffectWorldTest with the world chunk's test holders (see the file comment), skill templates bound through the real binder, and the two drivers
 * of the engine: `cast` is Java's `new Effect(effector, effected, template, level)`, `initialize()`, `applyEffect()` (SkillEngine.applyEffect,
 * SkillEngine.java:174-179; Skill.endCast creates and applies its effects the same way), and `observe` gives an effected npc a player who sees it,
 * with a connection that records what that player is sent (the npc's SM_ABNORMAL_EFFECT and SM_ATTACK_STATUS broadcasts).
 */
class EffectClassTest : public EffectWorldTest {
protected:
	void SetUp() override {
		ASSERT_TRUE(world::test::publishTestStaticData()) << "this binary publishes no real data";
		EffectWorldTest::SetUp();
		// SM_STATS_INFO, which a connected player is sent whenever an effect changes its stats, reads the experience table
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(
			xml::bindString<dataholders::PlayerExperienceTable>(experienceContext, cp::PLAYER_EXPERIENCE_TABLE_XML));
		// SM_NPC_INFO's town id without TownService (see the file comment)
		lookups.townIdByPosition = [](gameserver::model::gameobjects::Creature&) { return 0; };
		network::aion::serverpackets::detail::setPacketLookupsForTests(&lookups);
		world::knownlist::KnownList::resetNotifyFailureCountForTests();
	}

	void TearDown() override {
		EXPECT_EQ(world::knownlist::KnownList::notifyFailureCount(), 0u)
			<< "a see/notSee notification threw and KnownList swallowed it (the log has the exception)";
		for (Ref<Player>& player : players)
			player->setClientConnection(nullptr);
		clients.clear();
		EffectWorldTest::TearDown();
		network::aion::serverpackets::detail::setPacketLookupsForTests(nullptr);
		dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests();
	}

	/** Binds a <skill_template> with its <effects> through the real binder (skill_templates.xml's JAXB mapping); the test keeps it */
	const model::SkillTemplate* bindSkill(const std::string& xmlText) {
		xml::LoadContext context;
		return keep(xml::bindString<model::SkillTemplate>(context, xmlText));
	}

	/** The effect template at `index` of a bound skill's <effects> */
	static const effect::EffectTemplate& effectOf(const model::SkillTemplate& skill, size_t index) {
		return *skill.getEffects()->getEffects().at(index);
	}

	/**
	 * Java `Effect effect = new Effect(effector, effected, template, level); effect.initialize(); effect.applyEffect();`. Effect.applyEffect wraps
	 * what a template throws ("Error applying effect of skill ..."), so a failure also reports the cause chain before it is rethrown.
	 */
	static Ref<model::Effect> cast(Creature& effector, Creature& effected, const model::SkillTemplate* skill, int32_t level) {
		Ref<model::Effect> effect = model::Effect::create(effector, Ptr<Creature>(effected), skill, level);
		try {
			effect->initialize();
			effect->applyEffect();
		} catch (const std::exception& e) {
			ADD_FAILURE() << causeChain(e);
			throw;
		}
		return effect;
	}

	/** "type: message" of an exception and of every cause it wraps (commons::utils::Exception::cause) */
	static std::string causeChain(const std::exception& e) {
		std::string chain = commons::utils::exceptionTypeName(e) + ": " + e.what();
		if (const auto* wrapper = dynamic_cast<const commons::utils::Exception*>(&e); wrapper != nullptr && wrapper->cause()) {
			try {
				std::rethrow_exception(wrapper->cause());
			} catch (const std::exception& cause) {
				chain += "\n  caused by " + causeChain(cause);
			} catch (...) {
				chain += "\n  caused by a non-std exception";
			}
		}
		return chain;
	}

	/** A connection for the player (Java PlayerEnterWorldService: the connection knows the player and the player the connection) */
	cp::RecordingAionConnection& connect(Player& player, gameserver::model::account::Account& account) {
		clients.push_back(std::make_unique<cp::TestClient>());
		clients.back()->enterWorld(player, account);
		(*clients.back())->clearSent();
		return **clients.back();
	}

	/** A player at (500, 500, 100) who knows and sees the npc, with a recording connection (the npc's broadcasts reach it) */
	cp::RecordingAionConnection& observe(Npc& npc, int32_t observerId) {
		Ref<Player> observer = makePlayer(observerId);
		gameserver::model::account::Account& account = *accounts.back();
		EXPECT_TRUE(KnownListPairing::pair(npc, *observer));
		return connect(*observer, account);
	}

	/** A MONSTER npc (hostile to the players, so AggroList.isAware answers true and SM_NPC_INFO finds a relation) at the given spot */
	Ref<Npc> makeMonster(int32_t npcId, float x = 505, float y = 500) {
		if (!tribeRelationsPublished)
			publishTribeRelations();
		return makeNpc(npcId, {}, x, y, 100, "MONSTER");
	}

	std::vector<std::unique_ptr<cp::TestClient>> clients;
	xml::LoadContext experienceContext;
	network::aion::serverpackets::detail::PacketLookupsForTests lookups{};
};

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
