#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/commons/utils/ByteBuffer.h"
#include "aion/gameserver/model/gameobjects/UseableHouseObject.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"
#include "aion/gameserver/model/templates/housing/fwd.h"
#include "aion/gameserver/network/PacketWriteHelper.h"

namespace aion::gameserver::model::gameobjects {

/**
 * A house object with a use action and rewards (flower pots, cooking pots, ...) (Java `UseableHouseObject<HousingUseableItem>`, erased:
 * hub-headers.md §8.1). A visible object: `VisibleObject::create<UseableItemObject>(registry, objId, templateId)` (§10.1).
 * <p>
 * The static nested UseDataWriter is RefCounted through PacketWriteHelper and refers back to its object without a reference
 * (fieldmap.toml `UseableItemObject.UseDataWriter.obj`: it is created for the object by the constructor and only used synchronously by
 * writeUsageData). The use task of onUse pins the object and the player (runtime-architecture.md §7.3).
 *
 * @author Rolandas, Neon
 */
class UseableItemObject : public UseableHouseObject {
	AION_MAKE_REF_FRIEND
public:
	class UseDataWriter : public network::PacketWriteHelper {
		AION_MAKE_REF_FRIEND
	public:
		runtime::OwnerRef<UseableItemObject> obj; // fieldmap.toml: non-retaining back reference (see the class comment)

	protected:
		explicit UseDataWriter(UseableItemObject& obj);
		~UseDataWriter() override;

	public:
		/** Java `new UseDataWriter(obj)` */
		static runtime::Ref<UseDataWriter> create(UseableItemObject& obj);

		/** Java protected, called by the enclosing class (nested class members are public, hub-headers.md §9.3) */
		void writeMe(commons::utils::ByteBuffer& buffer) override;
	};

private:
	runtime::Field<bool> mustGiveLastReward{false}; // Java volatile
	const runtime::Ref<UseableItemObject::UseDataWriter> entryWriter;

protected:
	UseableItemObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId);
	~UseableItemObject() override;

public:
	/** Narrows HouseObject::getObjectTemplate (Java type variable T bound to HousingUseableItem, §8.2) */
	const templates::housing::HousingUseableItem* getObjectTemplate() const;

	void onUse(player::Player& player) override;

	void setMustGiveLastReward(bool value) { mustGiveLastReward.set(value); }

	bool canExpireNow() override;

	void writeUsageData(commons::utils::ByteBuffer& buffer);

	bool hasUseCooldown() override;
};

} // namespace aion::gameserver::model::gameobjects
