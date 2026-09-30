#include "aion/gameserver/services/teleport/TeleportService.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <unordered_map>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/runtime/sched/Future.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/main/SecurityConfig.h"
#include "aion/gameserver/controllers/FlyController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/FlyPathData.h"
#include "aion/gameserver/dataholders/InstanceExitData.h"
#include "aion/gameserver/dataholders/NpcData.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/dataholders/SpawnsData.h"
#include "aion/gameserver/dataholders/TeleLocationData.h"
#include "aion/gameserver/dataholders/TeleporterData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/CreatureType.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/actions/PlayerMode.h"
#include "aion/gameserver/model/animations/ArrivalAnimation.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/animations/TeleportAnimationInfo.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/gameobjects/state/CreatureState.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/siege/SiegeLocation.h"
#include "aion/gameserver/model/team/legion/LegionMember.h"
#include "aion/gameserver/model/templates/BoundRadius.h"
#include "aion/gameserver/model/templates/flypath/FlyPathEntry.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/portal/InstanceExit.h"
#include "aion/gameserver/model/templates/spawns/SpawnSearchResult.h"
#include "aion/gameserver/model/templates/spawns/SpawnSpotTemplate.h"
#include "aion/gameserver/model/templates/teleport/TeleLocIdData.h"
#include "aion/gameserver/model/templates/teleport/TelelocationTemplate.h"
#include "aion/gameserver/model/templates/teleport/TeleportLocation.h"
#include "aion/gameserver/model/templates/teleport/TeleporterTemplate.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_BIND_POINT_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_CHANNEL_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_SPAWN.h"
#include "aion/gameserver/network/aion/serverpackets/SM_STATS_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_LOC.h"
#include "aion/gameserver/network/aion/serverpackets/SM_TELEPORT_MAP.h"
#include "aion/gameserver/services/DuelService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/PrivateStoreService.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/item/ItemPacketService_ItemUpdateType.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/trade/PricesService.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/PositionUtil.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/geo/GeoService.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::teleport {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
// anonymous RequestResponseHandler at TeleportService.java:467 (com.aionemu.gameserver.services.teleport.TeleportService$1); local handler; storage:
// stored in ResponseRequester

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.teleport.TeleportService");

namespace {

/**
 * Java Double.toString (JDK 19+: the shortest decimal that rounds to the value) for the fly-path audit line of teleport: plain notation with
 * at least one fraction digit for 1e-3 <= |d| < 1e7, otherwise computerized scientific notation. A copy of AntiHackService.cpp's file-local
 * helper (P5-14; no shared header has one).
 */
std::string javaDoubleToString(double value) {
	if (std::isnan(value))
		return "NaN";
	if (std::isinf(value))
		return value > 0 ? "Infinity" : "-Infinity";
	if (value == 0)
		return std::signbit(value) ? "-0.0" : "0.0";
	char buffer[64];
	auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::scientific);
	static_cast<void>(error);
	std::string scientific(buffer, end); // e.g. "-1.2345e+02"
	bool negative = scientific.front() == '-';
	if (negative)
		scientific.erase(0, 1);
	size_t e = scientific.find('e');
	int exponent = std::atoi(scientific.c_str() + e + 1);
	std::string digits = scientific.substr(0, e);
	digits.erase(std::remove(digits.begin(), digits.end(), '.'), digits.end());
	std::string result;
	double magnitude = std::abs(value);
	if (magnitude >= 1e-3 && magnitude < 1e7) {
		if (exponent >= 0) {
			size_t integerDigits = static_cast<size_t>(exponent) + 1;
			if (digits.size() <= integerDigits)
				result = digits + std::string(integerDigits - digits.size(), '0') + ".0";
			else
				result = digits.substr(0, integerDigits) + "." + digits.substr(integerDigits);
		} else {
			result = "0." + std::string(static_cast<size_t>(-exponent - 1), '0') + digits;
		}
	} else {
		result = digits.substr(0, 1) + "." + (digits.size() > 1 ? digits.substr(1) : "0") + "E" + std::to_string(exponent);
	}
	return negative ? "-" + result : result;
}

} // namespace

