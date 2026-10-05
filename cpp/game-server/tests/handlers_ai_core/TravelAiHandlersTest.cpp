// M5f stage 1 (m5f-plan.md §2.4-§2.5, §5 V-01..V-05): the three AIs of the travel and instance paths - ResurrectAI ("resurrect", P5-05, the
// obelisk that binds the resurrection point), PortalAI ("portal") and PortalDialogAI ("portal_dialog") of chunk A1, under P5-05's lease of
// handlers/ai/portals/{PortalAI,PortalDialogAI}.* (chunks.cmake).
//
// Java: data/handlers/ai/ResurrectAI.java:43-107, data/handlers/ai/portals/PortalAI.java:30-57, PortalDialogAI.java:108-229.
//
// LINK WORKAROUND (the one QuestItemNpcAiTest.cpp describes): A1's handler library is not linked into this executable, so this file compiles
// the two leased sources itself; it is the only translation unit that does.
#include "aion/gameserver/handlers/ai/portals/PortalAI.cpp"
#include "aion/gameserver/handlers/ai/portals/PortalDialogAI.cpp"

#include "../cm_ak/ItemPacketTestSupport.h"
#include "../ai/AiWorldTestSupport.h"
#include "../dao/DaoTestDatabase.h"

// The fixture is ActionItemNpcAiTest's (a Poeta map instance of its own whose instance handler records AIActions.handleUseItemFinish, the
// player of tests/cm_ak/ItemPacketTestSupport.h moved into it, the DeterministicExecutor). The rows, verbatim with file:line, plus synthetic
// variants where a case needs another race, tribe or talk delay:
// - npc_templates.xml:439582-439586, 700013 obelisk (tribe FIELD_OBJECT_LIGHT, no race, talk distance 8) and bind_points.xml:6 (price 47);
// - npc_templates.xml:453720-453723 and :453730-453733, the Haramel entrance (a dialog npc with a 3 s talk delay) and exit;
// - npc_templates.xml:453735-453738, 730321 tower lift (ai portal_dialog);
// - synthetic: obelisks 790001 (race ASMODIANS, tribe GENERAL_DARK), 790002 (tribe FIELD_OBJECT_DARK), 790003 (race ASMODIANS, tribe
//   FIELD_OBJECT_ALL: the tribe skips the race checks);
//   portal npcs 831117, 730841, 731583, 731570 and 731549 (PortalDialogAI's hard-coded pages: the rows of the lift without talk delay under
//   those ids) and 790010 (a portal_dialog npc without talk delay with a portal_dialog row).
//
// Not covered here, said in place: PortalAI.handleDialogStart's QuestEngine.onDialog(USE_OBJECT) (no quest handler is registered in this
// executable, so it answers false and leaves no trace); PortalDialogAI.checkDialog's start-quest page (QuestService.checkStartConditions needs
// the quest data); the obelisk's prison refusal (the fixture's map is Poeta, not a prison).

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "aion/gameserver/ai/AIState.h"
#include "aion/gameserver/ai/AbstractAI.h"
#include "aion/gameserver/configs/main/AIConfig.h"
#include "aion/gameserver/configs/main/CustomConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/ObserveController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/EffectController.h"
#include "aion/gameserver/dataholders/AutoGroupData.bind.h"
#include "aion/gameserver/dataholders/AutoGroupData.h"
#include "aion/gameserver/dataholders/BindPointData.bind.h"
#include "aion/gameserver/dataholders/BindPointData.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.bind.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/dataholders/NpcData.bind.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/Portal2Data.bind.h"
#include "aion/gameserver/dataholders/Portal2Data.h"
#include "aion/gameserver/dataholders/PortalLocData.bind.h"
#include "aion/gameserver/dataholders/PortalLocData.h"
#include "aion/gameserver/dataholders/TeleLocationData.bind.h"
#include "aion/gameserver/dataholders/TeleLocationData.h"
#include "aion/gameserver/dataholders/TeleporterData.bind.h"
#include "aion/gameserver/dataholders/TeleporterData.h"
#include "aion/gameserver/dataholders/TribeRelationsData.bind.h"
#include "aion/gameserver/dataholders/TribeRelationsData.h"
#include "aion/gameserver/handlers/ai/ResurrectAI.h"
#include "aion/gameserver/instance/handlers/GeneralInstanceHandler.h"
#include "aion/gameserver/model/animations/ActionAnimation.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/PetCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PetList.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/spawns/SpawnGroup.h"
#include "aion/gameserver/model/templates/spawns/SpawnTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ACTION_ANIMATION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_QUESTION_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/world/WorldMap2DInstance.h"
#include "aion/gameserver/world/knownlist/NpcKnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

// The factory function the AION_AI marker of ResurrectAI.cpp defines (this executable links P5-05's library), declared exactly as
// Registry.ai.gen.cpp declares it
namespace aion::gameserver::handlers::ai {
::aion::gameserver::handlers::AIFactory ResurrectAI_aiFactory;
} // namespace aion::gameserver::handlers::ai

