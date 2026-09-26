#pragma once

#include <cstdint>
#include <iterator>
#include <memory>

#include "aion/gameserver/dataholders/loadingutils/XmlBindingFwd.h"
#include "aion/gameserver/dataholders/loadingutils/adapters/NpcEquipmentList.h"
#include "aion/gameserver/model/items/ItemSlot.h"
#include "aion/gameserver/model/templates/item/fwd.h"
#include "aion/gameserver/runtime/collections/Iterators.h"
#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"

namespace aion::gameserver::model::items {

/**
 * Java com.aionemu.gameserver.model.items.NpcEquippedGear: the equipment of an npc template (class-level adapter NpcEquippedGearAdapter) or
 * the dynamically overridden equipment of an Npc (Npc.overriddenEquipment).
 * <p>
 * C++: a K4 RefCounted object (fieldmap.json), static-data.md §2.4. NpcTemplate holds `runtime::Ref<NpcEquippedGear>` (the immortal template
 * retains it for the process lifetime, Java GC semantics); the binder creates it with create() and calls init(LoadContext&), which initializes
 * it eagerly after IDREF resolution (Java: lazily in getItemsMask()/iterator(), synchronized). The bound NpcEquipmentList is owned by
 * `ownedList`; `v` points to it until init() (Java sets it to null).
 *
 * @author Luno
 */
class NpcEquippedGear : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND

private:
	// fieldmap.toml: Java creates a TreeMap (NpcEquippedGear.java:39, iteration in ItemSlot order for SM_NPC_INFO); fieldmap.json guesses HashMap
	runtime::Field<runtime::Ref<runtime::RcTreeMap<ItemSlot, const templates::item::ItemTemplate*>>> items;
	runtime::Field<int32_t> mask;
	runtime::Field<const dataholders::loadingutils::adapters::NpcEquipmentList*> v;
	const std::unique_ptr<const dataholders::loadingutils::adapters::NpcEquipmentList> ownedList;

protected:
	/** Java `public NpcEquippedGear(NpcEquipmentList v)` */
	explicit NpcEquippedGear(std::unique_ptr<dataholders::loadingutils::adapters::NpcEquipmentList> v);
	~NpcEquippedGear() override;

public:
	/** Java `new NpcEquippedGear(v)` (NpcEquippedGearAdapter.unmarshal, Npc.setOverrideEquipment) */
	static runtime::Ref<NpcEquippedGear> create(std::unique_ptr<dataholders::loadingutils::adapters::NpcEquipmentList> v);

	int32_t getItemsMask();

	/** Java: Iterable<Entry<ItemSlot, ItemTemplate>>.iterator() over the TreeMap (a snapshot, hub-headers.md §7.2) */
	runtime::JavaIterator<runtime::MapEntry<ItemSlot, const templates::item::ItemTemplate*>> iterator();

	/** C++ only: range-for over a snapshot (§7.2) */
	runtime::SnapshotIterator<runtime::MapEntry<ItemSlot, const templates::item::ItemTemplate*>> begin();

	std::default_sentinel_t end() const noexcept { return {}; }

	/** Here NPC equipment mask is initialized. All NPC slot masks should be lower than 65536 (Java: synchronized) */
	void init();

	/**
	 * C++ only, the binder contract of NpcEquippedGearAdapter (generated NpcTemplate binder): registers init() to run after IDREF resolution
	 * (LoadContext::runAfterIdRefResolution), since the item IDREFs of the list are resolved only then.
	 */
	void init(xml::LoadContext& ctx);

	/** @return the item of the slot, nullptr (Java null) if there is none or the gear is not initialized */
	const templates::item::ItemTemplate* getItem(ItemSlot itemSlot);
};

} // namespace aion::gameserver::model::items
