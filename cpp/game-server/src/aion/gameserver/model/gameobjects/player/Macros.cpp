#include "aion/gameserver/model/gameobjects/player/Macros.h"

#include <string>

#include "aion/commons/utils/StringUtils.h"
#include "aion/gameserver/runtime/base/Exceptions.h"

namespace aion::gameserver::model::gameobjects::player {

Macros::Macro::Macro(int32_t id, std::string_view xml) : id_(id), xml_(std::string(xml)) {
}

Macros::Macro::~Macro() = default;

runtime::Ref<Macros::Macro> Macros::Macro::create(int32_t id, std::string_view xml) {
	return runtime::makeRef<Macro>(id, xml);
}

bool Macros::Macro::equals(const Macro& obj) const {
	return this == &obj || (id_ == obj.id_ && xml_ == obj.xml_);
}

int32_t Macros::Macro::hashCode() const {
	// Java record hashCode (java.lang.runtime.ObjectMethods): 31 * h + hash(component); String.hashCode over the UTF-16 code units
	uint32_t stringHash = 0;
	for (char16_t c : commons::utils::StringUtils::toUtf16(xml_))
		stringHash = 31 * stringHash + c;
	uint32_t h = static_cast<uint32_t>(id_);
	h = 31 * h + stringHash;
	return static_cast<int32_t>(h);
}

Macros::Macros() = default;

Macros::~Macros() = default;

runtime::Ref<Macros> Macros::create() {
	return runtime::makeRef<Macros>();
}

std::vector<runtime::Ptr<Macros::Macro>> Macros::getAll() {
	SYNCHRONIZED(*this) {
		return macrosById.values();
	}
}

bool Macros::add(int32_t macroId, std::string_view macroXML) {
	SYNCHRONIZED(*this) {
		if (macroId < 1 || macroId > 12)
			throw runtime::IllegalArgumentException("Invalid macro ID: " + std::to_string(macroId));
		return !macrosById.put(macroId, Macro::create(macroId, macroXML));
	}
}

bool Macros::remove(int32_t macroId) {
	SYNCHRONIZED(*this) {
		return static_cast<bool>(macrosById.remove(macroId));
	}
}

} // namespace aion::gameserver::model::gameobjects::player
