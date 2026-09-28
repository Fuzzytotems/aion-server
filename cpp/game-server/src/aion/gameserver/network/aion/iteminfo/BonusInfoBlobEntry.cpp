#include "aion/gameserver/network/aion/iteminfo/BonusInfoBlobEntry.h"

#include "aion/gameserver/model/stats/calc/functions/IStatFunction.h"
#include "aion/gameserver/model/stats/calc/functions/StatRateFunction.h"
#include "aion/gameserver/network/detail/ItemData.h"

namespace aion::gameserver::network::aion::iteminfo {

BonusInfoBlobEntry::BonusInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::STAT_BONUSES) {
}

BonusInfoBlobEntry::~BonusInfoBlobEntry() = default;

runtime::Ref<BonusInfoBlobEntry> BonusInfoBlobEntry::create() {
	return runtime::makeRef<BonusInfoBlobEntry>();
}

void BonusInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	runtime::Ptr<model::stats::calc::functions::IStatFunction> function = modifier.get(); // NullPointerException like Java when null
	const model::stats::container::StatEnum name = function->getName();
	writeH(buf, network::detail::itemStoneMaskOf(name)); // TODO(P5-01): StatEnum companion getItemStoneMask
	// Java int multiplication wraps
	writeD(buf, static_cast<int32_t>(static_cast<uint32_t>(function->getValue()) * static_cast<uint32_t>(network::detail::signOf(name))));
	writeC(buf, dynamic_cast<model::stats::calc::functions::StatRateFunction*>(&*function) != nullptr ? 1 : 0);
}

int32_t BonusInfoBlobEntry::getSize() {
	return 7;
}

} // namespace aion::gameserver::network::aion::iteminfo
