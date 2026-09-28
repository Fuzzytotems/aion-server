#pragma once

#include <filesystem>
#include <memory>

#include "aion/commons/configuration/Properties.h"

/**
 * This namespace is designed to simplify routine job with properties.
 * <p>
 * Java: com.aionemu.commons.utils.PropertiesUtils. It lives in the configuration library (and namespace) instead of utils because it depends on
 * Properties, which is part of this library.
 *
 * @author SoulKeeper
 */
namespace aion::commons::configuration::PropertiesUtils {

/**
 * Loads properties from a file (decoded as ISO-8859-1, like Java's Properties.load(InputStream)) with the specified defaults as a backup.
 *
 * @param file File to load properties from
 * @param defaults Default values for the new properties, can be nullptr
 * @return Loaded properties (empty if the file doesn't exist or is not a regular file, e.g. a directory)
 * @throws utils::IOException ("Could not parse &lt;file&gt;", caused by the I/O error) if the file could not be read
 * @throws utils::IllegalArgumentException if the file contains a malformed \\uXXXX escape (not wrapped, like in Java)
 */
Properties load(const std::filesystem::path& file, std::shared_ptr<const Properties> defaults = nullptr);

/**
 * Loads all regular files whose name ends with ".properties" (case-sensitive) from a directory and fills the loaded values into the given
 * Properties object. Values of later files replace values of earlier ones.
 * <p>
 * Java uses Files.find, whose order is the file system's listing order (NTFS: roughly alphabetical, ext4: hash order). Here the files are sorted
 * by path (element-wise, case-sensitive), which is deterministic and matches a depth-first traversal with sorted directory entries. Like Java,
 * symbolic links to files are skipped, directory symlinks below dir are not followed, and if dir itself is a matching file, it is loaded. Unlike
 * Java, dir itself may be a symbolic link to a directory.
 *
 * @param properties Properties to fill
 * @param dir Directory where the .properties files are located
 * @param recursive Whether to parse subdirectories or not
 * @throws utils::IOException if the directory does not exist or a file could not be read
 */
void loadFromDirectory(Properties& properties, const std::filesystem::path& dir, bool recursive);

} // namespace aion::commons::configuration::PropertiesUtils
