#pragma once

#include <filesystem>
#include <vector>

#include "aion/gameserver/utils/xml/fwd.h"

namespace aion::gameserver::utils::xml {

/**
 * C++: a static-only class (fieldmap K5) with the file listing of Java's XmlUtil. Deviation: the DOM and XML Schema methods (getDocument, getString,
 * getSchema, validate) belong to the JAXB/JAXP stack that the static data runtime replaces (docs/design/static-data.md: pugixml binders with
 * strict binding instead of XSD validation), so they have no C++ counterpart, like JAXBUtil, StringSchemaOutputResolver and
 * XmlValidationHandler (DEVIATIONS). Java's listFiles(String, boolean) and listFiles(File, boolean) are one function taking a path (a string_view
 * overload would make string literal arguments ambiguous).
 *
 * @author ?, Neon
 */
class XmlUtil {
public:
	XmlUtil() = delete;

	/**
	 * Searches for (non-hidden) .xml files and returns them in a list
	 *
	 * @param root
	 *          - Absolute/relative path to the base directory
	 * @param recursive
	 *          - If set to true, include all subdirectories of the root directory
	 * @return List of .xml files inside the root directory (regular files whose path ends with ".xml" in any case, in directory walk order;
	 *         symbolic links are not followed).
	 * @throws commons::utils::Exception
	 *           (Java: RuntimeException) with the IOException as its cause if the root does not exist or cannot be read
	 */
	static std::vector<std::filesystem::path> listFiles(const std::filesystem::path& root, bool recursive);
};

} // namespace aion::gameserver::utils::xml
