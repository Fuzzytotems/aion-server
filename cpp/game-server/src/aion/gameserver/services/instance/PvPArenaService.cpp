#include "aion/gameserver/services/instance/PvPArenaService.h"

#include <chrono>

#include "aion/gameserver/configs/main/AutoGroupConfig.h"
#include "aion/gameserver/model/autogroup/AutoGroup.h"
#include "aion/gameserver/model/autogroup/AutoGroupTypeInfo.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/PacketSendUtility.h"
#include "aion/gameserver/utils/time/ServerTime.h"

namespace aion::gameserver::services::instance {

using network::aion::serverpackets::SM_SYSTEM_MESSAGE;
using utils::PacketSendUtility;

/** Java ServerTime.now(): its hour and its getDayOfWeek().getValue() (ISO: Monday 1 .. Sunday 7) */
struct PvPArenaService_Now {
	int32_t hour;
	int32_t day;

	static PvPArenaService_Now get() {
		const utils::time::ServerTime::ZonedDateTime now = utils::time::ServerTime::now();
		const std::chrono::local_time<std::chrono::milliseconds> local = now.get_local_time();
		const std::chrono::local_days today = std::chrono::floor<std::chrono::days>(local);
		const std::chrono::hh_mm_ss<std::chrono::milliseconds> time(local - today);
		return {static_cast<int32_t>(time.hours().count()), static_cast<int32_t>(std::chrono::weekday(today).iso_encoding())};
	}
};

// Java PvPArenaService.java:19-30
bool PvPArenaService::isPvPArenaAvailable(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt) {
	if (configs::main::AutoGroupConfig::START_TIME_ENABLE.load() && !checkTime(agt)) {
		const model::autogroup::AutoGroup* found = model::autogroup::getTemplate(agt);
		if (found == nullptr) // Java: agt.getTemplate().getInstanceMapId() of a missing template
			throw runtime::NullPointerException("AutoGroupType without auto group template");
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CLOSED_TIME(found->getInstanceMapId()));
		return false;
	}
	if (!checkItem(player, agt)) {
		PacketSendUtility::sendPacket(player, SM_SYSTEM_MESSAGE::STR_MSG_INSTANCE_CANT_ENTER_WITHOUT_ITEM());
		return false;
	}
	// TODO check cool down
	return true;
}

// Java PvPArenaService.java:32-43
bool PvPArenaService::checkItem(model::gameobjects::player::Player& player, model::autogroup::AutoGroupType agt) {
	model::items::storage::Storage& inventory = player.getInventory();
	if (model::autogroup::isPvPFFAArena(agt) || model::autogroup::isPvPSoloArena(agt)) {
		return inventory.getItemCountByItemId(186000135) > 0;
	} else if (model::autogroup::isHarmonyArena(agt)) {
		return inventory.getItemCountByItemId(186000184) > 0;
	} else if (model::autogroup::isGloryArena(agt)) {
		return inventory.getItemCountByItemId(186000185) >= 3;
	}
	return true;
}

// Java PvPArenaService.java:45-54
bool PvPArenaService::checkTime(model::autogroup::AutoGroupType agt) {
	if (model::autogroup::isPvPFFAArena(agt) || model::autogroup::isPvPSoloArena(agt)) {
		return isPvPArenaAvailable();
	} else if (model::autogroup::isHarmonyArena(agt)) {
		return isHarmonyArenaAvailable();
	} else if (model::autogroup::isGloryArena(agt)) {
		return isGloryArenaAvailable();
	}
	return true;
}

// Java PvPArenaService.java:56-64
bool PvPArenaService::isPvPArenaAvailable() {
	const PvPArenaService_Now now = PvPArenaService_Now::get();
	int32_t hour = now.hour;
	int32_t day = now.day;
	if (day == 6 || day == 7) {
		return hour == 0 || hour == 1 || (hour >= 10 && hour <= 23);
	}
	return hour == 0 || hour == 1 || hour == 12 || hour == 13 || (hour >= 18 && hour <= 23);
}

// Java PvPArenaService.java:66-76
bool PvPArenaService::isHarmonyArenaAvailable() {
	const PvPArenaService_Now now = PvPArenaService_Now::get();
	int32_t hour = now.hour;
	int32_t day = now.day;
	if (day == 6)
		return hour >= 10 || hour == 1 || hour == 2;
	else if (day == 7)
		return hour == 0 || hour == 1 || hour >= 10;
	else
		return (hour >= 10 && hour < 14) || (hour >= 18 && hour <= 23);
}

// Java PvPArenaService.java:78-83
bool PvPArenaService::isGloryArenaAvailable() {
	const PvPArenaService_Now now = PvPArenaService_Now::get();
	int32_t hour = now.hour;
	int32_t day = now.day;
	return (day == 6 || day == 7) && hour >= 20 && hour < 22;
}

} // namespace aion::gameserver::services::instance
