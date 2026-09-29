// The npc travel path of TeleportService (P5-08, m5f-plan.md §5 T-01 and T-08, the early travel slice of §16): validateTeleporterAndGetTemplate,
// showMap, teleport with its REGULAR and FLIGHT arms, checkKinahForTransportation and teleportToFirstTeleportLocation; the two bodies of the
// P5-12a lease on the path (T-05: SiegeService::getSiegeIdByLocId, onEnterSiegeWorld); and DialogService's AIRLINE_SERVICE arm (44), which
// calls showMap.
//
// Java: TeleportService.java:65-177, 291-295; SiegeService.java:590-605, 612-674; DialogService.java:187-197.
// The fixture (TravelTestSupport.h) is the ascension lane's World of real map rows with the real teleporter, location, fly path, npc and item
// rows. Every price is PricesService.getPriceForService of the row's price with the shipped prices.properties and sieges off (125 % x 100 % x
// 113 %, each product truncated): 160 -> 226, 800 -> 1130 (m5f-plan.md §2.9).
//
// Synthetic rows, each published by the one case that needs it: a same-map REGULAR route (teleloc 900) for a player in a second instance of an
// instance map (the open maps of the fixture have one instance each, the server's usual twin count); fly path 13 moved to Poeta 7 m and 7.5 m
// from the actor, for the validator's accepting arm and its threshold (no shipped loc id / fly path pair lines up, m5f-plan.md D7); siege
// locations put into the SiegeService singleton (built with sieges off, so its maps are otherwise empty) for onEnterSiegeWorld's world filter.
//
// Not covered here, said in place:
// - onEnterSiegeWorld writing an artifact of the player's own world: SM_ABYSS_ARTIFACT_INFO3 calls ArtifactLocation.getStatus, still unported
//   (P5-12a); the artifact filter is pinned by an artifact of another world.
// - the kinah packet's item blob (ItemPacketService.sendItemPacket: SM_INVENTORY_UPDATE_ITEM) is P5-09's and pinned there; this file asserts the
//   count left, that one kinah update was sent and its update type (DEC_KINAH_FLY).
// - the FLIGHT arm's SM_EMOTION is compared with the port's own serialization of SM_EMOTION(START_FLYTELEPORT, id), built after the state
//   change (the gate's decodeEmotion reads its emotion id and state bits independently).

#include "TravelTestSupport.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/DialogAction.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/gameobjects/state/FlyState.h"
#include "aion/gameserver/model/siege/ArtifactLocation.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/templates/flypath/FlightPath.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.bind.h"
#include "aion/gameserver/model/templates/siegelocation/SiegeLocationTemplate.h"
#include "aion/gameserver/model/templates/teleport/TeleLocIdData.h"
#include "aion/gameserver/model/templates/teleport/TeleportLocation.h"
#include "aion/gameserver/model/templates/teleport/TeleportType.h"
#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/services/DialogService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/skillengine/SkillEngine.h"
#include "aion/gameserver/skillengine/model/Effect.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::teleport::test {
namespace {

using model::animations::TeleportAnimation;
using model::gameobjects::Npc;
using model::gameobjects::state::CreatureState;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using network::test::PacketWriter;

/** ServerPacketsOpcodes.java:33, 38, 40, 55, 78, 214, 236, 238 */
constexpr int32_t SM_PLAYER_SPAWN_OPCODE = 15;
constexpr int32_t SM_TELEPORT_LOC_OPCODE = 20;
constexpr int32_t SM_DELETE_OPCODE = 22;
constexpr int32_t SM_EMOTION_OPCODE = 37;
constexpr int32_t SM_DIALOG_WINDOW_OPCODE = 60;
constexpr int32_t SM_TELEPORT_MAP_OPCODE = 196;
constexpr int32_t SM_SHIELD_EFFECT_OPCODE = 218;
constexpr int32_t SM_ABYSS_ARTIFACT_INFO3_OPCODE = 220;
/** ServerPacketsOpcodes.java:47: SM_INVENTORY_UPDATE_ITEM, the kinah update of Storage.decreaseKinah (ItemPacketService.sendItemPacket) */
constexpr int32_t SM_INVENTORY_UPDATE_ITEM_OPCODE = 29;

constexpr const char* AUDIT_LOGGER = "AUDIT_LOG"; // AuditLogger.cpp
constexpr const char* TELEPORT_LOGGER = "com.aionemu.gameserver.services.teleport.TeleportService";

/** ObjectDeleteAnimation.JUMP_IN (ObjectDeleteAnimation.java:38), TeleportAnimation.JUMP_IN's delete animation (TeleportAnimation.java:58-67) */
constexpr int32_t DELETE_ANIMATION_JUMP_IN = 11;
/** DialogPage.NO_RIGHT (DialogPage.java) */
constexpr int32_t DIALOG_PAGE_NO_RIGHT = 27;

class TravelTeleportTest : public TravelTest {
protected:
	const model::templates::teleport::TeleportLocation* location(int32_t npcId, int32_t locId) {
		const model::templates::teleport::TeleporterTemplate* teleporter = dataholders::DataManager::TELEPORTER_DATA->getTeleporterTemplateByNpcId(npcId);
		EXPECT_NE(teleporter, nullptr) << npcId;
		const model::templates::teleport::TeleportLocation* found = teleporter->getTeleLocIdData()->getTeleportLocation(locId);
		EXPECT_NE(found, nullptr) << npcId << " " << locId;
		return found;
	}

