#include "aion/gameserver/skillengine/model/WeaponTypeWrapper.h"

#include <cstddef>
#include <string_view>
#include <typeinfo>
#include <utility>

#include "aion/gameserver/dataholders/loadingutils/EnumTraits.h"
#include "aion/gameserver/model/templates/item/enums/ItemGroup.h"

namespace aion::gameserver::skillengine::model {

using gameserver::model::templates::item::enums::ItemGroup;

namespace {

using WeaponPair = std::pair<std::optional<ItemGroup>, std::optional<ItemGroup>>;

/**
 * The body of Java's constructor (WeaponTypeWrapper.java:13-45): both members are final, so C++ computes the pair first and the initializer list
 * stores it (the assignments are Java's, in Java's order of cases).
 */
WeaponPair normalize(std::optional<ItemGroup> mainHand, std::optional<ItemGroup> offHand) {
	if (mainHand.has_value() && offHand.has_value()) {
		switch (*mainHand) {
			case ItemGroup::DAGGER:
				return {ItemGroup::DAGGER, ItemGroup::DAGGER};
			case ItemGroup::SWORD:
				return {ItemGroup::SWORD, ItemGroup::SWORD};
			case ItemGroup::MACE:
				return {ItemGroup::MACE, ItemGroup::MACE};
			case ItemGroup::TOOLHOES:
				return {ItemGroup::TOOLHOES, ItemGroup::TOOLHOES};
			case ItemGroup::GUN:
				return {ItemGroup::GUN, ItemGroup::GUN};
			default:
				return {mainHand, std::nullopt};
		}
	} else {
		return {mainHand, offHand};
	}
}

/** Java String.valueOf(enum) for a nullable enum: name() or "null" */
std::string_view nameOrNull(std::optional<ItemGroup> group) {
	return group.has_value() ? xml::enumName(*group) : std::string_view("null");
}

/** Java String.compareTo over the enum names (ASCII): the first differing char difference, else the length difference */
int32_t compareNames(std::string_view a, std::string_view b) {
	size_t limit = a.size() < b.size() ? a.size() : b.size();
	for (size_t k = 0; k < limit; ++k) {
		if (a[k] != b[k])
			return static_cast<int32_t>(static_cast<unsigned char>(a[k])) - static_cast<int32_t>(static_cast<unsigned char>(b[k]));
	}
	return static_cast<int32_t>(a.size()) - static_cast<int32_t>(b.size());
}

/**
 * Stand-in for Java's `Enum.hashCode()`, which is Object's identity hash and differs from run to run (docs/deviations/P5-02a.md): the ordinal,
 * so equal wrappers still hash equally (the only property Java's contract promises).
 */
int32_t enumHash(ItemGroup group) {
	return static_cast<int32_t>(group);
}

} // namespace

WeaponTypeWrapper::WeaponTypeWrapper(std::optional<ItemGroup> mainHandValue, std::optional<ItemGroup> offHandValue)
	: mainHand(normalize(mainHandValue, offHandValue).first), offHand(normalize(mainHandValue, offHandValue).second) {
}

WeaponTypeWrapper::~WeaponTypeWrapper() = default;

runtime::Ref<WeaponTypeWrapper> WeaponTypeWrapper::create(std::optional<ItemGroup> mainHandValue, std::optional<ItemGroup> offHandValue) {
	return runtime::makeRef<WeaponTypeWrapper>(mainHandValue, offHandValue);
}

bool WeaponTypeWrapper::equals(const WeaponTypeWrapper& obj) const {
	if (this == &obj)
		return true;
	// Java: `obj == null` cannot happen for a reference parameter
	if (typeid(*this) != typeid(obj))
		return false;
	const WeaponTypeWrapper& other = obj;
	if (mainHand != other.mainHand)
		return false;
	if (offHand != other.offHand)
		return false;
	return true;
}

std::string WeaponTypeWrapper::toString() {
	return "mainHand=\"" + std::string(nameOrNull(mainHand)) + "\"" + " offHand=\"" + std::string(nameOrNull(offHand)) + "\"";
}

int32_t WeaponTypeWrapper::hashCode() const {
	const uint32_t prime = 31; // Java int arithmetic wraps: unsigned here, cast back at the end
	uint32_t result = 1;
	result = prime * result + static_cast<uint32_t>(!mainHand.has_value() ? 0 : enumHash(*mainHand));
	result = prime * result + static_cast<uint32_t>(!offHand.has_value() ? 0 : enumHash(*offHand));
	return static_cast<int32_t>(result);
}

int32_t WeaponTypeWrapper::compareTo(const WeaponTypeWrapper& o) const {
	if (!mainHand.has_value() || !o.getMainHand().has_value())
		return 0;
	else if (offHand.has_value() && o.getOffHand().has_value())
		return 0;
	else if (offHand.has_value() && !o.getOffHand().has_value())
		return 1;
	else if (!offHand.has_value() && o.getOffHand().has_value())
		return -1;
	else
		return compareNames(xml::enumName(*mainHand), xml::enumName(*o.getMainHand()));
}

} // namespace aion::gameserver::skillengine::model
