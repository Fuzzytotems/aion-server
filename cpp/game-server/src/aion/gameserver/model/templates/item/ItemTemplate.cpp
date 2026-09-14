#include "aion/gameserver/model/templates/item/ItemTemplate.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::templates::item {

void ItemTemplate::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	AION_UNPORTED();
}

void ItemTemplate::setXmlUid(std::string_view /*uid*/) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::templates::item
