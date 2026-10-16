#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * This block is sent for all items that can be equipped. If item is equipped. This block says to which slot it's equipped. If not, then it says 0.
 *
 * @author -Nemesiss-, Rolandas
 */
class EquippedSlotBlobEntry : public ItemBlobEntry {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: package-private constructor */
	EquippedSlotBlobEntry();
	~EquippedSlotBlobEntry() override;

public:
	/** Java: new EquippedSlotBlobEntry() */
	static runtime::Ref<EquippedSlotBlobEntry> create();

	void writeThisBlob(commons::utils::ByteBuffer& buf) override;

	int32_t getSize() override;
};

} // namespace aion::gameserver::network::aion::iteminfo
