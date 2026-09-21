#include "aion/gameserver/services/player/PlayerLeaveWorldService.h"

#include <memory>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/controllers/NpcController.h"
#include "aion/gameserver/controllers/PetController.h"
#include "aion/gameserver/controllers/PlayerController.h"
#include "aion/gameserver/controllers/effect/PlayerEffectController.h"
#include "aion/gameserver/dao/ItemCooldownsDAO.h"
#include "aion/gameserver/dao/PlayerCooldownsDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dao/PlayerEffectsDAO.h"
#include "aion/gameserver/dao/PlayerLifeStatsDAO.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/PlayerInitialData.h"
#include "aion/gameserver/model/TaskId.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Npc.h"
#include "aion/gameserver/model/gameobjects/Pet.h"
#include "aion/gameserver/model/gameobjects/Summon.h"
#include "aion/gameserver/model/gameobjects/player/BindPointPosition.h"
#include "aion/gameserver/model/gameobjects/player/FriendList.h"
#include "aion/gameserver/model/gameobjects/player/LogoutBreakers.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/gameobjects/player/ResponseRequester.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/stats/container/PlayerLifeStats.h"
#include "aion/gameserver/model/summons/UnsummonType.h"
#include "aion/gameserver/model/team/alliance/PlayerAllianceService.h"
#include "aion/gameserver/model/team/group/PlayerGroupService.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/chatserver/ChatServer.h"
#include "aion/gameserver/questEngine/QuestEngine.h"
#include "aion/gameserver/questEngine/model/QuestEnv.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Finally.h"
#include "aion/gameserver/runtime/sched/Pin.h"
#include "aion/gameserver/services/AutoGroupService.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/services/DuelService.h"
#include "aion/gameserver/services/ExchangeService.h"
#include "aion/gameserver/services/KiskService.h"
#include "aion/gameserver/services/LegionService.h"
#include "aion/gameserver/services/RecallService.h"
#include "aion/gameserver/services/RepurchaseService.h"
#include "aion/gameserver/services/conquerorAndProtectorSystem/ConquerorAndProtectorService.h"
#include "aion/gameserver/services/findgroup/FindGroupService.h"
#include "aion/gameserver/services/instance/InstanceService.h"
#include "aion/gameserver/services/player/MultiClientingService.h"
#include "aion/gameserver/services/player/PlayerReviveService.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/summons/SummonsService.h"
#include "aion/gameserver/skillengine/task/AbstractInteractionTask.h"
#include "aion/gameserver/taskmanager/tasks/ExpireTimerTask.h"
#include "aion/gameserver/utils/ThreadPoolManager.h"
#include "aion/gameserver/utils/audit/GMService.h"
#include "aion/gameserver/world/World.h"
#include "aion/gameserver/world/WorldPosition.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::services::player {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3):
//   com.aionemu.gameserver.services.player.PlayerLeaveWorldService@L53:71 - the leaveWorld task of leaveWorldDelayed, pinned on the player

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.player.PlayerLeaveWorldService");

using model::gameobjects::player::Player;
using runtime::Ptr;

void PlayerLeaveWorldService::leaveWorldDelayed(Player& player, int64_t delayInMillis) {
	runtime::FutureRef leaveWorldTask =
		utils::ThreadPoolManager::getInstance().schedule(runtime::Pin(&player), [&player] { leaveWorld(player); }, delayInMillis);
	player.getController().addTask(model::TaskId::DESPAWN, std::move(leaveWorldTask));
}

