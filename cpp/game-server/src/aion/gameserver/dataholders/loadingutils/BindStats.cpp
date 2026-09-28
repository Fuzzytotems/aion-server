#include "aion/gameserver/dataholders/loadingutils/BindStats.h"

#include <algorithm>
#include <ostream>
#include <utility>

namespace aion::gameserver::xml {

namespace {

constexpr std::string_view TOTALS_FORMAT = "aion-staticdata-totals";
constexpr int TOTALS_VERSION = 1;

/** a JSON string literal (the escapes of Python's json.dumps with ensure_ascii=False) */
std::string jsonString(std::string_view text) {
	constexpr std::string_view HEX = "0123456789abcdef";
	std::string out;
	out.reserve(text.size() + 2);
	out += '"';
	for (char c : text) {
		switch (c) {
			case '"':
				out += R"(\")";
				break;
			case '\\':
				out += R"(\\)";
				break;
			case '\n':
				out += R"(\n)";
				break;
			case '\r':
				out += R"(\r)";
				break;
			case '\t':
				out += R"(\t)";
				break;
			case '\b':
				out += R"(\b)";
				break;
			case '\f':
				out += R"(\f)";
				break;
			default:
				if (auto byte = static_cast<unsigned char>(c); byte < 0x20) {
					out += R"(\u00)";
					out += HEX[byte >> 4];
					out += HEX[byte & 0xF];
				} else {
					out += c;
				}
				break;
		}
	}
	out += '"';
	return out;
}

/** Writes the members of one JSON object or array like Python's json.dumps(indent="\t"): opens with open(), closes with close(). */
class JsonBlock {
public:
	JsonBlock(std::ostream& out, int depth, char closing) noexcept : out(out), depth(depth), closing(closing) {}

	/** starts the next member: separator, newline and indentation */
	std::ostream& next() {
		out << (empty ? "\n" : ",\n");
		empty = false;
		indent(depth + 1);
		return out;
	}
	void close() {
		if (!empty) {
			out << '\n';
			indent(depth);
		}
		out << closing;
	}

private:
	void indent(int levels) {
		for (int i = 0; i < levels; ++i)
			out << '\t';
	}

