// M5i I-3 (m5i-plan.md 15.3): ArtifactAI ("artifact", data/handlers/ai/ArtifactAI.java:41-268), P5-05's root AI under P5-12a's lease of
// handlers/ai/ArtifactAI.* (chunks.cmake): the refusals of an activation, the 10 s channel that a move aborts, the item cost, and the cast
// 13 s later, once or every repeat interval of a repeating artifact.
//
// The artifact is a SiegeNpc on a SiegeSpawnTemplate of a standalone artifact (id 1012), which the cases put into SiegeService's maps as
// initSieges would; its activation is a test row (item 162000002 x2, skill 10034, the rows of ../cm_ak/ItemPacketTestSupport.h). The activator is
// that fixture's player, a real AionConnection whose send queue the cases read, on the fixture's DeterministicExecutor.
// Not covered here: the two question windows of a click (AIActions.addRequest and the client's answers) and the artifact skill's effect on the
// creatures of the artifact's zones (the location has no zones in this executable): the gate's (I-6).

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"
#include "QuestNpcAiTestSupport.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/handlers/ai/ArtifactAI.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/siege/SiegeNpc.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/ArtifactStatus.h"
#include "aion/gameserver/model/siege/SiegeModType.h"
#include "aion/gameserver/model/siege/SiegeRace.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/siegespawns/SiegeSpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

// The factory function the AION_AI marker of ArtifactAI.cpp defines, declared exactly as Registry.ai.gen.cpp declares it
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory ArtifactAI_aiFactory;
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

namespace roots = gameserver::handlers::ai;

using model::TaskId;
using model::siege::ArtifactStatus;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t ARTIFACT_ID = 1012;
constexpr int32_t ACTIVATION_ITEM = 162000002; // Minor Life Potion of ItemPacketTestSupport.h
constexpr int32_t ARTIFACT_SKILL = 10034;      // Increase Natural Healing of ItemPacketTestSupport.h

/** The rows of QuestNpcAiTestSupport.h, bound through NpcData as the server loads npc_templates.xml; kept for the process like DataManager's */
const model::templates::npc::NpcTemplate* artifactNpcTemplate() {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(
			context, "<npc_templates>" + std::string(gameserver::ai::testing::QUEST_NPC_AI_TEMPLATES_XML) + "</npc_templates>")
			.release();
	}();
	return holder->getNpcTemplate(gameserver::ai::testing::BALAUR_LEVEL_2);
}

class ArtifactAiTest : public ItemPacketTest {
protected:
	void SetUp() override {
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
	}

	void TearDown() override {
		services::SiegeService::getInstance().getArtifacts().remove(ARTIFACT_ID);
		services::SiegeService::getInstance().getSiegeLocations().remove(ARTIFACT_ID);
		artifactNpc = nullptr;
		artifact = nullptr;
		spawn = nullptr;
		group = nullptr;
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	/** The artifact location (activation item x2, skill 10034, `extra` attributes) and its artifact npc beside the player, with the real AI */
	roots::ArtifactAI& artifactWith(const std::string& extraActivationAttributes = "", int32_t skill = ARTIFACT_SKILL) {
		locationTemplate = xml::bindString<model::templates::siegelocation::SiegeLocationTemplate>(locationContext,
			R"(<siege_location id="1012" type="ARTIFACT" world="210010000"><artifact_activation item_id="162000002" count="2" skill=")" + std::to_string(skill) +
				R"(" cd="600" )" + extraActivationAttributes + "/></siege_location>");
		artifact = model::siege::ArtifactLocation::create(locationTemplate.get());
		services::SiegeService::getInstance().getArtifacts().put(ARTIFACT_ID, artifact);
		services::SiegeService::getInstance().getSiegeLocations().put(ARTIFACT_ID, artifact);

		group = model::templates::spawns::SpawnGroup::create(210010000, gameserver::ai::testing::BALAUR_LEVEL_2, 0, nullptr);
		spawn = model::templates::spawns::siegespawns::SiegeSpawnTemplate::create(ARTIFACT_ID, model::siege::SiegeRace::BALAUR,
			model::siege::SiegeModType::PEACE, *group, 101.0f, 100.0f, 50.0f, int8_t{0}, 0, std::nullopt, 0);
		artifactNpc = model::gameobjects::VisibleObject::create<model::gameobjects::siege::SiegeNpc>(std::make_unique<controllers::NpcController>(),
			*spawn, artifactNpcTemplate());
		artifactNpc->setKnownlist(std::make_unique<world::knownlist::NpcKnownList>(*artifactNpc));
		artifactNpc->setEffectController(std::make_unique<controllers::effect::EffectController>(*artifactNpc));
		artifactNpc->setPosition(
			world::WorldPosition::create(210010000, 101.0f, 100.0f, 50.0f, int8_t{0}, mapInstance->getRegion(101.0f, 100.0f, 50.0f)));
		artifactNpc->getPosition()->setIsSpawned(true);
		std::unique_ptr<gameserver::ai::AbstractAI> ai = roots::ArtifactAI_aiFactory(*artifactNpc);
		auto* typed = dynamic_cast<roots::ArtifactAI*>(ai.get());
		EXPECT_NE(typed, nullptr);
		artifactNpc->replaceAi(std::move(ai));
		typed->setStateIfNot(gameserver::ai::AIState::IDLE);
		return *typed;
	}

	int64_t potions() { return player().getInventory().getItemCountByItemId(ACTIVATION_ITEM); }

