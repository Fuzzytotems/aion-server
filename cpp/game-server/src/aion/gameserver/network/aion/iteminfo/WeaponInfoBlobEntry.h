#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * This blob is sent for weapons. It keeps info about slots that weapon can be equipped to.
 *
 * @author -Nemesiss-, Rolandas
 */
class WeaponInfoBlobEntry : public ItemBlobEntry {
	AION_MAKE_REF_FRIEND
protected:
	/** Java: package-private constructor */
	WeaponInfoBlobEntry();
	~WeaponInfoBlobEntry() override;

public:
	/** Java: new WeaponInfoBlobEntry() */
	static runtime::Ref<WeaponInfoBlobEntry> create();

	void writeThisBlob(commons::utils::ByteBuffer& buf) override;

	int32_t getSize() override;
};

} // namespace aion::gameserver::network::aion::iteminfo
