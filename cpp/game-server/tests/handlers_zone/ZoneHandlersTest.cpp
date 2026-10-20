// P6 leftovers (lane C, 2026-10-18): chunk Z1, the zone handlers of game-server/data/handlers/zone - _1012SensoryArea and the PvP zones
// (pvpZones/PvPZone, pvpZones/PvPAreaZone) - driven directly, as ZoneInstance.onEnter/onLeave/onDie call them, on a connected player of a
// plain instance (tests/handlers_inst_1/InstanceFixture.h). Left to a gate: the coliseum teleports (TeleportService) and a real zone transition.

#include "ZoneTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/controllers/observer/AbstractQuestZoneObserver.h"
#include "aion/gameserver/dataholders/QuestsData.bind.h"
#include "aion/gameserver/dataholders/QuestsData.h"
#include "aion/gameserver/dataholders/SkillData.bind.h"
#include "aion/gameserver/dataholders/SkillData.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/handlers/zone/_1012SensoryArea.h"
#include "aion/gameserver/handlers/zone/pvpZones/PvPAreaZone.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/geometry/PolyArea.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/templates/zone/Points.h"
#include "aion/gameserver/model/templates/zone/WorldZoneTemplate.h"
#include "aion/gameserver/model/templates/zone/ZoneInfo.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUEST_ACTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/world/zone/PvPZoneInstance.h"
#include "aion/gameserver/world/zone/ZoneInstance.h"

namespace aion::gameserver::handlers::zone {
::aion::gameserver::handlers::ZoneFactory _1012SensoryArea_zoneFactory;
}
namespace aion::gameserver::handlers::zone::pvpZones {
::aion::gameserver::handlers::ZoneFactory PvPAreaZone_zoneFactory;
}

namespace aion::gameserver::handlers::instance::test {
namespace {

namespace hz = ::aion::gameserver::handlers::zone;
using model::gameobjects::Creature;
using model::gameobjects::Npc;
using network::aion::serverpackets::SM_QUEST_ACTION;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using questEngine::model::QuestState;

/** A PvP zone instance whose creatures the case puts in directly (ZoneInstance.onEnter would also run the controller hooks) */
class TestPvPZone final : public world::zone::PvPZoneInstance {
	AION_MAKE_REF_FRIEND

public:
	TestPvPZone(int32_t mapId, model::templates::zone::ZoneInfo& template_) : PvPZoneInstance(mapId, template_) {}

	void hold(Creature& creature) { creatures.put(creature.getObjectId(), runtime::Ref<Creature>(creature)); }

protected:
	~TestPvPZone() override = default;
};

/** The same zone as a plain zone instance */
class TestZone final : public world::zone::ZoneInstance {
	AION_MAKE_REF_FRIEND

public:
	TestZone(int32_t mapId, model::templates::zone::ZoneInfo& template_) : ZoneInstance(mapId, template_) {}

