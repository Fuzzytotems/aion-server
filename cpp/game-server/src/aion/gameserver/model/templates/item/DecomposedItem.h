#pragma once

#include "aion/gameserver/model/templates/item/DecomposedItem.xml.h"

namespace aion::gameserver::model::templates::item {

/**
 * One reward alternative of a {@link DecomposableSet}, weighted against its siblings.
 * <p>
 * Java com.aionemu.gameserver.model.templates.item.DecomposedItem (upstream d254ea172).
 */
class DecomposedItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/item/DecomposedItem.xml.inc"
public:
};

} // namespace aion::gameserver::model::templates::item
