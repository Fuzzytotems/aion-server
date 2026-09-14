#pragma once

#include <memory>

#include "aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/NpcEquipmentList.h"

namespace aion::gameserver::model::items {

/** Java com.aionemu.gameserver.model.items.NpcEquippedGear (shell of the class-level adapter dataholders.loadingutils.adapters.NpcEquippedGearAdapter: the contract of the generated binders). @author Luno */
// fieldmap: shell; the other Java members arrive with the port of the class (fieldmap.py --class)
class NpcEquippedGear {
public:
	explicit NpcEquippedGear(std::unique_ptr<::aion::gameserver::dataholders::loadingutils::adapters::NpcEquipmentList> v);
	void init(::aion::gameserver::xml::LoadContext& ctx);

private:
	// fieldmap: owns the bound adapter value (Java: a reference kept by the GC) until the port decides its lifetime
	std::unique_ptr<::aion::gameserver::dataholders::loadingutils::adapters::NpcEquipmentList> v;
};

} // namespace aion::gameserver::model::items