// Java implements Runnable. teleportTo wraps it in a FutureTask kept as the player's TELEPORT task, and CM_TELEPORT_ANIMATION_DONE runs it on
// a later packet, so it is K4 (fieldmap.toml [kinds]): RefCounted, created with create(), retaining the player. The cycle player controller
// tasks -> Future -> task -> player is cut when the task runs or CreatureController.cancelAllTasks cancels it (design §5.1 Tasks).
class TeleportService::SpawnTask final : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
private:
	const runtime::Ref<model::gameobjects::player::Player> player;
	const int32_t worldId;
	const int32_t instanceId;
	const float x;
	const float y;
	const float z;
	const int8_t h;
	const model::animations::TeleportAnimation animation;

protected:
	SpawnTask(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z, int8_t h,
		model::animations::TeleportAnimation animation);
	~SpawnTask() override;

public:
	/** Java: new SpawnTask(player, worldId, instanceId, x, y, z, h, animation) */
	static runtime::Ref<SpawnTask> create(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y,
		float z, int8_t h, model::animations::TeleportAnimation animation);
	void run();
};

TeleportService::SpawnTask::SpawnTask(model::gameobjects::player::Player& value, int32_t worldIdValue, int32_t instanceIdValue, float xValue,
	float yValue, float zValue, int8_t hValue, model::animations::TeleportAnimation animationValue)
	: player(value), worldId(worldIdValue), instanceId(instanceIdValue), x(xValue), y(yValue), z(zValue), h(hValue), animation(animationValue) {
}

TeleportService::SpawnTask::~SpawnTask() = default;

runtime::Ref<TeleportService::SpawnTask> TeleportService::SpawnTask::create(model::gameobjects::player::Player& value, int32_t worldIdValue,
	int32_t instanceIdValue, float xValue, float yValue, float zValue, int8_t hValue, model::animations::TeleportAnimation animationValue) {
	return runtime::makeRef<SpawnTask>(value, worldIdValue, instanceIdValue, xValue, yValue, zValue, hValue, animationValue);
}

// Java TeleportService.java:500-536
void TeleportService::SpawnTask::run() {
	if (player->isSpawned())
		return;

	if (animation != model::animations::TeleportAnimation::NONE) { // this is a delayed teleport (triggered after animation end)
		// instance might be destroyed after animation end if unlucky
		if (player->isDead() || !instance::InstanceService::instanceExists(worldId, instanceId)) {
			utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_PLAYER_INFO(*player));
			world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(*player));
			return;
		}
		abortPlayerActions(*player);
	}

	int32_t currentWorldId = player->getWorldId();
	int32_t currentInstance = player->getInstanceId();
	if (currentWorldId != worldId || currentInstance != instanceId) {
		conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().onLeaveMap(*player);
		instance::InstanceService::onLeaveInstance(*player);
	}
	world::World::getInstance().setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(*player), worldId, instanceId, x, y, z, h);
	world::World::getInstance().setPosition(runtime::Ptr<model::gameobjects::VisibleObject>(player->getPet()), worldId, instanceId, x, y, z, h);

	player->setPortAnimation(model::animations::getDefaultArrivalAnimation(animation));
	if (currentWorldId == worldId && currentInstance == instanceId) {
		// instant teleport when map is the same
		spawnOnSameMap(*player);
	} else {
		// teleport with full map reloading, player will spawn via CM_LEVEL_READY
		utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_CHANNEL_INFO(player->getPosition()));
		utils::PacketSendUtility::sendPacket(*player, network::aion::serverpackets::SM_PLAYER_SPAWN(*player));
		// Java: `... .isInstance() && !WorldMapType.getWorld(worldId).isPersonal()`, the && short-circuit kept as a nested if because the
		// second operand throws NullPointerException for a map id without a WorldMapType constant (InstanceService.cpp:121-125)
		if (dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(worldId)->isInstance()) {
			std::optional<world::WorldMapType> worldMapType = world::getWorldMapType(worldId);
			if (!worldMapType)
				throw runtime::NullPointerException("WorldMapType.getWorld(" + std::to_string(worldId) + ")");
			if (!world::isPersonal(*worldMapType))
				utils::PacketSendUtility::sendPacket(*player,
					network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_DUNGEON_OPENED_FOR_SELF(worldId));
		}
	}
	if (player->isLegionMember() && player->getLegionMember()->getWorldId() != worldId)
		services::LegionService::getInstance().updateMemberInfo(*player);
}

