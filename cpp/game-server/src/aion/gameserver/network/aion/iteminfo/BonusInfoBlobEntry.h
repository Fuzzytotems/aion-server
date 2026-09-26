#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * @author Rolandas
 * <p>
 * C++: writeThisBlob reads StatEnum's getItemStoneMask/getSign from a private stand-in table (network/detail/ItemData.h) until the StatEnum
 * companion exists (P5-01)
 */
class BonusInfoBlobEntry : public ItemBlobEntry {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: public constructor */
	BonusInfoBlobEntry();
	~BonusInfoBlobEntry() override;

public:
	/** Java: new BonusInfoBlobEntry() */
	static runtime::Ref<BonusInfoBlobEntry> create();

	void writeThisBlob(commons::utils::ByteBuffer& buf) override;

	int32_t getSize() override;
};

} // namespace aion::gameserver::network::aion::iteminfo