	void hold(Creature& creature) { creatures.put(creature.getObjectId(), runtime::Ref<Creature>(creature)); }

protected:
	~TestZone() override = default;
};

/** PvPAreaZone with its protected doTeleport counted */
class CountingPvPAreaZone final : public hz::pvpZones::PvPAreaZone {
	AION_MAKE_REF_FRIEND

public:
	int teleports = 0;

protected:
	void doTeleport(model::gameobjects::player::Player& player, const world::zone::ZoneName* zoneName) override {
		++teleports;
		PvPAreaZone::doTeleport(player, zoneName);
	}
};

class ZoneHandlersTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		// QuestZoneHandler's constructor asks the quest data for its quest (quest_data.xml's row of 1012), plus the fixture's 1006
		dataholders::DataManager::QUEST_DATA.resetForTests();
		dataholders::DataManager::QUEST_DATA.publish(xml::bindString<dataholders::QuestsData>(questContext, R"x(<quests>
	<quest id="1006" name="Ascension" nameId="1102006" quest_zone="Ascension Quests" minlevel_permitted="9" max_repeat_count="1" cannot_share="true" cannot_giveup="true" race_permitted="ELYOS" category="MISSION">
		<rewards exp="73200"/>
	</quest>
	<quest id="1012" name="Masking the Smell" nameId="1102012" minlevel_permitted="11" max_repeat_count="1" race_permitted="ELYOS" category="MISSION"/>
</quests>)x"));
		instance = &create(ZONE_TEST_MAP, generalInstanceFactory);
		enter(*instance, 224.0f, 300.0f, 142.0f);
		zoneTemplate = std::make_unique<model::templates::zone::WorldZoneTemplate>(3000, ZONE_TEST_MAP, 0);
		area = model::geometry::PolyArea::create(zoneTemplate->getName(), ZONE_TEST_MAP, zoneTemplate->getPoints()->getPoint(),
			zoneTemplate->getPoints()->getBottom(), zoneTemplate->getPoints()->getTop());
		zoneInfo = model::templates::zone::ZoneInfo::create(*area, zoneTemplate.get());
		pvpZone = runtime::makeRef<TestPvPZone>(ZONE_TEST_MAP, *zoneInfo);
		plainZone = runtime::makeRef<TestZone>(ZONE_TEST_MAP, *zoneInfo);
	}

	void TearDown() override {
		pvpZone = nullptr;
		plainZone = nullptr;
		zoneInfo = nullptr;
		area = nullptr;
		zoneTemplate.reset();
		InstanceHandlerTest::TearDown();
	}

	runtime::Ptr<Npc> place(int32_t npcId) { return spawnNpc(*instance, npcId, 226.0f, 300.0f, 142.0f, int8_t{0}); }

	int64_t sentTo(network::aion::AionServerPacket&& packet) { return countSent(serialized(std::move(packet))); }

	/** Whether a timed buff of the player outlives the death's effect removal (EffectController.removeAllEffects: canRemoveOnDie, which
	 * PlayerEffectController answers with keepBuffsOnDie) - the only reader of the flag PvPAreaZone sets */
	bool buffSurvivesDeath() {
		if (dataholders::DataManager::SKILL_DATA->getSkillTemplate(9901) == nullptr) {
			dataholders::DataManager::SKILL_DATA.resetForTests();
			dataholders::DataManager::SKILL_DATA.publish(xml::bindString<dataholders::SkillData>(skillContext,
				R"(<skill_data><skill_template skill_id="9901" name="coliseum buff" nameId="1" stack="COLISEUM_BUFF" lvl="1" skilltype="MAGICAL")"
				R"( skillsubtype="NONE" tslot="BUFF" activation="ACTIVE" cooldown="0" duration="60000"><properties first_target="ME" />)"
				R"(<effects><statboost effectid="1" e="1" duration2="60000"><change stat="PHYSICAL_ATTACK" func="ADD" value="7" /></statboost></effects>)"
				R"(</skill_template></skill_data>)"));
		}
		runtime::Ref<skillengine::model::Effect> effect = skillengine::model::Effect::create(player(), runtime::Ptr<Creature>(player()),
			dataholders::DataManager::SKILL_DATA->getSkillTemplate(9901), 1, std::nullopt, skillengine::model::Effect::ForceType::DEFAULT);
		effect->initialize();
		effect->applyEffect();
		const size_t before = player().getEffectController()->getAllEffects().size();
		player().getEffectController()->removeAllEffects();
		const bool kept = player().getEffectController()->getAllEffects().size() == before && before > 0;
		player().getEffectController()->removeAllEffects(true); // logout: every effect, for the next probe
		return kept;
	}

