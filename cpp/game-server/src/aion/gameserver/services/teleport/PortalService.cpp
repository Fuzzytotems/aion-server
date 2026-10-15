#include "aion/gameserver/services/teleport/PortalService.h"

#include <optional>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/configs/administration/AdminConfig.h"
#include "aion/gameserver/configs/main/MembershipConfig.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/InstanceCooltimeData.h"
#include "aion/gameserver/dataholders/PortalLocData.h"
#include "aion/gameserver/dataholders/WorldMapsData.h"
#include "aion/gameserver/model/DialogPage.h"
#include "aion/gameserver/model/DialogPageInfo.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/RaceInfo.h"
#include "aion/gameserver/model/animations/TeleportAnimation.h"
#include "aion/gameserver/model/gameobjects/AionObject.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/player/AbyssRank.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/PortalCooldownList.h"
#include "aion/gameserver/model/gameobjects/player/QuestStateList.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/siege/FortressLocation.h"
#include "aion/gameserver/model/siege/SiegeRaceInfo.h"
#include "aion/gameserver/model/team/GeneralTeam.h"
#include "aion/gameserver/model/team/alliance/PlayerAlliance.h"
#include "aion/gameserver/model/team/group/PlayerGroup.h"
#include "aion/gameserver/model/team/league/League.h"
#include "aion/gameserver/model/templates/InstanceCooltime.h"
#include "aion/gameserver/model/templates/npc/NpcTemplate.h"
#include "aion/gameserver/model/templates/portal/ItemReq.h"
#include "aion/gameserver/model/templates/portal/PortalLoc.h"
#include "aion/gameserver/model/templates/portal/PortalPath.h"
#include "aion/gameserver/model/templates/portal/QuestReq.h"
#include "aion/gameserver/model/templates/world/WorldMapTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_DIALOG_WINDOW.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/questEngine/model/QuestState.h"
#include "aion/gameserver/questEngine/model/QuestStatus.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/SiegeService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/teleport/TeleportService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/stats/AbyssRankEnumInfo.h"
#include "aion/gameserver/world/WorldMapInstance.h"
#include "aion/gameserver/world/WorldMapTypeInfo.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::teleport {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.teleport.PortalService");

namespace {

using model::gameobjects::Npc;
using model::gameobjects::player::Player;
using model::templates::portal::PortalLoc;
using model::templates::portal::PortalPath;
using network::aion::serverpackets::SM_DIALOG_WINDOW;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/** Java `portalPath.getXxx()` on a null portal path: the NullPointerException of the dereference */
const PortalPath& requirePath(const PortalPath* portalPath) {
	if (portalPath == nullptr)
		throw runtime::NullPointerException("portalPath");
	return *portalPath;
}

/** Java `loc.getXxx()` on a null portal loc */
const PortalLoc& requireLoc(const PortalLoc* loc) {
	if (loc == nullptr)
		throw runtime::NullPointerException("loc");
	return *loc;
}

/** Java `npc.getObjectTemplate().isDialogNpc()` */
bool isDialogNpc(Npc& npc) {
	return npc.getObjectTemplate()->isDialogNpc();
}

} // namespace

// Java PortalService.java:45-47
void PortalService::port(const model::templates::portal::PortalPath* portalPath, model::gameobjects::player::Player& player,
	model::gameobjects::Npc& npc) {
	port(portalPath, player, npc, int8_t{0});
}

