#include "aion/gameserver/model/account/Account.h"

#include <memory>
#include <utility>

#include "aion/gameserver/model/Race.h"
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
	const Race race = accPlData->getPlayerCommonData()->getRace();
	// Java: PlayerAccountData oldData = players.put(...). The replaced part is retired to the Reclaimer, so the borrow stays valid in this task.
	runtime::Ptr<PlayerAccountData> oldData = players.get(playerObjId);
	players.put(playerObjId, std::move(accPlData));
	if (oldData)
		decrementCountOf(oldData->getPlayerCommonData()->getRace());
	switch (race) {
		case Race::ASMODIANS:
			numberOfAsmos++;
			break;
		case Race::ELYOS:
			numberOfElyos++;
			break;
		default:
			break;
	}
}

items::storage::Storage& Account::getAccountWarehouse() const {
	return *accountWarehouse;
}

void Account::setAccountWarehouse(std::unique_ptr<items::storage::Storage> value) {
	accountWarehouse.set(std::move(value));
}

runtime::Ptr<CharacterPasskey> Account::getCharacterPasskey() {
	// java-race: unsynchronized lazy creation, two threads may create (and one may use) different passkeys
	if (!characterPasskey)
		characterPasskey.set(CharacterPasskey::create());
	return characterPasskey.get();
}

int32_t Account::size() {
	return players.size();
}

std::vector<runtime::Ptr<PlayerAccountData>> Account::getPlayerAccDataList() {
	return players.values();
}

runtime::JavaIterator<runtime::Ptr<PlayerAccountData>> Account::iterator() {
	// Java: players.values().iterator(); remove() removes the entry of the last returned value
	std::vector<std::pair<int32_t, runtime::Ptr<PlayerAccountData>>> entries = players.snapshot();
	auto keys = std::make_shared<std::vector<int32_t>>();
	std::vector<runtime::Ptr<PlayerAccountData>> values;
	keys->reserve(entries.size());
	values.reserve(entries.size());
	for (const auto& [key, value] : entries) {
		keys->push_back(key);
		values.push_back(value);
	}
	return runtime::JavaIterator<runtime::Ptr<PlayerAccountData>>(std::move(values),
		runtime::JavaIterator<runtime::Ptr<PlayerAccountData>>::IndexedRemover(
			[this, keys](const runtime::Ptr<PlayerAccountData>& value, size_t index) {
				if (players.get((*keys)[index]) != value)
					return false;
				return players.remove((*keys)[index]);
			}));
}

runtime::SnapshotIterator<runtime::Ptr<PlayerAccountData>> Account::begin() {
	return runtime::SnapshotIterator<runtime::Ptr<PlayerAccountData>>(
		std::make_shared<const std::vector<runtime::Ptr<PlayerAccountData>>>(players.values()));
}

int32_t Account::getNumberOf(Race race) {
	switch (race) {
		case Race::ASMODIANS:
			return numberOfAsmos.get();
		case Race::ELYOS:
			return numberOfElyos.get();
		default:
			break;
	}
	return 0;
}

void Account::decrementCountOf(Race race) {
	switch (race) {
		case Race::ASMODIANS:
			numberOfAsmos--;
			break;
		case Race::ELYOS:
			numberOfElyos--;
			break;
		default:
			break;
	}
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
	return numberOfAsmos.get() == 0 && numberOfElyos.get() == 0;
}

int32_t Account::getMaxPlayerLevel() {
	int32_t maxLevel = 1;
	for (const runtime::Ptr<PlayerAccountData>& pad : players.values()) {
		if (pad->getPlayerCommonData()->getLevel() > maxLevel)
			maxLevel = pad->getPlayerCommonData()->getLevel();
	}
	return maxLevel;
}

std::string Account::toString() {
	return "Account [id=" + std::to_string(id) + ", name=" + name.get() + "]";
}

void Account::increasePassportStamps() {
	stamps++;
}

void Account::setPassportsList(runtime::Ptr<PassportsList> pp) {
	playerPassports.set(pp);
}

} // namespace aion::gameserver::model::account
