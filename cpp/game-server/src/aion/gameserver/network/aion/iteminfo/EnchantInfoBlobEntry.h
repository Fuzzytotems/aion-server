#pragma once

#include <cstdint>
#include <unordered_map>

#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/items/fwd.h"
#include "aion/gameserver/network/aion/iteminfo/ItemBlobEntry.h"
#include "aion/gameserver/network/aion/iteminfo/fwd.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::network::aion::iteminfo {

/**
 * This blob sends info about enchantment, bonus attributes, mana stones, god stone etc.
 *
 * @author -Nemesiss-, Rolandas
 */
class EnchantInfoBlobEntry : public ItemBlobEntry {
	AION_MAKE_REF_FRIEND
public:
	/** 8 + Item.MAX_BASIC_STONES * 4 + 4 + 13 + 5 + 5 + (18 * 4 + 1). Java: a non-final public static int (never written) */
	static constexpr int32_t SIZE = 138; // fieldmap.toml: never written in Java (an effectively constant non-final static)

protected:
	/** Java: package-private constructor */
	EnchantInfoBlobEntry();
	~EnchantInfoBlobEntry() override;

public:
	/** Java: new EnchantInfoBlobEntry() */
	static runtime::Ref<EnchantInfoBlobEntry> create();

	void writeThisBlob(commons::utils::ByteBuffer& buf) override;

	static void writeInfo(commons::utils::ByteBuffer& buf, model::gameobjects::Item& item);

	static void writeInfo(commons::utils::ByteBuffer& buf, model::gameobjects::Item& item, int32_t optionalManastoneSockets, int32_t enchantBonus);

private:
	/** @throws IllegalStateException if two stones share a slot (Java: Collectors.toMap duplicate key) */
	static std::unordered_map<int32_t, runtime::Ptr<model::items::ManaStone>> createManastoneMap(model::gameobjects::Item& item);

public:
	int32_t getSize() override;
};

} // namespace aion::gameserver::network::aion::iteminfo
