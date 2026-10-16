#pragma once

#include <cstdint>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/stats/calc/functions/fwd.h"
#include "aion/gameserver/network/PacketWriteHelper.h"
#include "aion/gameserver/network/aion/iteminfo/ItemInfoBlob_ItemBlobType.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"
#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * Entry item info packet data (contains blob entries with detailed info).
 * <p>
 * C++: RefCounted (fieldmap K4); the player may be null (Java callers pass null to newBlobEntry).
 *
 * @author -Nemesiss-, Rolandas
 */
class ItemInfoBlob : public PacketWriteHelper {
	AION_MAKE_REF_FRIEND
public:
	using ItemBlobType = ItemInfoBlob_ItemBlobType;

private:
	const runtime::Ref<model::gameobjects::player::Player> player;
	const runtime::Ref<model::gameobjects::Item> item;
	runtime::ArrayList<runtime::Ref<ItemBlobEntry>> itemBlobEntries{AION_LOCK_CLASS(ItemInfoBlob::itemBlobEntries)};

protected:
	ItemInfoBlob(runtime::Ptr<model::gameobjects::player::Player> player, model::gameobjects::Item& item);
	~ItemInfoBlob() override;

public:
	/** Java: new ItemInfoBlob(player, item) */
	static runtime::Ref<ItemInfoBlob> create(runtime::Ptr<model::gameobjects::player::Player> player, model::gameobjects::Item& item);

	void writeMe(commons::utils::ByteBuffer& buf) override;

	void addBlobEntry(ItemBlobType type);

	void addBonusBlobEntry(model::stats::calc::functions::IStatFunction& modifier);

	/** @throws UnsupportedOperationException for STAT_BONUSES */
	static runtime::Ref<ItemBlobEntry> newBlobEntry(ItemBlobType type, runtime::Ptr<model::gameobjects::player::Player> player,
		model::gameobjects::Item& item);

	static runtime::Ref<ItemInfoBlob> getFullBlob(runtime::Ptr<model::gameobjects::player::Player> player, model::gameobjects::Item& item);

	runtime::ArrayList<runtime::Ref<ItemBlobEntry>>& getBlobEntries() { return itemBlobEntries; }

	int32_t size();
};

/** Java: the constant-specific ItemBlobType.newBlobEntry() (companion function, ItemInfoBlob_ItemBlobTypeInfo.h has the constructor data) */
runtime::Ref<ItemBlobEntry> newBlobEntry(ItemInfoBlob_ItemBlobType type);

} // namespace aion::gameserver::network::aion::iteminfo
