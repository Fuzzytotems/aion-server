// P6I-10 (docs/design/p6-instances-plan.md, I4 split): AturamSkyFortressInstance (game-server/data/handlers/instance) on a real instance of
// its map. The doors (the fixture has no static doors: setDoorState only warns), the walks (no walker rows), the zone entry (no zones) and the
// skills' effects are left to a gate.

#include "I4TestSupport.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <regex>
#include <sstream>
#include <string>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/handlers/ai/GeneralNpcAI.h"
#include "aion/gameserver/handlers/instance/AturamSkyFortressInstance.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatAddFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatFunction.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/stats/container/StatEnum.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"

namespace aion::gameserver::handlers::instance::test {
namespace {

using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

class AturamTest : public InstanceHandlerTest {
protected:
	void SetUp() override {
		InstanceHandlerTest::SetUp();
		instance = &create(ATURAM_SKY_FORTRESS, AturamSkyFortressInstance_instanceFactory);
		enter(*instance, 300.0f, 300.0f, 300.0f);
	}

	runtime::Ptr<Npc> at(int32_t npcId, float x = 300.0f, float y = 300.0f) { return spawnNpc(*instance, npcId, x, y, 300.0f, int8_t{0}); }

	runtime::Ptr<Npc> dies(int32_t npcId) {
		runtime::Ptr<Npc> npc = at(npcId);
		instance->getInstanceHandler()->onDie(*npc);
		return npc;
	}

	/** A root AI on the npc (the fixture's npcs have a DummyAI; a skill's use asks for an NpcAI) */
	static void withAi(Npc& npc) {
		auto root = std::make_unique<ai::GeneralNpcAI>(npc);
		ai::GeneralNpcAI& typed = *root;
		npc.replaceAi(std::move(root));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
	}

	void use(Npc& npc) { instance->getInstanceHandler()->handleUseItemFinish(runtime::Ptr<Player>(player()), npc); }

	size_t count(std::initializer_list<int32_t> ids) {
		size_t n = 0;
		for (int32_t id : ids)
			n += npcs(*instance, id).size();
		return n;
	}

	world::WorldMapInstance* instance = nullptr;
};

TEST_F(AturamTest, TheHandlerIsTheJavaClassOfItsMap) {
	std::ifstream in(std::filesystem::path(AION_GAMESERVER_JAVA_DIR) / "data/handlers/instance/AturamSkyFortressInstance.java");
	std::stringstream text;
	text << in.rdbuf();
	std::string source = text.str();
	std::smatch match;
	ASSERT_TRUE(std::regex_search(source, match, std::regex(R"(@InstanceID\((\d+)\))")));
	EXPECT_EQ(std::stoi(match[1].str()), ATURAM_SKY_FORTRESS);
	EXPECT_NE(dynamic_cast<AturamSkyFortressInstance*>(instance->getInstanceHandler().get()), nullptr);
}

/** AturamSkyFortressInstance.java:49-71, 77-79: the objects' and bosses' deaths bring their pieces, at their places */
TEST_F(AturamTest, TheDeathsBringTheirPieces) {
	const std::pair<int32_t, int32_t> pieces[] = {{702651, 282281}, {700982, 282279}, {700983, 282278}, {702650, 282277}, {702652, 282280},
		{217373, 730375}};
	for (const auto& [dead, piece] : pieces) {
		EXPECT_EQ(count({piece}), 0u) << dead;
		dies(dead);
		EXPECT_EQ(count({piece}), 1u) << dead;
	}
	EXPECT_FLOAT_EQ(npcs(*instance, 282281).front()->getX(), 524.1896f);
	EXPECT_EQ(npcs(*instance, 282278).front()->getHeading(), 89);
	runtime::Ptr<Npc> hugen = at(217371, 310.0f, 320.0f);
	instance->getInstanceHandler()->onDie(*hugen);
	ASSERT_EQ(count({730374}), 1u);
	EXPECT_FLOAT_EQ(npcs(*instance, 730374).front()->getX(), 310.0f) << "at the boss";
	EXPECT_FLOAT_EQ(npcs(*instance, 730374).front()->getY(), 320.0f);
}

/** :72-76: the shield generator's death takes it and its barriers, and tells Hugen's shield is down */
TEST_F(AturamTest, TheShieldGeneratorTakesTheBarriers) {
	runtime::Ptr<Npc> barrier = at(701030);
	runtime::Ptr<Npc> other = at(900200);
	runtime::Ptr<Npc> generator = dies(701043);
	EXPECT_FALSE(generator->isSpawned());
	EXPECT_FALSE(barrier->isSpawned());
	EXPECT_TRUE(other->isSpawned());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_00())), 1);
}

/** :118-140: each power generator goes and tells the next stage while Hugen lives */
TEST_F(AturamTest, TheGeneratorsWeakenHugen) {
	publishSkills({19406, 19407, 19408, 18117});
	runtime::Ptr<Npc> hugen = at(217371, 400.0f, 400.0f);
	withAi(*hugen);
	SM_SYSTEM_MESSAGE (*const stages[])() = {&SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_01, &SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_02,
		&SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_03, &SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_04};
	for (size_t i = 0; i < 4; ++i) {
		runtime::Ptr<Npc> generator = dies(701029);
		EXPECT_FALSE(generator->isSpawned()) << i;
		for (size_t j = 0; j < 4; ++j)
			EXPECT_EQ(countSent(serialized(stages[j]())), j <= i ? 1 : 0) << i << " " << j;
	}
}