// Java TeleportService.java:65-70
void TeleportService::teleportToFirstTeleportLocation(model::gameobjects::player::Player& player, model::gameobjects::Npc& teleporter,
	model::animations::TeleportAnimation animation) {
	const model::templates::teleport::TeleporterTemplate* teleporterTemplate = validateTeleporterAndGetTemplate(player, teleporter);
	if (teleporterTemplate == nullptr)
		return;
	// Java `getTeleLocIdData().getTelelocations().getFirst()`: a template without <locations> is a NullPointerException, and so is one whose
	// <locations> has no <telelocation> - JAXB leaves that list null, and the bound C++ list is empty then (JAXB never produces an empty
	// non-null list: docs/deviations/P5-03.md, the BufEffect row)
	const model::templates::teleport::TeleLocIdData* teleLocIdData = teleporterTemplate->getTeleLocIdData();
	if (teleLocIdData == nullptr)
		throw runtime::NullPointerException("TeleporterTemplate.getTeleLocIdData() of teleporter " + std::to_string(teleporterTemplate->getTeleportId()));
	if (teleLocIdData->getTelelocations().empty())
		throw runtime::NullPointerException("TeleLocIdData.getTelelocations() of teleporter " + std::to_string(teleporterTemplate->getTeleportId()));
	teleport(player, &teleLocIdData->getTelelocations().front(), animation);
}

// Java TeleportService.java:72-132
void TeleportService::teleport(model::gameobjects::player::Player& player, const model::templates::teleport::TeleportLocation* location,
	model::animations::TeleportAnimation animation) {
	if (location == nullptr)
		throw runtime::NullPointerException("location"); // Java: location.getLocId() on null
	const model::templates::teleport::TelelocationTemplate* locationTemplate =
		dataholders::DataManager::TELELOCATION_DATA->getTelelocationTemplate(location->getLocId());
	if (locationTemplate == nullptr) {
		log.warn("Missing teleloc_template in teleport_location.xml with locId " + std::to_string(location->getLocId()));
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE());
		return;
	}

	// TODO: remove teleportation route if it's enemy fortress (1221, 1231, 1241)
	int32_t id = SiegeService::getInstance().getSiegeIdByLocId(location->getLocId());
	if (id > 0) {
		// Java `SiegeService.getInstance().getSiegeLocation(id).isCanTeleport(player)`: with sieges disabled the location map is empty
		// (Collections.emptyMap(), SiegeService.java:90), so a fortress route is a NullPointerException there - kept (docs/deviations/P5-08.md,
		// the fortress-route row)
		runtime::Ptr<model::siege::SiegeLocation> siegeLocation = SiegeService::getInstance().getSiegeLocation(id);
		if (!siegeLocation)
			throw runtime::NullPointerException("SiegeService.getSiegeLocation(" + std::to_string(id) + ")");
		if (!siegeLocation->isCanTeleport(runtime::Ptr<model::gameobjects::player::Player>(player))) {
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE());
			return;
		}
	}
	if (location->getRequiredQuest() != 0 && !player.isCompleteQuest(location->getRequiredQuest())) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NEED_FINISH_QUEST());
		return;
	}

	if (!checkKinahForTransportation(location, player))
		return;

	if (location->getType() == model::templates::teleport::TeleportType::FLIGHT) {
		// m5f-plan.md D7: the validator looks the fly path up by the location's loc id, a different number space than the fly path ids, so
		// with the validator on (default off) almost every flight is refused - Java's quirk, kept
		if (configs::main::SecurityConfig::ENABLE_FLYPATH_VALIDATOR.load()) {
			const model::templates::flypath::FlyPathEntry* flypath = dataholders::DataManager::FLY_PATH->getPathTemplate(location->getLocId());
			if (flypath == nullptr) {
				utils::audit::AuditLogger::log(player, "tried to use invalid flyPath #" + std::to_string(location->getLocId()));
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE());
				return;
			}

			double dist = utils::PositionUtil::getDistance(player, flypath->getStartX(), flypath->getStartY(), flypath->getStartZ());
			if (dist > 7) {
				utils::audit::AuditLogger::log(player,
					"tried to use flyPath #" + std::to_string(location->getLocId()) + " but he's too far " + javaDoubleToString(dist));
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE());
				return;
			}

			if (player.getWorldId() != flypath->getStartWorldId()) {
				utils::audit::AuditLogger::log(player, "tried to use flyPath #" + std::to_string(location->getLocId()) + " from invalid start world "
													   + std::to_string(player.getWorldId()) + ", expected " + std::to_string(flypath->getStartWorldId()));
				utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_NO_ROUTE());
				return;
			}
			player.setCurrentFlypath(flypath);
		}
		abortPlayerActions(player);
		player.setState(model::gameobjects::state::CreatureState::FLYING);
		player.unsetState(model::gameobjects::state::CreatureState::ACTIVE);
		player.setFlightTeleportId(location->getTeleportId());
		utils::PacketSendUtility::broadcastPacket(player,
			network::aion::serverpackets::SM_EMOTION(player, model::EmotionType::START_FLYTELEPORT, location->getTeleportId(), 0), true);
	} else {
		int32_t instanceId = 1;
		int32_t mapId = locationTemplate->getMapId();
		if (player.getWorldId() == mapId) {
			instanceId = player.getInstanceId();
		}
		sendLoc(player, mapId, instanceId, locationTemplate->getX(), locationTemplate->getY(), locationTemplate->getZ(),
			static_cast<int8_t>(locationTemplate->getHeading()), animation);
	}
}

