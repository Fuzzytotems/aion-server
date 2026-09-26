#pragma once

#include <vector>

#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"
#include "aion/gameserver/services/cron/CronExpression.h"
#include "aion/gameserver/taskmanager/AbstractCronTask.h"
#include "aion/gameserver/taskmanager/tasks/housing/fwd.h"

namespace aion::gameserver::taskmanager::tasks::housing {

/**
 * Handles registering unoccupied houses automatically for auction.
 * <p>
 * C++: RefCounted through AbstractCronTask; getInstance() returns the object of a never-released Ref created on first use (Java's static
 * instance) and runs AbstractCronTask::postConstruct() after construction. Java's Set<House> (identity equality) is a vector of distinct
 * borrowed houses.
 *
 * @author Neon
 */
class AuctionAutoFillTask : public AbstractCronTask {
	AION_MAKE_REF_FRIEND
public:
	static AuctionAutoFillTask& getInstance(); // Java singleton

private:
	AuctionAutoFillTask();

protected:
	~AuctionAutoFillTask() override;

	void executeTask() override;

private:
	void autoFillAuction(model::Race race);

	std::vector<runtime::Ptr<model::house::House>> findAuctionedHouses(model::Race race);

	runtime::Ptr<model::house::House> toHouse(model::house::HouseBids& houseBids);

	std::vector<runtime::Ptr<model::house::House>> findAuctionableHouses(model::Race race,
		const std::vector<runtime::Ptr<model::house::House>>& auctionedHouses);

	runtime::Ptr<model::house::House> findAndRemoveHouse(std::vector<runtime::Ptr<model::house::House>>& houses, model::templates::housing::HouseType houseType);
};

} // namespace aion::gameserver::taskmanager::tasks::housing
