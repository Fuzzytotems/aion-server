#include "aion/gameserver/services/player/PlayerReviveService.h"

#include <optional>
#include <unordered_map>

#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/attack/AggroList.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/instance/handlers/InstanceHandler.h"
#include "aion/gameserver/model/EmotionType.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/gameobjects/Kisk.h"
#include "aion/gameserver/model/gameobjects/VisibleObject.h"
#include "aion/gameserver/model/gameobjects/player/CustomPlayerState.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/motion/Motion.h"
#include "aion/gameserver/model/gameobjects/player/motion/MotionList.h"
#include "aion/gameserver/model/stats/container/PlayerGameStats.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/common/legacy/GroupEvent.h"
#include "aion/gameserver/model/team/common/legacy/PlayerAllianceEvent.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/model/vortex/VortexLocation.h"
#include "aion/gameserver/network/aion/serverpackets/SM_EMOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_MOTION.h"
#include "aion/gameserver/network/aion/serverpackets/SM_PLAYER_INFO.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/services/VortexService.h"
#include "aion/gameserver/services/panesterra/PanesterraService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/services/vortex/DimensionalVortex.h"
#include "aion/gameserver/skillengine/model/Effect.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldMap.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"
#include "aion/gameserver/world/knownlist/KnownList.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::player {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   com.aionemu.gameserver.services.player.PlayerReviveService@L251:92

void PlayerReviveService::duelRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerReviveService::skillRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

void PlayerReviveService::rebirthRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

// Java PlayerReviveService.java:100-102
void PlayerReviveService::bindRevive(model::gameobjects::player::Player& player) {
	bindRevive(player, 0);
}

// Java PlayerReviveService.java:104-133
void PlayerReviveService::bindRevive(model::gameobjects::player::Player& player, int32_t skillId) {
	using model::gameobjects::player::CustomPlayerState;
	if (player.isInCustomState(CustomPlayerState::EVENT_MODE))
		revive(player, 100, 100, false, skillId);
	else
		revive(player, 25, 25, true, skillId);
	if (skillId > 0)
		utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	player.getGameStats()->updateStatsAndSpeedVisually();
	if (player.isInPrison()) {
		teleport::TeleportService::teleportToPrison(player);
	} else if (player.isInCustomState(CustomPlayerState::EVENT_MODE)) {
		teleport::TeleportService::teleportToEvent(player);
	} else if (world::getWorldMapType(player.getWorldId()) == world::WorldMapType::BELUS) {
		// Java: WorldMapType.getWorld(player.getWorldId()) == WorldMapType.BELUS - a world id with no constant is null, which is != BELUS;
		// the C++ optional compares the same way (std::nullopt != any value).
		panesterra::PanesterraService::getInstance().reviveInEventLocation(player);
	} else if (!panesterra::PanesterraService::getInstance().teleportToStartPosition(player)) {
		runtime::Ptr<world::WorldPosition> resPos;
		for (const runtime::Ref<vortex::DimensionalVortex>& vortex : VortexService::getInstance().getActiveInvasions().values()) {
			// Java `player.getRace() == vortex.getVortexLocation().getInvadersRace()`: a template without offence_race makes the right side
			// null, so `==` is false and never throws. VortexLocation::isInvadersRace is that comparison (VortexLocation.h:69-73).
			if (vortex->getVortexLocation()->isInvadersRace(player.getRace()) && vortex->getVortexLocation()->isInsideLocation(player)) {
				resPos = vortex->getVortexLocation()->getResurrectionPoint();
				break;
			}
		}

		if (resPos)
			teleport::TeleportService::teleportTo(player, *resPos);
		else
			teleport::TeleportService::moveToBindLocation(player);
	}
	player.unsetResPosState();
}

// Java PlayerReviveService.java:135-137
void PlayerReviveService::kiskRevive(model::gameobjects::player::Player& player) {
	kiskRevive(player, 0);
}

// Java PlayerReviveService.java:139-155
void PlayerReviveService::kiskRevive(model::gameobjects::player::Player& player, int32_t skillId) {
	using model::gameobjects::player::CustomPlayerState;
	if (player.isInPrison())
		teleport::TeleportService::teleportToPrison(player);
	else if (player.isInCustomState(CustomPlayerState::EVENT_MODE))
		teleport::TeleportService::teleportToEvent(player);

	runtime::Ptr<model::gameobjects::Kisk> kisk = player.getKisk();
	if (kisk && kisk->isActive()) {
		kisk->resurrectionUsed();
		if (skillId > 0)
			utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
		revive(player, 30, 30, false, skillId);
		player.getGameStats()->updateStatsAndSpeedVisually();
		player.unsetResPosState();
		teleport::TeleportService::teleportTo(player, *kisk->getPosition());
	}
}

// Java PlayerReviveService.java:157-159
void PlayerReviveService::instanceRevive(model::gameobjects::player::Player& player) {
	instanceRevive(player, 0);
}

