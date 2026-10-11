#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/templates/housing/HouseAddress.h"
#include "aion/gameserver/network/aion/serverpackets/SM_SYSTEM_MESSAGE.h"
#include "aion/gameserver/services/HousingBidService.h"
#include "aion/gameserver/services/mail/MailFormatter.h"
#include "aion/gameserver/services/player/PlayerService.h"
#include "aion/gameserver/taskmanager/tasks/housing/MaintenanceTask.h"

#include <chrono>
#include <optional>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/services/HousingService.h"
#include "aion/gameserver/utils/PacketSendUtility.h"

namespace aion::gameserver::taskmanager::tasks::housing {

using model::house::House;
using runtime::Ptr;

MaintenanceTask::MaintenanceTask()
	: AbstractCronTask(configs::main::HousingConfig::HOUSE_MAINTENANCE_TIME.load(), "com.aionemu.gameserver.taskmanager.tasks.housing.MaintenanceTask") {
}

MaintenanceTask::~MaintenanceTask() = default;

MaintenanceTask& MaintenanceTask::getInstance() {
	// Java: private static final MaintenanceTask instance = new MaintenanceTask() - one Ref that is never released
	static const runtime::Ref<MaintenanceTask>* const instance = [] {
		auto* created = new runtime::Ref<MaintenanceTask>(runtime::makeRef<MaintenanceTask>());
		(*created)->postConstruct();
		return created;
	}();
	return **instance;
}

void MaintenanceTask::executeTask() {
	std::vector<Ptr<House>> housesToMaintain = findHousesToMaintain();
	log.info("Executing house maintenance for " + std::to_string(housesToMaintain.size()) + " houses");

	int64_t now = commons::utils::currentTimeMillis();
	for (const Ptr<House>& house : housesToMaintain) {
		if (!house->getNextPay()) { // the first week is free for newly acquired houses
			house->setNextPay(getNextRun());
			house->save();
			continue;
		} else if (house->getNextPay()->time_since_epoch().count() > now)
			continue;

		Ptr<model::gameobjects::player::PlayerCommonData> pcd = services::player::PlayerService::getOrLoadPlayerCommonData(house->getOwnerId());
		if (!pcd) { // player got deleted
			putHouseToAuction(*house, nullptr);
			continue;
		}

		std::optional<std::string> ownerName = house->getOwnerName();
		int64_t compensationKinah = 0;
		commons::database::Timestamp impoundDate = calculateImpoundDate(*house->getNextPay());
		if (impoundDate.time_since_epoch().count() <= commons::utils::currentTimeMillis()) {
			putHouseToAuction(*house, pcd);
			// return 90% of the house cost (https://aion.fandom.com/wiki/Housing) - Java: (long) (long * 0.9f)
			compensationKinah = static_cast<int64_t>(static_cast<float>(house->getDefaultAuctionPrice()) * 0.9f);
		}

		if (!ownerName) // Java passes the null name on to the mail (a NullPointerException in the recipient lookup)
			throw runtime::NullPointerException(house->toString() + " has no owner name");
		services::mail::MailFormatter::sendHouseMaintenanceMail(*house, *ownerName, impoundDate.time_since_epoch().count(), compensationKinah);
	}
}

commons::database::Timestamp MaintenanceTask::calculateImpoundDate(commons::database::Timestamp housePaidUntil) {
	commons::database::Timestamp paymentDueDate = housePaidUntil + std::chrono::days(14); // player must pay within two weeks
	commons::database::Timestamp impoundDate{std::chrono::milliseconds(commons::utils::currentTimeMillis())};
	while (impoundDate < paymentDueDate) {
		std::optional<commons::database::Timestamp> next = getNextRunAfter(impoundDate);
		if (!next) // Java: NullPointerException on before()
			throw runtime::NullPointerException("Cannot invoke \"java.util.Date.before(java.util.Date)\" because the next run is null");
		impoundDate = *next;
	}
	return impoundDate;
}

std::vector<runtime::Ptr<model::house::House>> MaintenanceTask::findHousesToMaintain() {
	if (!configs::main::HousingConfig::ENABLE_HOUSE_PAY.load())
		return {};
	std::vector<Ptr<House>> houses;
	for (const Ptr<House>& house : services::HousingService::getInstance().getCustomHouses()) {
		if (!house->isInactive() && house->getOwnerId() != 0)
			houses.push_back(house);
	}
	return houses;
}

// Java MaintenanceTask.java:86-92
void MaintenanceTask::putHouseToAuction(model::house::House& house, runtime::Ptr<model::gameobjects::player::PlayerCommonData> owner) {
	services::HousingService::getInstance().changeOwner(house, 0);
	services::HousingBidService::getInstance().auction(house, house.getDefaultAuctionPrice());
	log.info("Auctioned house " + std::to_string(house.getAddress()->getId()) + " because " +
		(!owner ? std::string("owner got deleted.") : std::string("maintenance fee was overdue.")));
	if (owner && owner->isOnline())
		utils::PacketSendUtility::sendPacket(*owner->getPlayer(), network::aion::serverpackets::SM_SYSTEM_MESSAGE::STR_MSG_HOUSING_SEQUESTRATE());
}

} // namespace aion::gameserver::taskmanager::tasks::housing
