#pragma once

#include "aion/gameserver/model/templates/globaldrops/GlobalDropItem.xml.h"

namespace aion::gameserver::model::templates::globaldrops {

/**
 * Java com.aionemu.gameserver.model.templates.globaldrops.GlobalDropItem.
 * <p>
 * C++: Java implements Chance. The shell has no Chance base (a base is a header request); `Chance::selectElement` is a template that only needs
 * getChance(), so the non-virtual getter serves it.
 *
 * @author AionCool
 */
class GlobalDropItem : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/globaldrops/GlobalDropItem.xml.inc"
public:
	float getChance() const { return chance; }
};

} // namespace aion::gameserver::model::templates::globaldrops
