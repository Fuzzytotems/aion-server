#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * This blob contains stigma info.
 *
 * @author -Nemesiss-, Rolandas, Neon
 */
class StigmaInfoBlobEntry : public ItemBlobEntry {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: package-private constructor */
	StigmaInfoBlobEntry();
	~StigmaInfoBlobEntry() override;

public:
	/** Java: new StigmaInfoBlobEntry() */
	static runtime::Ref<StigmaInfoBlobEntry> create();

	void writeThisBlob(commons::utils::ByteBuffer& buf) override;

	int32_t getSize() override;
};

} // namespace aion::gameserver::network::aion::iteminfo