namespace aion::gameserver::network::aion::clientpackets::testing::items {
namespace {

namespace roots = gameserver::handlers::ai;
namespace portals = gameserver::handlers::ai::portals;

using model::gameobjects::Npc;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_QUESTION_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;

constexpr int32_t AKARIOS_OBELISK = 700013;
constexpr int32_t ASMODIAN_OBELISK = 790001;
constexpr int32_t DARK_OBELISK = 790002;
constexpr int32_t ALL_OBELISK = 790003;
constexpr int32_t HARAMEL_ENTRANCE = 730318;
constexpr int32_t HARAMEL_EXIT = 730320;
constexpr int32_t TOWER_LIFT = 730321;
constexpr int32_t DIALOG_PORTAL = 790010;
constexpr int32_t QUESTION_REGISTER_RESURRECT_POINT = 160012; // SM_QUESTION_WINDOW.STR_ASK_REGISTER_RESURRECT_POINT
constexpr const char* RESURRECT_LOGGER = "ai.ResurrectAI";
constexpr const char* PORTAL_LOGGER = "com.aionemu.gameserver.services.teleport.PortalService";
constexpr const char* AUDIT_LOGGER = "AUDIT_LOG";

std::string row(int32_t npcId, const char* name, const char* extra, const char* ai, const char* talkInfo) {
	return "<npc_template npc_id=\"" + std::to_string(npcId) + "\" level=\"1\" name=\"" + name
		   + "\" name_id=\"350971\" height=\"2\" group_drop=\"NONE\" rank=\"DISCIPLINED\" rating=\"NORMAL\" " + extra + " type=\"GENERAL\" ai=\""
		   + ai + "\" sangle=\"0\" attack_speed=\"2000\" hpgauge=\"3\"><stats maxHp=\"172\" /><bound_radius front=\"0.25\" side=\"0.35\" upper=\"2\" />"
		   + talkInfo + "</npc_template>";
}

/** The rows of the header comment */
std::string npcRowsXml() {
		std::string rows = "<npc_templates>";
		// npc_templates.xml:439582-439586
		rows += row(AKARIOS_OBELISK, "obelisk", "tribe=\"FIELD_OBJECT_LIGHT\"", "resurrect", "<talk_info distance=\"8\" can_talk_invisible=\"false\" />");
		rows += row(ASMODIAN_OBELISK, "obelisk", "race=\"ASMODIANS\" tribe=\"GENERAL_DARK\"", "resurrect", "<talk_info distance=\"8\" can_talk_invisible=\"false\" />");
		rows += row(DARK_OBELISK, "obelisk", "tribe=\"FIELD_OBJECT_DARK\"", "resurrect", "<talk_info distance=\"8\" can_talk_invisible=\"false\" />");
		rows += row(ALL_OBELISK, "obelisk", "race=\"ASMODIANS\" tribe=\"FIELD_OBJECT_ALL\"", "resurrect", "<talk_info distance=\"8\" can_talk_invisible=\"false\" />");
		// npc_templates.xml:453720-453723, :453730-453733, :453735-453738
		rows += row(HARAMEL_ENTRANCE, "haramel secret entrance", "tribe=\"FIELD_OBJECT_ALL\"", "portal",
			"<talk_info distance=\"5\" delay=\"3\" is_dialog=\"true\" can_talk_invisible=\"false\" />");
		rows += row(HARAMEL_EXIT, "haramel exit", "tribe=\"FIELD_OBJECT_ALL\"", "portal", "<talk_info distance=\"5\" delay=\"3\" can_talk_invisible=\"false\" />");
		rows += row(TOWER_LIFT, "tower lift", "tribe=\"FIELD_OBJECT_ALL\"", "portal_dialog",
			"<talk_info distance=\"5\" delay=\"3\" is_dialog=\"true\" can_talk_invisible=\"false\" />");
		for (int32_t id : {831117, 730841, 731583, 731570, 731549, DIALOG_PORTAL})
			rows += row(id, "portal", "tribe=\"FIELD_OBJECT_ALL\"", "portal_dialog", "<talk_info distance=\"5\" is_dialog=\"true\" can_talk_invisible=\"false\" />");
		rows += "</npc_templates>";
		return rows;
}

/** The rows bound through NpcData; kept for the process like DataManager's */
const model::templates::npc::NpcTemplate* templateOf(int32_t npcId) {
	static const dataholders::NpcData* holder = [] {
		static xml::LoadContext context;
		return xml::bindString<dataholders::NpcData>(context, npcRowsXml()).release();
	}();
	return holder->getNpcTemplate(npcId);
}

/** GeneralInstanceHandler recording AIActions.handleUseItemFinish (player object id, npc object id) */
class FinishRecorder final : public ::aion::gameserver::instance::handlers::GeneralInstanceHandler {
	AION_MAKE_REF_FRIEND
public:
	explicit FinishRecorder(world::WorldMapInstance& instance) : GeneralInstanceHandler(instance) {}

	static runtime::Ref<FinishRecorder> create(world::WorldMapInstance& instance) { return runtime::makeRef<FinishRecorder>(instance); }

	void handleUseItemFinish(runtime::Ptr<model::gameobjects::player::Player> player, Npc& npc) override {
		finishes.emplace_back(player ? player->getObjectId() : 0, npc.getObjectId());
	}

