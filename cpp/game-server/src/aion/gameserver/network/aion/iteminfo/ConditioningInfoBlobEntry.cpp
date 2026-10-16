#include "aion/gameserver/network/aion/iteminfo/ConditioningInfoBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::network::aion::iteminfo {

ConditioningInfoBlobEntry::ConditioningInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::CONDITIONING_INFO) {
}

ConditioningInfoBlobEntry::~ConditioningInfoBlobEntry() = default;

runtime::Ref<ConditioningInfoBlobEntry> ConditioningInfoBlobEntry::create() {
	return runtime::makeRef<ConditioningInfoBlobEntry>();
}

void ConditioningInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	writeD(buf, ownerItem->getChargePoints());
}

int32_t ConditioningInfoBlobEntry::getSize() {
	return 4;
}

} // namespace aion::gameserver::network::aion::iteminfo
