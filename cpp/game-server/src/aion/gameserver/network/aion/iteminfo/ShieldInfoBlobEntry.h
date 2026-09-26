#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * This blob is sent for shields. It keeps info about slots that shield can be equipped to.
 *
 * @author -Nemesiss-, Rolandas
 */
class ShieldInfoBlobEntry : public ItemBlobEntry {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: package-private constructor */
	ShieldInfoBlobEntry();
	~ShieldInfoBlobEntry() override;

public:
	/** Java: new ShieldInfoBlobEntry() */
	static runtime::Ref<ShieldInfoBlobEntry> create();

	void writeThisBlob(commons::utils::ByteBuffer& buf) override;

	int32_t getSize() override;
};

} // namespace aion::gameserver::network::aion::iteminfo
