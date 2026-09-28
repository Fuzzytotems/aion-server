#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob_ItemBlobTypeInfo.h"

namespace aion::gameserver::network::aion::iteminfo {

ItemBlobEntry::ItemBlobEntry(ItemInfoBlob_ItemBlobType typeValue) : type(typeValue) {
}

ItemBlobEntry::~ItemBlobEntry() = default;

void ItemBlobEntry::setOwner(runtime::Ptr<model::gameobjects::player::Player> ownerValue, model::gameobjects::Item& item,
	runtime::Ptr<model::stats::calc::functions::IStatFunction> modifierValue) {
	this->owner.set(ownerValue);
	this->ownerItem.set(runtime::Ref<model::gameobjects::Item>(item));
	this->modifier.set(modifierValue);
}

void ItemBlobEntry::writeMe(commons::utils::ByteBuffer& buf) {
	writeC(buf, getEntryId(type));
	writeThisBlob(buf);
}

} // namespace aion::gameserver::network::aion::iteminfo
