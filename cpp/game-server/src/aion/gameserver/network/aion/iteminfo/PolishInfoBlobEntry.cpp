#include "aion/gameserver/network/aion/iteminfo/PolishInfoBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"
#include "aion/gameserver/model/items/IdianStone.h"

namespace aion::gameserver::network::aion::iteminfo {

PolishInfoBlobEntry::PolishInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::POLISH_INFO) {
}

PolishInfoBlobEntry::~PolishInfoBlobEntry() = default;

runtime::Ref<PolishInfoBlobEntry> PolishInfoBlobEntry::create() {
	return runtime::makeRef<PolishInfoBlobEntry>();
}

void PolishInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	// Idian charge value
	runtime::Ptr<model::items::IdianStone> stone = ownerItem->getIdianStone();
	writeD(buf, !stone ? 0 : stone->getPolishCharge());
}

int32_t PolishInfoBlobEntry::getSize() {
	return 4;
}

} // namespace aion::gameserver::network::aion::iteminfo
