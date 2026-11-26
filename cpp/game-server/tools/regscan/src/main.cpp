// aion_gs_regscan: scans the handler and client packet sources for registration markers and writes the registry tables (see README.md).

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "Emitter.h"
#include "Registry.h"

namespace fs = std::filesystem;
using namespace aion::gameserver::tools::regscan;

namespace {

constexpr std::string_view USAGE = R"(usage: aion_gs_regscan --out DIR [options]

  --out DIR                          output directory for Registry.<name>[.empty].gen.cpp and registry_report.txt (required)
  --handlers-root DIR                include root that contains aion/gameserver/handlers (handler sources)
  --clientpackets-root DIR           include root that contains aion/gameserver/network/aion/clientpackets
  --java-handlers DIR                game-server/data/handlers: Java keys for the cross-checks and the report
  --java-client-packet-factory FILE  AionClientPacketFactory.java: every AION_CLIENT_PACKET class must be in its table
  --java-ahead FILE                  known Java cross-check errors to tolerate while the Java tree is ahead of the port, one per line:
                                     "<path suffix> | <message>", '#' starts a comment; an entry that matches no error is an error
  --stamp FILE                       touched after a successful run (for build systems)
  --quiet                            no summary line

Exit code 0: outputs are up to date (files are only rewritten when their content changes). 1: errors (printed as
"file(line,column): error: message"; no output is written). 2: invalid arguments.
)";

struct Arguments {
	Options options;
	std::optional<fs::path> out;
	std::optional<fs::path> stamp;
	std::optional<fs::path> javaAhead;
	bool quiet = false;
};

/** One line of the --java-ahead file: an error whose file ends with pathSuffix and whose message equals message is tolerated */
struct JavaAheadEntry {
	std::string pathSuffix;
	std::string message;
	uint32_t line = 0; // in the --java-ahead file
	bool used = false;
};

std::string trim(std::string_view s) {
	size_t begin = s.find_first_not_of(" \t\r");
	if (begin == std::string_view::npos)
		return {};
	size_t end = s.find_last_not_of(" \t\r");
	return std::string(s.substr(begin, end - begin + 1));
}

std::vector<JavaAheadEntry> readJavaAhead(const fs::path& file, std::vector<Diagnostic>& errors) {
	std::vector<JavaAheadEntry> entries;
	std::ifstream in(file);
	if (!in) {
		errors.push_back(Diagnostic{file.string(), 0, 0, "cannot read the --java-ahead file"});
		return entries;
	}
	std::string text;
	uint32_t lineNumber = 0;
	while (std::getline(in, text)) {
		lineNumber++;
		std::string line = trim(text.substr(0, text.find('#')));
		if (line.empty())
			continue;
		size_t bar = line.find(" | ");
		if (bar == std::string::npos) {
			errors.push_back(Diagnostic{file.string(), lineNumber, 1, "expected \"<path suffix> | <message>\""});
			continue;
		}
		entries.push_back(JavaAheadEntry{trim(line.substr(0, bar)), trim(line.substr(bar + 3)), lineNumber});
	}
	return entries;
}

bool endsWith(std::string_view text, std::string_view suffix) {
	return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

std::optional<Arguments> parseArguments(int argc, char** argv) {
	Arguments args;
	for (int i = 1; i < argc; i++) {
		std::string_view arg = argv[i];
		auto value = [&]() -> std::optional<fs::path> {
			if (i + 1 >= argc) {
				std::cerr << "aion_gs_regscan: missing value for " << arg << "\n";
				return std::nullopt;
			}
			return fs::path(argv[++i]);
		};
		std::optional<fs::path>* target = nullptr;
		if (arg == "--out")
			target = &args.out;
		else if (arg == "--handlers-root")
			target = &args.options.handlersRoot;
		else if (arg == "--clientpackets-root")
			target = &args.options.clientPacketsRoot;
		else if (arg == "--java-handlers")
			target = &args.options.javaHandlers;
		else if (arg == "--java-client-packet-factory")
			target = &args.options.javaClientPacketFactory;
		else if (arg == "--stamp")
			target = &args.stamp;
		else if (arg == "--java-ahead")
			target = &args.javaAhead;
		if (target != nullptr) {
			auto path = value();
			if (!path)
				return std::nullopt;
			*target = std::move(path);
		} else if (arg == "--quiet") {
			args.quiet = true;
		} else if (arg == "--help" || arg == "-h") {
			std::cout << USAGE;
			std::exit(0);
		} else {
			std::cerr << "aion_gs_regscan: unknown argument " << arg << "\n" << USAGE;
			return std::nullopt;
		}
	}
	if (!args.out) {
		std::cerr << "aion_gs_regscan: --out is required\n" << USAGE;
		return std::nullopt;
	}
	return args;
}

int run(const Arguments& args) {
	std::vector<Diagnostic> errors;
	ScanInput input = readInput(args.options, errors);
	RegistryModel model = buildRegistry(input);
	errors.insert(errors.end(), model.errors.begin(), model.errors.end());
	if (args.javaAhead) {
		std::vector<Diagnostic> readErrors;
		std::vector<JavaAheadEntry> entries = readJavaAhead(*args.javaAhead, readErrors);
		std::erase_if(errors, [&entries](const Diagnostic& d) {
			std::string file = d.file;
			std::replace(file.begin(), file.end(), '\\', '/');
			for (JavaAheadEntry& entry : entries) {
				if (d.message == entry.message && endsWith(file, entry.pathSuffix)) {
					entry.used = true;
					return true;
				}
			}
			return false;
		});
		for (const JavaAheadEntry& entry : entries) {
			if (!entry.used)
				readErrors.push_back(Diagnostic{args.javaAhead->string(), entry.line, 1, "stale entry, no such error: " + entry.pathSuffix + " | " + entry.message});
		}
		errors.insert(errors.end(), readErrors.begin(), readErrors.end());
	}
	if (!errors.empty()) {
		for (const Diagnostic& diagnostic : errors)
			std::cerr << diagnostic.format() << "\n";
		std::cerr << "aion_gs_regscan: " << errors.size() << " error(s), registry tables not updated\n";
		return 1;
	}
	size_t written = 0;
	for (const GeneratedFile& file : generateFiles(model))
		written += writeIfChanged(*args.out / file.name, file.content) ? 1 : 0;
	if (args.stamp) {
		if (args.stamp->has_parent_path())
			fs::create_directories(args.stamp->parent_path());
		std::ofstream stamp(*args.stamp, std::ios::trunc);
		stamp << "aion_gs_regscan\n";
		if (!stamp)
			throw std::runtime_error("cannot write " + args.stamp->string());
	}
	if (!args.quiet)
		std::cout << "aion_gs_regscan: " << summaryLine(model) << " (" << written << " file(s) updated)\n";
	return 0;
}

} // namespace

int main(int argc, char** argv) {
	auto args = parseArguments(argc, argv);
	if (!args)
		return 2;
	try {
		return run(*args);
	} catch (const std::exception& e) {
		std::cerr << "aion_gs_regscan: error: " << e.what() << "\n";
		return 1;
	}
}
