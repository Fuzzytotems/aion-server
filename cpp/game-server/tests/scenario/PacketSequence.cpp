#include "PacketSequence.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace aion::gameserver::scenario {

namespace {

std::string_view trim(std::string_view text) {
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front())))
		text.remove_prefix(1);
	while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
		text.remove_suffix(1);
	return text;
}

bool isNameChar(char c) {
	return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

int parseNumber(std::string_view text, std::string_view pattern) {
	text = trim(text);
	if (text.empty() || !std::all_of(text.begin(), text.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)); }))
		throw std::invalid_argument("invalid repetition count '" + std::string(text) + "' in " + std::string(pattern));
	return std::stoi(std::string(text));
}

/** splits on top-level commas (not inside parentheses, brackets or braces) */
std::vector<std::string_view> splitElements(std::string_view pattern) {
	std::vector<std::string_view> parts;
	int depth = 0;
	size_t start = 0;
	for (size_t i = 0; i < pattern.size(); i++) {
		char c = pattern[i];
		if (c == '(' || c == '[' || c == '{')
			depth++;
		else if (c == ')' || c == ']' || c == '}')
			depth--;
		else if (c == ',' && depth == 0) {
			parts.push_back(pattern.substr(start, i - start));
			start = i + 1;
		}
	}
	parts.push_back(pattern.substr(start));
	return parts;
}

PacketSequence::Element parseElement(std::string_view text, std::string_view pattern) {
	text = trim(text);
	PacketSequence::Element element;
	std::string_view body;
	size_t end;
	bool optional = false;
	if (text.starts_with('[')) {
		end = text.find(']');
		if (end == std::string_view::npos)
			throw std::invalid_argument("missing ']' in " + std::string(pattern));
		body = text.substr(1, end - 1);
		optional = true;
		end++;
	} else if (text.starts_with('(')) {
		end = text.find(')');
		if (end == std::string_view::npos)
			throw std::invalid_argument("missing ')' in " + std::string(pattern));
		body = text.substr(1, end - 1);
		end++;
	} else {
		end = 0;
		while (end < text.size() && isNameChar(text[end]))
			end++;
		body = text.substr(0, end);
	}
	size_t start = 0;
	for (size_t bar = body.find('|'); ; bar = body.find('|', start)) {
		std::string_view name = trim(body.substr(start, bar == std::string_view::npos ? std::string_view::npos : bar - start));
		if (name.empty() || !std::all_of(name.begin(), name.end(), isNameChar))
			throw std::invalid_argument("invalid packet name '" + std::string(name) + "' in " + std::string(pattern));
		element.names.emplace_back(name);
		if (bar == std::string_view::npos)
			break;
		start = bar + 1;
	}
	element.min = optional ? 0 : 1;
	element.max = 1;
	std::string_view suffix = trim(text.substr(end));
	if (suffix.empty())
		return element;
	if (suffix == "+") {
		element.min = optional ? 0 : 1;
		element.max = -1;
	} else if (suffix == "*") {
		element.min = 0;
		element.max = -1;
	} else if (suffix.starts_with('{') && suffix.ends_with('}')) {
		std::string_view range = suffix.substr(1, suffix.size() - 2);
		size_t dots = range.find("..");
		if (dots == std::string_view::npos) {
			element.min = element.max = parseNumber(range, pattern);
		} else {
			element.min = parseNumber(range.substr(0, dots), pattern);
			element.max = parseNumber(range.substr(dots + 2), pattern);
		}
		if (element.max < element.min)
			throw std::invalid_argument("empty repetition range in " + std::string(pattern));
	} else {
		throw std::invalid_argument("invalid suffix '" + std::string(suffix) + "' in " + std::string(pattern));
	}
	return element;
}

} // namespace

bool PacketSequence::Element::matches(std::string_view name) const {
	return std::ranges::find(names, name) != names.end();
}