// Java PortalService.java:49-191
void PortalService::port(const model::templates::portal::PortalPath* portalPath, model::gameobjects::player::Player& player,
	model::gameobjects::Npc& npc, int8_t difficult) {
	const PortalLoc* loc = dataholders::DataManager::PORTAL_LOC_DATA->getPortalLoc(requirePath(portalPath).getLocId());
	if (loc == nullptr) {
		log.warn("No portal loc for locId " + std::to_string(portalPath->getLocId()));
		return;
	}

	bool instanceGroupReq = !(player.hasAccess(configs::administration::AdminConfig::INSTANCE_ENTER_ALL.load())
							  || player.hasPermission(configs::main::MembershipConfig::INSTANCES_GROUP_REQ.load()));
	int32_t mapId = loc->getWorldId();
	const model::templates::InstanceCooltime* instanceRestrictions = dataholders::DataManager::INSTANCE_COOLTIME_DATA->getInstanceCooltimeByWorldId(mapId);
	int32_t maxPlayers = instanceRestrictions == nullptr ? 0
						 : player.getRace() == model::Race::ELYOS ? instanceRestrictions->getMaxMemberLight()
																	: instanceRestrictions->getMaxMemberDark();

	if (!player.hasAccess(configs::administration::AdminConfig::INSTANCE_ENTER_ALL.load())) {
		if (!checkMentor(player, mapId))
			return;
		if (!checkRace(player, npc, portalPath))
			return;
		if (!checkRank(player, npc, portalPath))
			return;
		if (!checkTitle(player, npc, portalPath))
			return;
		if (!checkQuests(player, npc, portalPath))
			return;
		if (instanceGroupReq && !checkPlayerSize(player, npc, portalPath, maxPlayers)) {
			return;
		}
	}

	runtime::Ptr<world::WorldMapInstance> instance;
	switch (maxPlayers) {
		case 0: // 0 means target map has no player limit, so it shouldn't require a registration
			break;
		case 1: // solo
			instance = instance::InstanceService::getRegisteredInstance(mapId, player.getObjectId());
			break;
		case 3:
		case 6: // group
			if (player.getPlayerGroup()) {
				instance = instance::InstanceService::getRegisteredInstance(mapId, player.getPlayerGroup()->getTeamId());
			}
			break;
		default: // alliance
			if (player.isInAlliance()) {
				if (player.isInLeague()) {
					instance = instance::InstanceService::getRegisteredInstance(mapId, player.getPlayerAlliance()->getLeague()->getObjectId());
				} else {
					instance = instance::InstanceService::getRegisteredInstance(mapId, player.getPlayerAlliance()->getObjectId());
				}
			}
			break;
	}

	bool reenter = false;
	if (!instance || !instance->isRegistered(player.getObjectId())) {
		if (player.getPortalCooldownList().isPortalUseDisabled(mapId)) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANNOT_MAKE_INSTANCE_COOL_TIME());
			return;
		}
	} else if (player.getWorldId() != mapId || player.getInstanceId() != instance->getInstanceId()) {
		reenter = true;
	}

	if (!reenter) {
		if (!checkEnterLevel(player, npc, portalPath, instanceRestrictions)) {
			return;
		}
		if (!checkAndRemoveRequiredItems(player, npc, portalPath)) {
			return;
		}
		if (mapId == player.getWorldId()) { // teleport within this instance
			TeleportService::teleportTo(player, mapId, player.getInstanceId(), loc->getX(), loc->getY(), loc->getZ(), loc->getH());
			return;
		}
	}

	runtime::Ptr<model::team::group::PlayerGroup> group = player.getPlayerGroup();
	switch (maxPlayers) {
		case 0:
		case 1:
			// if already registered - just teleport
			if (instance && mapId != player.getWorldId())
				transfer(player, loc, *instance, reenter);
			else
				port(player, loc, reenter, maxPlayers);
			break;
		case 3:
		case 6:
			if (group || !instanceGroupReq) {
				instance = instance::InstanceService::getRegisteredInstance(mapId, group ? group->getTeamId() : player.getObjectId());

				// No instance (for group), group on and default requirement off
				if (!instance && group && !instanceGroupReq) {
					// For each player from group
					for (const runtime::Ptr<model::gameobjects::AionObject>& member : group->getMembers()) {
						// Get his instance
						instance = instance::InstanceService::getRegisteredInstance(mapId, member->getObjectId());

						// If some player is soloing and I found no one else yet, I get his instance
						if (instance) {
							break;
						}
					}

					// No solo instance found
					if (!instance) {
						instance = instance::InstanceService::getNextAvailableInstance(mapId, difficult, maxPlayers);
						instance->registerTeam(*group);
					}
				}
				// No instance and default requirement on = Group on
				else if (!instance && instanceGroupReq) {
					instance = instance::InstanceService::getNextAvailableInstance(mapId, difficult, maxPlayers);
					instance->registerTeam(*group);
				}
				// No instance, default requirement off, no group = Register new instance with player ID
				else if (!instance && !instanceGroupReq && !group) {
					instance = instance::InstanceService::getNextAvailableInstance(mapId, difficult, maxPlayers);
				}
				if (static_cast<int32_t>(instance->getPlayersInside().size()) < maxPlayers) {
					transfer(player, loc, *instance, reenter);
				}
			}
			break;
		default:
			runtime::Ptr<model::team::alliance::PlayerAlliance> allianceGroup = player.getPlayerAlliance();
			if (allianceGroup || !instanceGroupReq) {
				runtime::Ptr<model::team::GeneralTeam> team = allianceGroup;
				if (allianceGroup && allianceGroup->getLeague())
					team = allianceGroup->getLeague();
				int32_t teamId = !team ? player.getObjectId() : team->getObjectId();
				instance = instance::InstanceService::getRegisteredInstance(mapId, teamId);

				if (!instance) {
					instance = instance::InstanceService::getNextAvailableInstance(mapId, difficult, maxPlayers);
					if (team)
						instance->registerTeam(*team);
				}
				if (static_cast<int32_t>(instance->getPlayersInside().size()) < maxPlayers) {
					transfer(player, loc, *instance, reenter);
				}
			}
	}
}

