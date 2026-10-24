// D12-CP3 (owner, 2026-10-13: D12 = port): the Eternal Challenge's AIs (data/handlers/ai/instance/custom/eternalChallenge and
// ai/portals/CustomInstanceTeleporter), outside the Roah handler's instance and without a database (the DAO answers "no rank" and false), as
// TravelAiHandlersTest drives PortalAI: the .cpp files are compiled into this executable (I1's library is not linked here; lease in
// chunks.cmake). In the Roah instance the AIs are the gate's and the retest's (the instance needs the custom npc rows and a live player).

#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceBossAI.cpp"
#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceBulkyAI.cpp"
#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceDominatorAI.cpp"
#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceInfoBoard.cpp"
#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceReianTeleportShouter.cpp"
#include "aion/gameserver/handlers/ai/instance/custom/eternalChallenge/CustomInstanceScapiAI.cpp"
#include "aion/gameserver/handlers/ai/portals/CustomInstanceTeleporter.cpp"

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.bind.h"
#include "aion/gameserver/dataholders/PlayerExperienceTable.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

using namespace ::aion::gameserver::handlers::ai::instance::custom::eternalChallenge;
using ::aion::gameserver::handlers::ai::portals::CustomInstanceTeleporter;
using model::gameobjects::Npc;
using serverpackets::SM_MESSAGE;
using serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t SCAPI = 218483;
constexpr int32_t BOARD = 790101; // synthetic rows: the AIs are installed by hand
constexpr int32_t REIAN = 790102;
constexpr int32_t TELEPORTER = 790103;
constexpr int32_t BOSS = 231918;

std::string row(int32_t npcId, const char* name) {
	return "<npc_template npc_id=\"" + std::to_string(npcId) + "\" level=\"65\" name=\"" + name
		   + "\" name_id=\"350971\" height=\"2\" group_drop=\"NONE\" rank=\"DISCIPLINED\" rating=\"NORMAL\" tribe=\"GENERAL\" type=\"GENERAL\" ai=\"general\""
			 " sangle=\"0\" attack_speed=\"2000\" hpgauge=\"3\"><stats maxHp=\"172\" /><bound_radius front=\"0.25\" side=\"0.35\" upper=\"2\" />"
			 "<talk_info distance=\"5\" is_dialog=\"true\" can_talk_invisible=\"false\" /></npc_template>";
}

const model::templates::npc::NpcTemplate* templateOf(int32_t npcId) {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, "<npc_templates>" + row(SCAPI, "scapi") + row(BOARD, "board") + row(REIAN, "reian")
															   + row(TELEPORTER, "teleporter") + row(BOSS, "boss") + "</npc_templates>")
			.release();
	}();
	return holder->getNpcTemplate(npcId);
}

class CustomSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	CustomSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** The AIs with their protected hooks callable */
class InfoBoardProbe final : public CustomInstanceInfoBoard {
public:
	using CustomInstanceInfoBoard::CustomInstanceInfoBoard;
	using CustomInstanceInfoBoard::handleDialogStart;
};

class ShouterProbe final : public CustomInstanceReianTeleportShouter {
public:
	using CustomInstanceReianTeleportShouter::CustomInstanceReianTeleportShouter;
	using CustomInstanceReianTeleportShouter::handleCreatureMoved;
};

class TeleporterProbe final : public CustomInstanceTeleporter {
public:
	using CustomInstanceTeleporter::CustomInstanceTeleporter;
	using CustomInstanceTeleporter::handleUseItemFinish;
};

class BossProbe final : public CustomInstanceBossAI {
public:
	using CustomInstanceBossAI::CustomInstanceBossAI;
	using CustomInstanceBossAI::handleSpawned;
};

class CustomInstanceAiTest : public ItemPacketTest {
protected:
	void SetUp() override {
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		useInstance = world::WorldMap2DInstance::create(*map, 2, 0, 0, [](world::WorldMapInstance& instance) {
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(
				::aion::gameserver::instance::handlers::GeneralInstanceHandler::create(instance));
		});
		player().setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(100.0f, 100.0f, 50.0f)));
		player().getPosition()->setIsSpawned(true);
		clearSent();
	}

	void TearDown() override {
		npcs.clear();
		spawnGroups.clear();
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		useInstance->detachInstanceHandler();
		useInstance = nullptr;
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	Npc& npcAt(int32_t npcId, float dx) {
		const float x = 100.0f + dx;
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<CustomSpawnTemplate>(*group, x, 100.0f, 50.0f));
		runtime::Ref<Npc> npc = model::gameobjects::VisibleObject::create<Npc>(std::make_unique<controllers::NpcController>(), spawn, templateOf(npcId));
		npc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*npc));
		npc->setEffectController(std::make_unique<controllers::effect::EffectController>(*npc));
		npc->setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(x, 100.0f, 50.0f)));
		npc->getPosition()->setIsSpawned(true);
		spawnGroups.push_back(group);
		npcs.push_back(npc);
		return *npc;
	}

	template <class AI>
	AI& install(Npc& npc) {
		auto ai = std::make_unique<AI>(npc);
		AI& typed = *ai;
		npc.replaceAi(std::move(ai));
		typed.setStateIfNot(gameserver::ai::AIState::IDLE);
		return typed;
	}

	/** whether a sent packet holds the text as UTF-16LE (SM_MESSAGE writes its text with writeS) */
	bool sentText(std::string_view text) {
		std::vector<uint8_t> needle;
		for (char c : text) {
			needle.push_back(static_cast<uint8_t>(c));
			needle.push_back(0);
		}
		for (const std::vector<uint8_t>& bytes : sent())
			if (std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end()) != bytes.end())
				return true;
		return false;
	}

	bool wasSent(const std::vector<uint8_t>& packet) {
		for (const std::vector<uint8_t>& bytes : sent())
			if (bytes == packet)
				return true;
		return false;
	}

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	runtime::Ref<world::WorldMapInstance> useInstance;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
	xml::LoadContext tableContext;
};