	std::vector<std::pair<int32_t, int32_t>> finishes;

protected:
	~FinishRecorder() override = default;
};

class TravelSpawnTemplate final : public model::templates::spawns::SpawnTemplate {
public:
	TravelSpawnTemplate(model::templates::spawns::SpawnGroup& group, float x, float y, float z)
		: SpawnTemplate(group, x, y, z, int8_t{0}, 0, std::nullopt, 0, 0, std::nullopt) {}
};

/** PortalAI with its protected hooks callable */
class PortalAiProbe final : public portals::PortalAI {
public:
	using PortalAI::PortalAI;
	using PortalAI::handleSpawned;
	using PortalAI::handleUseItemFinish;
	const model::templates::teleport::TeleporterTemplate* teleporter() { return teleportTemplate.get(); }
};

/** PortalDialogAI with its protected hooks callable */
class PortalDialogAiProbe final : public portals::PortalDialogAI {
public:
	using PortalDialogAI::PortalDialogAI;
	using PortalDialogAI::checkDialog;
	using PortalDialogAI::handleUseItemFinish;
};

class TravelAiHandlersTest : public ItemPacketTest {
protected:
	void SetUp() override {
		gameserver::ai::testing::publishAiMapStaticDataOnce();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(
			[](model::gameobjects::player::Player&) { return std::vector<runtime::Ref<model::gameobjects::player::PetCommonData>>(); });
		ItemPacketTest::SetUp();
		savedMissingAiHandlers = configs::main::AIConfig::MISSING_AI_HANDLERS.get();
		configs::main::AIConfig::MISSING_AI_HANDLERS.set("warn");
		savedCrossFaction = configs::main::CustomConfig::ENABLE_CROSS_FACTION_BINDING.exchange(false);
		useInstance = world::WorldMap2DInstance::create(*map, 2, 0, 0, [this](world::WorldMapInstance& instance) {
			recorder = FinishRecorder::create(instance);
			return runtime::Ref<::aion::gameserver::instance::handlers::InstanceHandler>(recorder);
		});
		standIn(player());
		player().setQuestStateList(model::gameobjects::player::QuestStateList::create());
		dataholders::DataManager::BIND_POINT_DATA.publish(xml::bindString<dataholders::BindPointData>(context,
			R"(<bind_points><bind_point npcid="700013" name="Binding_Stone_akarios" price="47"/>)" // bind_points.xml:6
			R"(<bind_point npcid="790001" name="test" price="47"/><bind_point npcid="790002" name="test" price="47"/>)"
			R"(<bind_point npcid="790003" name="test" price="47"/></bind_points>)"));
		dataholders::DataManager::TRIBE_RELATIONS_DATA.publish(
			xml::bindString<dataholders::TribeRelationsData>(context, std::string(gameserver::ai::testing::AI_TRIBE_RELATIONS_XML).insert(
				std::string(gameserver::ai::testing::AI_TRIBE_RELATIONS_XML).rfind("</tribe_relations>"),
				// tribe_relations.xml:466-468, 477: the portals' and obelisks' tribes
				R"(<tribe name="FIELD_OBJECT_ALL" base="FIELD_OBJECT_LIGHT"><neutral>PC_DARK</neutral></tribe><tribe name="FIELD_OBJECT_LIGHT"/>)")));
		dataholders::DataManager::PORTAL_LOC_DATA.publish(xml::bindString<dataholders::PortalLocData>(context, "<portal_locs/>"));
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.publish(
			xml::bindString<dataholders::InstanceCooltimeData>(context,
			R"(<instance_cooltimes><instance_cooltime race="PC_ALL" worldId="300200000" id="46" sync_id="46"><type>DAILY</type>)" // :428-437
			R"(<ent_cool_time>900</ent_cool_time><maxcount>16</maxcount><max_member_light>1</max_member_light><max_member_dark>1</max_member_dark>)"
			R"(<enter_min_level_light>16</enter_min_level_light><enter_min_level_dark>16</enter_min_level_dark>)"
			R"(<can_enter_mentor>true</can_enter_mentor></instance_cooltime></instance_cooltimes>)"));
		dataholders::DataManager::AUTO_GROUP.publish(xml::bindString<dataholders::AutoGroupData>(context,
			R"(<auto_groups><auto_group id="1" instanceId="300110000" name_id="401193" title_id="401197" min_lvl="46" max_lvl="50"/></auto_groups>)"));
		// portal_template2.xml:157-168 (the Haramel exit and entrance) and a synthetic portal_dialog row (teleport page 1013, one route)
		dataholders::DataManager::PORTAL2_DATA.publish(xml::bindString<dataholders::Portal2Data>(context,
			R"(<portal_templates2>)"
			R"(<portal_use npc_id="730320"><portal_path loc_id="2100301" race="ELYOS" /><portal_path loc_id="2200301" race="ASMODIANS" /></portal_use>)"
			R"(<portal_use npc_id="730318"><portal_path loc_id="3002000" race="ELYOS" err_level="27" /></portal_use>)"
			R"(<portal_dialog npc_id="790010" teleport_dialog_id="1013"><portal_path dialog="1012" loc_id="3009999" race="PC_ALL" /></portal_dialog>)"
			R"(</portal_templates2>)"));
		// a teleporter row for the Haramel entrance npc (npc_teleporter.xml has none: PortalAI's teleporter arm needs one)
		dataholders::DataManager::TELEPORTER_DATA.publish(xml::bindString<dataholders::TeleporterData>(context,
			R"(<npc_teleporter><teleporter_template npc_ids="730320" teleportId="900"><locations>)"
			R"(<telelocation loc_id="4" price="100" pricePvp="100" type="REGULAR"/></locations></teleporter_template></npc_teleporter>)"));
		// QuestEngine.registerQuestNpc and TeleportService read DataManager's holders: the same npc rows, Sanctum's teleloc row
		// (teleport_location.xml:3) and no row of the portal teleporter's route (loc 4)
		dataholders::DataManager::NPC_DATA.publish(xml::bindString<dataholders::NpcData>(npcContext, npcRowsXml()));
		dataholders::DataManager::TELELOCATION_DATA.publish(xml::bindString<dataholders::TeleLocationData>(context,
			R"(<teleport_location><teleloc_template loc_id="2" mapid="110010000" name="Sanctum" name_id="400489" posX="1313.25" posY="1512.011" posZ="568.107"/></teleport_location>)"));
		clearSent();
	}

