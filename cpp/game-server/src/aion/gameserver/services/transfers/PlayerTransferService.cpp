#include "aion/gameserver/services/transfers/PlayerTransferService.h"

#include <memory>
#include <span>
#include <string>
#include <vector>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/Numbers.h"
#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/configs/main/PlayerTransferConfig.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/LegionMemberDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_PTRANSFER_CONTROL.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/AccountService.h"
#include "aion/gameserver/services/BrokerService.h"
#include "aion/gameserver/services/item/ItemFactory.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/services/transfers/CMT_CHARACTER_INFORMATION.h"
#include "aion/gameserver/services/transfers/PlayerTransfer.h"
#include "aion/gameserver/services/transfers/TransferablePlayer.h"

namespace aion::gameserver::services::transfers {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.transfers.PlayerTransferService");
static const auto textLog = commons::logging::LoggerFactory::getLogger("PLAYERTRANSFER");

PlayerTransferService& PlayerTransferService::getInstance() {
	static PlayerTransferService instance; // Java SingletonHolder
	return instance;
}

PlayerTransferService::PlayerTransferService() {
	std::shared_ptr<const std::string> removeSkillList = configs::main::PlayerTransferConfig::REMOVE_SKILL_LIST.get();
	if (*removeSkillList != "*") {
		for (const std::string& skillId : commons::utils::StringUtils::splitJava(*removeSkillList, ","))
			rsList.add(commons::utils::parseInt(skillId)); // Java: Integer.parseInt (NumberFormatException for a malformed id)
	}
	log.info("PlayerTransferService loaded. With " + std::to_string(rsList.size()) + " restricted skills.");
}

PlayerTransferService::~PlayerTransferService() = default;

// Java PlayerTransferService.java:52-125
void PlayerTransferService::startTransfer(int32_t accountId, int32_t targetAccountId, int32_t playerId, int8_t targetServerId, int32_t taskId) {
	using network::loginserver::LoginServer;
	using network::loginserver::serverpackets::SM_PTRANSFER_CONTROL;
	bool exist = false;
	for (int32_t id : dao::PlayerDAO::getPlayerOidsOnAccount(accountId))
		if (id == playerId) {
			exist = true;
			break;
		}

	if (!exist) {
		log.warn("transfer #" + std::to_string(taskId) + " player " + std::to_string(playerId) + " is not present on account " + std::to_string(accountId) +
				 ".");
		LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::TASK_STOP, taskId,
			"player " + std::to_string(playerId) + " is not present on account " + std::to_string(accountId)));
		return;
	}

	if (dao::LegionMemberDAO::isIdUsed(playerId)) {
		log.warn("cannot transfer #" + std::to_string(taskId) + " player with existing legion " + std::to_string(playerId) + ".");
		LoginServer::getInstance().sendPacket(
			SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::TASK_STOP, taskId, "cannot transfer player with existing legion " + std::to_string(playerId)));
		return;
	}

	runtime::Ref<model::gameobjects::player::PlayerCommonData> common = player::PlayerService::getOrLoadPlayerCommonData(playerId);
	if (common->isOnline()) {
		log.warn("cannot transfer #" + std::to_string(taskId) + " online players " + std::to_string(playerId) + ".");
		LoginServer::getInstance().sendPacket(
			SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::TASK_STOP, taskId, "cannot transfer online players " + std::to_string(playerId)));
		return;
	}

	// Java: REUSE_HOURS * 3600000 is int arithmetic
	if (configs::main::PlayerTransferConfig::REUSE_HOURS.load() > 0 &&
		common->getLastTransferTime() +
				static_cast<int32_t>(static_cast<uint32_t>(configs::main::PlayerTransferConfig::REUSE_HOURS.load()) * 3'600'000u) >
			commons::utils::currentTimeMillis()) {
		log.warn("cannot transfer #" + std::to_string(taskId) + " that player so often " + std::to_string(playerId) + ".");
		LoginServer::getInstance().sendPacket(
			SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::TASK_STOP, taskId, "cannot transfer that player so often " + std::to_string(playerId)));
		return;
	}

	runtime::Ref<model::gameobjects::player::Player> player =
		player::PlayerService::getPlayer(playerId, runtime::Ptr<model::account::Account>(AccountService::loadAccount(accountId)));
	int64_t kinah = player->getInventory().getKinah() + player->getWarehouse().getKinah();
	if (configs::main::PlayerTransferConfig::MAX_KINAH.load() > 0 && kinah >= configs::main::PlayerTransferConfig::MAX_KINAH.load()) {
		log.warn("cannot transfer #" + std::to_string(taskId) + " players with " + std::to_string(kinah) + " kinah in inventory/wh.");
		LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::TASK_STOP, taskId,
			"cannot transfer players with " + std::to_string(kinah) + " kinah in inventory/wh."));
		return;
	}

	if (BrokerService::getInstance().hasRegisteredItems(*player)) {
		log.warn("cannot transfer #" + std::to_string(taskId) + " player while he own some items in broker.");
		LoginServer::getInstance().sendPacket(
			SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::TASK_STOP, taskId, "cannot transfer player while he own some items in broker."));
		return;
	}

	runtime::Ref<TransferablePlayer> tp = TransferablePlayer::create(playerId, accountId, targetAccountId);
	tp->player.set(player);
	tp->targetServerId.set(targetServerId);
	tp->accountId.set(accountId);
	tp->targetAccountId.set(targetAccountId);
	tp->taskId.set(taskId);
	transfers.put(taskId, tp);

	textLog.info("taskId:" + std::to_string(taskId) + "; [StartTransfer]");
	LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::CHARACTER_INFORMATION, *tp));
	LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::ITEMS_INFORMATION, *tp));
	LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::DATA_INFORMATION, *tp));
	LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::SKILL_INFORMATION, *tp));
	LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::RECIPE_INFORMATION, *tp));
	LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::QUEST_INFORMATION, *tp));
}