	xml::LoadContext questContext;
	xml::LoadContext skillContext;
	world::WorldMapInstance* instance = nullptr;
	std::unique_ptr<model::templates::zone::WorldZoneTemplate> zoneTemplate;
	runtime::Ref<model::geometry::PolyArea> area;
	runtime::Ref<model::templates::zone::ZoneInfo> zoneInfo;
	runtime::Ref<TestPvPZone> pvpZone;
	runtime::Ref<TestZone> plainZone;
};

/** The two AION_ZONE_HANDLER factories build their classes; the quest zone takes the marker's quest id */
TEST_F(ZoneHandlersTest, TheFactoriesBuildTheirClasses) {
	EXPECT_NE(dynamic_cast<hz::_1012SensoryArea*>(hz::_1012SensoryArea_zoneFactory(1012).get()), nullptr);
	EXPECT_NE(dynamic_cast<hz::pvpZones::PvPAreaZone*>(hz::pvpZones::PvPAreaZone_zoneFactory(0).get()), nullptr);
}

/** _1012SensoryArea.java:21-37: moving in a sensory area at quest var 1 sets var 0 to 2 and updates the quest; another var or no quest: nothing */
TEST_F(ZoneHandlersTest, TheSensoryAreaMovesQuest1012ToStep2) {
	runtime::Ref<hz::_1012SensoryArea> handler = hz::_1012SensoryArea::create(1012);
	runtime::Ref<controllers::observer::AbstractQuestZoneObserver> observer = handler->createObserver(player(), zoneTemplate.get());
	observer->onMoved(1.0f, 2.0f, 1, 100);
	EXPECT_EQ(player().getQuestStateList()->getQuestState(1012), nullptr) << "no quest: nothing";

	runtime::Ref<QuestState> qs = QuestState::create(1012, questEngine::model::QuestStatus::START);
	player().getQuestStateList()->addQuest(1012, *qs);
	observer->onMoved(1.0f, 2.0f, 1, 100);
	EXPECT_EQ(qs->getQuestVarById(0), 0) << "var 0: nothing";
	qs->setQuestVar(3);
	observer->onMoved(1.0f, 2.0f, 1, 100);
	EXPECT_EQ(qs->getQuestVarById(0), 3) << "var 3: nothing";
	qs->setQuestVar(1);
	observer->onMoved(1.0f, 2.0f, 1, 100);
	EXPECT_EQ(qs->getQuestVarById(0), 2);
	EXPECT_EQ(sentTo(SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs)), 1);
	observer->onMoved(1.0f, 2.0f, 1, 100);
	EXPECT_EQ(sentTo(SM_QUEST_ACTION(SM_QUEST_ACTION::ActionType::UPDATE, *qs)), 1) << "step 2: once";
}

/** PvPAreaZone.java:18-28: entering keeps a player's buffs on death when CustomConfig.KEEP_BUFFS_IN_COLISEUM is set; leaving drops it */
TEST_F(ZoneHandlersTest, TheColiseumKeepsBuffsOnDeathByConfig) {
	runtime::Ref<hz::pvpZones::PvPAreaZone> handler = hz::pvpZones::PvPAreaZone::create();
	const bool keep = configs::main::CustomConfig::KEEP_BUFFS_IN_COLISEUM.load();
	configs::main::CustomConfig::KEEP_BUFFS_IN_COLISEUM.store(false);
	handler->onEnterZone(player(), *pvpZone);
	EXPECT_FALSE(buffSurvivesDeath()) << "the option is off";
	configs::main::CustomConfig::KEEP_BUFFS_IN_COLISEUM.store(true);
	handler->onEnterZone(*place(900200), *pvpZone); // an npc: nothing
	handler->onEnterZone(player(), *pvpZone);
	EXPECT_TRUE(buffSurvivesDeath());
	handler->onLeaveZone(*place(900200), *pvpZone);
	EXPECT_TRUE(buffSurvivesDeath()) << "an npc left";
	handler->onLeaveZone(player(), *pvpZone);
	EXPECT_FALSE(buffSurvivesDeath());
	configs::main::CustomConfig::KEEP_BUFFS_IN_COLISEUM.store(keep);
}

