#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/utils/xml/fwd.h"

namespace aion::gameserver::utils::xml {

/**
 * C++: a static-only class (fieldmap K5). Java uses java.util.zip Deflater/Inflater with the zlib wrapper (RFC 1950); the C++ calls zlib (the
 * vcpkg port, linked by aion_gs_base) the same way:
 * <ul>
 * <li>compress is Deflater() (level 6, windowBits 15, memLevel 8, default strategy) fed the whole input and finished: Java bundles zlib, so the
 * output bytes are Java's.</li>
 * <li>decompress is Inflater() driven by Java's loop (1024-byte buffer, Z_PARTIAL_FLUSH). Invalid data throws IllegalArgumentException with
 * zlib's message (Java: DataFormatException); a stream that ends early or needs a preset dictionary throws the Java RuntimeException "Bad zip
 * data, size: N" as an IllegalStateException. Bytes after the end of the stream are ignored, as Inflater.finished() ends Java's loop.</li>
 * </ul>
 *
 * @author Rolandas
 */
class CompressUtil final {
public:
	CompressUtil() = delete;

	static std::vector<uint8_t> decompress(std::span<const uint8_t> bytes);

	static std::vector<uint8_t> compress(std::span<const uint8_t> bytes);
};

} // namespace aion::gameserver::utils::xml
