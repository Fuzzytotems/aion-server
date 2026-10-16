#include "aion/gameserver/network/aion/clientpackets/CM_HOUSE_PAY_RENT.h"

#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/handlers/HandlerRegistry.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/items/storage/Storage.h"
#include "aion/gameserver/model/templates/housing/HousingLand.h"
#include "aion/gameserver/network/aion/AionConnection.h"
#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_PAY_RENT.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/taskmanager/tasks/housing/MaintenanceTask.h"
#include "aion/gameserver/utils/time/ServerTime.h"

#include "aion/commons/utils/WindowsMacroGuard.h" // after all headers that may include windows.h

namespace aion::gameserver::network::aion::clientpackets {

CM_HOUSE_PAY_RENT::CM_HOUSE_PAY_RENT(int32_t opcode, const StateSet& validStates) : AionClientPacket(opcode, validStates) {
}

// Java CM_HOUSE_PAY_RENT.java:29-31
void CM_HOUSE_PAY_RENT::readImpl() {
	weekCount = readUC();
}

// Java CM_HOUSE_PAY_RENT.java:34-62
void CM_HOUSE_PAY_RENT::runImpl() {
	using taskmanager::tasks::housing::MaintenanceTask;
	const runtime::Ptr<model::gameobjects::player::Player> player = getConnection()->getActivePlayer();
	if (!player) // Java: NullPointerException on player.getActiveHouse()
		throw runtime::NullPointerException("CM_HOUSE_PAY_RENT without an active player");
	const runtime::Ptr<model::house::House> house = player->getActiveHouse();
	if (configs::main::HousingConfig::ENABLE_HOUSE_PAY.load() && !house) // Java: NullPointerException on house.getLand()
		throw runtime::NullPointerException("CM_HOUSE_PAY_RENT without an active house");
	const int64_t cost = configs::main::HousingConfig::ENABLE_HOUSE_PAY.load() ? house->getLand()->getMaintenanceFee() * weekCount : 0;

	if (cost <= 0) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_F2P_CASH_HOUSE_FEE_FREE());
		return;
	}

	if (player->getInventory().getKinah() < cost) {
		sendPacket(serverpackets::SM_SYSTEM_MESSAGE::STR_NOT_ENOUGH_MONEY());
		return;
	}

	std::optional<commons::database::Timestamp> nextPay = house->getNextPay() ? house->getNextPay() : MaintenanceTask::getInstance().getNextRun();
	for (int32_t counter = 0; counter < weekCount; counter++) {
		if (!nextPay) // Java: NullPointerException in getNextRunAfter(null)
			throw runtime::NullPointerException("CM_HOUSE_PAY_RENT: no next maintenance run");
		nextPay = MaintenanceTask::getInstance().getNextRunAfter(*nextPay);
	}
	if (!nextPay) // Java: NullPointerException in ServerTime.atDate(null)
		throw runtime::NullPointerException("CM_HOUSE_PAY_RENT: no next maintenance run");

	// Java: ChronoUnit.WEEKS.between(ServerTime.now().with(LocalTime.MIDNIGHT), ServerTime.atDate(nextPay)) - whole weeks, truncated toward zero
	const std::chrono::local_time<std::chrono::milliseconds> now = utils::time::ServerTime::now().get_local_time();
	const auto midnight = utils::time::ServerTime::of(std::chrono::floor<std::chrono::days>(now)).get_sys_time();
	const auto paidUntil = utils::time::ServerTime::atDate(*nextPay).get_sys_time();
	const int64_t totalWeeksPaid = std::chrono::duration_cast<std::chrono::milliseconds>(paidUntil - midnight).count() / (7LL * 24 * 3600 * 1000);
	if (totalWeeksPaid > 4) // client cap
		return;

	player->getInventory().decreaseKinah(cost);
	house->setNextPay(nextPay);
	house->save();
	sendPacket(serverpackets::SM_HOUSE_PAY_RENT(weekCount));
}

AION_CLIENT_PACKET(CM_HOUSE_PAY_RENT);

} // namespace aion::gameserver::network::aion::clientpackets