	/** SM_TELEPORT_MAP.writeImpl (SM_TELEPORT_MAP.java): D targetObjId, H teleportId */
	static std::vector<uint8_t> teleportMap(int32_t npcObjectId, int32_t teleportId) {
		return javaPacket(SM_TELEPORT_MAP_OPCODE, PacketWriter().D(npcObjectId).H(teleportId));
	}

	/** SM_DIALOG_WINDOW.writeImpl (SM_DIALOG_WINDOW.java:29-41) of a page that is neither MAIL nor TOWN_CHALLENGE_TASK */
	static std::vector<uint8_t> dialogWindow(int32_t npcObjectId, int32_t page) {
		return javaPacket(SM_DIALOG_WINDOW_OPCODE, PacketWriter().D(npcObjectId).H(page).D(0).H(0).H(0));
	}

	void completeQuest(int32_t questId) {
		runtime::Ref<questEngine::model::QuestState> state = questEngine::model::QuestState::create(questId, questEngine::model::QuestStatus::COMPLETE);
		player().getQuestStateList()->addQuest(questId, *state);
	}

	/** Replaces the fly paths of the fixture by `rows` (flypath_location elements) for this case */
	void publishFlyPaths(std::string_view rows) {
		dataholders::DataManager::FLY_PATH.resetForTests();
		dataholders::DataManager::FLY_PATH.publish(
			xml::bindString<dataholders::FlyPathData>(context, "<flypath_template>" + std::string(rows) + "</flypath_template>"));
	}

	/** Runs the TELEPORT task the way CM_TELEPORT_ANIMATION_DONE does (CM_TELEPORT_ANIMATION_DONE.java:36-41: run(), get()) */
	void animationDone() {
		runtime::Ptr<runtime::Future> task = player().getController().getAndRemoveTask(model::TaskId::TELEPORT);
		ASSERT_TRUE(task) << "no TELEPORT task: sendLoc did not wait for the animation";
		task->run();
		task->get();
	}
};

// ---- the price (checkKinahForTransportation, TeleportService.java:158-177) -------------------------------------------------------------------

TEST_F(TravelTeleportTest, ThePriceIsGetPriceForServiceOfTheTelelocationPrice) {
	// m5f-plan.md §2.9: every telelocation price of the start maps and the capitals through the three truncating multiplications
	for (const auto& [base, price] : std::vector<std::pair<int64_t, int64_t>>{
			 {160, 226}, {100, 141}, {800, 1130}, {500, 706}, {1700, 2401}, {9300, 13136}}) {
		EXPECT_EQ(trade::PricesService::getPriceForService(base, model::Race::ELYOS), price) << base;
		EXPECT_EQ(trade::PricesService::getPriceForService(base, model::Race::ASMODIANS), price) << base;
	}
}

TEST_F(TravelTeleportTest, AFlightTakesThePriceOfItsLocationFromTheInventory) {
	spawnActor(1000);

	TeleportService::teleport(player(), location(KUSTANON, 13), TeleportAnimation::JUMP_IN);

	EXPECT_EQ(kinah(), 1000 - 226) << "loc 13's 160 through getPriceForService";
	const std::vector<std::vector<uint8_t>> kinahUpdates = ofOpcode(sent(), SM_INVENTORY_UPDATE_ITEM_OPCODE);
	ASSERT_EQ(kinahUpdates.size(), 1u) << "the kinah update";
	// SM_INVENTORY_UPDATE_ITEM.java:58-59: the packet ends with H(updateType.getMask()); ItemUpdateType.DEC_KINAH_FLY is 0x4B
	// (ItemPacketService.java:48, TeleportService.java:172), where DEC_KINAH_BUY would be 0x1D
	ASSERT_GE(kinahUpdates[0].size(), 2u);
	EXPECT_EQ(std::vector<uint8_t>(kinahUpdates[0].end() - 2, kinahUpdates[0].end()), (std::vector<uint8_t>{0x4B, 0x00}));
}

TEST_F(TravelTeleportTest, AnAbnormalEffectWithoutHiPassPaysTheFullPrice) {
	spawnActor(5000);
	skillengine::SkillEngine::getInstance().applyEffectDirectly(NO_HIPASS_SKILL, player(), player());
	ASSERT_TRUE(player().getEffectController()->hasAbnormalEffect(NO_HIPASS_SKILL)) << "10373's buff is on";
	clearSent();

	TeleportService::teleport(player(), location(DAINES, 4), TeleportAnimation::JUMP_IN);

	EXPECT_EQ(kinah(), 5000 - 1130) << "TeleportService.java:164-169: only an effect whose skill has hipass (Effect.isHiPass) makes it 1 kinah";
	EXPECT_FALSE(player().isSpawned()) << "the teleport went on";
}

TEST_F(TravelTeleportTest, HiPassMakesEveryRouteCostOneKinah) {
	spawnActor(5000);
	skillengine::SkillEngine::getInstance().applyEffectDirectly(HIPASS_SKILL, player(), player());
	clearSent();

	TeleportService::teleport(player(), location(DAINES, 4), TeleportAnimation::JUMP_IN);

	EXPECT_EQ(kinah(), 4999) << "TeleportService.java:164-165: HiPassEffect -> 1 kinah, not 1130";
	EXPECT_FALSE(player().isSpawned()) << "the teleport went on";
}

TEST_F(TravelTeleportTest, NotEnoughKinahRefusesWithThePriceAndDoesNotMove) {
	spawnActor(1129);

	TeleportService::teleport(player(), location(DAINES, 4), TeleportAnimation::JUMP_IN);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_KINA(1130))}));
	EXPECT_EQ(kinah(), 1129);
	EXPECT_TRUE(player().isSpawned());
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT));
}