// Java TeleportService.java:134-156
const model::templates::teleport::TeleporterTemplate* TeleportService::validateTeleporterAndGetTemplate(model::gameobjects::player::Player& player,
	model::gameobjects::Npc& teleporter) {
	const model::templates::teleport::TeleporterTemplate* template_ =
		dataholders::DataManager::TELEPORTER_DATA->getTeleporterTemplateByNpcId(teleporter.getNpcId());
	if (template_ == nullptr) {
		utils::audit::AuditLogger::log(player, "tried to use invalid teleporter " + teleporter.toString() + " (no teleporter data) at "
												   + player.getPosition()->toString());
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_WRONG_NPC());
		return nullptr;
	}
	model::CreatureType creatureType = teleporter.getType(player);
	if (creatureType != model::CreatureType::FRIEND && creatureType != model::CreatureType::SUPPORT) {
		utils::audit::AuditLogger::log(player, "tried to use invalid teleporter " + teleporter.toString() + " (wrong race) at "
												   + player.getPosition()->toString());
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_WRONG_NPC());
		return nullptr;
	}
	if (!utils::PositionUtil::isInTalkRange(player, teleporter)) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_MOVE_TO_AIRPORT_FAR_FROM_NPC());
		return nullptr;
	}
	if (player.isInFlyingState()) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_CANNOT_USE_AIRPORT_WHEN_FLYING());
		return nullptr;
	}
	return template_;
}

// Java TeleportService.java:158-177
bool TeleportService::checkKinahForTransportation(const model::templates::teleport::TeleportLocation* location,
	model::gameobjects::player::Player& player) {
	model::items::storage::Storage& inventory = player.getInventory();

	int64_t transportationPrice;

	// If HiPassEffect is active, then all flight/teleport prices are 1 kinah
	if (player.getEffectController()->hasAbnormalEffect([](skillengine::model::Effect& effect) { return effect.isHiPass(); }))
		transportationPrice = 1;
	else {
		int32_t basePrice = location->getPrice();
		// TODO check for location.getPricePvp()
		transportationPrice = trade::PricesService::getPriceForService(basePrice, player.getRace());
	}

	if (!inventory.tryDecreaseKinah(transportationPrice, item::ItemPacketService_ItemUpdateType::DEC_KINAH_FLY)) {
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_KINA(transportationPrice));
		return false;
	}
	return true;
}

