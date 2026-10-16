#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * This blob is sent for armors. It keeps info about slots that armor can be equipped to.
 *
 * @author -Nemesiss-, Rolandas
 */
class ArmorInfoBlobEntry : public ItemBlobEntry {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: package-private constructor */
	ArmorInfoBlobEntry();
	~ArmorInfoBlobEntry() override;

public:
	/** Java: new ArmorInfoBlobEntry() */
	static runtime::Ref<ArmorInfoBlobEntry> create();

	void writeThisBlob(commons::utils::ByteBuffer& buf) override;

	int32_t getSize() override;
};

} // namespace aion::gameserver::network::aion::iteminfo
