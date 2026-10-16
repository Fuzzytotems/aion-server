#include "aion/gameserver/network/aion/serverpackets/SM_HOUSE_SCRIPTS.h"

#include <vector>

#include "aion/gameserver/model/house/PlayerScript.h"
#include "aion/gameserver/network/aion/ServerPacketsOpcodes.gen.h"
#include "aion/gameserver/runtime/fields/Array.h"

namespace aion::gameserver::network::aion::serverpackets {

namespace {

/** Java: SCRIPT_PADDING.length (the constant is private to the class) */
constexpr size_t SM_HOUSE_SCRIPTS_PADDING_LENGTH = 8;

/** Java: byte[] of a PlayerScript's compressed bytes (the model holds a runtime::Array<int8_t>, NullPointerException for null) */
std::vector<uint8_t> toBytes(runtime::Ptr<runtime::Array<int8_t>> bytes) {
	std::vector<uint8_t> result;
	result.reserve(static_cast<size_t>(bytes->length()));
	for (int32_t i = 0; i < bytes->length(); i++)
		result.push_back(static_cast<uint8_t>(bytes->get(i)));
	return result;
}

} // namespace

namespace {

/** Java: the lambda of DYNAMIC_BODY_PART_SIZE_CALCULATOR (SM_HOUSE_SCRIPTS.java:19, key SM_HOUSE_SCRIPTS@L19:90) */
struct DynamicBodyPartSizeCalculator : runtime::TaskStruct {
	int32_t operator()(model::house::PlayerScript& script) const {
		return script.hasData() ? 11 + script.compressedBytes()->length() + static_cast<int32_t>(SM_HOUSE_SCRIPTS_PADDING_LENGTH) : 3;
	}
};

} // namespace

const runtime::PinnedCallback<int32_t(model::house::PlayerScript&)> SM_HOUSE_SCRIPTS::DYNAMIC_BODY_PART_SIZE_CALCULATOR{
	DynamicBodyPartSizeCalculator{}};

SM_HOUSE_SCRIPTS::SM_HOUSE_SCRIPTS(int32_t houseAddressValue, runtime::Ptr<model::house::PlayerScript> script)
	: AionServerPacket(opcodeOf<SM_HOUSE_SCRIPTS>), houseAddress(houseAddressValue) {
	// Java: script == null ? Collections.emptyList() : Collections.singletonList(script)
	if (script)
		scripts.emplace_back(script);
}

SM_HOUSE_SCRIPTS::SM_HOUSE_SCRIPTS(int32_t houseAddressValue, const std::vector<runtime::Ptr<model::house::PlayerScript>>& scriptsValue)
	: AionServerPacket(opcodeOf<SM_HOUSE_SCRIPTS>), houseAddress(houseAddressValue), scripts(scriptsValue.begin(), scriptsValue.end()) {
}

SM_HOUSE_SCRIPTS::~SM_HOUSE_SCRIPTS() = default;

void SM_HOUSE_SCRIPTS::writeImpl(AionConnection* con) {
	writeD(houseAddress);
	writeH(static_cast<int32_t>(scripts.size()));
	for (const runtime::Ref<model::house::PlayerScript>& script : scripts) {
		writeC(script->id());
		if (script->hasData()) {
			std::vector<uint8_t> scriptContent = toBytes(script->compressedBytes());
			const int32_t contentLength = static_cast<int32_t>(scriptContent.size());
			// total following byte size for this script
			writeH(8 + contentLength + static_cast<int32_t>(SCRIPT_PADDING.size()));
			writeD(contentLength + static_cast<int32_t>(SCRIPT_PADDING.size()));
			writeD(script->uncompressedSize());
			writeB(scriptContent);
			writeB(std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(SCRIPT_PADDING.data()), SCRIPT_PADDING.size()));
		} else {
			writeH(0); // removes script from the in-game list
		}
	}
}

} // namespace aion::gameserver::network::aion::serverpackets