	void TearDown() override {
		dataholders::DataManager::TELELOCATION_DATA.resetForTests();
		dataholders::DataManager::NPC_DATA.resetForTests();
		npcs.clear();
		spawnGroups.clear();
		configs::main::CustomConfig::ENABLE_CROSS_FACTION_BINDING.store(savedCrossFaction);
		if (savedMissingAiHandlers)
			configs::main::AIConfig::MISSING_AI_HANDLERS.set(*savedMissingAiHandlers);
		dataholders::DataManager::TELEPORTER_DATA.resetForTests();
		dataholders::DataManager::PORTAL2_DATA.resetForTests();
		dataholders::DataManager::AUTO_GROUP.resetForTests();
		dataholders::DataManager::INSTANCE_COOLTIME_DATA.resetForTests();
		dataholders::DataManager::PORTAL_LOC_DATA.resetForTests();
		dataholders::DataManager::TRIBE_RELATIONS_DATA.resetForTests();
		dataholders::DataManager::BIND_POINT_DATA.resetForTests();
		player().setQuestStateList(nullptr);
		useInstance->detachInstanceHandler();
		recorder = nullptr;
		useInstance = nullptr;
		ItemPacketTest::TearDown();
		model::gameobjects::player::PetList::setPlayerPetsLoaderForTests(nullptr);
	}

	void standIn(model::gameobjects::player::Player& user) {
		user.setPosition(world::WorldPosition::create(210010000, 100.0f, 100.0f, 50.0f, int8_t{0}, useInstance->getRegion(100.0f, 100.0f, 50.0f)));
		user.getPosition()->setIsSpawned(true);
	}

	/** Moves the player to x without notifying anything (a move the dialog observer does not see) */
	void placeAt(float x) {
		player().setPosition(world::WorldPosition::create(210010000, x, 100.0f, 50.0f, int8_t{7}, useInstance->getRegion(x, 100.0f, 50.0f)));
		player().getPosition()->setIsSpawned(true);
	}

	Npc& npcAt(int32_t npcId, float dx) {
		const float x = 100.0f + dx;
		runtime::Ref<model::templates::spawns::SpawnGroup> group = model::templates::spawns::SpawnGroup::create(210010000, npcId, 0, nullptr);
		model::templates::spawns::SpawnTemplate& spawn = group->addSpawnTemplate(std::make_unique<TravelSpawnTemplate>(*group, x, 100.0f, 50.0f));
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

	roots::ResurrectAI& installResurrect(Npc& npc) {
		std::unique_ptr<gameserver::ai::AbstractAI> ai = roots::ResurrectAI_aiFactory(npc);
		auto* typed = dynamic_cast<roots::ResurrectAI*>(ai.get());
		EXPECT_NE(typed, nullptr) << "the factory of the \"resurrect\" marker";
		npc.replaceAi(std::move(ai));
		typed->setStateIfNot(gameserver::ai::AIState::IDLE);
		return *typed;
	}

	std::vector<uint8_t> page(Npc& npc, int32_t page) { return serializedFor(SM_DIALOG_WINDOW(npc.getObjectId(), page)); }

	std::vector<uint8_t> question(Npc& obelisk) {
		return serializedFor(SM_QUESTION_WINDOW(QUESTION_REGISTER_RESURRECT_POINT, obelisk.getObjectId(), 5, std::vector<std::string>{"47"}));
	}

	bool wasSent(const std::vector<uint8_t>& packet) {
		for (const std::vector<uint8_t>& bytes : sent())
			if (bytes == packet)
				return true;
		return false;
	}

	void kinah(int64_t count) { stored(710901, KINAH, count); }

	xml::LoadContext context;
	xml::LoadContext npcContext;
	std::shared_ptr<const std::string> savedMissingAiHandlers;
	bool savedCrossFaction = false;
	runtime::Ref<world::WorldMapInstance> useInstance;
	runtime::Ref<FinishRecorder> recorder;
	std::vector<runtime::Ref<model::templates::spawns::SpawnGroup>> spawnGroups;
	std::vector<runtime::Ref<Npc>> npcs;
};

using Finishes = std::vector<std::pair<int32_t, int32_t>>;

// ---- ResurrectAI.handleDialogStart (ResurrectAI.java:43-73) ---------------------------------------------------------------------------------

TEST_F(TravelAiHandlersTest, AClickOnTheObeliskAsksToBindForItsPrice) {
	Npc& obelisk = npcAt(AKARIOS_OBELISK, 2.0f);
	installResurrect(obelisk);

	obelisk.getController().onDialogRequest(player());

	// AIActions.addRequest(this, player, STR_ASK_REGISTER_RESURRECT_POINT, request, price): range 5, the price as its parameter
	EXPECT_EQ(sent(), exactly({question(obelisk)}));
	EXPECT_TRUE(player().getObserveController()->hasObservers()) << "the dialog observer of the 5 m range";
}

TEST_F(TravelAiHandlersTest, AnObeliskWithoutBindPointRowOnlyLogs) {
	Npc& obelisk = npcAt(HARAMEL_EXIT, 2.0f); // no bind_points.xml row
	installResurrect(obelisk);
	network::test::LogCapture capture({RESURRECT_LOGGER}, spdlog::level::info);

	obelisk.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({}));
	EXPECT_TRUE(capture.contains("info|ai.ResurrectAI|There is no bind point template for npc: 730320")) << capture.dump();
}

TEST_F(TravelAiHandlersTest, ABindPointWithin20mOfTheObeliskIsAlreadyRegistered) {
	Npc& obelisk = npcAt(AKARIOS_OBELISK, 2.0f);
	installResurrect(obelisk);
	player().setBindPoint(model::gameobjects::player::BindPointPosition::create(210010000, 102.0f + 19.9f, 100.0f, 50.0f, int8_t{0}));

	obelisk.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_ALREADY_REGISTER_THIS_RESURRECT_POINT())}));
}

