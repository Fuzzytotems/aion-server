#include "aion/gameserver/services/item/HouseObjectFactory.h"

#include <string>

#include "aion/commons/utils/TimeUtils.h"
#include "aion/gameserver/dataholders/DataManager.h"
#include "aion/gameserver/dataholders/HousingObjectData.h"
#include "aion/gameserver/model/gameobjects/ChairObject.h"
#include "aion/gameserver/model/gameobjects/EmblemObject.h"
#include "aion/gameserver/model/gameobjects/HouseObject.h"
#include "aion/gameserver/model/gameobjects/JukeBoxObject.h"
#include "aion/gameserver/model/gameobjects/MoveableObject.h"
#include "aion/gameserver/model/gameobjects/NpcObject.h"
#include "aion/gameserver/model/gameobjects/PassiveObject.h"
#include "aion/gameserver/model/gameobjects/PictureObject.h"
#include "aion/gameserver/model/gameobjects/PostboxObject.h"
#include "aion/gameserver/model/gameobjects/StorageObject.h"
#include "aion/gameserver/model/gameobjects/UseableItemObject.h"
#include "aion/gameserver/model/house/House.h"
#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousingChair.h"
#include "aion/gameserver/model/templates/housing/HousingEmblem.h"
#include "aion/gameserver/model/templates/housing/HousingJukeBox.h"
#include "aion/gameserver/model/templates/housing/HousingMoveableItem.h"
#include "aion/gameserver/model/templates/housing/HousingNpc.h"
#include "aion/gameserver/model/templates/housing/HousingPicture.h"
#include "aion/gameserver/model/templates/housing/HousingPostbox.h"
#include "aion/gameserver/model/templates/housing/HousingStorage.h"
#include "aion/gameserver/model/templates/housing/HousingUseableItem.h"
#include "aion/gameserver/model/templates/housing/PlaceableHouseObject.h"
#include "aion/gameserver/model/templates/item/ItemTemplate.h"
#include "aion/gameserver/model/templates/item/actions/ItemActions.h"
#include "aion/gameserver/model/templates/item/actions/SummonHouseObjectAction.h"
#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/utils/idfactory/IDFactory.h"

namespace aion::gameserver::services::item {

// Java HouseObjectFactory.java:42-66
runtime::Ref<model::gameobjects::HouseObject> HouseObjectFactory::createNew(model::house::HouseRegistry& registry, int32_t objectId, int32_t objectTemplateId) {
	using model::gameobjects::VisibleObject;
	namespace housing = model::templates::housing;
	namespace go = model::gameobjects;
	const housing::PlaceableHouseObject* template_ = dataholders::DataManager::HOUSING_OBJECT_DATA->getTemplateById(objectTemplateId);
	// Java: `null instanceof X` is false for every X, so a missing template reaches `new PassiveObject(..., template.getTemplateId())`
	if (template_ == nullptr)
		throw runtime::NullPointerException("HousingObjectData.getTemplateById(" + std::to_string(objectTemplateId) + ")");
	const int32_t templateId = template_->getTemplateId();
	if (dynamic_cast<const housing::HousingChair*>(template_))
		return VisibleObject::create<go::ChairObject>(registry, objectId, templateId);
	else if (dynamic_cast<const housing::HousingJukeBox*>(template_))
		return VisibleObject::create<go::JukeBoxObject>(registry, objectId, templateId);
	else if (dynamic_cast<const housing::HousingMoveableItem*>(template_))
		return VisibleObject::create<go::MoveableObject>(registry, objectId, templateId);
	else if (dynamic_cast<const housing::HousingNpc*>(template_))
		return VisibleObject::create<go::NpcObject>(registry, objectId, templateId);
	else if (dynamic_cast<const housing::HousingPicture*>(template_))
		return VisibleObject::create<go::PictureObject>(registry, objectId, templateId);
	else if (dynamic_cast<const housing::HousingPostbox*>(template_))
		return VisibleObject::create<go::PostboxObject>(registry, objectId, templateId);
	else if (dynamic_cast<const housing::HousingStorage*>(template_))
		return VisibleObject::create<go::StorageObject>(registry, objectId, templateId);
	else if (dynamic_cast<const housing::HousingUseableItem*>(template_))
		return VisibleObject::create<go::UseableItemObject>(registry, objectId, templateId);
	else if (dynamic_cast<const housing::HousingEmblem*>(template_))
		return VisibleObject::create<go::EmblemObject>(registry, objectId, templateId);
	return VisibleObject::create<go::PassiveObject>(registry, objectId, templateId);
}

// Java HouseObjectFactory.java:68-86
runtime::Ref<model::gameobjects::HouseObject> HouseObjectFactory::createNew(model::house::House& house, const model::templates::item::ItemTemplate* itemTemplate) {
	if (itemTemplate == nullptr) // Java: itemTemplate.getActions() on null
		throw runtime::NullPointerException("itemTemplate");
	// Java: Objects.requireNonNull(..., message) throws a NullPointerException with the message
	if (itemTemplate->getActions() == nullptr)
		throw runtime::NullPointerException("template actions null");

	const model::templates::item::actions::SummonHouseObjectAction* action = itemTemplate->getActions()->getHouseObjectAction();
	if (action == nullptr)
		throw runtime::NullPointerException("template actions miss SummonHouseObjectAction");

	int32_t objectTemplateId = action->getTemplateId();
	runtime::Ptr<model::house::HouseRegistry> registry = house.getRegistry();
	if (registry == nullptr) // Java: createNew(null registry, ...) reaches the HouseObject constructor with null
		throw runtime::NullPointerException("House.getRegistry()");
	runtime::Ref<model::gameobjects::HouseObject> obj = createNew(*registry, utils::idfactory::IDFactory::getInstance().nextId(), objectTemplateId);
	int32_t useDays = obj->getObjectTemplate()->getUseDays();
	if (useDays > 0) {
		// Java: (int) (System.currentTimeMillis() / 1000 + TimeUnit.DAYS.toSeconds(useDays)) - long arithmetic, narrowed
		int32_t expireEnd = static_cast<int32_t>(static_cast<uint32_t>(commons::utils::currentTimeMillis() / 1000 + static_cast<int64_t>(useDays) * 86400));
		obj->setExpireTime(expireEnd);
	}
	return obj;
}

} // namespace aion::gameserver::services::item