/** PvPZone.java:31-54: an npc's death is not handled; a player's death in a plain zone is handled silently */
TEST_F(ZoneHandlersTest, OnlyAPlayersDeathInAPvPZoneIsAnnounced) {
	auto handler = runtime::makeRef<CountingPvPAreaZone>();
	runtime::Ptr<Npc> npc = place(900200);
	EXPECT_FALSE(handler->onDie(player(), *npc, *pvpZone));
	plainZone->hold(player());
	EXPECT_TRUE(handler->onDie(*npc, player(), *plainZone));
	EXPECT_EQ(sentTo(SM_SYSTEM_MESSAGE::STR_MSG_PvPZONE_MY_DEATH_TO_B(npc->getName())), 0);
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT));
	executor().advance(std::chrono::seconds(6));
	EXPECT_EQ(handler->teleports, 0);
}

/** PvPZone.java:35-52: the dying player hears who killed him; 5 s later he is teleported out and the zone hears it */
TEST_F(ZoneHandlersTest, TheDeadIsToldAndTeleportedOutAfterFiveSeconds) {
	auto handler = runtime::makeRef<CountingPvPAreaZone>();
	runtime::Ptr<Npc> killer = place(900200);
	pvpZone->hold(player());
	pvpZone->hold(*killer);
	EXPECT_TRUE(handler->onDie(*killer, player(), *pvpZone));
	EXPECT_EQ(sentTo(SM_SYSTEM_MESSAGE::STR_MSG_PvPZONE_MY_DEATH_TO_B(killer->getName())), 1);
	EXPECT_EQ(sentTo(SM_SYSTEM_MESSAGE::STR_MSG_PvPZONE_HOSTILE_DEATH_TO_ME(player().getName())), 0);
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::TELEPORT));
	executor().advance(std::chrono::milliseconds(4999));
	EXPECT_EQ(handler->teleports, 0);
	EXPECT_EQ(sentTo(SM_SYSTEM_MESSAGE::STR_PvPZONE_OUT_MESSAGE(player().getName())), 0);
	executor().advance(std::chrono::milliseconds(1));
	EXPECT_EQ(handler->teleports, 1);
	EXPECT_EQ(sentTo(SM_SYSTEM_MESSAGE::STR_PvPZONE_OUT_MESSAGE(player().getName())), 1);
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT)) << "removed by the task itself";
}

/** PvPZone.java:37-42: the killer hears whom he killed; a bystander hears both names */
TEST_F(ZoneHandlersTest, TheKillerAndTheBystandersAreTold) {
	auto handler = runtime::makeRef<CountingPvPAreaZone>();
	cptest::PlayerFixture other = cptest::makePlayer(430077, 9477, "Fallen");
	pvpZone->hold(player());
	pvpZone->hold(*other.player);
	EXPECT_TRUE(handler->onDie(player(), *other.player, *pvpZone));
	EXPECT_EQ(sentTo(SM_SYSTEM_MESSAGE::STR_MSG_PvPZONE_HOSTILE_DEATH_TO_ME(other.player->getName())), 1) << "the killer";
	EXPECT_EQ(sentTo(SM_SYSTEM_MESSAGE::STR_MSG_PvPZONE_MY_DEATH_TO_B(player().getName())), 0);
	runtime::Ptr<Npc> killer = place(900200);
	EXPECT_TRUE(handler->onDie(*killer, *other.player, *pvpZone));
	EXPECT_EQ(sentTo(SM_SYSTEM_MESSAGE::STR_MSG_PvPZONE_HOSTILE_DEATH_TO_B(killer->getName(), other.player->getName())), 1) << "a bystander";
	other.player->getController().cancelTask(model::TaskId::TELEPORT);
	other.player->getController().cancelTask(model::TaskId::TELEPORT);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
