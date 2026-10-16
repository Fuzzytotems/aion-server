#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "aion/commons/configuration/transformers/PropertyTransformer.h"

namespace aion::commons::configuration::transformers {

namespace FileTransformer {

/** Creates a path from a UTF-8 string (std::filesystem::path(std::string) would use the ANSI code page on Windows). */
std::filesystem::path toPath(std::string_view utf8);

} // namespace FileTransformer

/**
 * Transforms string to a file path. It's not checked if the file exists. An empty value yields an empty path (Java: new File("")).
 * <p>
 * Deviation: Java's File normalizes the path (on Windows '/' becomes '\\', duplicate and trailing separators are removed), which shows when the
 * path is printed. The path is kept as written here; std::filesystem accepts both separators.
 * <p>
 * Java: com.aionemu.commons.configuration.transformers.FileTransformer
 *
 * @author SoulKeeper
 */
template <>
struct PropertyTransformer<std::filesystem::path> {
	static std::string typeName() { return "File"; }

	static std::filesystem::path parseObject(std::string_view value) { return FileTransformer::toPath(value); }
};

} // namespace aion::commons::configuration::transformers
