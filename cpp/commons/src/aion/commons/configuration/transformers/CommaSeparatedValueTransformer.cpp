#include "aion/commons/configuration/transformers/CommaSeparatedValueTransformer.h"

#include "aion/commons/utils/StringUtils.h"

namespace aion::commons::configuration::transformers::CommaSeparatedValueTransformer {

namespace {

std::string trim(std::string_view input) {
	std::string_view output = utils::StringUtils::trim(input);
	// strip quotes if string starts AND ends with one
	if (output.size() > 1 && output.front() == '"' && output.back() == '"')
		output = output.substr(1, output.size() - 2);
	return std::string(output);
}

} // namespace

std::vector<std::string> splitAndTrimValues(std::string_view value) {
	// Operating on UTF-8 bytes is equivalent to Java's UTF-16 chars here: ',', '"' and the trimmed characters are ASCII, and no byte of a
	// multi-byte UTF-8 sequence is ASCII.
	std::vector<std::string> tokens;
	bool inQuotes = false;
	std::size_t tokenStart = 0;
	for (std::size_t i = 0; i < value.size(); i++) {
		char c = value[i];
		if (c == ',' && !inQuotes) {
			tokens.push_back(trim(value.substr(tokenStart, i - tokenStart)));
			tokenStart = i + 1;
		} else if (c == '"') {
			inQuotes = !inQuotes;
		}
	}
	std::string lastValue = trim(value.substr(tokenStart));
	if (!lastValue.empty()) // don't add empty strings if it's the only element (no comma present) or if it's the last one
		tokens.push_back(std::move(lastValue));
	return tokens;
}

} // namespace aion::commons::configuration::transformers::CommaSeparatedValueTransformer