TEST_F(TravelAiHandlersTest, ABindPoint20mAwayIsAnotherOne) {
	Npc& obelisk = npcAt(AKARIOS_OBELISK, 2.0f);
	installResurrect(obelisk);
	player().setBindPoint(model::gameobjects::player::BindPointPosition::create(210010000, 102.0f + 20.0f, 100.0f, 50.0f, int8_t{0}));

	obelisk.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({question(obelisk)})) << "< 20 only (ResurrectAI.java:54)";
}

TEST_F(TravelAiHandlersTest, ABindPointOnAnotherMapIsAnotherOne) {
	Npc& obelisk = npcAt(AKARIOS_OBELISK, 2.0f);
	installResurrect(obelisk);
	player().setBindPoint(model::gameobjects::player::BindPointPosition::create(210030000, 102.0f, 100.0f, 50.0f, int8_t{0}));

	obelisk.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({question(obelisk)}));
}

TEST_F(TravelAiHandlersTest, AnObeliskOfTheOtherRaceRefusesWithTheOppositeRace) {
	Npc& obelisk = npcAt(ASMODIAN_OBELISK, 2.0f);
	installResurrect(obelisk);

	obelisk.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_BINDSTONE_CANNOT_FOR_INVALID_RIGHT("ASMODIANS"))}));
}

TEST_F(TravelAiHandlersTest, ADarkObeliskRefusesAnElyos) {
	Npc& obelisk = npcAt(DARK_OBELISK, 2.0f);
	installResurrect(obelisk);

	obelisk.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_MSG_BINDSTONE_CANNOT_FOR_INVALID_RIGHT("ASMODIANS"))}));
}

TEST_F(TravelAiHandlersTest, AnObeliskForEveryoneSkipsTheRaceChecks) {
	Npc& obelisk = npcAt(ALL_OBELISK, 2.0f);
	installResurrect(obelisk);

	obelisk.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({question(obelisk)}));
}

TEST_F(TravelAiHandlersTest, CrossFactionBindingSkipsTheRaceChecks) {
	configs::main::CustomConfig::ENABLE_CROSS_FACTION_BINDING.store(true);
	Npc& obelisk = npcAt(DARK_OBELISK, 2.0f);
	installResurrect(obelisk);

	obelisk.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({question(obelisk)}));
}

// ---- the request (ResurrectAI.java:76-106) --------------------------------------------------------------------------------------------------

TEST_F(TravelAiHandlersTest, AcceptingWithoutEnoughKinahIsRefused) {
	kinah(46);
	Npc& obelisk = npcAt(AKARIOS_OBELISK, 2.0f);
	installResurrect(obelisk);
	obelisk.getController().onDialogRequest(player());
	clearSent();

	ASSERT_TRUE(player().getResponseRequester().respond(QUESTION_REGISTER_RESURRECT_POINT, 1));

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_CANNOT_REGISTER_RESURRECT_POINT_NOT_ENOUGH_FEE())}));
	EXPECT_FALSE(player().getBindPoint());
}

TEST_F(TravelAiHandlersTest, AcceptingMoreThan5mFromTheObeliskIsRefused) {
	kinah(100);
	Npc& obelisk = npcAt(AKARIOS_OBELISK, 2.0f);
	installResurrect(obelisk);
	obelisk.getController().onDialogRequest(player());
	placeAt(102.0f + 5.1f);
	clearSent();

	ASSERT_TRUE(player().getResponseRequester().respond(QUESTION_REGISTER_RESURRECT_POINT, 1));

	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_CANNOT_REGISTER_RESURRECT_POINT_FAR_FROM_NPC())}));
	EXPECT_FALSE(player().getBindPoint());
	EXPECT_EQ(player().getInventory().getKinah(), 100);
}

