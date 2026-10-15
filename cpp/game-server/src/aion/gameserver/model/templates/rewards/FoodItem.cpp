#include "aion/gameserver/model/templates/rewards/FoodItem.h"

#include "aion/commons/utils/Rnd.h"

namespace aion::gameserver::model::templates::rewards {

int64_t FoodItem::getCount() const {
	return commons::utils::Rnd::nextBoolean() ? 5 : 10;
}

} // namespace aion::gameserver::model::templates::rewards