// Java PlayerReviveService.java:161-187
void PlayerReviveService::instanceRevive(model::gameobjects::player::Player& player, int32_t skillId) {
	using model::gameobjects::player::CustomPlayerState;
	using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
	if (player.isInCustomState(CustomPlayerState::EVENT_MODE)) {
		revive(player, 100, 100, false, skillId);
		utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
		player.getGameStats()->updateStatsAndSpeedVisually();
		teleport::TeleportService::teleportToEvent(player);
		return;
	}
	if (player.getPosition()->getWorldMapInstance()->getInstanceHandler()->onReviveEvent(player))
		return;
	runtime::Ptr<world::WorldMap> map = world::World::getInstance().getWorldMap(player.getWorldId());
	if (!map) {
		bindRevive(player);
		return;
	}
	revive(player, 25, 25, true, skillId);
	utils::PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_REBIRTH_MASSAGE_ME());
	player.getGameStats()->updateStatsAndSpeedVisually();
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_PLAYER_INFO(player));
	// Java `new SM_MOTION(player.getObjectId(), player.getMotions().getActiveMotions())`: the packet's C++ constructor takes the map by value
	// type, so the RcLinkedHashMap is copied into it as TeleportService::spawnOnSameMap does (TeleportService.cpp:224-231)
	std::unordered_map<int32_t, runtime::Ptr<model::gameobjects::player::motion::Motion>> activeMotions;
	if (runtime::Ptr<runtime::RcLinkedHashMap<int32_t, runtime::Ref<model::gameobjects::player::motion::Motion>>> motions =
			player.getMotions().getActiveMotions()) {
		for (const auto& entry : motions->entrySet())
			activeMotions.emplace(entry.getKey(), entry.getValue());
	}
	utils::PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_MOTION(player.getObjectId(), activeMotions));
	if (map->isInstanceType() && player.getPosition()->getWorldMapInstance()->getStartPos()) {
		runtime::Ptr<world::WorldPosition> pos = player.getPosition()->getWorldMapInstance()->getStartPos();
		teleport::TeleportService::teleportTo(player, pos->getMapId(), pos->getX(), pos->getY(), pos->getZ());
	} else {
		bindRevive(player);
	}
	player.unsetResPosState();
}

// Java PlayerReviveService.java:189-213
void PlayerReviveService::revive(model::gameobjects::player::Player& player, int32_t hpPercent, int32_t mpPercent, bool setSoulSickness,
	int32_t resurrectionSkill) {
	using model::gameobjects::player::Player;
	player.getKnownList().forEachPlayer([&player](Player& p) {
		// Java `player.equals(p.getTarget())`: AionObject.equals(null) is false
		runtime::Ptr<model::gameobjects::VisibleObject> target = p.getTarget();
		if (target && player.equals(*target))
			p.setTarget(nullptr);
	});
	// M5b-1 skipped this guard with a constant false (docs/deviations/P5-08.md) until EffectController::hasAbnormalEffect(predicate) and
	// Effect::isNoResurrectPenalty were ported; M5b-2 closed it, as D14's isUnderNormalShield (SkillEngine::createCriticalProcEffect)
	const bool isNoResurrectPenalty =
		player.getEffectController()->hasAbnormalEffect([](skillengine::model::Effect& effect) { return effect.isNoResurrectPenalty(); });
	player.setPlayerResActivate(false);
	player.getLifeStats()->setCurrentHpPercent(isNoResurrectPenalty ? 100 : hpPercent);
	player.getLifeStats()->setCurrentMpPercent(isNoResurrectPenalty ? 100 : mpPercent);
	if (player.getCommonData()->getDp() > 0 && !isNoResurrectPenalty)
		player.getCommonData()->setDp(0);
	if (!isNoResurrectPenalty && setSoulSickness) {
		player.getController().updateSoulSickness(resurrectionSkill);
	}
	player.setResurrectionSkill(0);
	player.getAggroList().clear();
	player.getController().onBeforeSpawn();
	if (player.isInGroup()) {
		model::team::group::PlayerGroupService::updateGroup(player, model::team::common::legacy::GroupEvent::MOVEMENT);
	}
	if (player.isInAlliance()) {
		model::team::alliance::PlayerAllianceService::updateAlliance(player, model::team::common::legacy::PlayerAllianceEvent::MOVEMENT);
	}
	utils::PacketSendUtility::broadcastPacket(player,
		network::aion::serverpackets::SM_EMOTION(player, model::EmotionType::RESURRECT), true);
}

void PlayerReviveService::itemSelfRevive(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

// Java PlayerReviveService.java:250-260. The lambda (fieldmap callback PlayerReviveService@L251:92) captures the player and the skill id; it is
// pinned to the player until it has run, and stored as the controller's TELEPORT task as Java stores its Future (TeleportService.cpp's
// deferred spawn task is the same pair)
void PlayerReviveService::scheduleReviveAtBase(model::gameobjects::player::Player& player, int32_t delayMillis, int32_t skillId) {
	player.getController().addTask(model::TaskId::TELEPORT, utils::ThreadPoolManager::getInstance().schedule({&player}, [&player, skillId] {
		player.getController().getAndRemoveTask(model::TaskId::TELEPORT); // remove manually as it won't get removed automatically
		if (player.isInInstance())
			PlayerReviveService::instanceRevive(player, skillId);
		else if (player.getKisk())
			PlayerReviveService::kiskRevive(player, skillId);
		else
			PlayerReviveService::bindRevive(player, skillId);
	}, delayMillis));
}

} // namespace aion::gameserver::services::player
