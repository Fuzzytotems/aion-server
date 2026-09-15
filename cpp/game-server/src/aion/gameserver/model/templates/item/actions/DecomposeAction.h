#pragma once

#include "aion/gameserver/dataholders/fwd.h"
#include "aion/gameserver/model/templates/item/actions/DecomposeAction.xml.h"

namespace aion::gameserver::model::templates::item::actions {

/** Java com.aionemu.gameserver.model.templates.item.actions.DecomposeAction. @author oslo(a00441234) */
class DecomposeAction : public ::aion::gameserver::model::templates::item::actions::AbstractItemAction {
#include "aion/gameserver/model/templates/item/actions/DecomposeAction.xml.inc"
public:
	/**
	 * Checks that every random reward item id of the decompose action exists: the last DataManager post-processing step.
	 * <p>
	 * C++: Java reads DataManager.ITEM_DATA, which is not published yet during the C++ post-processing, so the holder is passed (static-data.md
	 * §3.3, header request holders-2).
	 *
	 * @throws IllegalArgumentException("Decomposable random reward item ID is invalid: <id>") for the first unknown id
	 */
	static void validateRandomItemIds(const dataholders::ItemData& itemData);
};

} // namespace aion::gameserver::model::templates::item::actions
