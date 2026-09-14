#include "aion/gameserver/model/account/Account.h"

#include <utility>

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/account/AccountTime.h"
#include "aion/gameserver/model/account/CharacterPasskey.h"
#include "aion/gameserver/model/account/PassportsList.h"
#include "aion/gameserver/model/account/PlayerAccountData.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/items/storage/Storage.h"

namespace aion::gameserver::model::account {

Account::Account(int32_t value) : id(value) {
}

Account::~Account() = default;

runtime::Ref<Account> Account::create(int32_t value) {
	return runtime::makeRef<Account>(value);
}

void Account::setAccountTime(runtime::Ptr<AccountTime> value) {
	accountTime.set(value);
}

bool Account::equals(const Account& o) const {
	return this == &o || id == o.id;
}

runtime::Ptr<PlayerAccountData> Account::getPlayerAccountData(int32_t chaOid) {
	return players.get(chaOid);
}

void Account::addPlayerAccountData(std::unique_ptr<PlayerAccountData> accPlData) {
	const int32_t playerObjId = accPlData->getPlayerCommonData()->getPlayerObjId();
	players.put(playerObjId, std::move(accPlData));
}

items::storage::Storage& Account::getAccountWarehouse() const {
	return *accountWarehouse;
}

void Account::setAccountWarehouse(std::unique_ptr<items::storage::Storage> value) {
	accountWarehouse.set(std::move(value));
}

runtime::Ptr<CharacterPasskey> Account::getCharacterPasskey() {
	AION_UNPORTED();
}

int32_t Account::size() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<PlayerAccountData>> Account::getPlayerAccDataList() {
	AION_UNPORTED();
}

runtime::JavaIterator<runtime::Ptr<PlayerAccountData>> Account::iterator() {
	AION_UNPORTED();
}

runtime::SnapshotIterator<runtime::Ptr<PlayerAccountData>> Account::begin() {
	AION_UNPORTED();
}

int32_t Account::getNumberOf(Race race) {
	AION_UNPORTED();
}

void Account::decrementCountOf(Race race) {
	AION_UNPORTED();
}

std::optional<std::string> Account::getAllowedHddSerial() const {
	std::string serial = allowedHddSerial.get();
	if (serial.empty())
		return std::nullopt;
	return serial;
}

void Account::setAllowedHddSerial(std::optional<std::string_view> value) {
	allowedHddSerial.set(value ? std::string(*value) : std::string());
}

bool Account::isEmpty() {
	AION_UNPORTED();
}

int32_t Account::getMaxPlayerLevel() {
	AION_UNPORTED();
}

std::string Account::toString() {
	AION_UNPORTED();
}

void Account::increasePassportStamps() {
	AION_UNPORTED();
}

void Account::setPassportsList(runtime::Ptr<PassportsList> pp) {
	playerPassports.set(pp);
}

} // namespace aion::gameserver::model::account
