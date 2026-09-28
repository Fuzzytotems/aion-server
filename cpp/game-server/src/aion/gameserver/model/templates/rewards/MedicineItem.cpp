#include "aion/gameserver/model/templates/rewards/MedicineItem.h"

#include "aion/commons/utils/Rnd.h"

namespace aion::gameserver::model::templates::rewards {

int64_t MedicineItem::getCount() const {
	return commons::utils::Rnd::get(1, 3);
}

} // namespace aion::gameserver::model::templates::rewards
