#pragma once

#include "aion/gameserver/model/templates/item/actions/ItemActions.xml.h"

#include "aion/gameserver/model/templates/item/actions/fwd.h"

namespace aion::gameserver::model::templates::item::actions {

/**
 * Java com.aionemu.gameserver.model.templates.item.actions.ItemActions.
 * <p>
 * C++ notes: the typed lookups return the first action of that type, nullptr for Java null (header request items-3 added getCraftLearnAction and
 * getPolishAction; the other typed lookups come with P5-07).
 *
 * @author ATracer
 */
class ItemActions : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/actions/ItemActions.xml.inc"
public:
	/** @return the first craft learn action, nullptr (Java null) if there is none */
	const CraftLearnAction* getCraftLearnAction() const;

	/** @return the first polish action, nullptr (Java null) if there is none */
	const PolishAction* getPolishAction() const;
};

} // namespace aion::gameserver::model::templates::item::actions
