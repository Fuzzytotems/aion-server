#include "aion/gameserver/model/items/NpcEquippedGear.h"

#include <utility>
#include <vector>

#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/model/items/ItemSlotInfo.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/sync/Monitor.h"

namespace aion::gameserver::model::items {

namespace {

using GearEntry = runtime::MapEntry<ItemSlot, const templates::item::ItemTemplate*>;

} // namespace

NpcEquippedGear::NpcEquippedGear(std::unique_ptr<dataholders::loadingutils::adapters::NpcEquipmentList> value)
	: items{}, mask{}, v{value.get()}, ownedList(std::move(value)) {
}

NpcEquippedGear::~NpcEquippedGear() = default;

runtime::Ref<NpcEquippedGear> NpcEquippedGear::create(std::unique_ptr<dataholders::loadingutils::adapters::NpcEquipmentList> value) {
	return runtime::makeRef<NpcEquippedGear>(std::move(value));
}

int32_t NpcEquippedGear::getItemsMask() {
	if (!items.get())
		init();
	return mask.get();
}

runtime::JavaIterator<GearEntry> NpcEquippedGear::iterator() {
	if (!items.get())
		init();
	return items.get()->entrySet().iterator();
}

runtime::SnapshotIterator<GearEntry> NpcEquippedGear::begin() {
	if (!items.get())
		init();
	return runtime::SnapshotIterator<GearEntry>(std::make_shared<const std::vector<GearEntry>>(items.get()->snapshot()));
}

void NpcEquippedGear::init() {
	SYNCHRONIZED(*this) {
		if (!items.get()) {
			// Deviation (D6): Java publishes the empty TreeMap first and fills it and the mask afterwards, so getItemsMask()/iterator(), which
			// test `items == null` outside the monitor, can return a partial mask or map (Npc.overrideEquipmentList creates gear at run time, and
			// SM_NPC_INFO initializes it on the sending threads). The map and the mask are built locally; the mask is stored first and the map
			// published last, so a reader that sees the map also sees the final mask (docs/deviations/P4-13.md).
			runtime::Ref<runtime::RcTreeMap<ItemSlot, const templates::item::ItemTemplate*>> gear =
				runtime::RcTreeMap<ItemSlot, const templates::item::ItemTemplate*>::create(AION_LOCK_CLASS(NpcEquippedGear::items));
			int32_t gearMask = mask.get();
			const dataholders::loadingutils::adapters::NpcEquipmentList* list = v.get();
			if (list == nullptr) // Java: v.items after v = null (only reachable if items was reset)
				throw runtime::NullPointerException("NpcEquippedGear.v");
			for (const templates::item::ItemTemplate* item : list->items) {
				if (item == nullptr) // Java: item.getItemSlot() on an unresolved (null) IDREF
					throw runtime::NullPointerException("NpcEquipmentList.items");
				std::vector<ItemSlot> itemSlots = getSlotsFor(item->getItemSlot());
				for (ItemSlot itemSlot : itemSlots) {
					if (gear->get(itemSlot) == nullptr) {
						gear->put(itemSlot, item);
						// Java: mask |= itemSlot.getSlotIdMask() narrows the long to int (all NPC slot masks are below 65536)
						gearMask = static_cast<int32_t>(static_cast<int64_t>(gearMask) | getSlotIdMask(itemSlot));
						break;
					}
				}
			}
			mask.set(gearMask);
			items.set(std::move(gear));
		}
		v.set(nullptr);
	}
}

void NpcEquippedGear::init(xml::LoadContext& ctx) {
	// Deviation (static-data.md §2.4): initialized once after IDREF resolution instead of on the first getItemsMask/iterator call
	ctx.runAfterIdRefResolution([self = runtime::Ref<NpcEquippedGear>(*this)] { self->init(); });
}

const templates::item::ItemTemplate* NpcEquippedGear::getItem(ItemSlot itemSlot) {
	runtime::Ptr<runtime::RcTreeMap<ItemSlot, const templates::item::ItemTemplate*>> gear = items.get();
	return gear ? gear->get(itemSlot) : nullptr;
}

} // namespace aion::gameserver::model::items
