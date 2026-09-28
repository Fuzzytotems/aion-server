#include "aion/commons/configuration/transformers/FileTransformer.h"

namespace aion::commons::configuration::transformers::FileTransformer {

std::filesystem::path toPath(std::string_view utf8) {
	// copy instead of reinterpret_cast: accessing char data through char8_t lvalues would violate strict aliasing
	return std::filesystem::path(std::u8string(utf8.begin(), utf8.end()));
}

} // namespace aion::commons::configuration::transformers::FileTransformer
