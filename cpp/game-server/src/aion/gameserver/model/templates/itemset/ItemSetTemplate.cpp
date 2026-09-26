#include "aion/gameserver/model/templates/itemset/ItemSetTemplate.h"

namespace aion::gameserver::model::templates::itemset {

void ItemSetTemplate::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	if (fullbonus != nullptr) {
		// Set number of items to apply the full bonus
		fullbonus->setNumberOfItems(static_cast<int32_t>(itempart.size()));
	}
}

} // namespace aion::gameserver::model::templates::itemset
