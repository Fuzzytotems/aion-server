#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include "aion/gameserver/utils/xml/fwd.h"

namespace aion::gameserver::utils::xml {

/**
 * C++: a static-only class (fieldmap K5). Java uses java.util.zip Deflater/Inflater with the zlib wrapper (RFC 1950); the game server does not
 * link zlib (commons' DeflateEncoder has the same constraint), so both directions are ports:
 * <ul>
 * <li>compress is zlib's deflate at Java's Deflater() defaults (level 6, 32K window, memLevel 8): the output bytes equal Java's, so a script
 * the client compressed to at most SM_HOUSE_SCRIPTS.MAX_COMPRESSED_SCRIPT_SIZE keeps the size Java gives it when HouseScriptsDAO
 * re-compresses it on load.</li>
 * <li>decompress is a complete RFC 1950/1951 inflater (stored, fixed and dynamic blocks, header and Adler-32 checks) with the checks and
 * messages of zlib's inflate. Invalid data throws IllegalArgumentException with zlib's message (Java: DataFormatException); a stream that ends
 * early or needs a preset dictionary throws the Java RuntimeException "Bad zip data, size: N" as an IllegalStateException. Bytes after the end
 * of the stream are ignored, as Inflater.finished() ends Java's loop.</li>
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