// Java TeleportService.java:179-193
void TeleportService::sendLoc(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z, int8_t h,
	model::animations::TeleportAnimation animation) {
	abortPlayerActions(player);
	// despawn from world and send animation to others (also ends flying)
	world::World::getInstance().despawn(player, model::animations::getDefaultObjectDeleteAnimation(animation));

	runtime::Ref<SpawnTask> spawnTask = SpawnTask::create(player, worldId, instanceId, x, y, z, h, animation);
	if (animation == model::animations::TeleportAnimation::NONE) { // instant teleport (don't wait for player fade-out)
		spawnTask->run();
	} else {
		// send teleport animation to player and trigger CM_TELEPORT_ANIMATION_DONE when the animation ended
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TELEPORT_LOC(worldId, instanceId, x, y, z, h, animation));
		// task will be triggered from CM_TELEPORT_ANIMATION_DONE. Java: new FutureTask<Void>(spawnTask, null), a task bound to no executor;
		// Future::deferred is its C++ form (Future.h:118-127) and the Pin keeps the SpawnTask - and with it the player - alive until it runs
		// or the controller cancels it.
		player.getController().addTask(model::TaskId::TELEPORT,
			runtime::Future::deferred(runtime::Pin(spawnTask), [task = spawnTask.get()] { task->run(); }));
	}
}

// Java TeleportService.java:195-206
void TeleportService::abortPlayerActions(model::gameobjects::player::Player& player) {
	if (player.hasStore())
		PrivateStoreService::closePrivateStore(player);
	RecallService::getInstance().cancel(player, RecallService::CancelReason::CANCELLED);
	player.getController().cancelCurrentSkill(nullptr);
	player.setTarget(nullptr);
	player.unsetPlayerMode(model::actions::PlayerMode::RIDE);
	if (player.isUsingFlightTransporterOrWindstream()) {
		player.setFlightPath(nullptr);
		player.getFlyController().endFly(false);
	}
}

// Java TeleportService.java:208-219
void TeleportService::spawnOnSameMap(model::gameobjects::player::Player& player) {
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_CHANNEL_INFO(player.getPosition()));
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PLAYER_INFO(player));
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_STATS_INFO(player));
	// Java `new SM_MOTION(player.getObjectId(), player.getMotions().getActiveMotions())`: the packet's C++ constructor takes the map by value
	// type, so the RcLinkedHashMap is copied into it exactly as PlayerController::sendPlayerInfoPackets does (PlayerController.cpp:209-214).
	std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::motion::Motion>> activeMotions;
	if (runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<model::gameobjects::player::motion::Motion>>> motions =
			player.getMotions().getActiveMotions()) {
		for (const auto& entry : motions->entrySet())
			activeMotions.emplace(entry.getKey(), entry.getValue());
	}
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_MOTION(player.getObjectId(), activeMotions));
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(player));
	world::World::getInstance().spawn(runtime::Ptr<model::gameobjects::VisibleObject>(player.getPet()));
	player.getController().startProtectionActiveTask();
	player.getEffectController()->updatePlayerEffectIcons(nullptr);
	player.getController().updateZone();
	player.setPortAnimation(model::animations::ArrivalAnimation::NONE);
}

void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldPosition& pos) {
	AION_UNPORTED();
}

void TeleportService::teleportDeadTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z,
	int8_t heading) {
	AION_UNPORTED();
}

// Java TeleportService.java:249-251
void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z) {
	teleportTo(player, worldId, x, y, z, player.getHeading(), model::animations::TeleportAnimation::NONE);
}

// Java TeleportService.java:253-255
void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z, int8_t h) {
	teleportTo(player, worldId, x, y, z, h, model::animations::TeleportAnimation::NONE);
}

// Java TeleportService.java:257-259
void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, float x, float y, float z, int8_t h,
	model::animations::TeleportAnimation animation) {
	teleportTo(player, worldId, player.getWorldId() != worldId ? 1 : player.getInstanceId(), x, y, z, h, animation);
}

