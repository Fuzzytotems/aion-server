#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * This blob sends info about conditioning.
 *
 * @author -Nemesiss-, Rolandas
 */
class ConditioningInfoBlobEntry : public ItemBlobEntry {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: package-private constructor */
	ConditioningInfoBlobEntry();
	~ConditioningInfoBlobEntry() override;

public:
	/** Java: new ConditioningInfoBlobEntry() */
	static runtime::Ref<ConditioningInfoBlobEntry> create();

	void writeThisBlob(commons::utils::ByteBuffer& buf) override;

	int32_t getSize() override;
};

} // namespace aion::gameserver::network::aion::iteminfo