/** :120: without Hugen the generators only go */
TEST_F(AturamTest, TheGeneratorsTellNothingWithoutHugen) {
	runtime::Ptr<Npc> generator = dies(701029);
	EXPECT_FALSE(generator->isSpawned());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_HugenNM_01())), 0);
}

/** :81-92, 143-168: the fourth officer sends the officers' patrol, the eighth Marbata's; each officer goes */
TEST_F(AturamTest, TheOfficersSendThePatrols) {
	for (int32_t i = 1; i <= 3; ++i)
		EXPECT_FALSE(dies(217370)->isSpawned());
	EXPECT_EQ(count({217655, 217656, 217369, 218577}), 0u);
	dies(217370);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_3FDoor_311())), 1);
	EXPECT_EQ(count({217655}), 3u);
	EXPECT_EQ(count({217656}), 1u);
	EXPECT_EQ(count({217369}), 2u);
	EXPECT_FLOAT_EQ(npcs(*instance, 217656).front()->getX(), 144.11845f);
	for (int32_t i = 5; i <= 7; ++i)
		dies(217370);
	EXPECT_EQ(count({218577}), 0u);
	dies(217370);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_3FDoor_311())), 2) << "Marbata's event tells it again";
	EXPECT_EQ(count({218577}), 1u);
	EXPECT_EQ(count({217655}), 5u);
	EXPECT_FLOAT_EQ(npcs(*instance, 218577).front()->getX(), 193.45583f);
	executor().advance(std::chrono::milliseconds(2000)); // the walks start (no walker rows: they stay)
	EXPECT_EQ(count({218577}), 1u);
}

/** :93-104: the first chief sends the officers' patrol again, the second nothing more; each chief goes */
TEST_F(AturamTest, TheChiefsSendOnePatrol) {
	EXPECT_FALSE(dies(217656)->isSpawned());
	EXPECT_EQ(count({217656}), 1u) << "the patrol's chief";
	EXPECT_EQ(count({217655}), 3u);
	EXPECT_FALSE(dies(217656)->isSpawned());
	EXPECT_EQ(count({217655}), 3u);
	executor().advance(std::chrono::milliseconds(10000));
}

/** :105-118: Marbata's death brings Marbata's true form; its death gives 540 AP to the most damaging player and tells the door */
TEST_F(AturamTest, MarbataGivesTheApToTheBestPlayer) {
	dies(218577);
	ASSERT_EQ(count({217382}), 1u);
	runtime::Ptr<Npc> marbata = npcs(*instance, 217382).front();
	EXPECT_FLOAT_EQ(marbata->getX(), 258.3894f);
	EXPECT_EQ(marbata->getHeading(), 80);
	hit(*marbata);
	int32_t ap = player().getAbyssRank()->getAp();
	instance->getInstanceHandler()->onDie(*marbata);
	EXPECT_EQ(player().getAbyssRank()->getAp(), ap + 540);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_3FDoor_322())), 1);
}

/** :130-134: the patrol's soldiers go when they die; other npcs stay */
TEST_F(AturamTest, TheSoldiersGo) {
	for (int32_t id : {217369, 217368, 217655})
		EXPECT_FALSE(dies(id)->isSpawned()) << id;
	EXPECT_TRUE(dies(900200)->isSpawned());
}

/** :37-39, 179-182: after the instance's destroy no death does anything */
TEST_F(AturamTest, NothingHappensAfterTheDestroy) {
	instance->getInstanceHandler()->onInstanceDestroy();
	EXPECT_TRUE(dies(217655)->isSpawned());
	dies(702651);
	EXPECT_EQ(count({282281}), 0u);
}

/** :184-207: the doping objects: the heal gives 5205 HP and MP and goes; the buff tells it; another object does nothing */
TEST_F(AturamTest, TheDopingObjects) {
	publishSkills({19520});
	// the fixture's player has less than 5205 max hp: a bonus of 10000 (a stat function without an owner) shows the heal's exact amount
	namespace functions = model::stats::calc::functions;
	player().getGameStats()->addEffect(nullptr, {runtime::Ptr<functions::IStatFunction>(
		functions::RcStatFunction<functions::StatAddFunction>::create(model::stats::container::StatEnum::MAXHP, 10000, true))});
	ASSERT_GT(player().getLifeStats()->getMaxHp(), 5206);
	player().getLifeStats()->setCurrentHp(1);
	player().getLifeStats()->setCurrentMp(1);
	runtime::Ptr<Npc> heal = at(730398);
	use(*heal);
	EXPECT_EQ(player().getLifeStats()->getCurrentHp(), 5206);
	EXPECT_EQ(player().getLifeStats()->getCurrentMp(), std::min(5206, player().getLifeStats()->getMaxMp()));
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_Doping_02())), 1);
	EXPECT_FALSE(heal->isSpawned());
	runtime::Ptr<Npc> buff = at(730397);
	withAi(*buff);
	use(*buff);
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_Doping_01())), 1);
	EXPECT_TRUE(buff->isSpawned());
	runtime::Ptr<Npc> other = at(900200);
	use(*other);
	EXPECT_TRUE(other->isSpawned());
	EXPECT_EQ(countSent(serialized(SM_SYSTEM_MESSAGE::STR_MSG_IDStation_Doping_01())), 1);
}

} // namespace
} // namespace aion::gameserver::handlers::instance::test
