#include "aion/gameserver/services/AccountService.h"

#include <string>
#include <vector>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/GSConfig.h"
#include "aion/gameserver/controllers/ControllerStandIns.h"
#include "aion/gameserver/dao/InventoryDAO.h"
#include "aion/gameserver/dao/ItemStoneListDAO.h"
#include "aion/gameserver/dao/PlayerAppearanceDAO.h"
#include "aion/gameserver/dao/PlayerDAO.h"
#include "aion/gameserver/dao/PlayerPunishmentsDAO.h"
#include "aion/gameserver/model/Race.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/AccountTime.h"
#include "aion/gameserver/model/account/CharacterBanInfo.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/PlayerAppearance.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/PlayerStorage.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/items/storage/StorageType.h"
#include "aion/gameserver/services/player/PlayerService.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.AccountService");

using model::account::Account;
using model::account::PlayerAccountData;

runtime::Ref<Account> AccountService::getAccount(int32_t accountId, std::string_view accountName, int64_t creationDate,
	model::account::AccountTime& accountTime, int8_t accessLevel, int8_t membership, std::string_view allowedHddSerial) {
	log.debug("[AS] request for account: " + std::to_string(accountId));

	runtime::Ref<Account> account = loadAccount(accountId);
	account->setName(accountName);
	account->setCreationDate(creationDate);
	account->setAccountTime(accountTime);
	account->setAccessLevel(accessLevel);
	account->setMembership(membership);
	// C++: Account keeps an empty string for Java's null (Account.h); the LS link passes the serial as read (CM_ACCOUNT_AUTH_RESPONSE)
	account->setAllowedHddSerial(allowedHddSerial);
	removeDeletedCharacters(*account);
	return account;
}

void AccountService::removeDeletedCharacters(Account& account) {
	/* Removes chars that should be removed */
	for (auto it = account.iterator(); it.hasNext();) {
		runtime::Ptr<PlayerAccountData> pad = it.next();
		model::Race race = pad->getPlayerCommonData()->getRace();
		int64_t deletionTime = !pad->getDeletionDate() ? 0 : pad->getDeletionDate()->time_since_epoch().count();
		if (deletionTime != 0 && deletionTime <= commons::utils::currentTimeMillis()) {
			it.remove();
			account.decrementCountOf(race);
			player::PlayerService::deletePlayerFromDB(pad->getPlayerCommonData()->getPlayerObjId());
			if (configs::main::GSConfig::ENABLE_RATIO_LIMITATION.load() &&
				pad->getPlayerCommonData()->getLevel() >= configs::main::GSConfig::RATIO_MIN_REQUIRED_LEVEL.load()) {
				if (account.getNumberOf(race) == 0) {
					// Java: GameServer.updateRatio (P5-14; GameServer.h does not exist yet, the P4-11b stand-in is used until stage 2)
					controllers::standins::gameServerUpdateRatio(pad->getPlayerCommonData()->getRace(), -1);
				}
			}
			if (account.isEmpty()) {
				dao::InventoryDAO::deleteAccountWH(account.getId());
				account.setAccountWarehouse(loadAccountWarehouse(account));
				break;
			}
		}
	}
}

runtime::Ref<Account> AccountService::loadAccount(int32_t accountId) {
	runtime::Ref<Account> account = Account::create(accountId);
	std::vector<int32_t> playerIdList = dao::PlayerDAO::getPlayerOidsOnAccount(accountId);
	for (int32_t playerId : playerIdList)
		account->addPlayerAccountData(loadPlayerAccountData(*account, playerId));
	account->setAccountWarehouse(loadAccountWarehouse(*account));
	return account;
}

std::unique_ptr<PlayerAccountData> AccountService::loadPlayerAccountData(Account& account, int32_t playerId) {
	runtime::Ref<model::gameobjects::player::PlayerCommonData> playerCommonData = dao::PlayerDAO::loadPlayerCommonData(playerId);
	runtime::Ref<model::account::CharacterBanInfo> cbi = dao::PlayerPunishmentsDAO::getCharBanInfo(playerId);
	runtime::Ref<model::gameobjects::player::PlayerAppearance> appereance = dao::PlayerAppearanceDAO::load(playerId);
	// Load only equipment and its stones to display on character selection screen
	std::vector<runtime::Ref<PlayerAccountData::VisibleItem>> equipment = dao::InventoryDAO::loadVisibleEquipment(playerId);
	auto playerAccData = std::make_unique<PlayerAccountData>(account, *playerCommonData, *appereance, cbi, std::move(equipment));
	dao::PlayerDAO::setCreationDeletionTime(*playerAccData);
	return playerAccData;
}

std::unique_ptr<model::items::storage::Storage> AccountService::loadAccountWarehouse(Account& account) {
	std::unique_ptr<model::items::storage::Storage> wh =
		std::make_unique<model::items::storage::PlayerStorage>(account, model::items::storage::StorageType::ACCOUNT_WAREHOUSE);
	dao::InventoryDAO::loadStorage(account.getId(), *wh);
	dao::ItemStoneListDAO::load(wh->getItems());
	return wh;
}

} // namespace aion::gameserver::services
