#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * This blob is sending info about the item that were fused with current item.
 *
 * @author -Nemesiss-, Rolandas
 */
class CompositeItemBlobEntry : public ItemBlobEntry {
	AION_MAKE_REF_FRIEND
private:
	void writeFusionStones(commons::utils::ByteBuffer& buf);

protected:
	/** Java: package-private constructor */
	CompositeItemBlobEntry();
	~CompositeItemBlobEntry() override;

public:
	/** Java: new CompositeItemBlobEntry() */
	static runtime::Ref<CompositeItemBlobEntry> create();

	void writeThisBlob(commons::utils::ByteBuffer& buf) override;

	int32_t getSize() override;
};

} // namespace aion::gameserver::network::aion::iteminfo
