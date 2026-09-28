#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/network/PacketWriteHelper.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob_ItemBlobType.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * ItemInfo blob entry (contains detailed item info). Client does have blob tree as implemented, it contains sequence of blobs. Just blame Nemesiss
 * for deep recursion to get the right size [RR] :P
 * <p>
 * C++: RefCounted (fieldmap K4, ItemInfoBlob.itemBlobEntries). Java package-private members are public; writeMe is public because ItemInfoBlob
 * calls it on its entries (Java: same package).
 *
 * @author -Nemesiss-, Rolandas
 */
class ItemBlobEntry : public PacketWriteHelper {
	AION_MAKE_REF_FRIEND
private:
	const ItemInfoBlob_ItemBlobType type; // fieldmap.toml: the generated nested enum ItemInfoBlob.ItemBlobType, spelled Outer_Inner (CONVENTIONS)

public:
	/** null for entries created by ItemInfoBlob.newBlobEntry(type, null, item) */
	runtime::Field<runtime::Ref<model::gameobjects::player::Player>> owner{};
	runtime::Field<runtime::Ref<model::gameobjects::Item>> ownerItem{};
	/** null except for STAT_BONUSES entries */
	runtime::Field<runtime::Ref<model::stats::calc::functions::IStatFunction>> modifier{};

protected:
	explicit ItemBlobEntry(ItemInfoBlob_ItemBlobType type);
	~ItemBlobEntry() override;

public:
	/** Java package-private */
	void setOwner(runtime::Ptr<model::gameobjects::player::Player> owner, model::gameobjects::Item& item,
		runtime::Ptr<model::stats::calc::functions::IStatFunction> modifier);

	void writeMe(commons::utils::ByteBuffer& buf) override;

	virtual void writeThisBlob(commons::utils::ByteBuffer& buf) = 0;

	virtual int32_t getSize() = 0;
};

} // namespace aion::gameserver::network::aion::iteminfo