// Java PlayerTransferService.java:130-170
void PlayerTransferService::cloneCharacter(int32_t taskId, PlayerTransfer& transfer) {
	using network::loginserver::LoginServer;
	using network::loginserver::serverpackets::SM_PTRANSFER_CONTROL;
	playerTransfers.remove(taskId);
	std::string name = transfer.getName();
	std::string account = transfer.getAccount();
	int32_t targetAccountId = transfer.getTargetAccount();
	if (player::PlayerService::isNameUsedOrReserved(std::nullopt, name)) {
		if (configs::main::PlayerTransferConfig::BLOCK_SAMENAME.load()) {
			LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::ERROR_, taskId, "Name is already in use"));
			return;
		}

		log.info("Name is already in use `" + name + "`");
		textLog.info("taskId:" + std::to_string(taskId) + "; [CloneCharacter:!isFreeName]");
		std::string newName = name;

		int32_t i = 0;
		while (player::PlayerService::isNameUsedOrReserved(std::nullopt, newName)) {
			newName = name + "_" + std::to_string(++i);
		}
		name = newName;
	}
	if (AccountService::loadAccount(targetAccountId)->size() >= configs::main::GSConfig::CHARACTER_LIMIT_COUNT.load()) {
		LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::ERROR_, taskId, "No free character slots"));
		return;
	}

	// Java: new CMT_CHARACTER_INFORMATION(transfer.getDB()); the bytes outlive the reading packet, which wraps them little endian
	std::vector<uint8_t> db = transfer.getDB();
	runtime::Ptr<model::gameobjects::player::Player> cha =
		CMT_CHARACTER_INFORMATION(commons::utils::ByteBuffer::wrap(std::span<uint8_t>(db))).readInfo(name, targetAccountId, account, rsList.snapshot(), textLog);

	if (cha == nullptr) { // something went wrong!
		log.error("clone failed #" + std::to_string(taskId) + " `" + name + "`");
		LoginServer::getInstance().sendPacket(
			SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::ERROR_, taskId, "unexpected sql error while creating a clone"));
	} else {
		if (transfer.getName() != cha->getName())
			dao::InventoryDAO::store(*item::ItemFactory::newItem(169670001), *cha); // [Event] Name Change Ticket
		dao::PlayerDAO::setPlayerLastTransferTime(cha->getObjectId(), commons::utils::currentTimeMillis());
		LoginServer::getInstance().sendPacket(SM_PTRANSFER_CONTROL(SM_PTRANSFER_CONTROL::OK, taskId));
		log.info("clone successful #" + std::to_string(taskId) + " `" + name + "`");
		textLog.info("taskId:" + std::to_string(taskId) + "; [CloneCharacter:Done]");
	}
}

// Java PlayerTransferService.java:175-179
void PlayerTransferService::onOk(int32_t taskId) {
	runtime::Ptr<TransferablePlayer> tplayer = this->transfers.remove(taskId);
	textLog.info("taskId:" + std::to_string(taskId) + "; [TransferComplete]");
	if (tplayer == nullptr) // Java: tplayer.playerId of a task the map does not hold
		throw runtime::NullPointerException("TransferablePlayer");
	player::PlayerService::deletePlayerFromDB(tplayer->playerId.get());
}

// Java PlayerTransferService.java:184-187
void PlayerTransferService::onError(int32_t taskId, std::string_view reason) {
	this->transfers.remove(taskId);
	textLog.info("taskId:" + std::to_string(taskId) + "; [Error. Transfer failed] " + std::string(reason));
}

// Java PlayerTransferService.java:189-191
void PlayerTransferService::putTransfer(int32_t taskId, PlayerTransfer& playerTransfer) {
	playerTransfers.put(taskId, runtime::Ref<PlayerTransfer>(playerTransfer));
}

// Java PlayerTransferService.java:193-195
runtime::Ptr<PlayerTransfer> PlayerTransferService::getTransfer(int32_t taskId) {
	return playerTransfers.get(taskId);
}

} // namespace aion::gameserver::services::transfers