TEST_F(TravelAiHandlersTest, AcceptingOnAnotherMapDoesNothing) {
	kinah(100);
	Npc& obelisk = npcAt(AKARIOS_OBELISK, 2.0f);
	installResurrect(obelisk);
	obelisk.getController().onDialogRequest(player());
	// another world, no region needed, 50 m from where the obelisk stands on its map: only the world check stops the request
	player().setPosition(world::WorldPosition::create(210030000, 152.0f, 100.0f, 50.0f, int8_t{0}));
	clearSent();

	ASSERT_TRUE(player().getResponseRequester().respond(QUESTION_REGISTER_RESURRECT_POINT, 1));

	EXPECT_EQ(sent(), exactly({}));
	EXPECT_FALSE(player().getBindPoint());
	standIn(player());
}

// The accepted bind (ResurrectAI.java:91-100): the point at the PLAYER's position (not the obelisk's), stored by PlayerBindPointDAO, then the
// raw price taken (no PricesService), SM_BIND_POINT_INFO, the BIND_KISK animation and STR_DEATH_REGISTER_RESURRECT_POINT. The DAO needs the
// test database of the DAO tests with the player's row (the foreign key of player_bind_point); the case initializes DatabaseFactory for its
// process (ctest runs every case in its own).
TEST_F(TravelAiHandlersTest, AcceptingStoresTheBindPointAtThePlayersPositionAndTakesThePrice) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: PlayerBindPointDAO.store writes the database";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables();
	dao::test::insertPlayer(player().getObjectId(), "Holder", 9901);
	kinah(100);
	Npc& obelisk = npcAt(AKARIOS_OBELISK, 2.0f);
	installResurrect(obelisk);
	obelisk.getController().onDialogRequest(player());
	placeAt(103.5f); // 1.5 m from the obelisk, heading 7
	clearSent();

	ASSERT_TRUE(player().getResponseRequester().respond(QUESTION_REGISTER_RESURRECT_POINT, 1));

	runtime::Ptr<model::gameobjects::player::BindPointPosition> bound = player().getBindPoint();
	ASSERT_TRUE(bound);
	EXPECT_EQ(bound->getMapId(), 210010000);
	EXPECT_FLOAT_EQ(bound->getX(), 103.5f) << "the player's position, not the obelisk's (102)";
	EXPECT_EQ(bound->getHeading(), 7);
	EXPECT_EQ(player().getInventory().getKinah(), 100 - 47) << "the raw price";
	EXPECT_EQ(dao::test::queryLong("SELECT map_id FROM player_bind_point WHERE player_id = " + std::to_string(player().getObjectId())),
		std::optional<int64_t>(210010000));
	EXPECT_TRUE(wasSent(serializedFor(network::aion::serverpackets::SM_BIND_POINT_INFO(210010000, 103.5f, 100.0f, 50.0f))));
	EXPECT_TRUE(wasSent(serializedFor(network::aion::serverpackets::SM_ACTION_ANIMATION(player().getObjectId(),
		model::animations::ActionAnimation::BIND_KISK))));
	EXPECT_EQ(sent().back(), serializedFor(SM_SYSTEM_MESSAGE::STR_DEATH_REGISTER_RESURRECT_POINT()));
}

TEST_F(TravelAiHandlersTest, AFailedStoreKeepsTheOldBindPoint) {
	if (!dao::test::isEnabled())
		GTEST_SKIP() << "set AION_TEST_GS_DATABASE_URL: PlayerBindPointDAO.store writes the database";
	dao::test::setUpDatabaseOnce();
	dao::test::clearTables(); // no players row: the foreign key refuses the insert
	kinah(100);
	runtime::Ref<model::gameobjects::player::BindPointPosition> old =
		model::gameobjects::player::BindPointPosition::create(210030000, 1.0f, 2.0f, 3.0f, int8_t{0});
	player().setBindPoint(old);
	Npc& obelisk = npcAt(AKARIOS_OBELISK, 2.0f);
	installResurrect(obelisk);
	obelisk.getController().onDialogRequest(player());
	clearSent();

	ASSERT_TRUE(player().getResponseRequester().respond(QUESTION_REGISTER_RESURRECT_POINT, 1));

	EXPECT_EQ(player().getBindPoint().get(), old.get()) << "ResurrectAI.java:101-103";
	EXPECT_EQ(player().getInventory().getKinah(), 100);
	EXPECT_EQ(sent(), exactly({}));
}

// ---- PortalAI (PortalAI.java:30-57) ---------------------------------------------------------------------------------------------------------

TEST_F(TravelAiHandlersTest, APortalAnswersEveryDialogSelect) {
	Npc& portal = npcAt(HARAMEL_EXIT, 2.0f);
	PortalAiProbe& ai = install<PortalAiProbe>(portal);

	EXPECT_TRUE(ai.onDialogSelect(player(), 1012, 0, 0));
	EXPECT_EQ(sent(), exactly({}));
}

