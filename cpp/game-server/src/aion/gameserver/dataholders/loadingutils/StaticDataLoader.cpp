#include "aion/gameserver/dataholders/loadingutils/StaticDataLoader.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <numeric>

#include "aion/commons/logging/LoggerFactory.h"
#include "aion/gameserver/runtime/sched/ForkJoinPool.h"

namespace aion::gameserver::xml {

namespace {

const commons::logging::Logger& log() {
	static const auto* logger =
	  new commons::logging::Logger(commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dataholders.loadingutils.XmlDataLoader"));
	return *logger;
}

bool isNameChar(char c) noexcept {
	return c != '\0' && c != '>' && c != '/' && !isXmlWhitespace(c);
}

} // namespace

void HolderRegistry::add(HolderRegistration registration) {
	if (find(registration.rootTag) != nullptr)
		throw commons::utils::IllegalArgumentException("Holder root tag <" + registration.rootTag + "> is registered twice");
	registrations.push_back(std::move(registration));
}

const HolderRegistration* HolderRegistry::find(std::string_view rootTag) const noexcept {
	auto it = std::ranges::find(registrations, rootTag, &HolderRegistration::rootTag);
	return it == registrations.end() ? nullptr : &*it;
}

std::string StaticDataLoader::peekRootTag(const std::filesystem::path& file) {
	std::ifstream in(file, std::ios::binary);
	if (!in)
		throw StaticDataException("Cannot open " + file.generic_string());
	std::string head(64 * 1024, '\0');
	in.read(head.data(), static_cast<std::streamsize>(head.size()));
	head.resize(static_cast<size_t>(in.gcount()));
	size_t pos = 0;
	while ((pos = head.find('<', pos)) != std::string::npos) {
		if (head.compare(pos, 4, "<!--") == 0) {
			size_t end = head.find("-->", pos + 4);
			if (end == std::string::npos)
				return {};
			pos = end + 3;
		} else if (head.compare(pos, 2, "<?") == 0) {
			size_t end = head.find("?>", pos + 2);
			if (end == std::string::npos)
				return {};
			pos = end + 2;
		} else if (head.compare(pos, 2, "<!") == 0) {
			size_t end = head.find('>', pos + 2);
			if (end == std::string::npos)
				return {};
			pos = end + 1;
		} else {
			size_t end = pos + 1;
			while (end < head.size() && isNameChar(head[end]))
				++end;
			return head.substr(pos + 1, end - pos - 1);
		}
	}
	return {};
}

std::vector<std::unique_ptr<XmlDocument>> StaticDataLoader::parseFiles(std::span<const std::filesystem::path> files, bool parallel) {
	std::vector<std::unique_ptr<XmlDocument>> documents(files.size());
	if (parallel && files.size() > 1) {
		std::vector<size_t> indices(files.size());
		std::iota(indices.begin(), indices.end(), size_t{0});
		runtime::ForkJoinPool::commonPool().parallelForEach(indices, [&](size_t index) { documents[index] = XmlDocument::parseFile(files[index]); });
	} else {
		for (size_t i = 0; i < files.size(); ++i)
			documents[i] = XmlDocument::parseFile(files[i]);
	}
	return documents;
}

void StaticDataLoader::load(LoadContext& context, const std::filesystem::path& staticDataXml) const {
	std::vector<StaticDataImport> imports = StaticDataImports::resolve(staticDataXml, context.options().countryCode, context.options().strict);
	load(context, imports);
}

void StaticDataLoader::load(LoadContext& context, std::span<const StaticDataImport> imports) const {
	auto start = std::chrono::steady_clock::now();
	size_t loadedImports = 0;
	size_t loadedFiles = 0;
	for (const StaticDataImport& entry : imports) {
		if (context.options().holders) {
			std::string tag = peekRootTag(entry.files.front());
			if (!context.options().holders->contains(tag))
				continue;
		}
		loadFiles(context, entry.files);
		++loadedImports;
		loadedFiles += entry.files.size();
	}
	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
	log().info("Loaded {} static data import(s) from {} file(s) in {} ms", loadedImports, loadedFiles, millis);
}

void StaticDataLoader::loadFiles(LoadContext& context, std::span<const std::filesystem::path> files) const {
	if (files.empty())
		throw commons::utils::IllegalArgumentException("loadFiles needs at least one file");
	std::vector<std::unique_ptr<XmlDocument>> documents = parseFiles(files, context.options().parallelParse);
	std::vector<const XmlDocument*> views;
	views.reserve(documents.size());
	for (const auto& document : documents)
		views.push_back(document.get());
	bindDocuments(context, views);
}

void StaticDataLoader::bindDocuments(LoadContext& context, std::span<const XmlDocument* const> documents) const {
	const XmlDocument& first = *documents.front();
	std::string tag = first.root().name();
	const HolderRegistration* registration = registry.find(tag);
	if (registration == nullptr)
		throw StaticDataException(first.describe(first.locate(first.root())) + ": Unknown static data holder <" + tag + ">");
	ErasedHolder holder = registration->create();
	context.beginHolder(registration->rootTag, registration->type, &registration->dependencies);
	try {
		BindContext binding(context);
		registration->bind(binding, holder.get(), documents, context.root());
		context.endHolder();
		// still inside the binding scope: if addHolder throws (a strict-mode duplicate tag), the holder is destroyed and the BindContext rolls
		// back its XmlIDs, IDREF slots and after-IDREF tasks; after the scope the undo journal is committed
		context.addHolder(registration->rootTag, std::move(holder));
	} catch (...) {
		context.endHolder();
		throw;
	}
}

} // namespace aion::gameserver::xml
