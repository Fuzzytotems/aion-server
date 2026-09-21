#pragma once

#include <vector>

#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/taskmanager/AbstractCronTask.h"
#include "aion/gameserver/taskmanager/tasks/housing/fwd.h"

namespace aion::gameserver::taskmanager::tasks::housing {

/**
 * Handles house maintenance as well as impoundment if a player didn't pay for two weeks.
 * <p>
 * C++: RefCounted through AbstractCronTask; getInstance() returns the object of a never-released Ref created on first use (Java's static
 * instance) and runs AbstractCronTask::postConstruct() after construction. Dates are commons::database::Timestamp (hub-headers.md §6).
 *
 * @author Rolandas, Neon
 */
class MaintenanceTask : public AbstractCronTask {
	AION_MAKE_REF_FRIEND
public:
	static MaintenanceTask& getInstance(); // Java singleton

private:
	MaintenanceTask();

protected:
	~MaintenanceTask() override;

	void executeTask() override;

private:
	commons::database::Timestamp calculateImpoundDate(commons::database::Timestamp housePaidUntil);

	std::vector<runtime::Ptr<model::house::House>> findHousesToMaintain();

	/** @param owner null if the owner got deleted */
	void putHouseToAuction(model::house::House& house, runtime::Ptr<model::gameobjects::player::PlayerCommonData> owner);
};

} // namespace aion::gameserver::taskmanager::tasks::housing
