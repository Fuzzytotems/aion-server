#include "aion/gameserver/services/AtreianPassportService.h"

#include <algorithm>
#include <chrono>
#include <string>
#include <vector>

#include "aion/gameserver/dao/AccountPassportsDAO.h"
#include "aion/gameserver/dataholders/AtreianPassportData.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemData.h"
#include "aion/gameserver/model/AttendType.h"
#include "aion/gameserver/model/account/Account.h"
#include "aion/gameserver/model/account/Passport.h"
#include "aion/gameserver/model/account/PassportsList.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/event/AtreianPassport.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/network/aion/serverpackets/SM_ATREIAN_PASSPORT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/cron/CronService.h"
#include "aion/gameserver/services/item/ItemPacketService.h"
#include "aion/gameserver/services/item/ItemService.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/utils/audit/AuditLogger.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/time/ServerTime.h"
#include "aion/gameserver/world/World.h"

namespace aion::gameserver::services {

using commons::database::Date;
using commons::database::Timestamp;
using model::AttendType;
using model::account::Account;
using model::account::Passport;
using model::account::PassportsList;
using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;
using utils::time::ServerTime;
using LocalDateTime = std::chrono::local_time<std::chrono::milliseconds>;

namespace {

/** Java LocalDateTime.toLocalDate() */
Date toLocalDate(LocalDateTime dateTime) {
	return Date(std::chrono::floor<std::chrono::days>(dateTime));
}

/** Java: a period date of a passport template (required attributes: a null value throws NullPointerException like Java's compare) */
LocalDateTime periodDate(const std::optional<LocalDateTime>& date, int32_t passportId) {
	if (!date)
		throw runtime::NullPointerException("Atreian passport " + std::to_string(passportId) + " has no period date");
	return *date;
}

/** Java: player.getAccount() (never null for a player in the world) */
Account& accountOf(model::gameobjects::player::Player& player) {
	runtime::Ptr<Account> account = player.getAccount();
	if (!account)
		throw runtime::NullPointerException("Player " + player.getName() + " has no account");
	return *account;
}

/** Java: account.getPassportsList() (loaded with the account) */
PassportsList& passportsOf(Account& account) {
	runtime::Ptr<PassportsList> passports = account.getPassportsList();
	if (!passports)
		throw runtime::NullPointerException("Account " + std::to_string(account.getId()) + " has no passport list");
	return *passports;
}

/** Java: player.getCreationDate() of a stored player (never null there) */
Timestamp creationDateOf(model::gameobjects::player::Player& player) {
	std::optional<Timestamp> creationDate = player.getCreationDate();
	if (!creationDate)
		throw runtime::NullPointerException("Player " + player.getName() + " has no creation date");
	return *creationDate;
}

/** Java: passport.getArriveDate().toInstant() */
Timestamp arriveDateOf(Passport& passport) {
	std::optional<Timestamp> arriveDate = passport.getArriveDate();
	if (!arriveDate)
		throw runtime::NullPointerException("Passport " + std::to_string(passport.getId()) + " has no arrive date");
	return *arriveDate;
}

} // namespace

// callback at AtreianPassportService.java:47 (fieldmap key AtreianPassportService@L47:50): CronService.schedule, pin {this} (Immortal)
AtreianPassportService::AtreianPassportService() : expireDate(calculatePassportExpireDate()) {
	if (!isAtreianPassportDisabled()) {
		cronInfo.set(cron::CronService::getInstance().schedule(cron::CronJob({this}, [this] {
			if (isAtreianPassportDisabled()) {
				cron::CronService::getInstance().cancel(cronInfo.get());
				cronInfo.set(nullptr);
				return;
			}
			const bool isFirstDayOfMonth = static_cast<unsigned>(toLocalDate(ServerTime::now().get_local_time()).day()) == 1;
			dao::AccountPassportsDAO::resetAllLastStamps();
			if (isFirstDayOfMonth) {
				dao::AccountPassportsDAO::resetAllStamps();
			}
			world::World::getInstance().forEachPlayer([this, isFirstDayOfMonth](model::gameobjects::player::Player& player) {
				Account& acc = accountOf(player);
				acc.setLastStamp(std::nullopt);
				if (isFirstDayOfMonth) {
					acc.setPassportStamps(0);
				}
				onLogin(player);
			});
		}), DAILY_CRON_AT_09_00));
	}
}

AtreianPassportService::~AtreianPassportService() = default;

AtreianPassportService& AtreianPassportService::getInstance() {
	static AtreianPassportService instance; // Java SingletonHolder
	return instance;
}

bool AtreianPassportService::isAtreianPassportDisabled() {
	return isAtreianPassportDisabled(ServerTime::now().get_local_time());
}

bool AtreianPassportService::isAtreianPassportDisabled(std::chrono::local_time<std::chrono::milliseconds> checkDateTime) {
	return expireDate && checkDateTime > *expireDate;
}

std::optional<std::chrono::local_time<std::chrono::milliseconds>> AtreianPassportService::findLastRewardTime() {
	// Java: stream().filter(DAILY or CUMULATIVE).max(Comparator.comparing(getPeriodEnd)) - maxBy keeps the first of equal elements
	const model::templates::event::AtreianPassport* lastPossibleReward = nullptr;
	for (const auto& [id, passport] : dataholders::DataManager::ATREIAN_PASSPORT_DATA->getAll()) {
		if (passport->getAttendType() != AttendType::DAILY && passport->getAttendType() != AttendType::CUMULATIVE)
			continue;
		if (lastPossibleReward == nullptr ||
			periodDate(passport->getPeriodEnd(), passport->getId()) > periodDate(lastPossibleReward->getPeriodEnd(), lastPossibleReward->getId()))
			lastPossibleReward = passport;
	}
	if (lastPossibleReward == nullptr)
		return std::nullopt;
	return periodDate(lastPossibleReward->getPeriodEnd(), lastPossibleReward->getId());
}

std::optional<std::chrono::local_time<std::chrono::milliseconds>> AtreianPassportService::calculatePassportExpireDate() {
	std::optional<LocalDateTime> disableDateTime = findLastRewardTime();
	if (!disableDateTime) {
		return std::nullopt;
	}
	// Java: toLocalDate().atTime(LocalTime.MAX).plusDays(14). LocalTime.MAX is 23:59:59.999999999; at millisecond precision 23:59:59.999 is after
	// exactly the same millisecond times
	std::chrono::local_days day = std::chrono::floor<std::chrono::days>(*disableDateTime);
	return LocalDateTime(day + std::chrono::days(1) - std::chrono::milliseconds(1)) + std::chrono::days(14);
}

// Java AtreianPassportService.java:93-145. C++: Java iterates its HashMap/HashSet; the C++ caller hands unordered containers, iterated here in
// ascending order (docs/deviations/P5-09a.md) - the order shows only when the cube fills up in the middle of a claim
void AtreianPassportService::takeReward(model::gameobjects::player::Player& player, const std::unordered_map<int32_t, std::unordered_set<int32_t>>& passports) {
	if (isAtreianPassportDisabled()) {
		return;
	}
	std::vector<runtime::Ref<Passport>> toRemove; // C++: Refs - a removed passport leaves the list before it is stored
	PassportsList& ppl = *player.getAccount()->getPassportsList();
	std::vector<int32_t> passIds;
	for (const auto& entry : passports)
		passIds.push_back(entry.first);
	std::ranges::sort(passIds);
	for (int32_t passId : passIds) {
		std::vector<int32_t> times(passports.at(passId).begin(), passports.at(passId).end());
		std::ranges::sort(times);
		for (int32_t time : times) {
			runtime::Ptr<Passport> passport = ppl.getPassport(passId, time);
			if (passport == nullptr) {
				utils::audit::AuditLogger::log(player,
					"tried to get non-existing passport (ID: " + std::to_string(passId) + ", time: " + std::to_string(time) + ").");
				continue;
			}
			if (passport->isRewarded() || passport->getPersistentState() == model::gameobjects::Persistable::PersistentState::DELETED) {
				utils::audit::AuditLogger::log(player, "tried to get passport which is already rewarded (ID: " + std::to_string(passId) + ").");
				continue;
			}
			if (player.getInventory().isFull()) {
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_WAREHOUSE_FULL_INVENTORY());
				break; // Java: breaks the inner loop only
			}
			const model::templates::event::AtreianPassport* atp = dataholders::DataManager::ATREIAN_PASSPORT_DATA->getAtreianPassportId(passId);
			if (atp == nullptr) // Java: atp.getRewardPermitLevel() on null
				throw runtime::NullPointerException("AtreianPassportData.getAtreianPassportId(" + std::to_string(passId) + ")");
			int32_t minLevel = atp->getRewardPermitLevel();
			if (minLevel > 0 && player.getLevel() < minLevel) {
				std::string itemName;
				const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(atp->getRewardItemId());
				if (itemTemplate != nullptr)
					itemName = itemTemplate->getL10n();
				PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ATTEND_REWARD_INVALID_LEVEL(minLevel, itemName));
				continue;
			}
			int32_t expireMin = atp->getRewardExpireMinutes();
			if (expireMin > 0) {
				std::optional<Timestamp> arriveDate = passport->getArriveDate();
				if (!arriveDate) // Java: passport.getArriveDate().toInstant() on null
					throw runtime::NullPointerException("Passport.getArriveDate()");
				const Timestamp deadline = *arriveDate + std::chrono::seconds(expireMin * 60LL);
				if (std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()) > deadline) {
					passport->setPersistentState(model::gameobjects::Persistable::PersistentState::DELETED);
					ppl.removePassport(*passport);
					toRemove.emplace_back(*passport);
					continue;
				}
			}
			item::ItemService::addItem(player, atp->getRewardItemId(), atp->getRewardItemCount(), true,
				*item::ItemService::ItemUpdatePredicate::create(item::ItemPacketService_ItemAddType::ITEM_COLLECT,
					item::ItemPacketService_ItemUpdateType::INC_PASSPORT_ADD));
			passport->setRewarded(true);
			passport->setPersistentState(model::gameobjects::Persistable::PersistentState::UPDATE_REQUIRED);
			toRemove.emplace_back(*passport);
		}
	}
	if (!toRemove.empty()) {
		dao::AccountPassportsDAO::storePassportList(player.getAccount()->getId(), std::vector<runtime::Ptr<Passport>>(toRemove.begin(), toRemove.end()));
	}
	onLogin(player);
}