// Java TeleportService.java:261-263 (DialogService's ENTER_PVP / LEAVE_PVP arms: the arena npcs of Sanctum, Pandaemonium and Eltnen)
void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z) {
	teleportTo(player, worldId, instanceId, x, y, z, player.getHeading(), model::animations::TeleportAnimation::NONE);
}

// Java TeleportService.java:265-267
void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z,
	int8_t h) {
	teleportTo(player, worldId, instanceId, x, y, z, h, model::animations::TeleportAnimation::NONE);
}

// Java TeleportService.java:269-271
void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z) {
	teleportTo(player, instance.getMapId(), instance.getInstanceId(), x, y, z, player.getHeading(), model::animations::TeleportAnimation::NONE);
}

// Java TeleportService.java:273-275
void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z, int8_t h) {
	teleportTo(player, instance.getMapId(), instance.getInstanceId(), x, y, z, h, model::animations::TeleportAnimation::NONE);
}

// Java TeleportService.java:277-279
void TeleportService::teleportTo(model::gameobjects::player::Player& player, world::WorldMapInstance& instance, float x, float y, float z, int8_t h,
	model::animations::TeleportAnimation animation) {
	teleportTo(player, instance.getMapId(), instance.getInstanceId(), x, y, z, h, animation);
}

// Java TeleportService.java:281-288
void TeleportService::teleportTo(model::gameobjects::player::Player& player, int32_t worldId, int32_t instanceId, float x, float y, float z,
	int8_t heading, model::animations::TeleportAnimation animation) {
	if (player.isDead()) {
		player::PlayerReviveService::revive(player, 20, 20, true, 0);
	} else if (DuelService::getInstance().isDueling(player)) {
		DuelService::getInstance().loseDuel(player);
	}
	sendLoc(player, worldId, instanceId, x, y, z, heading, animation);
}

// Java TeleportService.java:291-295
void TeleportService::showMap(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc) {
	const model::templates::teleport::TeleporterTemplate* template_ = validateTeleporterAndGetTemplate(player, npc);
	if (template_ != nullptr)
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_TELEPORT_MAP(npc.getObjectId(), template_->getTeleportId()));
}

void TeleportService::teleportToPrison(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

// Java TeleportService.java:304-333. Ported by phase 6's route-hand lane under a lease of this file (chunks.cmake, Q09 LEASE): quest 2007's
// last step calls it (_2007WheresRaeThisTime.java:130). A cross-map arm goes through InstanceService::getOrRegisterInstance or the main
// instance of the npc's map, as Java does.
void TeleportService::teleportToNpc(model::gameobjects::player::Player& player, int32_t npcId) {
	std::optional<model::templates::spawns::SpawnSearchResult> searchResult =
		dataholders::DataManager::SPAWNS_DATA->getFirstSpawnByNpcId(player.getWorldId(), npcId);

	if (!searchResult) {
		log.warn("No npc spawn found for : " + std::to_string(npcId));
		return;
	}

	const model::templates::spawns::SpawnSpotTemplate& spot = searchResult->getSpot();
	const model::templates::npc::NpcTemplate* npcTemplate = dataholders::DataManager::NPC_DATA->getNpcTemplate(npcId);
	// StaticObject has no npcTemplate since it's no npc
	float npcRadius = npcTemplate == nullptr ? 1 : npcTemplate->getBoundRadius()->getFront();
	runtime::Ptr<world::WorldMapInstance> instance;
	if (player.getWorldId() == searchResult->getWorldId())
		instance = player.getPosition()->getWorldMapInstance();
	else if (world::World::getInstance().getWorldMap(searchResult->getWorldId())->isInstanceType())
		instance = instance::InstanceService::getOrRegisterInstance(searchResult->getWorldId(), player);
	else
		instance = world::World::getInstance().getWorldMap(searchResult->getWorldId())->getMainWorldMapInstance();

	// calculate position 1m in front of the npc
	double radian = utils::PositionUtil::convertHeadingToAngle(spot.getHeading()) * 0.017453292519943295; // Java: Math.toRadians (JDK 9+)
	float x = spot.getX() + static_cast<float>(std::cos(radian)) * (1.0f + npcRadius);
	float y = spot.getY() + static_cast<float>(std::sin(radian)) * (1.0f + npcRadius);
	float z = world::geo::GeoService::getInstance().getZ(searchResult->getWorldId(), x, y, spot.getZ(), instance->getInstanceId());
	if (std::isnan(z)) // no collision found or geo disabled
		z = spot.getZ() + 0.5f;
	// look towards npc (Java: (byte) of the int arithmetic)
	int8_t heading = static_cast<int8_t>((spot.getHeading() & 0xFF) >= 60 ? spot.getHeading() - 60 : spot.getHeading() + 60);

	teleportTo(player, *instance, x, y, z, heading, model::animations::TeleportAnimation::NONE);
}

void TeleportService::sendObeliskBindPoint(model::gameobjects::player::Player& player) {
	int32_t worldId;
	float x, y, z;
	if (runtime::Ptr<model::gameobjects::player::BindPointPosition> bplist = player.getBindPoint()) {
		worldId = bplist->getMapId();
		x = bplist->getX();
		y = bplist->getY();
		z = bplist->getZ();
	} else {
		const dataholders::PlayerInitialData::LocationData& locationData = dataholders::DataManager::PLAYER_INITIAL_DATA->getSpawnLocation(player.getRace());
		worldId = locationData.getMapId();
		x = locationData.getX();
		y = locationData.getY();
		z = locationData.getZ();
	}
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_BIND_POINT_INFO(worldId, x, y, z));
}