TEST_F(TravelAiHandlersTest, ThePortalsSpawnLooksUpItsTeleporter) {
	Npc& exit = npcAt(HARAMEL_EXIT, 2.0f);
	PortalAiProbe& withTeleporter = install<PortalAiProbe>(exit);
	Npc& entrance = npcAt(HARAMEL_ENTRANCE, 2.0f);
	PortalAiProbe& without = install<PortalAiProbe>(entrance);

	withTeleporter.handleSpawned();
	without.handleSpawned();

	EXPECT_NE(withTeleporter.teleporter(), nullptr);
	EXPECT_EQ(without.teleporter(), nullptr);
}

TEST_F(TravelAiHandlersTest, TheUseBarEndsInThePortalPathOfThePlayersRace) {
	Npc& entrance = npcAt(HARAMEL_ENTRANCE, 2.0f);
	install<PortalAiProbe>(entrance).handleSpawned();
	network::test::LogCapture capture({PORTAL_LOGGER}, spdlog::level::info);

	entrance.getController().onDialogRequest(player()); // talk delay 3 s: ActionItemNpcAI's bar
	executor->advance(std::chrono::milliseconds(3000));

	// PortalService.port(the Elyos path, loc 3002000): the fixture has no portal_loc rows, so port ends at its warning
	EXPECT_TRUE(capture.contains("warning|" + std::string(PORTAL_LOGGER) + "|No portal loc for locId 3002000")) << capture.dump();
	EXPECT_EQ(recorder->finishes, Finishes{}) << "not the plain use-item finish";
}

TEST_F(TravelAiHandlersTest, WithoutAPortalPathTheTeleporterTakesItsFirstLocation) {
	Npc& exit = npcAt(HARAMEL_EXIT, 2.0f);
	PortalAiProbe& ai = install<PortalAiProbe>(exit);
	ai.handleSpawned();
	dataholders::DataManager::PORTAL2_DATA.resetForTests();
	dataholders::DataManager::PORTAL2_DATA.publish(xml::bindString<dataholders::Portal2Data>(context, "<portal_templates2/>"));
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	ai.handleUseItemFinish(player());

	// teleportToFirstTeleportLocation(FADE_OUT_BEAM) -> validateTeleporterAndGetTemplate accepts the friendly portal in talk range ->
	// teleport(its first route, loc 4), which has no teleloc row here: the warning and NO_ROUTE - the arm was taken
	EXPECT_EQ(sent(), exactly({serializedFor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE())}));
	EXPECT_EQ(recorder->finishes, Finishes{});
}

TEST_F(TravelAiHandlersTest, WithoutPathAndTeleporterThePlainFinishRuns) {
	Npc& entrance = npcAt(HARAMEL_ENTRANCE, 2.0f);
	PortalAiProbe& ai = install<PortalAiProbe>(entrance);
	ai.handleSpawned();
	dataholders::DataManager::PORTAL2_DATA.resetForTests();
	dataholders::DataManager::PORTAL2_DATA.publish(xml::bindString<dataholders::Portal2Data>(context, "<portal_templates2/>"));

	ai.handleUseItemFinish(player());

	EXPECT_EQ(recorder->finishes, (Finishes{{player().getObjectId(), entrance.getObjectId()}}));
	EXPECT_EQ(sent(), exactly({}));
}

// ---- PortalDialogAI (PortalDialogAI.java:108-229) ------------------------------------------------------------------------------------------

TEST_F(TravelAiHandlersTest, APortalDialogWithoutTalkDelayShowsItsTeleportPageAtOnce) {
	Npc& portal = npcAt(DIALOG_PORTAL, 2.0f);
	install<PortalDialogAiProbe>(portal);

	portal.getController().onDialogRequest(player());

	EXPECT_EQ(sent(), exactly({page(portal, 1013)})) << "Portal2Data.getTeleportDialogId: the row's teleport_dialog_id";
}

TEST_F(TravelAiHandlersTest, APortalDialogWithATalkDelayRunsTheBarFirst) {
	Npc& lift = npcAt(TOWER_LIFT, 2.0f);
	install<PortalDialogAiProbe>(lift);

	lift.getController().onDialogRequest(player());
	EXPECT_TRUE(player().getController().hasScheduledTask(model::TaskId::ACTION_ITEM_NPC));
	clearSent();
	executor->advance(std::chrono::milliseconds(3000));

	// the bar's end: handleUseItemFinish -> checkDialog; no portal_dialog row: page 1011 (Portal2Data.getTeleportDialogId's default)
	EXPECT_EQ(sent().back(), page(lift, 1011));
	EXPECT_EQ(recorder->finishes, Finishes{});
}

TEST_F(TravelAiHandlersTest, TheHardCodedPortalsShowTheirPages) {
	for (const auto& [npcId, expected] : std::vector<std::pair<int32_t, int32_t>>{{831117, 1012}, {730841, 4762}, {731583, 10}, {731570, 1011},
			 {731549, 1011}}) {
		Npc& portal = npcAt(npcId, 2.0f);
		PortalDialogAiProbe& ai = install<PortalDialogAiProbe>(portal);
		clearSent();

		ai.checkDialog(player());

		EXPECT_EQ(sent(), exactly({page(portal, expected)})) << npcId << " for an Elyos";
	}
}