void AtreianPassportService::onLogin(model::gameobjects::player::Player& player) {
	const LocalDateTime now = ServerTime::now().get_local_time();
	if (isAtreianPassportDisabled(now)) {
		return;
	}
	purgeExpiredPassports(player);
	Account& pa = accountOf(player);
	const bool doReward = checkOnlineDate(pa, now) && pa.getPassportStamps() < 28;
	for (const auto& [id, atp] : dataholders::DataManager::ATREIAN_PASSPORT_DATA->getAll()) {
		if (atp->isActive() && periodDate(atp->getPeriodStart(), atp->getId()) < now && periodDate(atp->getPeriodEnd(), atp->getId()) > now) {
			switch (atp->getAttendType()) {
				case AttendType::DAILY: {
					if (doReward) {
						const Date attendDay = getAttendDay(now);
						if (!passportsOf(pa).hasPassportForDay(atp->getId(), attendDay)) {
							Timestamp ts = nowTs();
							runtime::Ref<Passport> passport = Passport::create(atp->getId(), false, ts);
							passport->setPersistentState(Passport::PersistentState::NEW);
							passportsOf(pa).addPassport(*passport);
						}
					}
					break;
				}
				case AttendType::CUMULATIVE: {
					if (doReward && atp->getAttendNum() == pa.getPassportStamps() + 1) {
						Timestamp ts = nowTs();
						runtime::Ref<Passport> passport = Passport::create(atp->getId(), false, ts);
						passport->setPersistentState(Passport::PersistentState::NEW);
						passportsOf(pa).addPassport(*passport);
					} else if (!passportsOf(pa).isPassportPresent(atp->getId())) {
						Timestamp ts = nowTs();
						runtime::Ref<Passport> passport = Passport::create(atp->getId(), false, ts);
						passport->setFakeStamp(true);
						if (atp->getAttendNum() <= pa.getPassportStamps()) {
							passport->setRewarded(true);
						}
						passportsOf(pa).addPassport(*passport);
					}
					break;
				}
				case AttendType::ANNIVERSARY: {
					int32_t monthsAlive = getAccountAgeInMonths(player, toLocalDate(now));
					int32_t target = atp->getAttendNum();
					if (passportsOf(pa).isPassportPresent(atp->getId())) {
						break;
					}
					if (monthsAlive == target) {
						Timestamp ts = nowTs();
						runtime::Ref<Passport> passport = Passport::create(atp->getId(), false, ts);
						passport->setPersistentState(Passport::PersistentState::NEW);
						passportsOf(pa).addPassport(*passport);
					} else if (monthsAlive > target) {
						Timestamp ts = nowTs();
						runtime::Ref<Passport> passport = Passport::create(atp->getId(), false, ts);
						passport->setFakeStamp(true);
						passport->setRewarded(true);
						passportsOf(pa).addPassport(*passport);
					}
					break;
				}
				default:
					break;
			}
		}
	}
	if (doReward) {
		pa.increasePassportStamps();
		pa.setLastStamp(nowTs());
		checkPassportLimit(player);
		dao::AccountPassportsDAO::storePassport(pa);
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_ATTEND_MSG_ATTEND_REWARD_GET());
	}
	sendPassport(player);
}