TEST_F(TravelTeleportTest, ExactlyThePriceIsEnough) {
	spawnActor(1130);

	TeleportService::teleport(player(), location(DAINES, 4), TeleportAnimation::JUMP_IN);

	EXPECT_EQ(kinah(), 0);
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::TELEPORT));
}

// ---- teleport's refusals (TeleportService.java:72-92) -----------------------------------------------------------------------------------------

TEST_F(TravelTeleportTest, ARequiredQuestThatIsNotCompleteRefusesBeforeThePrice) {
	spawnActor(5000);
	// a started 1006 is not a completed one (Player.isCompleteQuest)
	runtime::Ref<questEngine::model::QuestState> started = questEngine::model::QuestState::create(1006, questEngine::model::QuestStatus::START);
	player().getQuestStateList()->addQuest(1006, *started);

	TeleportService::teleport(player(), location(DAINES, 2), TeleportAnimation::JUMP_IN); // loc 2, Sanctum: required_quest 1006

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NEED_FINISH_QUEST())}));
	EXPECT_EQ(kinah(), 5000);
	EXPECT_TRUE(player().isSpawned());
}

TEST_F(TravelTeleportTest, ALocationWithoutTeleportLocationRowIsNoRoute) {
	spawnActor(5000);
	network::test::LogCapture capture({TELEPORT_LOGGER}, spdlog::level::info);
	model::templates::teleport::TeleportLocation missing;
	missing.locId = 999;
	missing.price = 100;

	TeleportService::teleport(player(), &missing, TeleportAnimation::JUMP_IN);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE())}));
	EXPECT_TRUE(capture.contains("warning|" + std::string(TELEPORT_LOGGER) + "|Missing teleloc_template in teleport_location.xml with locId 999"))
		<< capture.dump();
	EXPECT_EQ(kinah(), 5000);
}

TEST_F(TravelTeleportTest, AFortressRouteWithSiegesOffIsJavasNullPointerException) {
	spawnActor(5000);
	model::templates::teleport::TeleportLocation divineFortress; // loc 49: SiegeService.getSiegeIdByLocId -> 1011
	divineFortress.locId = 49;
	divineFortress.price = 100;

	// docs/deviations/P5-08.md, the fortress-route row: getSiegeLocation(1011) is null with sieges off, and Java calls isCanTeleport on it
	EXPECT_THROW(TeleportService::teleport(player(), &divineFortress, TeleportAnimation::JUMP_IN), runtime::NullPointerException);
	EXPECT_EQ(kinah(), 5000) << "the siege check comes before the price";
}

// ---- teleport REGULAR (TeleportService.java:123-131 -> sendLoc, :179-193) -------------------------------------------------------------------

TEST_F(TravelTeleportTest, ARegularRouteSendsTheTeleportLocAndSpawnsInTheNewMapAfterTheAnimation) {
	spawnActor(5000);
	const int32_t actorId = player().getObjectId();

	TeleportService::teleport(player(), location(DAINES, 4), TeleportAnimation::JUMP_IN);

	EXPECT_EQ(kinah(), 5000 - 1130);
	// sendLoc: SM_TELEPORT_LOC(JUMP_IN = 3, map, map (no instance map), loc 4's x/y/z, heading 0), SM_TELEPORT_LOC.java writeImpl
	const std::vector<uint8_t> teleportLoc = javaPacket(SM_TELEPORT_LOC_OPCODE,
		PacketWriter().C(3).D(VERTERON).D(VERTERON).F(VERTERON_X).F(VERTERON_Y).F(VERTERON_Z).C(0));
	std::vector<std::vector<uint8_t>> actorPackets = sent();
	EXPECT_EQ(std::count(actorPackets.begin(), actorPackets.end(), teleportLoc), 1) << "one SM_TELEPORT_LOC";
	EXPECT_TRUE(ofOpcode(actorPackets, SM_DELETE_OPCODE).empty()) << "World.despawn marks him unspawned first: he gets no SM_DELETE of anything";
	// everybody who knew him: SM_DELETE(him, JUMP_IN's delete animation 11)
	EXPECT_EQ(ofOpcode(watcherSent(), SM_DELETE_OPCODE), cptest::exactly({javaPacket(SM_DELETE_OPCODE, PacketWriter().D(actorId).C(DELETE_ANIMATION_JUMP_IN))}));
	EXPECT_FALSE(player().isSpawned());
	EXPECT_EQ(player().getWorldId(), POETA) << "not moved before CM_TELEPORT_ANIMATION_DONE";
	EXPECT_TRUE(player().getController().hasTask(model::TaskId::TELEPORT));
	clearSent();

	animationDone();

	EXPECT_EQ(player().getWorldId(), VERTERON);
	EXPECT_EQ(player().getInstanceId(), 1) << "another map: instance 1 (TeleportService.java:124)";
	EXPECT_FLOAT_EQ(player().getX(), VERTERON_X);
	EXPECT_FLOAT_EQ(player().getY(), VERTERON_Y);
	EXPECT_FLOAT_EQ(player().getZ(), VERTERON_Z);
	EXPECT_EQ(player().getHeading(), 0);
	// SpawnTask.run's map-reloading arm (TeleportService.java:525-531): SM_CHANNEL_INFO, SM_PLAYER_SPAWN; the spawn waits for CM_LEVEL_READY
	EXPECT_EQ(sent(), cptest::exactly({forActor(network::aion::serverpackets::SM_CHANNEL_INFO(player().getPosition())),
						  forActor(network::aion::serverpackets::SM_PLAYER_SPAWN(player()))}));
	EXPECT_FALSE(player().isSpawned());
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(player())); // CM_LEVEL_READY
	EXPECT_TRUE(player().isSpawned());
}

