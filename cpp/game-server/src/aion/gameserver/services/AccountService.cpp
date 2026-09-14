#include "aion/gameserver/services/AccountService.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/items/storage/Storage.h"

namespace aion::gameserver::services {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.services.AccountService");

runtime::Ref<model::account::Account> AccountService::getAccount(int32_t accountId, std::string_view accountName, int64_t creationDate, model::account::AccountTime& accountTime, int8_t accessLevel, int8_t membership, std::string_view allowedHddSerial) {
	AION_UNPORTED();
}

void AccountService::removeDeletedCharacters(model::account::Account& account) {
	AION_UNPORTED();
}

runtime::Ref<model::account::Account> AccountService::loadAccount(int32_t accountId) {
	AION_UNPORTED();
}

std::unique_ptr<model::account::PlayerAccountData> AccountService::loadPlayerAccountData(int32_t playerId) {
	AION_UNPORTED();
}

std::unique_ptr<model::items::storage::Storage> AccountService::loadAccountWarehouse(model::account::Account& account) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::services