void TeleportService::sendKiskBindPoint(model::gameobjects::player::Player& player) {
	if (player.getKisk())
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_BIND_POINT_INFO(player.getKisk()));
}

// Java TeleportService.java:362-383
void TeleportService::moveToBindLocation(model::gameobjects::player::Player& player) {
	float x, y, z;
	int32_t worldId;
	int8_t h;

	if (runtime::Ptr<model::gameobjects::player::BindPointPosition> bplist = player.getBindPoint()) {
		worldId = bplist->getMapId();
		x = bplist->getX();
		y = bplist->getY();
		z = bplist->getZ();
		h = bplist->getHeading();
	} else {
		const dataholders::PlayerInitialData::LocationData& locationData =
			dataholders::DataManager::PLAYER_INITIAL_DATA->getSpawnLocation(player.getRace());
		worldId = locationData.getMapId();
		x = locationData.getX();
		y = locationData.getY();
		z = locationData.getZ();
		h = locationData.getHeading();
	}
	teleportTo(player, worldId, x, y, z, h);
}

void TeleportService::moveToTargetWithDistance(model::gameobjects::VisibleObject& object, model::gameobjects::player::Player& player,
	int32_t direction, int32_t distance) {
	AION_UNPORTED();
}

// Java TeleportService.java:394-403
void TeleportService::moveToInstanceExit(model::gameobjects::player::Player& player, int32_t worldId, model::Race race) {
	const model::templates::portal::InstanceExit* instanceExit = dataholders::DataManager::INSTANCE_EXIT_DATA->getInstanceExit(worldId, race);
	if (instanceExit != nullptr && instance::InstanceService::instanceExists(instanceExit->getExitWorld(), 1)) {
		teleportTo(player, instanceExit->getExitWorld(), instanceExit->getX(), instanceExit->getY(), instanceExit->getZ(), instanceExit->getH());
	} else {
		if (instanceExit == nullptr)
			log.warn("No instance exit found for race: " + std::string(xml::enumName(race)) + " " + std::to_string(worldId));
		moveToBindLocation(player);
	}
}

void TeleportService::useTeleportScroll(model::gameobjects::player::Player& player, std::string_view portalName, int32_t worldId) {
	AION_UNPORTED();
}

void TeleportService::changeChannel(model::gameobjects::player::Player& player, int32_t channel) {
	AION_UNPORTED();
}

void TeleportService::setEventPos(world::WorldPosition& pos, model::Race race) {
	AION_UNPORTED();
}

void TeleportService::teleportToEvent(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool TeleportService::sendTeleportRequest(model::gameobjects::player::Player& player, int32_t npcId) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services::teleport