TEST_F(TravelTeleportTest, TheAsmodianRouteToAltgardTakesTheLocationsHeading) {
	spawnActor(5000, model::Race::ASMODIANS, ISHALGEN, Spot{OSMAR_SPOT.x + 1.0f, OSMAR_SPOT.y, OSMAR_SPOT.z, int8_t{0}});

	TeleportService::teleport(player(), location(OSMAR, 9), TeleportAnimation::JUMP_IN_STATUE);

	// teleport_location.xml:10, loc 9: heading 60, and the statue's animation 4
	EXPECT_EQ(ofOpcode(sent(), SM_TELEPORT_LOC_OPCODE), cptest::exactly({javaPacket(SM_TELEPORT_LOC_OPCODE,
		PacketWriter().C(4).D(ALTGARD).D(ALTGARD).F(1752.5322f).F(1806.6096f).F(254.66133f).C(60))}));
	animationDone();
	EXPECT_EQ(player().getWorldId(), ALTGARD);
	EXPECT_EQ(player().getHeading(), 60);
	EXPECT_EQ(kinah(), 5000 - 1130);
}

/** Karamatis (310020000, an instance map of 1024 m): where the actor stands in its second instance, and the synthetic teleloc 900 on it */
constexpr Spot KARAMATIS_SPOT{512.0f, 512.0f, 100.0f, int8_t{0}};
constexpr std::string_view KARAMATIS_TELELOC_ROW =
	R"xml(<teleloc_template loc_id="900" mapid="310020000" name="synthetic" name_id="0" posX="600" posY="450" posZ="120" heading="30"/>)xml";

TEST_F(TravelTeleportTest, ARouteOnThePlayersOwnMapKeepsHisInstance) {
	// TeleportService.java:124-127: `instanceId = 1`, but the player's own instance when the destination is his current map
	std::string rows(TRAVEL_TELEPORT_LOCATION_XML);
	rows.insert(rows.rfind("</teleport_location>"), std::string(KARAMATIS_TELELOC_ROW));
	dataholders::DataManager::TELELOCATION_DATA.resetForTests();
	dataholders::DataManager::TELELOCATION_DATA.publish(xml::bindString<dataholders::TeleLocationData>(context, rows));
	const int32_t instanceId = newInstance(KARAMATIS_B);
	ASSERT_GT(instanceId, 1);
	spawnActor(5000, model::Race::ELYOS, KARAMATIS_B, KARAMATIS_SPOT, false, instanceId);
	model::templates::teleport::TeleportLocation sameMap;
	sameMap.locId = 900;
	sameMap.price = 100;
	sameMap.type = model::templates::teleport::TeleportType::REGULAR;

	TeleportService::teleport(player(), &sameMap, TeleportAnimation::JUMP_IN);

	// SM_TELEPORT_LOC.java:37: an instance map's packet carries the instance id where an open map's repeats the map id
	EXPECT_EQ(ofOpcode(sent(), SM_TELEPORT_LOC_OPCODE), cptest::exactly({javaPacket(SM_TELEPORT_LOC_OPCODE,
		PacketWriter().C(3).D(KARAMATIS_B).D(instanceId).F(600.0f).F(450.0f).F(120.0f).C(30))}));
	EXPECT_EQ(kinah(), 5000 - 141);
	animationDone();
	EXPECT_EQ(player().getWorldId(), KARAMATIS_B);
	EXPECT_EQ(player().getInstanceId(), instanceId) << "the same map: his instance, not 1";
	EXPECT_FLOAT_EQ(player().getX(), 600.0f);
	EXPECT_TRUE(player().isSpawned()) << "the same map and instance: SpawnTask's spawnOnSameMap, no CM_LEVEL_READY";
}

TEST_F(TravelTeleportTest, ARouteToAnotherMapGoesToInstanceOneFromAnyInstance) {
	const int32_t instanceId = newInstance(KARAMATIS_B);
	ASSERT_GT(instanceId, 1);
	spawnActor(5000, model::Race::ELYOS, KARAMATIS_B, KARAMATIS_SPOT, false, instanceId);

	TeleportService::teleport(player(), location(DAINES, 4), TeleportAnimation::JUMP_IN);
	animationDone();

	EXPECT_EQ(player().getWorldId(), VERTERON);
	EXPECT_EQ(player().getInstanceId(), 1) << "another map: instance 1 (TeleportService.java:124), not the player's " << instanceId;
	EXPECT_FLOAT_EQ(player().getX(), VERTERON_X);
}

// ---- teleport FLIGHT (TeleportService.java:94-122) -----------------------------------------------------------------------------------------

