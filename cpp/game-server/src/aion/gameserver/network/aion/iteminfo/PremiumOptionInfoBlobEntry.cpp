#include "aion/gameserver/network/aion/iteminfo/PremiumOptionInfoBlobEntry.h"

#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::network::aion::iteminfo {

PremiumOptionInfoBlobEntry::PremiumOptionInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::PREMIUM_OPTION) {
}

PremiumOptionInfoBlobEntry::~PremiumOptionInfoBlobEntry() = default;

runtime::Ref<PremiumOptionInfoBlobEntry> PremiumOptionInfoBlobEntry::create() {
	return runtime::makeRef<PremiumOptionInfoBlobEntry>();
}

void PremiumOptionInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	runtime::Ptr<model::gameobjects::Item> item = ownerItem.get();
	writeC(buf, !item->isIdentified() ? -1 : item->getBonusStatsId());
	writeC(buf, !item->isIdentified() ? 0 : item->getTuneCount());
	writeC(buf, 0);
}

int32_t PremiumOptionInfoBlobEntry::getSize() {
	return 3;
}

} // namespace aion::gameserver::network::aion::iteminfo
