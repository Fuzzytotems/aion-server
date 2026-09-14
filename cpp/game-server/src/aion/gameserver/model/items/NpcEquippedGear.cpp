#include "aion/gameserver/model/items/NpcEquippedGear.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::items {

NpcEquippedGear::NpcEquippedGear(std::unique_ptr<::aion::gameserver::dataholders::loadingutils::adapters::NpcEquipmentList> value) : v(std::move(value)) {
}

// lint: L7 unported stub; Java synchronizes it, the port adds SYNCHRONIZED or documents the load-time confinement
void NpcEquippedGear::init(::aion::gameserver::xml::LoadContext& /*ctx*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::items