TEST_F(TravelTeleportTest, AFlightStartsTheFlightTeleportInPlaceWithoutTeleportLoc) {
	spawnActor(1000);
	Npc& kustanon = npc(KUSTANON, POETA, KUSTANON_SPOT);
	player().setTarget(runtime::Ptr<model::gameobjects::VisibleObject>(kustanon)); // abortPlayerActions drops it

	TeleportService::teleport(player(), location(KUSTANON, 13), TeleportAnimation::JUMP_IN);

	EXPECT_TRUE(player().isInState(CreatureState::FLYING));
	EXPECT_FALSE(player().isInState(CreatureState::ACTIVE));
	ASSERT_TRUE(player().getFlightPath());
	EXPECT_EQ(player().getFlightPath()->getType(), model::templates::flypath::FlightPath_Type::FLIGHT_TRANSPORTER);
	EXPECT_EQ(player().getFlightPath()->getId(), 5001) << "loc 13's teleportid";
	EXPECT_FALSE(player().getTarget()) << "abortPlayerActions (TeleportService.java:118)";
	// broadcast to himself and to everybody who sees him: SM_EMOTION(START_FLYTELEPORT, 5001)
	const std::vector<uint8_t> emotion =
		forActor(network::aion::serverpackets::SM_EMOTION(player(), model::EmotionType::START_FLYTELEPORT, 5001, 0));
	EXPECT_EQ(ofOpcode(sent(), SM_EMOTION_OPCODE), cptest::exactly({emotion}));
	EXPECT_EQ(ofOpcode(watcherSent(), SM_EMOTION_OPCODE).size(), 1u);
	EXPECT_TRUE(ofOpcode(sent(), SM_TELEPORT_LOC_OPCODE).empty()) << "a flight is no teleport";
	EXPECT_TRUE(player().isSpawned());
	EXPECT_EQ(player().getWorldId(), POETA);
	EXPECT_FLOAT_EQ(player().getX(), ACTOR_SPOT.x);
	EXPECT_FALSE(player().getController().hasTask(model::TaskId::TELEPORT));
	EXPECT_EQ(kinah(), 1000 - 226);
}

TEST_F(TravelTeleportTest, TheFlypathValidatorLooksTheFlightUpByLocIdAndRefuses) {
	// m5f-plan.md D7, the quirk kept: fly path 13 is Altgard's, so loc 13 of Poeta is refused as too far (and loc 12's fly path starts in Ishalgen)
	configs::main::SecurityConfig::ENABLE_FLYPATH_VALIDATOR.store(true);
	spawnActor(1000);
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	TeleportService::teleport(player(), location(KUSTANON, 13), TeleportAnimation::JUMP_IN);

	EXPECT_EQ(ofOpcode(sent(), SM_EMOTION_OPCODE).size(), 0u);
	EXPECT_FALSE(player().isInState(CreatureState::FLYING));
	EXPECT_TRUE(capture.contains("tried to use flyPath #13 but he's too far ")) << capture.dump();
	EXPECT_EQ(sent().back(), forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE()));
	EXPECT_EQ(kinah(), 1000 - 226) << "the price is taken before the validator (TeleportService.java:91-95)";
}

TEST_F(TravelTeleportTest, TheFlypathValidatorAcceptsAStartExactly7mAwayAndKeepsThePath) {
	// fly path 13 moved to Poeta, 7 m east of the actor: `dist > 7` is false (TeleportService.java:104)
	configs::main::SecurityConfig::ENABLE_FLYPATH_VALIDATOR.store(true);
	publishFlyPaths(R"xml(<flypath_location id="13" sx="812.5" sy="1243.6" sz="118.986" sworld="210010000" ex="426.17" ey="1742.29" ez="119.85" eworld="210010000" time="38"/>)xml");
	spawnActor(1000);
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	TeleportService::teleport(player(), location(KUSTANON, 13), TeleportAnimation::JUMP_IN);

	EXPECT_FALSE(capture.contains("tried to use")) << capture.dump();
	ASSERT_NE(player().getCurrentFlyPath(), nullptr) << "player.setCurrentFlypath (TeleportService.java:116)";
	EXPECT_EQ(player().getCurrentFlyPath(), dataholders::DataManager::FLY_PATH->getPathTemplate(13));
	EXPECT_TRUE(player().isInState(CreatureState::FLYING));
	EXPECT_EQ(ofOpcode(sent(), SM_EMOTION_OPCODE).size(), 1u);
	EXPECT_EQ(kinah(), 1000 - 226);
}

TEST_F(TravelTeleportTest, TheFlypathValidatorRefusesAStartMoreThan7mAwayWithTheDistance) {
	configs::main::SecurityConfig::ENABLE_FLYPATH_VALIDATOR.store(true);
	publishFlyPaths(R"xml(<flypath_location id="13" sx="813" sy="1243.6" sz="118.986" sworld="210010000" ex="426.17" ey="1742.29" ez="119.85" eworld="210010000" time="38"/>)xml");
	spawnActor(1000);
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	TeleportService::teleport(player(), location(KUSTANON, 13), TeleportAnimation::JUMP_IN);

	// 7.5 m; the audit line ends with Java's Double.toString of the distance
	EXPECT_NE(capture.dump().find(" tried to use flyPath #13 but he's too far 7.5\n"), std::string::npos) << capture.dump();
	EXPECT_EQ(sent().back(), forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE()));
	EXPECT_FALSE(player().isInState(CreatureState::FLYING));
	EXPECT_EQ(player().getCurrentFlyPath(), nullptr);
}

TEST_F(TravelTeleportTest, TheFlypathValidatorRefusesAStartInAnotherWorld) {
	configs::main::SecurityConfig::ENABLE_FLYPATH_VALIDATOR.store(true);
	// on Poeta at fly path 12's start point, which is Ishalgen's
	spawnActor(1000, model::Race::ELYOS, POETA, Spot{938.1f, 1710.94f, 258.81f, int8_t{0}});
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	TeleportService::teleport(player(), location(AERO, 12), TeleportAnimation::JUMP_IN);

	EXPECT_TRUE(capture.contains("tried to use flyPath #12 from invalid start world 210010000, expected 220010000")) << capture.dump();
	EXPECT_EQ(sent().back(), forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE()));
	EXPECT_FALSE(player().isInState(CreatureState::FLYING));
}