	std::ostream& out;
	int depth;
	char closing;
	bool empty = true;
};

} // namespace

BindStats::Entry& BindStats::entry(std::string_view tag) {
	auto it = elements.find(tag);
	if (it == elements.end())
		it = elements.emplace(std::string(tag), Entry{}).first;
	return it->second;
}

NodeCounts& BindStats::attributeEntry(std::string_view tag, std::string_view attribute) {
	Entry& e = entry(tag);
	auto it = e.attributes.find(attribute);
	if (it == e.attributes.end())
		it = e.attributes.emplace(std::string(attribute), NodeCounts{}).first;
	return it->second;
}

void BindStats::elementBound(std::string_view tag, uint64_t count) {
	entry(tag).counts.bound += count;
}

void BindStats::elementIgnored(std::string_view tag, uint64_t count) {
	entry(tag).counts.ignored += count;
}

void BindStats::textUnknown(std::string_view tag) {
	++entry(tag).unknownText;
}

void BindStats::attributeBound(std::string_view tag, std::string_view attribute) {
	++attributeEntry(tag, attribute).bound;
}

void BindStats::attributeIgnored(std::string_view tag, std::string_view attribute) {
	++attributeEntry(tag, attribute).ignored;
}

void BindStats::attributeUnknown(std::string_view tag, std::string_view attribute) {
	++attributeEntry(tag, attribute).unknown;
}

void BindStats::namespaceAttribute(std::string_view name) {
	auto it = namespaceCounts.find(name);
	if (it == namespaceCounts.end())
		it = namespaceCounts.emplace(std::string(name), 0).first;
	++it->second;
}

void BindStats::rootSkipped(std::string file, std::string_view tag, std::vector<std::string> attributes) {
	skipped.push_back(SkippedRoot{std::move(file), std::string(tag), std::move(attributes)});
}

uint64_t BindStats::namespaceAttributes(std::string_view name) const {
	auto it = namespaceCounts.find(name);
	return it == namespaceCounts.end() ? 0 : it->second;
}

NodeCounts BindStats::element(std::string_view tag) const {
	auto it = elements.find(tag);
	return it == elements.end() ? NodeCounts{} : it->second.counts;
}

NodeCounts BindStats::attribute(std::string_view tag, std::string_view attribute) const {
	auto it = elements.find(tag);
	if (it == elements.end())
		return {};
	auto attr = it->second.attributes.find(attribute);
	return attr == it->second.attributes.end() ? NodeCounts{} : attr->second;
}

uint64_t BindStats::unknownText(std::string_view tag) const {
	auto it = elements.find(tag);
	return it == elements.end() ? 0 : it->second.unknownText;
}

NodeCounts BindStats::totalElements() const {
	NodeCounts total;
	for (const auto& [tag, e] : elements) {
		total.bound += e.counts.bound;
		total.ignored += e.counts.ignored;
		total.unknown += e.counts.unknown;
	}
	return total;
}

NodeCounts BindStats::totalAttributes() const {
	NodeCounts total;
	for (const auto& [tag, e] : elements) {
		for (const auto& [name, counts] : e.attributes) {
			total.bound += counts.bound;
			total.ignored += counts.ignored;
			total.unknown += counts.unknown;
		}
	}
	return total;
}

std::vector<BindStats::ElementStat> BindStats::snapshot() const {
	std::vector<ElementStat> result;
	result.reserve(elements.size());
	for (const auto& [tag, e] : elements) {
		ElementStat stat{tag, e.counts, e.unknownText, {}};
		stat.attributes.reserve(e.attributes.size());
		for (const auto& [name, counts] : e.attributes)
			stat.attributes.push_back({name, counts});
		std::ranges::sort(stat.attributes, {}, &AttributeStat::name);
		result.push_back(std::move(stat));
	}
	std::ranges::sort(result, {}, &ElementStat::tag);
	return result;
}

void BindStats::write(std::ostream& out) const {
	for (const ElementStat& e : snapshot()) {
		out << "element\t" << e.tag << '\t' << e.counts.bound << '\t' << e.counts.ignored << '\t' << e.unknownText << '\n';
		for (const AttributeStat& a : e.attributes)
			out << "attribute\t" << e.tag << '\t' << a.name << '\t' << a.counts.bound << '\t' << a.counts.ignored << '\t' << a.counts.unknown << '\n';
	}
}

void BindStats::writeTotals(std::ostream& out, std::string_view staticDataDir) const {
	std::string prefix(staticDataDir);
	std::ranges::replace(prefix, '\\', '/');
	if (!prefix.empty() && prefix.back() != '/')
		prefix += '/';

	std::vector<ElementStat> stats = snapshot();
	uint64_t elementCount = 0;
	uint64_t attributeCount = 0;
	uint64_t unknownAttributes = 0;
	uint64_t unknownTexts = 0;
	for (const ElementStat& e : stats) {
		elementCount += e.counts.bound + e.counts.ignored;
		unknownTexts += e.unknownText;
		for (const AttributeStat& a : e.attributes) {
			attributeCount += a.counts.bound + a.counts.ignored;
			unknownAttributes += a.counts.unknown;
		}
	}

	out << '{';
	JsonBlock document(out, 0, '}');
	document.next() << R"("format": )" << jsonString(TOTALS_FORMAT);
	document.next() << R"("version": )" << TOTALS_VERSION;
	document.next() << R"("elements": )" << elementCount;
	document.next() << R"("attributes": )" << attributeCount;

	document.next() << R"("byTag": {)";
	JsonBlock byTag(out, 1, '}');
	for (const ElementStat& e : stats) {
		uint64_t count = e.counts.bound + e.counts.ignored;
		bool anyAttribute = std::ranges::any_of(e.attributes, [](const AttributeStat& a) { return a.counts.bound + a.counts.ignored > 0; });
		if (count == 0 && !anyAttribute)
			continue; // only unknown attributes or text of an element counted elsewhere (cannot happen with the binder's counting rules)
		byTag.next() << jsonString(e.tag) << ": {";
		JsonBlock tag(out, 2, '}');
		tag.next() << R"("count": )" << count;
		tag.next() << R"("attributes": {)";
		JsonBlock attributes(out, 3, '}');
		for (const AttributeStat& a : e.attributes) {
			if (uint64_t n = a.counts.bound + a.counts.ignored; n > 0)
				attributes.next() << jsonString(a.name) << ": " << n;
		}
		attributes.close();
		tag.close();
	}
	byTag.close();

	document.next() << R"("namespaceAttributes": {)";
	JsonBlock namespaced(out, 1, '}');
	std::vector<std::pair<std::string_view, uint64_t>> sortedNamespaced(namespaceCounts.begin(), namespaceCounts.end());
	std::ranges::sort(sortedNamespaced);
	for (const auto& [name, count] : sortedNamespaced)
		namespaced.next() << jsonString(name) << ": " << count;
	namespaced.close();

	document.next() << R"("skippedRoots": [)";
	JsonBlock roots(out, 1, ']');
	for (const SkippedRoot& root : skipped) {
		std::string file = root.file;
		std::ranges::replace(file, '\\', '/');
		if (!prefix.empty() && file.starts_with(prefix))
			file.erase(0, prefix.size());
		std::vector<std::string> names = root.attributes;
		std::ranges::sort(names);
		roots.next() << '{';
		JsonBlock entryBlock(out, 2, '}');
		entryBlock.next() << R"("file": )" << jsonString(file);
		entryBlock.next() << R"("tag": )" << jsonString(root.tag);
		entryBlock.next() << R"("attributes": [)";
		JsonBlock nameList(out, 3, ']');
		for (const std::string& name : names)
			nameList.next() << jsonString(name);
		nameList.close();
		entryBlock.close();
	}
	roots.close();

	// C++ only (compare-totals ignores other members): what the per-tag counts leave out
	document.next() << R"("unknown": {)";
	JsonBlock unknown(out, 1, '}');
	unknown.next() << R"("attributes": )" << unknownAttributes;
	unknown.next() << R"("text": )" << unknownTexts;
	unknown.close();
	document.next() << R"("namespaceDeclarations": )" << namespaceDeclarationCount;
	document.close();
	out << '\n';
}

void BindStats::merge(const BindStats& other) {
	for (const auto& [tag, e] : other.elements) {
		Entry& mine = entry(tag);
		mine.counts.bound += e.counts.bound;
		mine.counts.ignored += e.counts.ignored;
		mine.counts.unknown += e.counts.unknown;
		mine.unknownText += e.unknownText;
		for (const auto& [name, counts] : e.attributes) {
			NodeCounts& a = attributeEntry(tag, name);
			a.bound += counts.bound;
			a.ignored += counts.ignored;
			a.unknown += counts.unknown;
		}
	}
	for (const auto& [name, count] : other.namespaceCounts) {
		auto it = namespaceCounts.find(name);
		if (it == namespaceCounts.end())
			it = namespaceCounts.emplace(name, 0).first;
		it->second += count;
	}
	namespaceDeclarationCount += other.namespaceDeclarationCount;
	skipped.insert(skipped.end(), other.skipped.begin(), other.skipped.end());
}

void BindStats::clear() noexcept {
	elements.clear();
	namespaceCounts.clear();
	namespaceDeclarationCount = 0;
	skipped.clear();
}

} // namespace aion::gameserver::xml
