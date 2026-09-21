#include "aion/gameserver/taskmanager/tasks/housing/MaintenanceTask.h"

#include <chrono>
#include <optional>
#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/configs/main/HousingConfig.h"
#include "aion/gameserver/model/gameobjects/player/PlayerCommonData.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/services/HousingService.h"

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

		// Java: the owner lookup, impoundment (putHouseToAuction) and MailFormatter.sendHouseMaintenanceMail
		AION_PARTIAL("overdue house maintenance (impoundment and maintenance mail) is not ported yet (M5a E2-05)");
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

void MaintenanceTask::putHouseToAuction(model::house::House& house, runtime::Ptr<model::gameobjects::player::PlayerCommonData> owner) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::taskmanager::tasks::housing