TEST_F(TravelTeleportTest, TheFlypathValidatorRefusesALocIdWithoutFlyPath) {
	configs::main::SecurityConfig::ENABLE_FLYPATH_VALIDATOR.store(true);
	dataholders::DataManager::FLY_PATH.resetForTests();
	dataholders::DataManager::FLY_PATH.publish(xml::bindString<dataholders::FlyPathData>(context, "<flypath_template/>"));
	spawnActor(1000);
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	TeleportService::teleport(player(), location(KUSTANON, 13), TeleportAnimation::JUMP_IN);

	EXPECT_TRUE(capture.contains("tried to use invalid flyPath #13")) << capture.dump();
	EXPECT_EQ(sent().back(), forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE()));
	EXPECT_FALSE(player().isInState(CreatureState::FLYING));
}

// ---- validateTeleporterAndGetTemplate (TeleportService.java:134-156) -------------------------------------------------------------------------

TEST_F(TravelTeleportTest, ATeleporterInTalkRangeOfAFriendAnswersItsTemplate) {
	spawnActor(0);
	const model::templates::teleport::TeleporterTemplate* found =
		TeleportService::validateTeleporterAndGetTemplate(player(), npc(DAINES, POETA, DAINES_SPOT));
	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->getTeleportId(), 2);
	EXPECT_TRUE(sent().empty());
}

TEST_F(TravelTeleportTest, AnNpcWithoutTeleporterDataIsTheWrongNpc) {
	spawnActor(0, model::Race::ELYOS, POETA, Spot{FULLA_SPOT.x + 1.0f, FULLA_SPOT.y, FULLA_SPOT.z, int8_t{0}});
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);
	Npc& fulla = npc(FULLA, POETA, FULLA_SPOT);

	EXPECT_EQ(TeleportService::validateTeleporterAndGetTemplate(player(), fulla), nullptr);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_WRONG_NPC())}));
	EXPECT_TRUE(capture.contains("tried to use invalid teleporter " + fulla.toString() + " (no teleporter data) at " + player().getPosition()->toString()))
		<< capture.dump();
}

TEST_F(TravelTeleportTest, AnotherRacesTeleporterIsTheWrongNpc) {
	spawnActor(0);
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);
	Npc& osmar = npc(OSMAR, POETA, DAINES_SPOT); // GENERAL_DARK and an Elyos: TribeRelationService.isNone -> PEACE

	EXPECT_EQ(TeleportService::validateTeleporterAndGetTemplate(player(), osmar), nullptr);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_WRONG_NPC())}));
	EXPECT_TRUE(capture.contains("tried to use invalid teleporter " + osmar.toString() + " (wrong race) at ")) << capture.dump();
}

TEST_F(TravelTeleportTest, ATeleporterOutOfTalkRangeIsTooFar) {
	spawnActor(0);
	network::test::LogCapture capture({AUDIT_LOGGER}, spdlog::level::info);

	// Aero stands at Melponeh's Campsite, 630 m away
	EXPECT_EQ(TeleportService::validateTeleporterAndGetTemplate(player(), npc(AERO, POETA, AERO_SPOT)), nullptr);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_FAR_FROM_NPC())}));
	EXPECT_FALSE(capture.contains("tried to use")) << "no audit for distance";
}

TEST_F(TravelTeleportTest, TheTalkRangeIsTheTalkDistancePlusOne) {
	// Daines: talk_info distance 3, so 4 m between the bounding radii (PositionUtil.isInTalkRange, not centre to centre)
	spawnActor(0, model::Race::ELYOS, POETA, Spot{DAINES_SPOT.x + 4.5f, DAINES_SPOT.y, DAINES_SPOT.z, int8_t{0}}, false);
	Npc& daines = npc(DAINES, POETA, DAINES_SPOT);
	EXPECT_NE(TeleportService::validateTeleporterAndGetTemplate(player(), daines), nullptr) << "4.5 m centre to centre";
	world::World::getInstance().updatePosition(player(), DAINES_SPOT.x + 5.5f, DAINES_SPOT.y, DAINES_SPOT.z, int8_t{0});
	EXPECT_EQ(TeleportService::validateTeleporterAndGetTemplate(player(), daines), nullptr) << "5.5 m centre to centre";
}

TEST_F(TravelTeleportTest, AFlyingPlayerCannotUseATeleporter) {
	spawnActor(0);
	player().setFlyState(model::gameobjects::state::FlyState::FLYING);

	EXPECT_EQ(TeleportService::validateTeleporterAndGetTemplate(player(), npc(DAINES, POETA, DAINES_SPOT)), nullptr);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_USE_AIRPORT_WHEN_FLYING())}));
}

TEST_F(TravelTeleportTest, AFlyingPlayerOutOfTalkRangeIsToldHeIsTooFar) {
	// TeleportService.java:147-154: the talk range is checked before the flying state
	spawnActor(0);
	player().setFlyState(model::gameobjects::state::FlyState::FLYING);

	EXPECT_EQ(TeleportService::validateTeleporterAndGetTemplate(player(), npc(AERO, POETA, AERO_SPOT)), nullptr);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_FAR_FROM_NPC())}));
}