std::string PacketSequence::Element::toString() const {
	std::string text;
	if (names.size() > 1)
		text += '(';
	for (size_t i = 0; i < names.size(); i++) {
		if (i > 0)
			text += " | ";
		text += names[i];
	}
	if (names.size() > 1)
		text += ')';
	if (min == 1 && max == 1)
		return text;
	if (min == 0 && max == 1)
		return names.size() > 1 ? "[" + text.substr(1, text.size() - 2) + "]" : "[" + text + "]";
	if (min == 1 && max == -1)
		return text + "+";
	if (min == 0 && max == -1)
		return text + "*";
	if (min == max)
		return text + "{" + std::to_string(min) + "}";
	return text + "{" + std::to_string(min) + ".." + (max < 0 ? std::string("") : std::to_string(max)) + "}";
}

PacketSequence PacketSequence::parse(std::string_view pattern) {
	PacketSequence sequence;
	if (trim(pattern).empty())
		return sequence;
	for (std::string_view part : splitElements(pattern))
		sequence.elements_.push_back(parseElement(part, pattern));
	return sequence;
}

PacketSequence& PacketSequence::then(std::string name, int min, int max) {
	elements_.push_back(Element{{std::move(name)}, min, max});
	return *this;
}

PacketSequence& PacketSequence::then(std::vector<std::string> names, int min, int max) {
	elements_.push_back(Element{std::move(names), min, max});
	return *this;
}

PacketSequence& PacketSequence::append(const PacketSequence& other) {
	elements_.insert(elements_.end(), other.elements_.begin(), other.elements_.end());
	return *this;
}

PacketSequence::Result PacketSequence::match(const std::vector<std::string>& packets, const std::function<bool(size_t index)>& isAsyncAllowed) const {
	// an NFA simulation: a state is (element, repetitions so far); repetitions of unbounded elements are capped at max(min, 1)
	const size_t elementCount = elements_.size();
	using State = std::pair<size_t, int>;
	auto closure = [&](std::vector<State> states) {
		for (size_t i = 0; i < states.size(); i++) {
			auto [e, c] = states[i];
			if (e < elementCount && c >= elements_[e].min) {
				State next{e + 1, 0};
				if (std::ranges::find(states, next) == states.end())
					states.push_back(next);
			}
		}
		return states;
	};
	// what the sequence could accept next: the elements that may still repeat, and the end
	auto describeExpected = [&](const std::vector<State>& states) {
		std::vector<std::string> expected;
		for (const State& state : states) {
			std::string text;
			if (state.first == elementCount)
				text = "the end of the packets";
			else if (const Element& element = elements_[state.first]; element.max < 0 || state.second < element.max)
				text = element.toString();
			if (!text.empty() && std::ranges::find(expected, text) == expected.end())
				expected.push_back(text);
		}
		std::string joined;
		for (size_t i = 0; i < expected.size(); i++)
			joined += (i == 0 ? "" : " or ") + expected[i];
		return joined;
	};

	std::vector<State> states = closure({State{0, 0}});
	Result result;
	for (size_t p = 0; p < packets.size(); p++) {
		const bool async = isAsyncAllowed && isAsyncAllowed(p);
		std::vector<State> next;
		auto add = [&next](State state) {
			if (std::ranges::find(next, state) == next.end())
				next.push_back(state);
		};
		for (const State& state : states) {
			auto [e, c] = state;
			if (e < elementCount) {
				const Element& element = elements_[e];
				if (element.matches(packets[p]) && (element.max < 0 || c < element.max))
					add(State{e, element.max < 0 ? std::min(c + 1, std::max(element.min, 1)) : c + 1});
			}
			if (async)
				add(state);
		}
		next = closure(std::move(next));
		if (next.empty()) {
			result.failedAt = p;
			result.message =
				"the packets match the sequence up to packet #" + std::to_string(p) + " " + packets[p] + ", where it expected " + describeExpected(states);
			return result;
		}
		states = std::move(next);
	}
	if (std::ranges::find(states, State{elementCount, 0}) != states.end()) {
		result.matched = true;
		return result;
	}
	result.failedAt = packets.size();
	result.message = "the packets match the sequence up to the end of the packets, where it expected " + describeExpected(states);
	return result;
}

std::string PacketSequence::toString() const {
	std::string text;
	for (size_t i = 0; i < elements_.size(); i++) {
		if (i > 0)
			text += ", ";
		text += elements_[i].toString();
	}
	return text;
}

} // namespace aion::gameserver::scenario