TEST_F(TravelAiHandlersTest, TheDanuarPortalsShowTheOtherPageToAnAsmodian) {
	player().getCommonData()->setRace(model::Race::ASMODIANS);
	for (const auto& [npcId, expected] : std::vector<std::pair<int32_t, int32_t>>{{731570, 1352}, {731549, 1352}}) {
		Npc& portal = npcAt(npcId, 2.0f);
		PortalDialogAiProbe& ai = install<PortalDialogAiProbe>(portal);
		clearSent();

		ai.checkDialog(player());

		EXPECT_EQ(sent(), exactly({page(portal, expected)})) << npcId << " for an Asmodian";
	}
	player().getCommonData()->setRace(model::Race::ELYOS);
}

TEST_F(TravelAiHandlersTest, ARunningTalkQuestShowsTheQuestPage) {
	Npc& portal = npcAt(831117, 2.0f);
	PortalDialogAiProbe& ai = install<PortalDialogAiProbe>(portal);
	questEngine::QuestEngine::getInstance().registerQuestNpc(831117)->addOnTalkEvent(1006); // as a quest handler registers it
	player().getQuestStateList()->addQuest(1006, *questEngine::model::QuestState::create(1006, questEngine::model::QuestStatus::START));

	ai.checkDialog(player());

	EXPECT_EQ(sent(), exactly({page(portal, 10)})) << "questDialogId, not the npc's hard-coded 1012";
}

TEST_F(TravelAiHandlersTest, ARewardStepNoHandlerTakesShowsTheQuestPage) {
	Npc& portal = npcAt(831117, 2.0f);
	PortalDialogAiProbe& ai = install<PortalDialogAiProbe>(portal);
	questEngine::QuestEngine::getInstance().registerQuestNpc(831117)->addOnTalkEvent(1006); // as a quest handler registers it
	player().getQuestStateList()->addQuest(1006, *questEngine::model::QuestState::create(1006, questEngine::model::QuestStatus::REWARD));

	ai.checkDialog(player());

	EXPECT_EQ(sent(), exactly({page(portal, 10)})) << "QuestEngine.onDialog(USE_OBJECT) answers false without a handler";
}

TEST_F(TravelAiHandlersTest, ASelectionWithoutQuestTakesThePortalDialogPath) {
	Npc& portal = npcAt(DIALOG_PORTAL, 2.0f);
	PortalDialogAiProbe& ai = install<PortalDialogAiProbe>(portal);
	network::test::LogCapture capture({PORTAL_LOGGER}, spdlog::level::info);

	EXPECT_TRUE(ai.onDialogSelect(player(), 1012, 0, 0));

	EXPECT_TRUE(capture.contains("warning|" + std::string(PORTAL_LOGGER) + "|No portal loc for locId 3009999")) << capture.dump();
}

TEST_F(TravelAiHandlersTest, ASelectionWithoutPathStillAnswersTrue) {
	Npc& portal = npcAt(DIALOG_PORTAL, 2.0f);
	PortalDialogAiProbe& ai = install<PortalDialogAiProbe>(portal);
	network::test::LogCapture capture({PORTAL_LOGGER}, spdlog::level::info);

	EXPECT_TRUE(ai.onDialogSelect(player(), 1013, 0, 0));

	EXPECT_FALSE(capture.contains("No portal loc")) << capture.dump();
	EXPECT_EQ(sent(), exactly({}));
}

TEST_F(TravelAiHandlersTest, ASelectionOfAQuestNoHandlerTakesAnswersFalse) {
	Npc& portal = npcAt(DIALOG_PORTAL, 2.0f);
	PortalDialogAiProbe& ai = install<PortalDialogAiProbe>(portal);
	network::test::LogCapture capture({PORTAL_LOGGER}, spdlog::level::info);

	EXPECT_FALSE(ai.onDialogSelect(player(), 1012, 1006, 0));

	EXPECT_FALSE(capture.contains("No portal loc")) << "questId != 0: no portal path (PortalDialogAI.java:140-146)\n" << capture.dump();
}

TEST_F(TravelAiHandlersTest, SelectOneOneWithoutRecruitableInstancesFallsThroughToThePortalPath) {
	Npc& portal = npcAt(DIALOG_PORTAL, 2.0f);
	PortalDialogAiProbe& ai = install<PortalDialogAiProbe>(portal);
	network::test::LogCapture capture({PORTAL_LOGGER}, spdlog::level::info);

	EXPECT_TRUE(ai.onDialogSelect(player(), model::DialogAction::SELECT1_1, 0, 0)); // 1012, no auto group of the npc

	EXPECT_TRUE(capture.contains("No portal loc for locId 3009999")) << capture.dump();
}

TEST_F(TravelAiHandlersTest, ThePartyMatchSelectionReachesTheUnportedAutoGroupType) {
	Npc& portal = npcAt(DIALOG_PORTAL, 2.0f);
	PortalDialogAiProbe& ai = install<PortalDialogAiProbe>(portal);

	// AutoGroupType.getAutoGroup has no C++ companion yet (docs/deviations/P5-05.md, M5f stage 1): the arm is AION_UNPORTED
	EXPECT_THROW(ai.onDialogSelect(player(), model::DialogAction::INSTANCE_PARTY_MATCH, 0, 0), runtime::UnportedException);
}

} // namespace
} // namespace aion::gameserver::network::aion::clientpackets::testing::items
