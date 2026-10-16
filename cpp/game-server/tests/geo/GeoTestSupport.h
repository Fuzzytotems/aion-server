#pragma once

#include <bit>
#include <cstdint>
#include <format>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/base/TaskInfo.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/TaskScope.h"
#include "aion/gameserver/geoEngine/math/Matrix3f.h"
#include "aion/gameserver/geoEngine/math/Vector3f.h"
#include "aion/gameserver/geoEngine/scene/Mesh.h"

namespace aion::gameserver::geoEngine::test {

#define GEO_TEST_SCOPE ::aion::gameserver::runtime::TaskScope geoTestScope(AION_TASK_INFO(::aion::gameserver::runtime::TaskKind::TEST))

inline uint32_t bitsOf(float value) {
	return std::bit_cast<uint32_t>(value);
}

/** Bit-exact float comparison; any two NaNs match. */
inline ::testing::AssertionResult sameFloat(float actual, float expected) {
	if ((actual != actual && expected != expected) || bitsOf(actual) == bitsOf(expected))
		return ::testing::AssertionSuccess();
	return ::testing::AssertionFailure() << std::format("actual {} (0x{:08x}) != expected {} (0x{:08x})", actual, bitsOf(actual), expected,
	                                                    bitsOf(expected));
}

#define EXPECT_GEO_FLOAT(actual, expected) EXPECT_TRUE(::aion::gameserver::geoEngine::test::sameFloat((actual), (expected)))

/** Captures the messages of one logger ("level|message" per line) while it exists */
class LogCapture {
public:
	explicit LogCapture(std::string loggerName) : name(std::move(loggerName)) {
		auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(stream);
		sink->set_pattern("%l|%v");
		commons::logging::LoggerFactory::configure(name, {.sinks = {sink}, .additive = false});
	}
	~LogCapture() { commons::logging::LoggerFactory::removeConfig(name); }
	LogCapture(const LogCapture&) = delete;
	LogCapture& operator=(const LogCapture&) = delete;

	bool contains(std::string_view text) const { return stream.str().find(text) != std::string::npos; }
	std::string text() const { return stream.str(); }

private:
	std::string name;
	std::ostringstream stream;
};

/** A mesh from vertex coordinates and triangle indices (short indices, or byte indices if `byteIndices`). */
inline runtime::Ref<scene::Mesh> makeMesh(std::span<const float> vertices, std::span<const int32_t> indices, int8_t intentions, int8_t materialId = 0,
	bool byteIndices = false) {
	runtime::Ref<scene::Mesh> mesh = scene::Mesh::create();
	mesh->setVertices(vertices);
	if (byteIndices) {
		std::vector<int8_t> values;
		for (int32_t index : indices)
			values.push_back(static_cast<int8_t>(index));
		mesh->setIndices(std::span<const int8_t>(values));
	} else {
		std::vector<int16_t> values;
		for (int32_t index : indices)
			values.push_back(static_cast<int16_t>(index));
		mesh->setIndices(std::span<const int16_t>(values));
	}
	mesh->setCollisionIntentions(intentions);
	mesh->setMaterialId(materialId);
	return mesh;
}

inline math::Matrix3f identityRotation() {
	return math::Matrix3f();
}

} // namespace aion::gameserver::geoEngine::test
