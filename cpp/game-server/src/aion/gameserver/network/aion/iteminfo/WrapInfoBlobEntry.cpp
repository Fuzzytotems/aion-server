#include "aion/gameserver/network/aion/iteminfo/WrapInfoBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::network::aion::iteminfo {

WrapInfoBlobEntry::WrapInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::WRAP_INFO) {
}

WrapInfoBlobEntry::~WrapInfoBlobEntry() = default;

runtime::Ref<WrapInfoBlobEntry> WrapInfoBlobEntry::create() {
	return runtime::makeRef<WrapInfoBlobEntry>();
}

void WrapInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	writeC(buf, ownerItem->getPackCount());
}

int32_t WrapInfoBlobEntry::getSize() {
	return 1;
}

} // namespace aion::gameserver::network::aion::iteminfo
