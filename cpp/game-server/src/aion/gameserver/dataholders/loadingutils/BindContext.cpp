#include "aion/gameserver/dataholders/loadingutils/BindContext.h"

#include <algorithm>
#include <exception>

namespace aion::gameserver::xml {

namespace {

constexpr std::string_view XSI_NAMESPACE = "http://www.w3.org/2001/XMLSchema-instance";

/** the namespace URI bound to `prefix` on `element` or its ancestors (empty if undeclared) */
std::string_view namespaceOf(pugi::xml_node element, std::string_view prefix) {
	std::string declaration = "xmlns:" + std::string(prefix);
	for (pugi::xml_node node = element; node && node.type() == pugi::node_element; node = node.parent()) {
		for (pugi::xml_attribute attribute = node.first_attribute(); attribute; attribute = attribute.next_attribute()) {
			if (declaration == attribute.name())
				return attribute.value();
		}
	}
	return {};
}

bool isNamespaceDeclaration(std::string_view name) noexcept {
	return name == "xmlns" || name.starts_with("xmlns:");
}

void countIgnoredSubtree(BindStats& stats, pugi::xml_node element) {
	stats.elementIgnored(element.name());
	for (pugi::xml_attribute attribute = element.first_attribute(); attribute; attribute = attribute.next_attribute()) {
		std::string_view name = attribute.name();
		if (isNamespaceDeclaration(name))
			stats.namespaceDeclaration();
		else if (name.find(':') != std::string_view::npos) // a prefixed attribute (XML namespaces: xsi:type, ...)
			stats.namespaceAttribute(name);
		else
			stats.attributeIgnored(element.name(), name);
	}
	for (pugi::xml_node child = element.first_child(); child; child = child.next_sibling()) {
		if (child.type() == pugi::node_element)
			countIgnoredSubtree(stats, child);
	}
}

} // namespace

// ---- ChildCounts (XmlBinding.h) ------------------------------------------------------------------------------------------------------------------

size_t ChildCounts::operator[](std::string_view name) const noexcept {
	for (const auto& [childName, count] : counts) {
		if (childName == name)
			return count;
	}
	return 0;
}

void ChildCounts::addChildren(pugi::xml_node parent) {
	size_t lastHit = 0;
	for (pugi::xml_node child = parent.first_child(); child; child = child.next_sibling()) {
		if (child.type() != pugi::node_element)
			continue;
		std::string_view name = child.name();
		++totalCount;
		if (lastHit < counts.size() && counts[lastHit].first == name) {
			++counts[lastHit].second;
			continue;
		}
		auto it = std::ranges::find(counts, name, &std::pair<std::string_view, size_t>::first);
		if (it == counts.end()) {
			counts.emplace_back(name, 1);
			lastHit = counts.size() - 1;
		} else {
			++it->second;
			lastHit = static_cast<size_t>(it - counts.begin());
		}
	}
}

// ---- BindContext
// ------------------------------------------------------------------------------------------------------------------------------------

BindContext::BindContext(LoadContext& load)
    : loadContext(load), previousBinding(load.setBinding(this)), checkpoint(load.checkpoint()), uncaughtExceptions(std::uncaught_exceptions()),
      collectStats(load.options().collectStats) {}

BindContext::~BindContext() {
	if (std::uncaught_exceptions() > uncaughtExceptions)
		loadContext.rollback(checkpoint);
	else if (previousBinding == nullptr)
		loadContext.commit();
	loadContext.setBinding(previousBinding);
}

BindContext::FrameGuard::FrameGuard(BindContext& context, Frame frame)
    : context(context), savedAttribute(context.currentAttribute), savedPosition(context.positionNode) {
	context.frames.push_back(frame);
	context.currentAttribute = pugi::xml_attribute();
	context.positionNode = pugi::xml_node();
}

BindContext::FrameGuard::~FrameGuard() {
	context.frames.pop_back();
	context.currentAttribute = savedAttribute;
	context.positionNode = savedPosition;
}

BindContext::Frame& BindContext::top() {
	if (frames.empty())
		throw commons::utils::IllegalStateException("BindContext: no element is being bound");
	return frames.back();
}

const BindContext::Frame* BindContext::topOrNull() const noexcept {
	return frames.empty() ? nullptr : &frames.back();
}

void BindContext::countElementBound(pugi::xml_node element) {
	++consumed;
	if (collectStats)
		loadContext.stats().elementBound(element.name());
}

bool BindContext::isIgnoredAttribute(pugi::xml_node element, std::string_view name) const {
	if (name == "xmlns") {
		if (*element.attribute("xmlns").value() != '\0')
			failAt(element, "Namespaced elements are not supported (default namespace declaration xmlns=\"" +
			                  std::string(element.attribute("xmlns").value()) + "\")");
		return true;
	}
	size_t colon = name.find(':');
	if (colon == std::string_view::npos)
		return false;
	std::string_view prefix = name.substr(0, colon);
	if (prefix == "xmlns")
		return true;
	return namespaceOf(element, prefix) == XSI_NAMESPACE;
}

void BindContext::handleAttribute(pugi::xml_node element, pugi::xml_attribute attribute, bool bound) {
	if (!bound) {
		unknownAttribute(element, attribute);
		return;
	}
	if (!collectStats)
		return;
	if (attributeIgnoredByBinder)
		loadContext.stats().attributeIgnored(element.name(), attribute.name());
	else
		loadContext.stats().attributeBound(element.name(), attribute.name());
}

void BindContext::countNamespaceAttribute(std::string_view name) {
	if (!collectStats)
		return;
	if (isNamespaceDeclaration(name))
		loadContext.stats().namespaceDeclaration();
	else
		loadContext.stats().namespaceAttribute(name);
}

void BindContext::ignoreAttribute() {
	if (!bindingAttribute)
		throw commons::utils::IllegalStateException("BindContext::ignoreAttribute called outside XmlBinding<T>::attribute");
	attributeIgnoredByBinder = true;
}

void BindContext::unknownAttribute(pugi::xml_node element, pugi::xml_attribute attribute) {
	std::string_view className = currentClassName();
	std::string message = "Unknown attribute '" + std::string(attribute.name()) + "' on <" + element.name() + "> (" + std::string(className) + ")";
	if (strict())
		fail(message);
	if (collectStats)
		loadContext.stats().attributeUnknown(element.name(), attribute.name());
	warnOnce(std::string(className) + "@" + attribute.name(), message);
}

void BindContext::unknownElement(pugi::xml_node element) {
	failAt(element,
	       "Unexpected element <" + std::string(element.name()) + "> in <" + currentNode().name() + "> (" + std::string(currentClassName()) + ")");
}

void BindContext::unexpectedText(pugi::xml_node text) {
	std::string value = text.value();
	std::ranges::replace_if(value, [](char c) { return isXmlWhitespace(c); }, ' ');
	std::string message =
	  "Unexpected text '" + std::string(trimXml(value)) + "' in <" + currentNode().name() + "> (" + std::string(currentClassName()) + ")";
	if (strict())
		failAt(text, message);
	if (collectStats)
		loadContext.stats().textUnknown(currentNode().name());
	positionAt(text);
	warnOnce(std::string(currentClassName()) + "#text", message);
	positionAt(pugi::xml_node());
}

void BindContext::notCounted(pugi::xml_node element) {
	throw commons::utils::IllegalStateException(describeCurrent() + ": binder of " + std::string(currentClassName()) + " accepted <" + element.name() +
	                                            "> without consuming it through a BindContext helper");
}

void BindContext::checkListCapacity(size_t size, size_t capacity, pugi::xml_node element) {
	if (size >= capacity)
		throw commons::utils::IllegalStateException(
		  describeCurrent() + ": list for <" + element.name() + "> in " + std::string(currentClassName()) +
		  " is not reserved (XmlBinding<T>::reserve must reserve every in-place list so bound elements never move)");
}

void BindContext::checkRepeated(bool alreadySet, pugi::xml_node element) {
	if (!alreadySet)
		return;
	std::string message = "Repeated element <" + std::string(element.name()) + "> in <" + currentNode().name() + "> (" +
	                      std::string(currentClassName()) + "), the last one wins";
	if (strict())
		failAt(element, message);
	positionAt(element);
	warnOnce(std::string(currentClassName()) + "<" + element.name() + ">repeated", message);
	positionAt(pugi::xml_node());
}

void BindContext::warnEmptyString() {
	std::string name = currentAttribute ? std::string(currentAttribute.name()) : "<" + std::string(currentNode().name()) + ">";
	warnOnce(std::string(currentClassName()) + "@" + name + "=\"\"",
	         "Empty value for " + name + " of " + std::string(currentClassName()) +
	           ": a non-optional std::string cannot tell it from an absent value (Java: \"\" vs null)");
}

void BindContext::mergeRootTag(pugi::xml_node first, pugi::xml_node root) {
	if (std::string_view(first.name()) != root.name()) {
		std::string message = "Root <" + std::string(root.name()) + "> differs from <" + first.name() +
		                      "> of the first file of this directory import; its children are merged into " + std::string(currentClassName());
		if (strict())
			failAt(root, message);
		positionAt(root);
		warnOnce(std::string(currentClassName()) + "<" + root.name() + ">merged", message);
		positionAt(pugi::xml_node());
	}
	if (!collectStats)
		return;
	std::vector<std::string> attributes; // the merge drops the root with its attributes: listed, not counted per tag
	for (pugi::xml_attribute attribute = root.first_attribute(); attribute; attribute = attribute.next_attribute()) {
		if (!isNamespaceDeclaration(attribute.name()))
			attributes.emplace_back(attribute.name());
	}
	loadContext.stats().rootSkipped(top().document->displayName(), root.name(), std::move(attributes));
}

std::string BindContext::elementText(pugi::xml_node element) {
	countElementBound(element);
	FrameGuard guard(*this, Frame{element, top().document, top().file, top().className, top().object, {}});
	for (pugi::xml_attribute attribute = element.first_attribute(); attribute; attribute = attribute.next_attribute()) {
		currentAttribute = attribute;
		if (isIgnoredAttribute(element, attribute.name()))
			countNamespaceAttribute(attribute.name());
		else
			unknownAttribute(element, attribute);
		currentAttribute = pugi::xml_attribute();
	}
	std::string text;
	for (pugi::xml_node child = element.first_child(); child; child = child.next_sibling()) {
		pugi::xml_node_type type = child.type();
		if (type == pugi::node_pcdata || type == pugi::node_cdata)
			text += child.value();
		else if (type == pugi::node_element)
			failAt(child, "Unexpected element <" + std::string(child.name()) + "> in text element <" + element.name() + "> (" +
			                std::string(currentClassName()) + ")");
	}
	return text;
}

void BindContext::ignoreElement(pugi::xml_node element) {
	++consumed;
	if (collectStats)
		countIgnoredSubtree(loadContext.stats(), element);
}

void BindContext::checkRequiredAttributes(std::initializer_list<std::string_view> names) const {
	const Frame* frame = topOrNull();
	if (frame == nullptr)
		throw commons::utils::IllegalStateException("checkRequiredAttributes outside binding");
	for (std::string_view name : names) {
		bool present = false;
		for (pugi::xml_attribute attribute = frame->node.first_attribute(); attribute && !present; attribute = attribute.next_attribute())
			present = name == attribute.name();
		if (!present)
			fail("Missing required attribute '" + std::string(name) + "'");
	}
}

void BindContext::checkRequiredElements(std::initializer_list<std::string_view> names) const {
	const Frame* frame = topOrNull();
	if (frame == nullptr)
		throw commons::utils::IllegalStateException("checkRequiredElements outside binding");
	auto hasChild = [](pugi::xml_node parent, std::string_view name) {
		for (pugi::xml_node child = parent.first_child(); child; child = child.next_sibling()) {
			if (child.type() == pugi::node_element && name == child.name())
				return true;
		}
		return false;
	};
	for (std::string_view name : names) {
		bool present = false;
		if (frame->documents.empty()) {
			present = hasChild(frame->node, name);
		} else {
			for (const XmlDocument* document : frame->documents)
				present = present || hasChild(document->root(), name);
		}
		if (!present)
			fail("Missing required element <" + std::string(name) + ">");
	}
}

std::string BindContext::pathString() const {
	std::string path;
	for (const Frame& frame : frames) {
		// text elements, wrappers and choice items push frames for the same node as their child helpers; skip consecutive duplicates
		if (!path.empty() && &frame != &frames.front() && (&frame - 1)->node == frame.node)
			continue;
		if (!path.empty())
			path += '/';
		path += frame.node.name();
	}
	return path;
}

std::pair<uint32_t, XmlLocation> BindContext::currentPosition() const noexcept {
	const Frame* frame = topOrNull();
	if (frame == nullptr)
		return {0, {}};
	if (currentAttribute)
		return {frame->file, frame->document->locate(currentAttribute)};
	if (positionNode)
		return {frame->file, frame->document->locate(positionNode)};
	return {frame->file, frame->document->locate(frame->node)};
}

std::string BindContext::describeCurrent() const {
	const Frame* frame = topOrNull();
	if (frame == nullptr)
		return {};
	XmlLocation location = currentPosition().second;
	std::string result = frame->document->describe(location) + ": " + pathString();
	if (currentAttribute)
		result += "@" + std::string(currentAttribute.name());
	return result + " (" + std::string(frame->className) + ")";
}

void BindContext::fail(std::string_view message) const {
	std::string where = describeCurrent();
	throw StaticDataException(where.empty() ? std::string(message) : where + ": " + std::string(message));
}

void BindContext::failAt(pugi::xml_node node, std::string_view message) const {
	const Frame* frame = topOrNull();
	if (frame == nullptr)
		throw StaticDataException(std::string(message));
	std::string path = pathString();
	if (node != frame->node) {
		if (!path.empty())
			path += '/';
		path += node.type() == pugi::node_element ? node.name() : "text()";
	}
	throw StaticDataException(frame->document->describe(frame->document->locate(node)) + ": " + path + " (" + std::string(frame->className) +
	                          "): " + std::string(message));
}

void BindContext::warnOnce(std::string_view key, std::string_view message) {
	loadContext.warnOnce(key, message);
}

pugi::xml_node BindContext::currentNode() const noexcept {
	const Frame* frame = topOrNull();
	return frame == nullptr ? pugi::xml_node() : frame->node;
}

std::string_view BindContext::currentClassName() const noexcept {
	const Frame* frame = topOrNull();
	return frame == nullptr ? std::string_view{} : frame->className;
}

XmlParent BindContext::currentObject() const noexcept {
	const Frame* frame = topOrNull();
	return frame == nullptr ? XmlParent{} : frame->object;
}

} // namespace aion::gameserver::xml