TEST_F(TravelTeleportTest, ASupportTeleporterAnswersItsTemplate) {
	// TeleportService.java:141-142: FRIEND or SUPPORT. Npc.getType answers an overridden type before the tribe relation.
	spawnActor(0);
	Npc& daines = npc(DAINES, POETA, DAINES_SPOT);
	daines.overrideNpcType(model::CreatureType::SUPPORT);
	ASSERT_EQ(daines.getType(player()), model::CreatureType::SUPPORT);
	clearSent();

	const model::templates::teleport::TeleporterTemplate* found = TeleportService::validateTeleporterAndGetTemplate(player(), daines);

	ASSERT_NE(found, nullptr);
	EXPECT_EQ(found->getTeleportId(), 2);
	EXPECT_TRUE(sent().empty());
}

// ---- showMap (TeleportService.java:291-295) and teleportToFirstTeleportLocation (:65-70) ----------------------------------------------------

TEST_F(TravelTeleportTest, ShowMapSendsTheTeleportersMap) {
	spawnActor(0);
	Npc& daines = npc(DAINES, POETA, DAINES_SPOT);

	TeleportService::showMap(player(), daines);

	EXPECT_EQ(sent(), cptest::exactly({teleportMap(daines.getObjectId(), 2)}));
}

TEST_F(TravelTeleportTest, ShowMapOfARefusedTeleporterSendsOnlyTheRefusal) {
	spawnActor(0);

	TeleportService::showMap(player(), npc(AERO, POETA, AERO_SPOT));

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_FAR_FROM_NPC())}));
}

TEST_F(TravelTeleportTest, TeleportToFirstTeleportLocationTakesTheFirstRoute) {
	spawnActor(1000);

	TeleportService::teleportToFirstTeleportLocation(player(), npc(KUSTANON, POETA, KUSTANON_SPOT), TeleportAnimation::FADE_OUT_BEAM);

	ASSERT_TRUE(player().getFlightPath());
	EXPECT_EQ(player().getFlightPath()->getId(), 5001) << "Kustanon's only location, loc 13";
	EXPECT_EQ(kinah(), 1000 - 226);
}

TEST_F(TravelTeleportTest, TeleportToFirstTeleportLocationOfDainesTakesSanctumAndItsQuest) {
	spawnActor(1000);

	TeleportService::teleportToFirstTeleportLocation(player(), npc(DAINES, POETA, DAINES_SPOT), TeleportAnimation::FADE_OUT_BEAM);

	// the first route is loc 2 (Sanctum, required_quest 1006), not loc 4
	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NEED_FINISH_QUEST())}));
	EXPECT_EQ(kinah(), 1000);
}

TEST_F(TravelTeleportTest, TeleportToFirstTeleportLocationOfARefusedTeleporterDoesNothingElse) {
	spawnActor(1000);

	TeleportService::teleportToFirstTeleportLocation(player(), npc(AERO, POETA, AERO_SPOT), TeleportAnimation::FADE_OUT_BEAM);

	EXPECT_EQ(sent(), cptest::exactly({forActor(SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_FAR_FROM_NPC())}));
	EXPECT_EQ(kinah(), 1000);
	EXPECT_FALSE(player().getFlightPath());
}

TEST_F(TravelTeleportTest, TeleportToFirstTeleportLocationOfATeleporterWithoutRoutesIsJavasNullPointerException) {
	// <locations> without a <telelocation>: JAXB leaves TeleLocIdData.locids null, so getTelelocations().getFirst() throws NullPointerException
	dataholders::DataManager::TELEPORTER_DATA.resetForTests();
	dataholders::DataManager::TELEPORTER_DATA.publish(xml::bindString<dataholders::TeleporterData>(context,
		R"xml(<npc_teleporter><teleporter_template npc_ids="203194" teleportId="2"><locations/></teleporter_template></npc_teleporter>)xml"));
	spawnActor(1000);

	EXPECT_THROW(TeleportService::teleportToFirstTeleportLocation(player(), npc(DAINES, POETA, DAINES_SPOT), TeleportAnimation::FADE_OUT_BEAM),
		runtime::NullPointerException);
	EXPECT_EQ(kinah(), 1000);
}

// ---- DialogService AIRLINE_SERVICE (DialogService.java:187-197) ----------------------------------------------------------------------------

TEST_F(TravelTeleportTest, ThePoetaTeleporterRefusesANonDaevaAndShowsADaevaItsMap) {
	spawnActor(0);
	Npc& daines = npc(DAINES, POETA, DAINES_SPOT);

	DialogService::onDialogSelect(model::DialogAction::AIRLINE_SERVICE, player(), daines, 0, 0);
	EXPECT_EQ(sent(), cptest::exactly({dialogWindow(daines.getObjectId(), DIALOG_PAGE_NO_RIGHT)}));
	clearSent();

	player().getCommonData()->setDaeva(true);
	DialogService::onDialogSelect(model::DialogAction::AIRLINE_SERVICE, player(), daines, 0, 0);
	EXPECT_EQ(sent(), cptest::exactly({teleportMap(daines.getObjectId(), 2)}));
}

TEST_F(TravelTeleportTest, TheIshalgenTeleporterRefusesANonDaevaAndShowsADaevaItsMap) {
	spawnActor(0, model::Race::ASMODIANS, ISHALGEN, Spot{OSMAR_SPOT.x + 1.0f, OSMAR_SPOT.y, OSMAR_SPOT.z, int8_t{0}});
	Npc& osmar = npc(OSMAR, ISHALGEN, OSMAR_SPOT);

	DialogService::onDialogSelect(model::DialogAction::AIRLINE_SERVICE, player(), osmar, 0, 0);
	EXPECT_EQ(sent(), cptest::exactly({dialogWindow(osmar.getObjectId(), DIALOG_PAGE_NO_RIGHT)}));
	clearSent();

	player().getCommonData()->setDaeva(true);
	DialogService::onDialogSelect(model::DialogAction::AIRLINE_SERVICE, player(), osmar, 0, 0);
	EXPECT_EQ(sent(), cptest::exactly({teleportMap(osmar.getObjectId(), 51)}));
}