	std::shared_ptr<const std::string> savedMissingAiHandlers;
	xml::LoadContext locationContext;
	std::unique_ptr<model::templates::siegelocation::SiegeLocationTemplate> locationTemplate;
	runtime::Ref<model::siege::ArtifactLocation> artifact;
	runtime::Ref<model::templates::spawns::SpawnGroup> group;
	runtime::Ref<model::templates::spawns::siegespawns::SiegeSpawnTemplate> spawn;
	runtime::Ref<model::gameobjects::siege::SiegeNpc> artifactNpc;
};

// ArtifactAI.java:94-97: an artifact on cooldown, or one that is not idle, is out of order
TEST_F(ArtifactAiTest, AnArtifactOnCooldownOrInUseIsOutOfOrder) {
	roots::ArtifactAI& ai = artifactWith();
	stored(900001, ACTIVATION_ITEM, 5);
	artifact->setLastActivation(commons::utils::currentTimeMillis());
	ai.onActivate(player());
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ARTIFACT_OUT_OF_ORDER())}));

	clearSent();
	artifact->setLastActivation(0);
	artifact->setStatus(ArtifactStatus::CASTING);
	ai.onActivate(player());
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ARTIFACT_OUT_OF_ORDER())}));
	EXPECT_EQ(potions(), 5);
}

// ArtifactAI.java:99-104: a legion's artifact answers only its members with the ARTIFACT right
TEST_F(ArtifactAiTest, ALegionsArtifactRefusesOutsiders) {
	roots::ArtifactAI& ai = artifactWith();
	stored(900001, ACTIVATION_ITEM, 5);
	artifact->setLegionId(77);
	ai.onActivate(player());
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_ARTIFACT_HAVE_NO_AUTHORITY())}));
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::IDLE);
}

// ArtifactAI.java:106-107: without the item count nothing happens
TEST_F(ArtifactAiTest, WithoutTheItemsNothingHappens) {
	roots::ArtifactAI& ai = artifactWith();
	stored(900001, ACTIVATION_ITEM, 1);
	ai.onActivate(player());
	EXPECT_EQ(sent(), exactly({}));
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::IDLE);
	EXPECT_FALSE(player().getObserveController()->hasObservers());
}

// ArtifactAI.java:114-199: the channel, the cost and the cast of a one-shot artifact
TEST_F(ArtifactAiTest, AnActivationChannelsTenSecondsPaysAndCastsThirteenSecondsLater) {
	roots::ArtifactAI& ai = artifactWith();
	stored(900001, ACTIVATION_ITEM, 5);
	ai.onActivate(player());
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::ACTIVATION);
	EXPECT_TRUE(player().getObserveController()->hasObservers());
	EXPECT_TRUE(player().getController().hasScheduledTask(TaskId::ACTION_ITEM_NPC));
	EXPECT_FALSE(sent().empty()) << "STR_ARTIFACT_CASTING, the artifact info, SM_USE_OBJECT and the emotion";

	executor->advance(std::chrono::milliseconds(9999));
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::ACTIVATION);
	EXPECT_EQ(potions(), 5);

	const int64_t before = commons::utils::currentTimeMillis();
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(potions(), 3) << "the activation cost";
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::CASTING);
	EXPECT_GE(artifact->getLastActivation(), before);
	EXPECT_FALSE(player().getObserveController()->hasObservers()) << "the task removes the observer";

	executor->advance(std::chrono::milliseconds(12999));
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::CASTING);
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::ACTIVATED) << "ArtifactUseSkill's first run";
	EXPECT_GT(artifact->getCoolDown(), 0);
}

// ArtifactAI.java:184-194: a repeating artifact casts every repeat interval and is idle again after its last run
TEST_F(ArtifactAiTest, ARepeatingArtifactIsIdleAgainAfterItsRuns) {
	roots::ArtifactAI& ai = artifactWith(R"(repeat_count="3" repeat_interval="5")");
	stored(900001, ACTIVATION_ITEM, 5);
	ai.onActivate(player());
	executor->advance(std::chrono::milliseconds(10000 + 13000));
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::ACTIVATED);
	executor->advance(std::chrono::milliseconds(3 * 5000 - 1));
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::ACTIVATED);
	executor->advance(std::chrono::milliseconds(1));
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::IDLE) << "13 s + interval x count after the cost: the repeating task is cancelled";
}

// ArtifactAI.java:132-153: a move during the channel aborts it - no cost, the artifact idle again
TEST_F(ArtifactAiTest, AMoveDuringTheChannelAbortsIt) {
	roots::ArtifactAI& ai = artifactWith();
	stored(900001, ACTIVATION_ITEM, 5);
	ai.onActivate(player());
	player().getObserveController()->notifyMoveObservers();
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::IDLE);
	EXPECT_FALSE(player().getController().hasScheduledTask(TaskId::ACTION_ITEM_NPC));
	executor->advance(std::chrono::milliseconds(30000));
	EXPECT_EQ(potions(), 5);
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::IDLE);
}

// ArtifactAI.java:89-92: an activation skill without a template is logged and refused
TEST_F(ArtifactAiTest, AnUnknownActivationSkillIsRefused) {
	roots::ArtifactAI& ai = artifactWith("", 99999); // a skill SKILL_DATA has not got
	stored(900001, ACTIVATION_ITEM, 5);
	ai.onActivate(player());
	EXPECT_EQ(sent(), exactly({}));
	EXPECT_EQ(artifact->getStatus(), ArtifactStatus::IDLE);
	EXPECT_EQ(potions(), 5);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
