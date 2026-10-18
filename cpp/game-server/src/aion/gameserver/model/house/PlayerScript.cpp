#include "aion/gameserver/model/house/PlayerScript.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string_view>
#include <vector>

#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/utils/xml/CompressUtil.h"

namespace aion::gameserver::model::house {

namespace {

/**
 * The text block of PlayerScript.java:15-34, with Java's incidental indentation stripped. The closing delimiter's two tabs are the
 * minimum, so every line loses two tabs. The line breaks are LF, and the block ends with one.
 */
constexpr std::string_view LUA_SANDBOX_FIX_SCRIPT =
	"<?xml version=\"1.0\" encoding=\"UTF-16\" ?>\n"
	"<lboxes>\n"
	"\t<lbox>\n"
	"\t\t<id>101</id>\n"
	"\t\t<name><![CDATA[Lua Fix]]></name>\n"
	"\t\t<desc><![CDATA[Secures the Lua environment against malicious actors]]></desc>\n"
	"\t\t<script><![CDATA[\n"
	"_G.debug = nil\n"
	"_G.dofile = nil\n"
	"_G.io = nil\n"
	"_G.load = nil\n"
	"_G.loadfile = nil\n"
	"_G.loadstring = nil\n"
	"_G.package = nil\n"
	"_G.require = nil\n"
	"]]></script>\n"
	"\t\t<icon>1</icon>\n"
	"\t</lbox>\n"
	"</lboxes>\n";

/** Java: String.getBytes(StandardCharsets.UTF_16LE) of an ASCII text: each char as its low byte, then 0 */
std::vector<uint8_t> utf16le(std::string_view ascii) {
	std::vector<uint8_t> bytes;
	bytes.reserve(ascii.size() * 2);
	for (char c : ascii) {
		bytes.push_back(static_cast<uint8_t>(c));
		bytes.push_back(0);
	}
	return bytes;
}

} // namespace

// Java PlayerScript.java:14-37 (the static initializer): new PlayerScript(0, CompressUtil.compress(script), script.length)
const runtime::Ref<PlayerScript>& PlayerScript::LUA_SANDBOX_FIX = *new runtime::Ref<PlayerScript>([] {
	runtime::TaskScope scope(AION_TASK_INFO(runtime::TaskKind::STARTUP)); // a static initializer (hub-headers.md §2); it pins nothing
	const std::vector<uint8_t> script = utf16le(LUA_SANDBOX_FIX_SCRIPT);
	const std::vector<uint8_t> compressed = utils::xml::CompressUtil::compress(script);
	runtime::Ref<runtime::Array<int8_t>> compressedBytes = runtime::Array<int8_t>::make(static_cast<int32_t>(compressed.size()));
	for (size_t i = 0; i < compressed.size(); ++i)
		(*compressedBytes)[static_cast<int32_t>(i)].set(static_cast<int8_t>(compressed[i]));
	return runtime::makeRef<PlayerScript>(0, compressedBytes, static_cast<int32_t>(script.size()));
}());

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
