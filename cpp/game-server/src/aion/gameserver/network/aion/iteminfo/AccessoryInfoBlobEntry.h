#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * This blob is sent for accessory items (such as ring, earring, waist). It keeps info about slots that item can be equipped to.
 *
 * @author -Nemesiss-, Rolandas
 */
class AccessoryInfoBlobEntry : public ItemBlobEntry {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: package-private constructor */
	AccessoryInfoBlobEntry();
	~AccessoryInfoBlobEntry() override;

public:
	/** Java: new AccessoryInfoBlobEntry() */
	static runtime::Ref<AccessoryInfoBlobEntry> create();

	void writeThisBlob(commons::utils::ByteBuffer& buf) override;

	int32_t getSize() override;
};

} // namespace aion::gameserver::network::aion::iteminfo
