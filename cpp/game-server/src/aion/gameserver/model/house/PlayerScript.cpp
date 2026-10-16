#include "aion/gameserver/model/house/PlayerScript.h"

#include <cstddef>
#include <functional>

#include "aion/gameserver/runtime/base/Unported.h"

namespace aion::gameserver::model::house {

// Java static initializer: new PlayerScript(0, CompressUtil.compress(script), script.length) - null until CompressUtil is ported (header)
const runtime::Ref<PlayerScript> PlayerScript::LUA_SANDBOX_FIX{};

PlayerScript::PlayerScript(int32_t id, runtime::Ptr<runtime::Array<int8_t>> compressedBytes, int32_t uncompressedSize)
	: id_(id), compressedBytes_(compressedBytes), uncompressedSize_(uncompressedSize) {
}

PlayerScript::~PlayerScript() = default;

runtime::Ref<PlayerScript> PlayerScript::create(int32_t id, runtime::Ptr<runtime::Array<int8_t>> compressedBytes, int32_t uncompressedSize) {
	return runtime::makeRef<PlayerScript>(id, compressedBytes, uncompressedSize);
}

bool PlayerScript::hasData() {
	return compressedBytes_ && compressedBytes_->length() > 0;
}

bool PlayerScript::equals(const PlayerScript& obj) const {
	return this == &obj || (id_ == obj.id_ && compressedBytes_.get() == obj.compressedBytes_.get() && uncompressedSize_ == obj.uncompressedSize_);
}

int32_t PlayerScript::hashCode() const {
	// Java record hashCode: 31 * h + hash(component); arrays hash by identity (System.identityHashCode), here the address
	uint32_t h = static_cast<uint32_t>(id_);
	h = 31 * h + static_cast<uint32_t>(std::hash<const void*>{}(compressedBytes_.get()));
	h = 31 * h + static_cast<uint32_t>(uncompressedSize_);
	return static_cast<int32_t>(h);
}

} // namespace aion::gameserver::model::house
