#pragma once

// Test support of the M5e effect classes of this chunk (m5e-plan.md E-01, §15.4: P5-03's 20 classes a Daeva of levels 10-20, the skills those
// skills launch and the monsters of Verteron and Altgard reach). The fixture is EffectClassTest (EffectClassTestSupport.h) with the additions
// these classes need:
// - SKILL_DATA holds every template a case bound (StateEffectsTest's bindSkill): the classes launch other skills by id through SkillEngine
//   (CondSkillLauncherEffect 563 -> 8930, AuraEffect 1809 -> 8998, DelayedSkillEffect 4285 -> 8905, CarveSignetEffect 3385 -> 8303-8305);
// - GeoService has its empty GeoMaps (init() with geo data off): DashEffect and BackDashEffect ask it for the closest collision;
// - a case fails when it reaches an AION_UNPORTED site (TearDown), so a green case ran its path through ported code only;
// - `forced` applies a template the way SkillEngine.applyEffectDirectly does (ForceType.DEFAULT: no condition, pre-effect, dodge or resist
//   roll), for the monster skills whose first position is a hit that the case does not want to roll;
// - `daeva` makes a player with its FlyController (the effects that stun, fear or fell a player reach it).
//
// Two players who see each other are sent each other's SM_PLAYER_INFO (PlayerController.see), whose parts and lookups this fixture does not
// build, so the broadcasts of these classes are observed through an npc (EffectClassTest::observe) where both arms share the call.
//
// Where a case leaves positions of a data template out, it says which and why: the left-out classes are P5-04's (effects-mz, ported in a
// parallel lane) or roll a dodge the case is not about.

#include "EffectClassTestSupport.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/model/stats/container/CreatureGameStats.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/skillengine/model/Effect_ForceType.h"
#include "aion/gameserver/skillengine/model/SkillTemplate.h"
#include "aion/gameserver/world/geo/GeoService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::skillengine::effecttest {
namespace {

/** A <skill_template> of the given attributes with the given <effects> body (the parts of a data template an Effect reads) */
inline std::string templateXml(std::string_view attributes, std::string_view effects) {
	return "<skill_template " + std::string(attributes) + "><effects>" + std::string(effects) + "</effects></skill_template>";
}

class DaevaEffectTest : public EffectClassTest {
protected:
	void SetUp() override {
		EffectClassTest::SetUp();
		EFFECT_TEST_SCOPE;
		static const bool geoInitialised = [] {
			world::geo::GeoService::getInstance().init(); // geo data off: one empty GeoMap per test map
			return true;
		}();
		static_cast<void>(geoInitialised);
		runtime::resetUnportedHitsForTests();
	}

	void TearDown() override {
		// every path a case drives through these classes is ported to its end: no AION_UNPORTED site was reached
		EXPECT_EQ(runtime::unportedHitCount(), 0u) << [] {
			std::string sites;
			for (const runtime::UnportedHit& hit : runtime::unportedHits())
				sites += "\n  " + hit.file + ":" + std::to_string(hit.line) + " " + hit.function;
			return sites;
		}();
		EffectClassTest::TearDown();
	}

	/**
	 * Binds a <skill_template> through the real binder and publishes it in DataManager::SKILL_DATA with every template this case bound before,
	 * and answers the published template (SkillEngine.applyEffect(skillId, ...) and EffectController.findBySkillId look templates up there). The
	 * holder is immortal, so a republish only forgets the previous one and the templates already in use stay valid.
	 */
	const model::SkillTemplate* bindSkill(const std::string& xmlText) {
		const std::string::size_type at = xmlText.find(R"(skill_id=")");
		EXPECT_NE(at, std::string::npos);
		const int32_t skillId = std::stoi(xmlText.substr(at + 10));
		publishedXml += xmlText;
		dataholders::DataManager::SKILL_DATA.resetForTests();
		publishSkillData(publishedXml);
		return dataholders::DataManager::SKILL_DATA->getSkillTemplate(skillId);
	}

