#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "Registry.h"

/** Turns the registry model into the generated registry TUs, their *_empty variants and registry_report.txt. */
namespace aion::gameserver::tools::regscan {

struct GeneratedFile {
	std::string name; // file name inside the output directory
	std::string content;
};

/** The registries, in this order: ai, instance, zone, quest, commands, clientpackets, npcids */
const std::vector<std::string>& registryNames();

/**
 * Registry.<name>.gen.cpp and Registry.<name>.empty.gen.cpp for every registry name, then registry_report.txt. The content depends only on the
 * model (no timestamps, no absolute paths), so unchanged inputs give byte-identical outputs.
 */
std::vector<GeneratedFile> generateFiles(const RegistryModel& model);

/** The report alone (also part of generateFiles) */
std::string generateReport(const RegistryModel& model);

/** A one-line summary of the report for the build log, e.g. "ai 3/457, instance 0/73, ..." */
std::string summaryLine(const RegistryModel& model);

/**
 * Writes the file only if its content differs from what is on disk, so unchanged tables are not recompiled.
 * @return true if the file was written; throws std::filesystem::filesystem_error or std::runtime_error on I/O errors
 */
bool writeIfChanged(const std::filesystem::path& path, const std::string& content);

} // namespace aion::gameserver::tools::regscan
