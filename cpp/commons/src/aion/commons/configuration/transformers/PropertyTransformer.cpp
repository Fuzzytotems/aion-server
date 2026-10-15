#include "aion/commons/configuration/transformers/PropertyTransformer.h"

namespace aion::commons::configuration::transformers::detail {

std::string cppTypeName(const std::type_info& type) {
	std::string name = type.name();
	for (std::string_view prefix : {"class ", "struct ", "enum ", "union "}) {
		for (size_t pos; (pos = name.find(prefix)) != std::string::npos;)
			name.erase(pos, prefix.size());
	}
	return name;
}

} // namespace aion::commons::configuration::transformers::detail
