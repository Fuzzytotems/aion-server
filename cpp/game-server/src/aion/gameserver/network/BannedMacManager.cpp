#include "aion/gameserver/network/BannedMacManager.h"

#include <chrono>
#include <optional>
#include <string>

#include <fmt/format.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/network/BannedMacEntry.h"
#include "aion/gameserver/network/loginserver/LoginServer.h"
#include "aion/gameserver/network/loginserver/serverpackets/SM_MACBAN_CONTROL.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network {

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.network.BannedMacManager");

namespace {

/** Java: Timestamp.toString() - "yyyy-mm-dd hh:mm:ss.f..." in the default time zone, fraction without trailing zeros but at least one digit */
std::string timestampToString(const std::optional<commons::database::Timestamp>& timestamp) {
	if (!timestamp)
		return "null";
	const std::chrono::zoned_time<std::chrono::milliseconds> local(std::chrono::current_zone(), *timestamp);
	const std::chrono::local_time<std::chrono::milliseconds> time = local.get_local_time();
	const auto day = std::chrono::floor<std::chrono::days>(time);
	const std::chrono::year_month_day ymd(day);
	const std::chrono::hh_mm_ss<std::chrono::milliseconds> hms(time - day);
	std::string millis = fmt::format("{:03}", hms.subseconds().count());
	while (millis.size() > 1 && millis.back() == '0')
		millis.pop_back();
	return fmt::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}.{}", static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()),
		static_cast<unsigned>(ymd.day()), hms.hours().count(), hms.minutes().count(), hms.seconds().count(), millis);
}

} // namespace

BannedMacManager::BannedMacManager() = default;

BannedMacManager::~BannedMacManager() = default;

BannedMacManager& BannedMacManager::getInstance() {
	static BannedMacManager manager; // Java: private static BannedMacManager manager = new BannedMacManager()
	return manager;
}

void BannedMacManager::banAddress(std::string_view address, int64_t newTime, std::string_view details) {
	runtime::Ref<BannedMacEntry> entry;
	const std::string key(address);
	// java-race: check-then-act on the map (Java: unsynchronized HashMap); two concurrent bans of one address may both create and put an entry
	if (bannedList.containsKey(key)) {
		if (bannedList.get(key)->isActiveTill(newTime)) {
			return;
		} else {
			entry = bannedList.get(key);
			entry->updateTime(newTime);
		}
	} else
		entry = BannedMacEntry::create(address, newTime);

	entry->setDetails(details);

	bannedList.put(key, entry);

	log.info("banned " + key + " to " + timestampToString(entry->getTime()) + " for " + std::string(details));
	loginserver::LoginServer::getInstance().sendPacket(loginserver::serverpackets::SM_MACBAN_CONTROL(static_cast<int8_t>(1), address, newTime, details));
}

bool BannedMacManager::unbanAddress(std::string_view address, std::string_view details) {
	runtime::Ref<BannedMacEntry> bannedMacEntry = bannedList.remove(std::string(address));
	if (bannedMacEntry) {
		log.info("unbanned " + std::string(address) + " for " + std::string(details));
		loginserver::LoginServer::getInstance().sendPacket(loginserver::serverpackets::SM_MACBAN_CONTROL(static_cast<int8_t>(0), address, 0, details));
		return true;
	} else
		return false;
}

bool BannedMacManager::isBanned(std::string_view address) {
	runtime::Ptr<BannedMacEntry> bannedMacEntry = bannedList.get(std::string(address));
	return bannedMacEntry && bannedMacEntry->isActive();
}

void BannedMacManager::dbLoad(std::string_view address, int64_t time, std::string_view details) {
	bannedList.put(std::string(address),
		BannedMacEntry::create(address, commons::database::Timestamp(std::chrono::milliseconds(time)), details));
}

void BannedMacManager::onEnd() {
	log.info("Loaded " + std::to_string(bannedList.size()) + " banned mac addresses");
}

} // namespace aion::gameserver::network