void PlayerLeaveWorldService::leaveWorld(Player& player) {
	// C++ only (runtime-architecture.md §5.3 RR-5): the logout breakers run on every exit, also when a call below throws
	auto breakers = runtime::finally([&player]() noexcept { model::gameobjects::player::LogoutBreakers::run(player); });
	std::shared_ptr<network::aion::AionConnection> con = player.getClientConnection();
	player.setClientConnection(nullptr); // this sets the player semi-offline, PacketSendUtility will not send packets anymore

	Ptr<world::WorldPosition> pos = player.getPosition();
	if (!pos || !pos->getMapRegion()) { // ensure safe logout
		log.warn(player.toString() + " had invalid position: " + (pos ? pos->toString() : std::string("null")) + " so he was reset to bind point");
		Ptr<model::gameobjects::player::BindPointPosition> bp = player.getBindPoint();
		runtime::Ref<world::WorldPosition> newPosition;
		if (bp)
			newPosition = world::World::getInstance().createPosition(bp->getMapId(), bp->getX(), bp->getY(), bp->getZ(), bp->getHeading(), 1);
		else {
			const dataholders::PlayerInitialData::LocationData& ld = dataholders::DataManager::PLAYER_INITIAL_DATA->getSpawnLocation(player.getRace());
			newPosition = world::World::getInstance().createPosition(ld.getMapId(), ld.getX(), ld.getY(), ld.getZ(), ld.getHeading(), 1);
		}
		player.setPosition(newPosition);
	}

	findgroup::FindGroupService::getInstance().onLogout(player);
	RecallService::getInstance().cancel(player, RecallService::CancelReason::CANCELLED);
	player.getResponseRequester().denyAll();
	player.getFriendList().setStatus(model::gameobjects::player::FriendList::Status::OFFLINE, *player.getCommonData());
	BrokerService::getInstance().removePlayerCache(player);
	ExchangeService::getInstance().cancelExchange(player);
	RepurchaseService::getInstance().removeRepurchaseItems(player);
	if (configs::main::AutoGroupConfig::AUTO_GROUP_ENABLE.load())
		AutoGroupService::getInstance().onLogout(player);
	conquerorAndProtectorSystem::ConquerorAndProtectorService::getInstance().onLeaveMap(player);
	MultiClientingService::onLeaveWorld(player);
	instance::InstanceService::onLogout(player);
	utils::audit::GMService::getInstance().onPlayerLogout(player);
	KiskService::getInstance().onLogout(player);

	if (player.isDead()) {
		if (player.isInInstance() || player.getWorldId() == 400030000)
			PlayerReviveService::instanceRevive(player);
		else
			PlayerReviveService::bindRevive(player);
	} else if (DuelService::getInstance().isDueling(player)) {
		DuelService::getInstance().loseDuel(player);
	}
	player.getEffectController()->removeNonStorableEffectsForLogout();
	dao::PlayerEffectsDAO::storePlayerEffects(player);
	dao::ItemCooldownsDAO::storeItemCooldowns(player);
	dao::PlayerLifeStatsDAO::updatePlayerLifeStat(player);

	model::team::group::PlayerGroupService::onPlayerLogout(player);
	model::team::alliance::PlayerAllianceService::onPlayerLogout(player);
	// fix legion warehouse exploits
	LegionService::getInstance().LegionWhUpdate(player);
	player.getEffectController()->removeAllEffects(true);
	player.getLifeStats()->cancelAllTasks();

	Ptr<model::gameobjects::Summon> summon = player.getSummon();
	if (summon)
		summons::SummonsService::release(*summon, model::summons::UnsummonType::LOGOUT); // puts the summoning skill on cooldown, so store cooldowns afterwards
	dao::PlayerCooldownsDAO::storePlayerCooldowns(player);
	if (player.getPet())
		player.getPet()->getController().delete_();
	if (player.getPostman())
		player.getPostman()->getController().delete_();

	taskmanager::tasks::ExpireTimerTask::getInstance().unregisterExpirables(player);
	if (player.getInteractionTask())
		player.getInteractionTask()->abort();

	runtime::Ref<questEngine::model::QuestEnv> questEnv = questEngine::model::QuestEnv::create(nullptr, player, 0);
	questEngine::QuestEngine::getInstance().onLogOut(*questEnv);
	const commons::database::Timestamp lastOnline{std::chrono::milliseconds(commons::utils::currentTimeMillis())};
	player.getController().delete_();
	player.getCommonData()->setOnline(false);
	player.getCommonData()->setLastOnline(lastOnline);
	if (player.isLegionMember()) // must be called after setOnline and setLastOnline
		LegionService::getInstance().onLogout(player);
	player.getCommonData()->setX(player.getX());
	player.getCommonData()->setY(player.getY());
	player.getCommonData()->setZ(player.getZ());
	player.getCommonData()->setHeading(player.getHeading());

	network::chatserver::ChatServer::getInstance().sendPlayerLogout(player);

	PlayerService::storePlayer(player);

	player.getInventory().setOwner(nullptr);
	player.getWarehouse().setOwner(nullptr);
	player.getAccount()->getAccountWarehouse().setOwner(nullptr);

	dao::PlayerDAO::storeOldCharacterLevel(player.getObjectId(), player.getLevel());
	dao::PlayerDAO::storeLastOnlineTime(player.getObjectId(), lastOnline);
	dao::PlayerDAO::onlinePlayer(player, false); // marks that player was fully saved and may enter world again

	if (!con) // Java: NullPointerException
		throw runtime::NullPointerException("Cannot invoke \"AionConnection.setActivePlayer(Player)\" because \"con\" is null");
	con->setActivePlayer(nullptr);
}

} // namespace aion::gameserver::services::player
