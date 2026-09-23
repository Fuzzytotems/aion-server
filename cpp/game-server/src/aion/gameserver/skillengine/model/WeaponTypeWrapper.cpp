#include "aion/gameserver/skillengine/model/WeaponTypeWrapper.h"

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::skillengine::model {

WeaponTypeWrapper::WeaponTypeWrapper(std::optional<gameserver::model::templates::item::enums::ItemGroup> /*mainHand*/,
	std::optional<gameserver::model::templates::item::enums::ItemGroup> /*offHand*/) {
	AION_UNPORTED();
}

WeaponTypeWrapper::~WeaponTypeWrapper() = default;

runtime::Ref<WeaponTypeWrapper> WeaponTypeWrapper::create(std::optional<gameserver::model::templates::item::enums::ItemGroup> mainHandValue,
	std::optional<gameserver::model::templates::item::enums::ItemGroup> offHandValue) {
	return runtime::makeRef<WeaponTypeWrapper>(mainHandValue, offHandValue);
}

bool WeaponTypeWrapper::equals(const WeaponTypeWrapper& /*obj*/) const {
	AION_UNPORTED();
}

std::string WeaponTypeWrapper::toString() {
	AION_UNPORTED();
}

int32_t WeaponTypeWrapper::hashCode() const {
	AION_UNPORTED();
}

int32_t WeaponTypeWrapper::compareTo(const WeaponTypeWrapper& /*o*/) const {
	AION_UNPORTED();
}

} // namespace aion::gameserver::skillengine::model
