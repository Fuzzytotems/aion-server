#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace aion::gameserver::xml {

/** Counters of one element tag or (tag, attribute) pair. */
struct NodeCounts {
	/** consumed by a binder */
	uint64_t bound = 0;
	/** deliberately skipped: BindContext::ignoreElement subtrees, BindContext::ignoreAttribute ([ignore_attributes]) */
	uint64_t ignored = 0;
	/** not known to the binder (only possible for attributes and text outside strict mode; strict mode throws instead) */
	uint64_t unknown = 0;

	uint64_t total() const noexcept { return bound + ignored + unknown; }
	bool operator==(const NodeCounts&) const = default;
};

/**
 * Coverage counters of a load (verification V3, docs/design/static-data.md §4): per element tag and per (tag, attribute), how many were
 * bound, ignored or unknown. The independent Python counter (tools/oracle, staticdata_oracle/totals.py) counts every element and attribute
 * of the merged document; the totals must be equal, so nothing is dropped silently. Collected only with LoadOptions::collectStats (it costs a
 * hash lookup per node).
 *
 * Counting rules of the binder runtime (the same partition as the oracle's totals document):
 * - every element of an imported file is counted exactly once: holder roots of the first file and object/text/wrapper/IDREF elements as
 *   bound, ignored subtrees (ignoreElement) with all descendants as ignored; the roots of the later files of a singleRootTag directory import,
 *   which the merge drops, are listed as skipped roots instead (with their attribute names);
 * - every attribute exactly once: bound when a binder accepted it, ignored when the binder accepted it with ignoreAttribute or it is inside an
 *   ignored subtree, unknown otherwise; namespace declarations (xmlns, xmlns:*) and prefixed attributes (xsi:noNamespaceSchemaLocation) are
 *   counted separately and never per tag;
 * - non-whitespace text inside an element that expects no text is counted per enclosing tag as unknown text.
 *
 * Report formats:
 * - write(): sorted by tag, then attribute; one line each; fields separated by a tab
 *   <pre>
 *   element	<tag>	<bound>	<ignored>	<unknown text>
 *   attribute	<tag>	<attr>	<bound>	<ignored>	<unknown>
 *   </pre>
 * - writeTotals(): the oracle's totals document (`aion-staticdata-totals` version 1) for `oracle.py compare-totals --actual FILE`:
 *   byTag[tag] = {count: bound + ignored elements, attributes: {name: bound + ignored}} (unknown attributes are left out, so a lenient load
 *   that dropped attributes shows up as a difference), plus namespaceAttributes, skippedRoots and an informational `unknown` summary.
 *
 * Thread-safety: none (the load context is confined to the loading thread).
 */
class BindStats {
public:
	struct AttributeStat {
		std::string name;
		NodeCounts counts;
	};
	struct ElementStat {
		std::string tag;
		NodeCounts counts;
		uint64_t unknownText = 0;
		/** sorted by name */
		std::vector<AttributeStat> attributes;
	};
	/** the root of a later file of a singleRootTag directory import (dropped by the merge) */
	struct SkippedRoot {
		std::string file;
		std::string tag;
		/** attribute names in document order, without namespace declarations */
		std::vector<std::string> attributes;
		bool operator==(const SkippedRoot&) const = default;
	};

	void elementBound(std::string_view tag, uint64_t count = 1);
	void elementIgnored(std::string_view tag, uint64_t count = 1);
	void textUnknown(std::string_view tag);
	void attributeBound(std::string_view tag, std::string_view attribute);
	void attributeIgnored(std::string_view tag, std::string_view attribute);
	void attributeUnknown(std::string_view tag, std::string_view attribute);
	/** an xmlns or xmlns:* declaration */
	void namespaceDeclaration() noexcept { ++namespaceDeclarationCount; }
	/** a prefixed attribute such as xsi:noNamespaceSchemaLocation (not a declaration) */
	void namespaceAttribute(std::string_view name);
	void rootSkipped(std::string file, std::string_view tag, std::vector<std::string> attributes);

	/** counts of one tag (all zero if never seen) */
	NodeCounts element(std::string_view tag) const;
	/** counts of one attribute (all zero if never seen) */
	NodeCounts attribute(std::string_view tag, std::string_view attribute) const;
	uint64_t unknownText(std::string_view tag) const;
	NodeCounts totalElements() const;
	NodeCounts totalAttributes() const;
	uint64_t namespaceDeclarations() const noexcept { return namespaceDeclarationCount; }
	/** count of one prefixed attribute name */
	uint64_t namespaceAttributes(std::string_view name) const;
	/** in binding order */
	const std::vector<SkippedRoot>& skippedRoots() const noexcept { return skipped; }

	/** sorted snapshot */
	std::vector<ElementStat> snapshot() const;
	void write(std::ostream& out) const;
	/**
	 * Writes the totals document of verification V3 (JSON, deterministic: tags and attribute names sorted by byte order). Skipped root file
	 * names are made relative to `staticDataDir` when they start with it (the oracle lists them relative to data/static_data).
	 */
	void writeTotals(std::ostream& out, std::string_view staticDataDir = {}) const;
	void merge(const BindStats& other);
	void clear() noexcept;
	bool empty() const noexcept { return elements.empty() && namespaceCounts.empty() && skipped.empty() && namespaceDeclarationCount == 0; }

private:
	struct StringHash {
		using is_transparent = void;
		size_t operator()(std::string_view s) const noexcept { return std::hash<std::string_view>{}(s); }
	};
	struct Entry {
		NodeCounts counts;
		uint64_t unknownText = 0;
		std::unordered_map<std::string, NodeCounts, StringHash, std::equal_to<>> attributes;
	};

	Entry& entry(std::string_view tag);
	NodeCounts& attributeEntry(std::string_view tag, std::string_view attribute);

	std::unordered_map<std::string, Entry, StringHash, std::equal_to<>> elements;
	std::unordered_map<std::string, uint64_t, StringHash, std::equal_to<>> namespaceCounts;
	uint64_t namespaceDeclarationCount = 0;
	std::vector<SkippedRoot> skipped;
};

} // namespace aion::gameserver::xml