// Java PortalService.java:193-202
bool PortalService::checkMentor(model::gameobjects::player::Player& player, int32_t mapId) {
	const model::templates::InstanceCooltime* instancecooltime = dataholders::DataManager::INSTANCE_COOLTIME_DATA->getInstanceCooltimeByWorldId(mapId);
	if (instancecooltime != nullptr && player.isMentor()) {
		if (!instancecooltime->getCanEnterMentor()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_MENTOR_CANT_ENTER(mapId));
			return false;
		}
	}
	return true;
}

// Java PortalService.java:204-224
bool PortalService::checkEnterLevel(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath, const model::templates::InstanceCooltime* instanceRestrictions) {
	if (player.hasPermission(configs::main::MembershipConfig::INSTANCES_LEVEL_REQ.load()))
		return true;
	int32_t enterMinLvl = requirePath(portalPath).getMinLevel();
	int32_t enterMaxLvl = 0;
	if (instanceRestrictions != nullptr) {
		if (enterMinLvl == 0)
			enterMinLvl = player.getRace() == model::Race::ELYOS ? instanceRestrictions->getEnterMinLevelLight() : instanceRestrictions->getEnterMinLevelDark();
		enterMaxLvl = player.getRace() == model::Race::ELYOS ? instanceRestrictions->getEnterMaxLevelLight() : instanceRestrictions->getEnterMaxLevelDark();
	}
	int32_t lvl = player.getLevel();
	if (lvl < enterMinLvl || (enterMaxLvl > 0 && lvl > enterMaxLvl)) {
		if (portalPath->getErrLevel() != 0) {
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), portalPath->getErrLevel()));
		} else {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_CANT_INSTANCE_ENTER_LEVEL());
		}
		return false;
	}
	return true;
}

// Java PortalService.java:226-240
bool PortalService::checkRace(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath) {
	if (player.hasPermission(configs::main::MembershipConfig::INSTANCES_RACE_REQ.load()))
		return true;
	int32_t siegeId = requirePath(portalPath).getSiegeId();
	model::Race portalRace = portalPath->getRace();
	if ((portalRace != model::Race::PC_ALL && player.getRace() != portalRace) || (siegeId != 0 && !checkSiegeId(player, siegeId))) {
		if (isDialogNpc(npc)) {
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
		} else {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MOVE_PORTAL_ERROR_INVALID_RACE());
		}
		return false;
	}
	return true;
}

// Java PortalService.java:242-250
bool PortalService::checkSiegeId(model::gameobjects::player::Player& player, int32_t sigeId) {
	runtime::Ptr<model::siege::FortressLocation> loc = SiegeService::getInstance().getFortress(sigeId);
	if (loc) {
		if (model::siege::getRaceId(loc->getRace()) != model::getRaceId(player.getRace())) {
			return false;
		}
	}
	return true;
}

// Java PortalService.java:252-258
bool PortalService::checkRank(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath) {
	if (utils::stats::detail::abyssRankData(player.getAbyssRank()->getRank()).id < requirePath(portalPath).getMinRank()) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
		return false;
	}
	return true;
}

// Java PortalService.java:260-282
bool PortalService::checkPlayerSize(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath, int32_t maxPlayers) {
	if (maxPlayers == 6 || maxPlayers == 3) { // group
		if (!player.isInGroup()) {
			if (requirePath(portalPath).getErrGroup() != 0) {
				PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), portalPath->getErrGroup()));
			} else {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ENTER_ONLY_PARTY_DON());
			}
			return false;
		}
	} else if (maxPlayers > 6 && maxPlayers <= 24) { // alliance
		if (!player.isInAlliance()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ENTER_ONLY_FORCE_DON());
			return false;
		}
	} else if (maxPlayers > 24) { // league
		if (!player.isInLeague()) {
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ENTER_ONLY_UNION_DON());
			return false;
		}
	}
	return true;
}

// Java PortalService.java:284-293
bool PortalService::checkTitle(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath) {
	if (player.hasPermission(configs::main::MembershipConfig::INSTANCES_TITLE_REQ.load()))
		return true;
	int32_t titleId = requirePath(portalPath).getTitleId();
	if (titleId != 0 && player.getCommonData()->getTitleId() != titleId) {
		PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
		return false;
	}
	return true;
}

