#include "aion/gameserver/model/gameobjects/PictureObject.h"

#include "aion/gameserver/model/house/HouseRegistry.h"
#include "aion/gameserver/model/templates/housing/HousingPicture.h"

namespace aion::gameserver::model::gameobjects {

PictureObject::PictureObject(CreateKey key, house::HouseRegistry& registry, int32_t objId, int32_t templateId)
	: HouseObject(key, registry, objId, templateId) {
}

PictureObject::~PictureObject() = default;

const templates::housing::HousingPicture* PictureObject::getObjectTemplate() const {
	return static_cast<const templates::housing::HousingPicture*>(HouseObject::getObjectTemplate());
}

void PictureObject::onUse(player::Player& player) {
}

} // namespace aion::gameserver::model::gameobjects