TEST_F(TravelTeleportTest, AFlightMasterShowsANonDaevaItsMap) {
	spawnActor(0);
	Npc& kustanon = npc(KUSTANON, POETA, KUSTANON_SPOT);

	DialogService::onDialogSelect(model::DialogAction::AIRLINE_SERVICE, player(), kustanon, 0, 0);

	EXPECT_EQ(sent(), cptest::exactly({teleportMap(kustanon.getObjectId(), 103)})) << "only 203679 and 203194 ask for a Daeva";
}

// ---- SiegeService (the P5-12a lease, T-05) ------------------------------------------------------------------------------------------------

TEST(TravelSiegeTest, GetSiegeIdByLocIdMapsTheFortressRoutes) {
	// SiegeService.java:612-674, every label, and a few routes that are no fortress
	const std::vector<std::pair<std::vector<int32_t>, int32_t>> rows{
		{{49, 61}, 1011},
		{{36, 54}, 1131},
		{{37, 55}, 1132},
		{{39, 56}, 1141},
		{{44, 62}, 1211},
		{{45, 57, 72, 75}, 1221},
		{{46, 58, 73, 76}, 1231},
		{{47, 59, 74, 77}, 1241},
		{{48, 60}, 1251},
		{{90}, 2011},
		{{91}, 2021},
		{{93}, 3011},
		{{94}, 3021},
		{{322, 323, 358, 359}, 7011},
		{{316, 317, 368, 369}, 7012},
		{{370, 371}, 7013},
		{{372, 373}, 7014},
		{{0, 2, 4, 9, 13, 38, 50, 92, 95, 315, 318, 324, 374, -1}, 0},
	};
	for (const auto& [locIds, siegeId] : rows)
		for (int32_t locId : locIds)
			EXPECT_EQ(SiegeService::getInstance().getSiegeIdByLocId(locId), siegeId) << "loc " << locId;
}

TEST_F(TravelTeleportTest, EnteringASiegeWorldWithSiegesOffSendsTheTwoEmptyPackets) {
	spawnActor(0);

	SiegeService::getInstance().onEnterSiegeWorld(player());

	// SiegeService.java:603-604: SM_SHIELD_EFFECT and SM_ABYSS_ARTIFACT_INFO3 of no location (H 0 each)
	EXPECT_EQ(sent(), cptest::exactly({javaPacket(SM_SHIELD_EFFECT_OPCODE, PacketWriter().H(0)),
						  javaPacket(SM_ABYSS_ARTIFACT_INFO3_OPCODE, PacketWriter().H(0))}));
}

TEST_F(TravelTeleportTest, EnteringASiegeWorldSendsOnlyTheLocationsOfThePlayersWorld) {
	// SiegeService.java:595-601: both loops keep the locations whose world is the player's. The singleton's maps get what initSieges would put
	// there with sieges on, and lose it again at the end of the case.
	spawnActor(0);
	using model::templates::siegelocation::SiegeLocationTemplate;
	const std::unique_ptr<SiegeLocationTemplate> hereTemplate =
		xml::bindString<SiegeLocationTemplate>(context, R"(<siege_location id="9001" type="FORTRESS" world="210010000"/>)");
	const std::unique_ptr<SiegeLocationTemplate> thereTemplate =
		xml::bindString<SiegeLocationTemplate>(context, R"(<siege_location id="1011" type="FORTRESS" world="400010000"/>)");
	const std::unique_ptr<SiegeLocationTemplate> artifactTemplate =
		xml::bindString<SiegeLocationTemplate>(context, R"(<siege_location id="1012" type="ARTIFACT" world="400010000"/>)");
	SiegeService& service = SiegeService::getInstance();
	ASSERT_EQ(service.getSiegeLocations().size(), 0) << "sieges are off";
	ASSERT_EQ(service.getArtifacts().size(), 0);
	struct Restore {
		SiegeService& service;
		~Restore() {
			service.getSiegeLocations().remove(9001);
			service.getSiegeLocations().remove(1011);
			service.getArtifacts().remove(1012);
		}
	} restore{service};
	runtime::Ref<model::siege::SiegeLocation> here = model::siege::SiegeLocation::create(hereTemplate.get());
	here->setUnderShield(true);
	service.getSiegeLocations().put(9001, here);
	service.getSiegeLocations().put(1011, model::siege::SiegeLocation::create(thereTemplate.get()));
	// an artifact of Reshanta only: one of the player's world would be written with ArtifactLocation.getStatus, still unported (P5-12a)
	service.getArtifacts().put(1012, model::siege::ArtifactLocation::create(artifactTemplate.get()));

	service.onEnterSiegeWorld(player());

	// SM_SHIELD_EFFECT.java writeImpl: H count, then D location id and C under shield per location; SM_ABYSS_ARTIFACT_INFO3: H count
	EXPECT_EQ(sent(), cptest::exactly({javaPacket(SM_SHIELD_EFFECT_OPCODE, PacketWriter().H(1).D(9001).C(1)),
						  javaPacket(SM_ABYSS_ARTIFACT_INFO3_OPCODE, PacketWriter().H(0))}));
}

} // namespace
} // namespace aion::gameserver::services::teleport::test