void AtreianPassportService::sendPassport(model::gameobjects::player::Player& player) {
	Account& pa = accountOf(player);
	Date playerCreationDate = toLocalDate(ServerTime::atDate(creationDateOf(player)).get_local_time());
	PacketSendUtility::sendPacket(player, network::aion::serverpackets::SM_ATREIAN_PASSPORT(passportsOf(pa), pa.getPassportStamps(), playerCreationDate));
}

bool AtreianPassportService::checkOnlineDate(model::account::Account& pa, std::chrono::local_time<std::chrono::milliseconds> now) {
	std::optional<Timestamp> last = pa.getLastStamp();
	if (!last) {
		return true;
	}
	Date lastAttendDay = getAttendDay(ServerTime::atDate(*last).get_local_time());
	Date currentAttendDay = getAttendDay(now);
	return currentAttendDay != lastAttendDay;
}

commons::database::Date AtreianPassportService::getAttendDay(std::chrono::local_time<std::chrono::milliseconds> serverTime) {
	return toLocalDate(serverTime - std::chrono::hours(ATTEND_RESET_HOUR));
}

void AtreianPassportService::checkPassportLimit(model::gameobjects::player::Player& player) {
	Account& pa = accountOf(player);
	std::vector<runtime::Ptr<Passport>> pl = passportsOf(pa).getAllPassports().snapshot();
	// More than 50 passports cannot be saved.
	if (pl.size() < 50) {
		return;
	}
	// Java: stream().min(Comparator.comparing(Passport::getArriveDate)) - minBy keeps the first of equal elements
	auto oldestOf = [](const std::vector<runtime::Ptr<Passport>>& passports, bool skipFakeStamps) {
		runtime::Ptr<Passport> oldest;
		for (const runtime::Ptr<Passport>& pp : passports) {
			if (skipFakeStamps && pp->isFakeStamp())
				continue;
			if (!oldest || arriveDateOf(*pp) < arriveDateOf(*oldest))
				oldest = pp;
		}
		return oldest;
	};
	runtime::Ptr<Passport> oldest = oldestOf(pl, true);
	if (!oldest) {
		oldest = oldestOf(pl, false);
	}
	if (oldest) {
		oldest->setPersistentState(Passport::PersistentState::DELETED);
		passportsOf(pa).removePassport(*oldest);
		dao::AccountPassportsDAO::storePassportList(pa.getId(), {oldest});
		const model::templates::event::AtreianPassport* oldestTemplate = oldest->getTemplate();
		if (oldestTemplate == nullptr) // Java: NullPointerException on getRewardItemId()
			throw runtime::NullPointerException("Passport " + std::to_string(oldest->getId()) + " has no template");
		const model::templates::item::ItemTemplate* itemTemplate = dataholders::DataManager::ITEM_DATA->getItemTemplate(oldestTemplate->getRewardItemId());
		if (itemTemplate == nullptr) // Java: NullPointerException on getL10n()
			throw runtime::NullPointerException("No item template " + std::to_string(oldestTemplate->getRewardItemId()));
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_ATTEND_REWARD_REMOVE_EXCESS(itemTemplate->getL10n()));
	}
}

