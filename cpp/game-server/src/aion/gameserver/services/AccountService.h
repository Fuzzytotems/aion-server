#pragma once

#include <cstdint>
#include <memory>
#include <string_view>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/items/storage/fwd.h"
#include "aion/gameserver/services/fwd.h"

namespace aion::gameserver::services {

/**
 * This class is a front-end for daos and it's responsibility is to retrieve the Account objects
 * <p>
 * C++: a static-only class (hub-headers.md §11.1). loadAccount/getAccount return the new Account (Ref); loadPlayerAccountData and
 * loadAccountWarehouse return new parts of the account (Account.players PartMap, Account.accountWarehouse PartSlot; hub-headers.md §5).
 * loadPlayerAccountData takes the owning account (C++ only; header request 5a-pre-1): a PlayerAccountData part is constructed with its owner
 * (PlayerAccountData.h), and Java's callers add the result to that account right away (AccountService.java:79, PlayerEnterWorldService.java:119-122).
 *
 * @author Luno, cura
 */
class AccountService {
public:
	static runtime::Ref<model::account::Account> getAccount(int32_t accountId, std::string_view accountName, int64_t creationDate, model::account::AccountTime& accountTime, int8_t accessLevel, int8_t membership, std::string_view allowedHddSerial);
	/** Removes from db characters that should be deleted (their deletion time has passed). */
	static void removeDeletedCharacters(model::account::Account& account);
	static runtime::Ref<model::account::Account> loadAccount(int32_t accountId);
	static std::unique_ptr<model::account::PlayerAccountData> loadPlayerAccountData(model::account::Account& account, int32_t playerId);
	static std::unique_ptr<model::items::storage::Storage> loadAccountWarehouse(model::account::Account& account);
};

} // namespace aion::gameserver::services
