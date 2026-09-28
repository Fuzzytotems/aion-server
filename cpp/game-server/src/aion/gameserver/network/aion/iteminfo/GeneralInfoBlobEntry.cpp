#include "aion/gameserver/network/aion/iteminfo/GeneralInfoBlobEntry.h"

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/ItemRestrictionCleanupData.h"
#include "aion/gameserver/model/gameobjects/Item.h"

namespace aion::gameserver::network::aion::iteminfo {

GeneralInfoBlobEntry::GeneralInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::GENERAL_INFO) {
}

GeneralInfoBlobEntry::~GeneralInfoBlobEntry() = default;

runtime::Ref<GeneralInfoBlobEntry> GeneralInfoBlobEntry::create() {
	return runtime::makeRef<GeneralInfoBlobEntry>();
}

void GeneralInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	// TODO what with kinah?
	runtime::Ptr<model::gameobjects::Item> item = ownerItem.get();
	writeH(buf, item->getItemMask());
	writeQ(buf, item->getItemCount());
	writeS(buf, item->getItemCreator()); // Creator name
	writeC(buf, 0);
	writeD(buf, item->secondsUntilExpiration()); // Disappear time
	writeD(buf, 0);
	writeD(buf, item->getTemporaryExchangeTimeRemaining());
	bool storabilityDisabled = dataholders::DataManager::ITEM_CLEAN_UP->hasAccountOrLegionWhStorabilityDisabled(item->getItemId());
	writeH(buf, storabilityDisabled ? 3 : 0); // TODO: Item Sealing - 1=sealed, 2=unsealing state, 3=special sealed(gm), 4=special unsealing(gm)
	writeD(buf, 0); // Remaining unsealing time
	writeH(buf, 18); // unk 4.7.5
}

int32_t GeneralInfoBlobEntry::getSize() {
	return 29 + commons::utils::StringUtils::utf16Length(ownerItem->getItemCreator()) * 2 + 4;
}

} // namespace aion::gameserver::network::aion::iteminfo