void AtreianPassportService::purgeExpiredPassports(model::gameobjects::player::Player& player) {
	Account& pa = accountOf(player);
	PassportsList& ppl = passportsOf(pa);
	Timestamp now = std::chrono::time_point_cast<std::chrono::milliseconds>(ServerTime::now().get_sys_time());
	std::vector<runtime::Ptr<Passport>> toRemove;
	for (const runtime::Ptr<Passport>& pp : ppl.getAllPassports().snapshot()) {
		if (pp->isRewarded() || pp->isFakeStamp()) {
			continue;
		}
		const model::templates::event::AtreianPassport* atp = dataholders::DataManager::ATREIAN_PASSPORT_DATA->getAtreianPassportId(pp->getId());
		if (atp == nullptr) {
			continue;
		}
		int32_t expireMin = atp->getRewardExpireMinutes();
		if (expireMin <= 0) {
			continue;
		}
		Timestamp deadline = arriveDateOf(*pp) + std::chrono::seconds(expireMin * int64_t{60});
		if (now > deadline) {
			pp->setPersistentState(Passport::PersistentState::DELETED);
			ppl.removePassport(*pp);
			toRemove.push_back(pp);
		}
	}
	if (!toRemove.empty()) {
		dao::AccountPassportsDAO::storePassportList(pa.getId(), toRemove);
	}
}

int32_t AtreianPassportService::getAccountAgeInMonths(model::gameobjects::player::Player& player, commons::database::Date now) {
	Date creationDate = toLocalDate(ServerTime::atDate(creationDateOf(player)).get_local_time());
	int32_t months = (static_cast<int32_t>(now.year()) - static_cast<int32_t>(creationDate.year())) * 12 +
		(static_cast<int32_t>(static_cast<unsigned>(now.month())) - static_cast<int32_t>(static_cast<unsigned>(creationDate.month())));
	if (static_cast<unsigned>(now.day()) < static_cast<unsigned>(creationDate.day())) {
		months--;
	}
	return std::max(0, months);
}

commons::database::Timestamp AtreianPassportService::nowTs() {
	return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::floor<std::chrono::seconds>(ServerTime::now().get_sys_time()));
}

} // namespace aion::gameserver::services