	/** Java `new Effect(effector, effected, template, level); effect.initialize();` without applyEffect */
	static Ref<model::Effect> calculated(Creature& effector, Creature& effected, const model::SkillTemplate* skill, int32_t level = 1) {
		Ref<model::Effect> effect = model::Effect::create(effector, Ptr<Creature>(effected), skill, level);
		effect->initialize();
		return effect;
	}

	/** SkillEngine.applyEffectDirectly's `new Effect(effector, effected, template, level, null, ForceType.DEFAULT)`, initialize, applyEffect */
	static Ref<model::Effect> forced(Creature& effector, Creature& effected, const model::SkillTemplate* skill, int32_t level = 1) {
		Ref<model::Effect> effect =
			model::Effect::create(effector, Ptr<Creature>(effected), skill, level, std::nullopt, model::Effect_ForceType::DEFAULT);
		try {
			effect->initialize();
			effect->applyEffect();
		} catch (const std::exception& e) {
			ADD_FAILURE() << causeChain(e);
			throw;
		}
		return effect;
	}

	/** The magical resist rate of a magical effect leaves nothing to roll: the effected's resist is not above the caster's accuracy */
	static void assertNoMagicalResist(Creature& caster, Creature& target) {
		ASSERT_LE(target.getGameStats()->getMResist()->getCurrent() - caster.getGameStats()->getMAccuracy()->getCurrent(), 0);
	}

	/**
	 * A spawned player of the class (EffectWorldTest::makePlayer) with the FlyController PlayerService.getPlayer gives every player: the effects
	 * that stun, fear or fell a player end its glide or flight through it (StunEffect.startEffect, FallEffect.applyEffect)
	 */
	Ref<Player> daeva(int32_t objectId, gameserver::model::PlayerClass playerClass, float x = 500, float y = 500, float z = 100) {
		Ref<Player> player = makePlayer(objectId, playerClass, gameserver::model::Race::ELYOS, x, y, z);
		player->setFlyController(std::make_unique<controllers::FlyController>(*player));
		return player;
	}

	/**
	 * tribe_relations.xml reduced as EffectTestSupport.h's EFFECT_TEST_TRIBE_RELATIONS_XML (MONSTER hostile to the player tribes) plus the
	 * neutral GENERAL tribe of makeNpc's default template, so Npc.isEnemy can be asked of such an npc (TribeRelationService reads the npc's tribe
	 * entry). Call it before the first makeMonster; TearDown forgets it with EffectWorldTest's flag.
	 */
	void publishTribeRelationsWithNeutral() {
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(xml::bindString<dataholders::TribeRelationsData>(tribeRelationsContext,
			R"(<tribe_relations><tribe name="PC"/><tribe name="PC_DARK"/><tribe name="MONSTER"><hostile>PC</hostile><hostile>PC_DARK</hostile>)"
			R"(</tribe><tribe name="GENERAL"/></tribe_relations>)"));
		tribeRelationsPublished = true;
	}

	/** A connection for a player this case made (the last account belongs to the last player made) */
	cp::RecordingAionConnection& connectLast(Player& player) { return connect(player, *accounts.back()); }

	/** How many of the connection's SM_SYSTEM_MESSAGEs are exactly `message` */
	static int64_t countMessages(cp::RecordingAionConnection& connection, network::aion::serverpackets::SM_SYSTEM_MESSAGE message) {
		const std::vector<uint8_t> expected = cp::serialized(message, &connection);
		const std::vector<std::vector<uint8_t>> messages = packetsOf<network::aion::serverpackets::SM_SYSTEM_MESSAGE>(connection);
		return std::count(messages.begin(), messages.end(), expected);
	}

	/** A seed whose first Rnd.chance() of the calling thread satisfies `accept` (the thread is left seeded with it, the stream not advanced) */
	template <class Predicate>
	static void seedWhereFirstChance(Predicate accept) {
		for (uint64_t seed = 1; seed < 100000; ++seed) {
			Rnd::seedCurrentThreadForTests(seed);
			if (accept(Rnd::chance())) {
				Rnd::seedCurrentThreadForTests(seed);
				return;
			}
		}
		ADD_FAILURE() << "no seed found";
	}

	std::string publishedXml;
};

} // namespace
} // namespace aion::gameserver::skillengine::effecttest