// Java PortalService.java:295-315. JAXB leaves the list of an element without <quest_req> null, which the bound C++ list holds as empty
// (docs/deviations/P5-03.md, the BufEffect row), so `questReq != null` is "not empty" here.
bool PortalService::checkQuests(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath) {
	if (player.hasPermission(configs::main::MembershipConfig::INSTANCES_QUEST_REQ.load()))
		return true;
	const std::vector<model::templates::portal::QuestReq>& questReq = requirePath(portalPath).getQuestReq();
	if (!questReq.empty()) {
		for (const model::templates::portal::QuestReq& quest : questReq) {
			int32_t questId = quest.getQuestId();
			int32_t questStep = quest.getQuestStep();
			const runtime::Ptr<questEngine::model::QuestState> qs = player.getQuestStateList()->getQuestState(questId);
			if (qs && (qs->getStatus() == questEngine::model::QuestStatus::COMPLETE || (questStep > 0 && qs->getQuestVarById(0) >= questStep))) {
				return true; // one requirement matched
			}
		}
		if (isDialogNpc(npc))
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
		else
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_SKILL_CAN_NOT_USE_GROUPGATE_NO_RIGHT()); // seems there's no default msg
		return false;
	}
	return true;
}

// Java PortalService.java:317-343 (an absent <item_req> list is empty here, as in checkQuests)
bool PortalService::checkAndRemoveRequiredItems(model::gameobjects::player::Player& player, model::gameobjects::Npc& npc,
	const model::templates::portal::PortalPath* portalPath) {
	model::items::storage::Storage& inventory = player.getInventory();
	if (inventory.getKinah() < requirePath(portalPath).getKinah()) {
		if (isDialogNpc(npc))
			PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
		else
			PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_NOT_ENOUGH_KINA(static_cast<int64_t>(portalPath->getKinah())));
		return false;
	}
	if (!portalPath->getItemReq().empty()) {
		for (const model::templates::portal::ItemReq& item : portalPath->getItemReq()) {
			if (inventory.getItemCountByItemId(item.getItemId()) < item.getItemCount()) {
				if (isDialogNpc(npc))
					PacketSendUtility::sendPacket(player, SM_DIALOG_WINDOW(npc.getObjectId(), model::id(model::DialogPage::NO_RIGHT)));
				else
					PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CANT_ENTER_WITHOUT_ITEM());
				return false;
			}
		}
		for (const model::templates::portal::ItemReq& item : portalPath->getItemReq())
			inventory.decreaseByItemId(item.getItemId(), item.getItemCount());
	}
	if (portalPath->getKinah() > 0)
		inventory.decreaseKinah(portalPath->getKinah());

	return true;
}

// Java PortalService.java:345-355
void PortalService::port(model::gameobjects::player::Player& requester, const model::templates::portal::PortalLoc* loc, bool reenter,
	int32_t maxPlayers) {
	const model::templates::world::WorldMapTemplate* worldTemplate = dataholders::DataManager::WORLD_MAPS_DATA->getTemplate(requireLoc(loc).getWorldId());
	if (worldTemplate == nullptr)
		throw runtime::NullPointerException("WorldMapsData.getTemplate(" + std::to_string(loc->getWorldId()) + ")");
	if (worldTemplate->isInstance()) {
		// Java: WorldMapType.getWorld(loc.getWorldId()).isPersonal() (NullPointerException for a map id without a WorldMapType constant)
		std::optional<world::WorldMapType> worldMapType = world::getWorldMapType(loc->getWorldId());
		if (!worldMapType)
			throw runtime::NullPointerException("WorldMapType.getWorld(" + std::to_string(loc->getWorldId()) + ")");
		bool isPersonal = world::isPersonal(*worldMapType);
		runtime::Ptr<world::WorldMapInstance> instance =
			instance::InstanceService::getNextAvailableInstance(loc->getWorldId(), isPersonal ? requester.getObjectId() : 0, int8_t{0}, maxPlayers, true);
		instance->register_(requester.getObjectId());
		transfer(requester, loc, *instance, reenter);
	} else {
		TeleportService::teleportTo(requester, loc->getWorldId(), loc->getX(), loc->getY(), loc->getZ(), loc->getH(),
			model::animations::TeleportAnimation::FADE_OUT_BEAM);
	}
}

// Java PortalService.java:357-367
void PortalService::transfer(model::gameobjects::player::Player& player, const model::templates::portal::PortalLoc* loc,
	world::WorldMapInstance& instance, bool reenter) {
	if (!instance.getStartPos())
		instance.setStartPos(world::WorldPosition::create(requireLoc(loc).getWorldId(), loc->getX(), loc->getY(), loc->getZ(), loc->getH()));
	instance.register_(player.getObjectId());
	TeleportService::teleportTo(player, requireLoc(loc).getWorldId(), instance.getInstanceId(), loc->getX(), loc->getY(), loc->getZ(), loc->getH(),
		model::animations::TeleportAnimation::FADE_OUT_BEAM);
	int64_t useDelay = dataholders::DataManager::INSTANCE_COOLTIME_DATA->calculateInstanceEntranceCooltime(player, instance.getMapId());
	if (useDelay > 0 && !reenter) {
		player.getPortalCooldownList().addPortalCooldown(loc->getWorldId(), useDelay);
	}
}

} // namespace aion::gameserver::services::teleport