/** CustomInstanceScapiAI.java:19-22, CustomInstanceBossAI.java:64-72: the damage modifiers */
TEST_F(CustomInstanceAiTest, TheDamageModifiers) {
	Npc& scapi = npcAt(SCAPI, 3);
	CustomInstanceScapiAI& scapiAi = install<CustomInstanceScapiAI>(scapi);
	EXPECT_EQ(scapiAi.modifyDamage(player(), 5000.0f, nullptr), 1.0f);
	Npc& boss = npcAt(BOSS, 4);
	BossProbe& bossAi = install<BossProbe>(boss);
	EXPECT_EQ(bossAi.modifyDamage(player(), 1000.0f, nullptr), 1000.0f * 0.42f);
	EXPECT_EQ(bossAi.modifyOwnerDamage(1000.0f, player(), nullptr), 1000.0f * 0.58f);
	bossAi.handleSpawned(); // not in the Roah handler's instance: nothing is copied
	EXPECT_EQ(boss.getLifeStats()->getMaxHp(), 172);
}

/** C++-dev only: the boss's ATTACK event (its aggro list takes hate) runs CustomInstanceBossAI.handleAttack; outside the Roah handler's
 * instance it tells no handler (the Roah handler's report is driven in RoahCustomInstanceHandlerTest) */
TEST_F(CustomInstanceAiTest, TheBossAttackEventOutsideTheRoahInstance) {
	Npc& boss = npcAt(BOSS, 4);
	install<BossProbe>(boss);
	EXPECT_NO_THROW(boss.getAi().onCreatureEvent(gameserver::ai::event::AIEventType::ATTACK, player()));
}

/** CustomInstanceTeleporter.java:22-35: below level 65 the level message; at 65 without a stored rank the entry, whose updateLastEntry fails
 * without a database: the shugo message */
TEST_F(CustomInstanceAiTest, TheTeleporter) {
	Npc& teleporter = npcAt(TELEPORTER, 2);
	TeleporterProbe& ai = install<TeleporterProbe>(teleporter);
	ai.handleUseItemFinish(player());
	EXPECT_TRUE(wasSent(serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_LEVEL())));
	clearSent();
	std::string table = "<player_experience_table>";
	for (int64_t level = 0; level < 66; level++)
		table += "<exp>" + std::to_string(level * 1000) + "</exp>";
	dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.resetForTests(); // the base fixture's short table; its TearDown resets this one
	dataholders::DataManager::PLAYER_EXPERIENCE_TABLE.publish(xml::bindString<dataholders::PlayerExperienceTable>(tableContext, table + "</player_experience_table>"));
	player().getCommonData()->setLevel(65);
	ai.handleUseItemFinish(player());
	EXPECT_TRUE(sentText("Sorry. Some shugo broke our database, please report this in our bugtracker :(")) << "CustomInstanceService.onEnter's first arm";
	EXPECT_FALSE(wasSent(serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_LEVEL())));
}

/** CustomInstanceInfoBoard.java:22-37: the master name; a dialog within 3 seconds of the last one is ignored (the first one asks
 * openLeaderboard, whose page template this test has not cached: Java's NullPointerException) */
TEST_F(CustomInstanceAiTest, TheInfoBoard) {
	Npc& board = npcAt(BOARD, 2);
	InfoBoardProbe& ai = install<InfoBoardProbe>(board);
	EXPECT_EQ(board.getMasterName(), std::optional<std::string>("Eternal Challenge"));
	EXPECT_THROW(ai.handleDialogStart(player()), runtime::NullPointerException) << "the leaderboard is opened";
	EXPECT_NO_THROW(ai.handleDialogStart(player())) << "flood protection: within 3 s nothing happens";
}

/** CustomInstanceReianTeleportShouter.java:27-43: the guards: a player 30 m away, and an npc whose region no player has woken, hear nothing (the invitation itself is the retest's: this
 * fixture wakes no region) */
TEST_F(CustomInstanceAiTest, TheShouter) {
	Npc& reian = npcAt(REIAN, 30);
	ShouterProbe& ai = install<ShouterProbe>(reian);
	ai.handleCreatureMoved(player());
	EXPECT_TRUE(sent().empty()) << "30 m away";
	Npc& near = npcAt(REIAN, 5);
	ShouterProbe& nearAi = install<ShouterProbe>(near);
	clearSent();
	nearAi.handleCreatureMoved(player());
	EXPECT_TRUE(sent().empty()) << "no player has woken the region (MapRegion.isActive)";
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
