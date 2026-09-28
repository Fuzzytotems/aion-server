#include "aion/gameserver/network/aion/iteminfo/StigmaShardInfoBlobEntry.h"

namespace aion::gameserver::network::aion::iteminfo {

StigmaShardInfoBlobEntry::StigmaShardInfoBlobEntry() : ItemBlobEntry(ItemInfoBlob_ItemBlobType::STIGMA_SHARD) {
}

StigmaShardInfoBlobEntry::~StigmaShardInfoBlobEntry() = default;

runtime::Ref<StigmaShardInfoBlobEntry> StigmaShardInfoBlobEntry::create() {
	return runtime::makeRef<StigmaShardInfoBlobEntry>();
}

void StigmaShardInfoBlobEntry::writeThisBlob(commons::utils::ByteBuffer& buf) {
	writeD(buf, 0);
}

int32_t StigmaShardInfoBlobEntry::getSize() {
	return 4;
}

} // namespace aion::gameserver::network::aion::iteminfo
