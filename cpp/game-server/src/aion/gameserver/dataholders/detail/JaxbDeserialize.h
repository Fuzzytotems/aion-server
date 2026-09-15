#pragma once

#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "aion/commons/utils/Exception.h"
#include "aion/gameserver/dataholders/loadingutils/LoadContext.h"
#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"

namespace aion::gameserver::dataholders::detail {

/**
 * C++ only: Java `JAXBUtil.deserialize(new File(path), T.class)` for the JAXB roots outside static data (configs/ingameshop, configs/schedule):
 * binds the root element of the file strictly with hooks (the XmlValidationHandler of JAXBUtil turns binding errors into exceptions; like the
 * static data loader, C++ also rejects unknown attributes). Include it only in the translation unit that also includes T's .bind.h.
 *
 * @param javaClassName the Java class name of T for the message
 * @throws commons::utils::Exception("Failed to unmarshal class <javaClassName> from <file>") with the binding error as cause
 */
template <class T>
std::unique_ptr<T> deserializeFile(const std::filesystem::path& file, std::string_view javaClassName) {
	try {
		xml::LoadContext context;
		return xml::bindFile<T>(context, file);
	} catch (...) {
		throw commons::utils::Exception("Failed to unmarshal class " + std::string(javaClassName) + " from " + file.generic_string(),
		                                std::current_exception());
	}
}

} // namespace aion::gameserver::dataholders::detail
