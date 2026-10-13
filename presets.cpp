/*
 * Copyright (c) 2026
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in
 * the LICENSE file in the root directory of this source tree.
 */

#include "pch.h"
#include "CheckpointPlugin.h"
#include "IMGUI/imfilebrowser.h"
#include <algorithm>
#include <unordered_set>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {
	constexpr const char* PRESET_FOLDER_NAME = "FreeplayCheckpoint";
	constexpr const char* PRESET_SUBFOLDER_NAME = "presets";
	constexpr const char* DEFAULT_PRESET_FILE_NAME = "freeplaycheckpoint.data";

	// Version 1 GameState::write() serializes:
	// 20 floats + 6 int32 values + 1 bool = 105 bytes with the MSVC ABI.
	constexpr std::uintmax_t SERIALIZED_GAME_STATE_SIZE = 20 * sizeof(float) + 6 * sizeof(std::int32_t) + sizeof(bool);

	// Defensive limits for imported files.
	// Windows limits filenames to 255 characters including extensions so with .data, but a bit of extrapadding is good
	// to allow for duplicate import files "..._1, ..._2, etc."
	constexpr std::size_t MAX_PRESET_NAME_LENGTH = 240;
	constexpr std::int32_t MAX_IMPORTED_CHECKPOINTS = 50000;
	constexpr std::int32_t MAX_IMPORTED_LOCKS = 50000;
	constexpr std::uintmax_t MAX_PRESET_FILE_SIZE = 64ull * 1024ull * 1024ull;

	std::string trimWhitespace(std::string value) {
		auto notSpace = [](unsigned char c) {
			return !std::isspace(c);
		};

		value.erase(
			value.begin(),
			std::find_if(value.begin(), value.end(), notSpace)
		);

		value.erase(
			std::find_if(value.rbegin(), value.rend(), notSpace).base(),
			value.end()
		);

		return value;
	}

	void migrateFileIfNeeded(
		const std::filesystem::path& oldPath,
		const std::filesystem::path& newPath
	) {
		std::error_code ec;

		if (!std::filesystem::exists(oldPath, ec) || ec) {
			return;
		}

		ec.clear();
		if (std::filesystem::exists(newPath, ec) || ec) {
			return;
		}

		ec.clear();
		std::filesystem::rename(oldPath, newPath, ec);

		if (!ec) {
			return;
		}

		// rename() can fail if the source and destination are on different
		// filesystems. Fall back to copy + remove.
		ec.clear();
		std::filesystem::copy_file(
			oldPath,
			newPath,
			std::filesystem::copy_options::none,
			ec
		);

		if (ec) {
			return;
		}

		ec.clear();
		std::filesystem::remove(oldPath, ec);
	}

}

bool isValidPresetFile(
    const std::filesystem::path& path
) {
	std::error_code ec;
	const auto status = std::filesystem::symlink_status(path, ec);
	if (
		ec ||
		std::filesystem::is_symlink(status) ||
		!std::filesystem::is_regular_file(status)
	) {
		return false;
	}

    const auto size = std::filesystem::file_size(path, ec);

    if (ec) {
        return false;
    }

    // Minimum:
    // version + checkpointCount + lockCount
    if (size < 12) {
        return false;
    }

    if (size > MAX_PRESET_FILE_SIZE) {
        return false;
    }

    std::ifstream in(path, std::ios::binary);

    if (!in) {
        return false;
    }

    uint32_t version = 0;

    in.read(reinterpret_cast<char*>(&version), sizeof(version));

    if (!in || version != SAVE_FILE_VERSION) {
        return false;
    }

    int32_t checkpointCount = 0;

    in.read(reinterpret_cast<char*>(&checkpointCount), sizeof(checkpointCount));

    if (
        !in ||
        checkpointCount < 0 ||
        checkpointCount > MAX_IMPORTED_CHECKPOINTS
    ) {
        return false;
    }

    const std::uintmax_t checkpointBytes = static_cast<std::uintmax_t>(checkpointCount) * SERIALIZED_GAME_STATE_SIZE;

    const std::uintmax_t lockCountOffset = sizeof(uint32_t) + sizeof(int32_t) + checkpointBytes;

    // Need enough bytes left to even contain lockCount.
    if (lockCountOffset + sizeof(int32_t) > size
    ) {
        return false;
    }

    in.seekg(static_cast<std::streamoff>(lockCountOffset), std::ios::beg);

    if (!in) {
        return false;
    }

    int32_t lockCount = 0;

    in.read(reinterpret_cast<char*>(&lockCount),sizeof(lockCount));

    if (
        !in ||
        lockCount < 0 ||
        lockCount > MAX_IMPORTED_LOCKS
    ) {
        return false;
    }

    const std::uintmax_t expectedSize =
        lockCountOffset
        + sizeof(int32_t)
        + static_cast<std::uintmax_t>(lockCount)
	* sizeof(bool);

    // Reject truncated files AND files with appended garbage.
    if (expectedSize != size) {
        return false;
    }

    return true;
}

bool CheckpointPlugin::createEmptyPresetFile(
	const std::filesystem::path& path
) {
	std::ofstream out(path, std::ios::binary | std::ios::out | std::ios::trunc);
	if (!out.is_open()) {
		cvarManager->log("Freeplay Checkpoint: could not create preset file: " + path.string());
		return false;
	}
	const auto version = SAVE_FILE_VERSION;
	const int32_t zero = 0;
	writePOD(out, version);
	writePOD(out, zero); // checkpoint count
	writePOD(out, zero); // lock count
	if (!out) {
		out.close();
		std::error_code ec;
		std::filesystem::remove(path, ec);
		cvarManager->log("Freeplay Checkpoint: failed to write preset file: " + path.string());
		return false;
	}
	return true;
}

std::filesystem::path CheckpointPlugin::getPresetDirectory() {
	auto presetDirectory =
		gameWrapper->GetDataFolder()
		/ PRESET_FOLDER_NAME
		/ PRESET_SUBFOLDER_NAME;

	std::error_code ec;
	std::filesystem::create_directories(presetDirectory, ec);

	if (ec) {
		cvarManager->log(
			"Freeplay Checkpoint: could not create preset directory: "
			+ presetDirectory.string()
		);
	}

	return presetDirectory;
}

std::filesystem::path CheckpointPlugin::getPresetPath(
	const std::string& filename
) {
	// filename() strips any supplied parent path. cpt_filename should identify
	// a preset inside our preset directory, never an arbitrary filesystem path.
	auto safeFilename = std::filesystem::path(filename).filename();
	if (
		safeFilename.empty() ||
		safeFilename == "." ||
		safeFilename == ".."
	) {
		cvarManager->log("Freeplay Checkpoint: rejected unsafe preset filename: " + filename);
		return getPresetPath(DEFAULT_PRESET_FILE_NAME);
	}
	return getPresetDirectory() / safeFilename;
}

bool isSafePresetFilename(
	const std::string& filename
) {
	if (filename.empty()) {
		return false;
	}

	std::filesystem::path path(filename);

	// It must be exactly one filename, not a path.
	if (
		path.has_root_path() ||
		path.has_parent_path() ||
		path != path.filename() ||
		path.filename().empty() ||
		path == "." ||
		path == ".."
	) {
		return false;
	}

	if (path.extension() != ".data") {
		return false;
	}

	if (!std::all_of(filename.begin(), filename.end(), [](unsigned char c) {
		return
			c >= 32 &&
			c != '@' &&
			c != '&' &&
			c != '|';
		})) {
		return false;
}

	return true;
}

std::filesystem::path CheckpointPlugin::getCurrentPresetPath() {
	auto filename = cvarManager->getCvar("cpt_filename").getStringValue();
	if (!isSafePresetFilename(filename)) {
		cvarManager->log("Freeplay Checkpoint: rejected unsafe preset filename: " + filename);
		return getPresetPath(DEFAULT_PRESET_FILE_NAME);
	}
	return getPresetPath(filename);
}

std::vector<std::string> CheckpointPlugin::getPresetFiles() {
	std::vector<std::string> presets;
	auto presetDirectory = getPresetDirectory();

	std::error_code ec;
	std::filesystem::directory_iterator iterator(presetDirectory, ec);

	if (ec) {
		cvarManager->log("Freeplay Checkpoint: could not enumerate preset directory: " + presetDirectory.string());
		return presets;
	}

	for (const auto& entry : iterator) {
		std::error_code entryError;
		const auto status = entry.symlink_status(entryError);
		if (
			entryError ||
			std::filesystem::is_symlink(status) ||
			!std::filesystem::is_regular_file(status)
		) {
			continue;
		}
		const auto filename = entry.path().filename().string();
		if (!isSafePresetFilename(filename)) {
			continue;
		}
		presets.push_back(filename);
	}

	std::sort(
		presets.begin(),
		presets.end(),
		[](const std::string& left, const std::string& right) {
				std::string lowerLeft = left;
				std::string lowerRight = right;

				std::transform(
					lowerLeft.begin(), lowerLeft.end(), lowerLeft.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); }
				);

				std::transform(
					lowerRight.begin(), lowerRight.end(), lowerRight.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); }
				);

				return lowerLeft < lowerRight;
		}
	);
	return presets;
}

std::string CheckpointPlugin::sanitizePresetName(
    const std::string& rawName
) {
    std::string name = trimWhitespace(rawName);
    if (name.empty()) {
	    return"";
    }

    // Replace filesystem-invalid characters AND characters used by
    // BakkesMod's .set combobox syntax.
    for (char& c : name) {
        const unsigned char uc = static_cast<unsigned char>(c);

        if (
            uc < 32 ||
            c == '<' ||
            c == '>' ||
            c == ':' ||
            c == '"' ||
            c == '/' ||
            c == '\\' ||
            c == '|' ||
            c == '?' ||
            c == '*' ||
            c == '@' ||
            c == '&'
        ) {
            c = '_';
        }
    }

    name = trimWhitespace(name);

    // Windows doesn't permit trailing dots/spaces.
    while (!name.empty() && (name.back() == '.' || name.back() == ' ')) {
        name.pop_back();
    }

    if (name.empty() || name == "." || name == "..") {
        return "";
    }

    std::filesystem::path path(name);

    // Don't double-add .data.
    if (path.extension() == ".data") {
        name = path.stem().string();
    }

    if (name.empty()) {
        return "";
    }

    // Keep filenames comfortably short.

    if (name.size() > MAX_PRESET_NAME_LENGTH) {
        name.resize(MAX_PRESET_NAME_LENGTH);
    }

    // Windows device names are invalid even when an extension is present.
    std::string upperName = name;

    std::transform(
        upperName.begin(),
        upperName.end(),
        upperName.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::toupper(c));
        }
    );

    static const std::unordered_set<std::string> reservedNames = {
        "CON",
        "PRN",
        "AUX",
        "NUL",
        "COM1", "COM2", "COM3", "COM4", "COM5",
        "COM6", "COM7", "COM8", "COM9",
        "LPT1", "LPT2", "LPT3", "LPT4", "LPT5",
        "LPT6", "LPT7", "LPT8", "LPT9"
    };

    if (reservedNames.find(upperName) != reservedNames.end()) {
        name += "_";
    }

    return name + ".data";
}

void CheckpointPlugin::createPreset(
	std::vector<std::string> command
) {
	auto presetNameCvar = cvarManager->getCvar("cpt_new_preset_name");

	if (presetNameCvar.IsNull()) {
		cvarManager->log("Freeplay Checkpoint: cpt_new_preset_name is not registered");
		return;
	}

	std::string filename = sanitizePresetName(presetNameCvar.getStringValue());

	if (filename.empty()) {
		cvarManager->log("Freeplay Checkpoint: preset name cannot be empty");
		return;
	}

	std::filesystem::path presetPath = getPresetPath(filename);
	std::error_code ec;

	if (std::filesystem::exists(presetPath, ec)) {
		// If the file already exists just switch to it, don't overwrite
		if (!isValidPresetFile(presetPath)) {
			cvarManager->log("Freeplay Checkpoint: existing preset file is invalid: " + filename);
			return;
		}
		cvarManager->getCvar("cpt_filename").setValue(filename);
		presetNameCvar.setValue("");
		cvarManager->log("Freeplay Checkpoint: switched to existing preset " + filename);
		return;
	}

	if (ec) {
		cvarManager->log("Freeplay Checkpoint: could not check preset path: " + presetPath.string());
		return;
	}
	// A new preset starts as an empty saved-checkpoint collection. The global quick checkpoint is intentionally preserved.
	// Create a valid empty preset before selecting it.
	if (!createEmptyPresetFile(presetPath)) {
		return;
	}
	// The file now exists, so the cpt_filename callback can safely select/load it.
	cvarManager->getCvar("cpt_filename").setValue(filename);
	presetNameCvar.setValue("");
	writeSettingsFile();

	cvarManager->log("Freeplay Checkpoint: created preset " + filename);
}

void CheckpointPlugin::renamePreset(
	std::vector<std::string> command
) {
	auto renameCvar = cvarManager->getCvar("cpt_rename_preset_name");
	if (renameCvar.IsNull()) {
		return;
	}
	const std::string newFilename = sanitizePresetName(renameCvar.getStringValue());
	if (newFilename.empty()) {
		cvarManager->log("Freeplay Checkpoint: new preset name cannot be empty");
		return;
	}

	const auto oldPath = getCurrentPresetPath();
	const auto newPath = getPresetPath(newFilename);

	if (oldPath.filename() == newPath.filename()) {
		renameCvar.setValue("");
		return;
	}

	std::error_code ec;
	if (!std::filesystem::exists(oldPath, ec) || ec) {
		cvarManager->log("Freeplay Checkpoint: current preset file does not exist");
		return;
	}

	ec.clear();
	if (std::filesystem::exists(newPath, ec)) {
		cvarManager->log("Freeplay Checkpoint: a preset with that name already exists");
		return;
	}

	if (ec) {
		cvarManager->log("Freeplay Checkpoint: could not check rename destination");
		return;
	}

	ec.clear();
	std::filesystem::rename(oldPath, newPath, ec);

	if (ec) {
		cvarManager->log("Freeplay Checkpoint: could not rename preset: " + ec.message());
		return;
	}

	// Updating cpt_filename reloads the same preset under its new name.
	cvarManager->getCvar("cpt_filename").setValue(newFilename);
	renameCvar.setValue("");
	writeSettingsFile();

	cvarManager->log("Freeplay Checkpoint: renamed preset to " + newFilename);
}

void CheckpointPlugin::deletePreset(
	std::vector<std::string> command
) {
	auto allowDeleteCvar = cvarManager->getCvar("cpt_allow_delete_preset");

	if (allowDeleteCvar.IsNull() || !allowDeleteCvar.getBoolValue()) {
		cvarManager->log("Freeplay Checkpoint: preset deletion is not enabled");
		return;
	}

	auto presets = getPresetFiles();

	if (presets.size() <= 1) {
		allowDeleteCvar.setValue(false);
		cvarManager->log("Freeplay Checkpoint: cannot delete the only preset");
		return;
	}

	const std::string currentFilename = std::filesystem::path(
			cvarManager->getCvar("cpt_filename").getStringValue()
		).filename().string();

	auto currentIt = std::find(presets.begin(), presets.end(), currentFilename);

	if (currentIt == presets.end()) {
		allowDeleteCvar.setValue(false);
		cvarManager->log("Freeplay Checkpoint: current preset was not found");
		return;
	}

	const auto currentIndex = static_cast<std::size_t>(std::distance(presets.begin(), currentIt));

	const std::string replacementFilename =
		(currentIndex + 1 < presets.size())
			? presets[currentIndex + 1]
			: presets[currentIndex - 1];

	std::error_code ec;
	if (!std::filesystem::remove(getCurrentPresetPath(), ec) || ec) {
		allowDeleteCvar.setValue(false);
		cvarManager->log("Freeplay Checkpoint: could not delete preset: " + ec.message());
		return;
	}

	allowDeleteCvar.setValue(false);

	// The cpt_filename callback clears preset-specific rewind state and loads
	// the replacement preset. It must NOT clear the global quick checkpoint.
	cvarManager->getCvar("cpt_filename").setValue(replacementFilename);
	writeSettingsFile();

	cvarManager->log("Freeplay Checkpoint: deleted preset " + currentFilename);
}

void CheckpointPlugin::importPresetFile(
    const std::filesystem::path& source
) {
    std::error_code ec;

	if (source.extension() != ".data") {
		cvarManager->log("Freeplay Checkpoint: only .data files can be imported");
		return;
	}

    // Make sure the selected path actually exists and is a regular file.
    if (
        !std::filesystem::exists(source, ec)
        || ec
        || !std::filesystem::is_regular_file(source, ec)
        || ec
    ) {
        cvarManager->log("Freeplay Checkpoint: selected import file does not exist");
        return;
    }

    // Only accept valid Freeplay Checkpoint files.
    if (!isValidPresetFile(source)) {
        cvarManager->log("Freeplay Checkpoint: selected file is not a valid preset");
        return;
    }

    // Turn the external filename into a safe preset filename.
    //
    // Example:
    //     "Flip Resets.data"
    //          ↓
    //     "flip_resets.data"
    std::string filename =sanitizePresetName(source.filename().string());

    if (filename.empty()) {
        cvarManager->log("Freeplay Checkpoint: imported preset has an invalid filename");
        return;
    }

    std::filesystem::path destination = getPresetPath(filename);

    /*
     * Don't overwrite an existing preset.
     *
     * flip_resets.data
     * flip_resets_2.data
     * flip_resets_3.data
     * ...
     */
    if (std::filesystem::exists(destination)) {
        const std::filesystem::path filenamePath(filename);

        const std::string stem = filenamePath.stem().string();

        const std::string extension = filenamePath.extension().string();

        int suffix = 2;

        do {
            filename = stem + "_" + std::to_string(suffix) + extension;
            destination = getPresetPath(filename);
            ++suffix;
        } while (std::filesystem::exists(destination));
    }

    // Copy the external file into our managed preset directory.
    ec.clear();

    std::filesystem::copy_file(
        source,
        destination,
        std::filesystem::copy_options::none,
        ec
    );

    if (ec) {
        cvarManager->log("Freeplay Checkpoint: failed to import preset: "+ ec.message());
        return;
    }

    /*
     * Select the imported preset.
     *
     * This triggers the existing cpt_filename callback, which:
     *   - resets curCheckpoint
     *   - clears preset-specific rewind state
     *   - calls loadCheckpointFile()
     *
     * The global quickCheckpoint is intentionally left untouched.
     */
    cvarManager->getCvar("cpt_filename").setValue(filename);

    // Regenerate the .set file so the new preset appears in the dropdown.
    writeSettingsFile();

    cvarManager->log("Freeplay Checkpoint: imported preset " + filename);
}

void CheckpointPlugin::importPreset(
	std::vector<std::string> command
) {
	// Tell the ImGui file browser to open on its next Display().
	presetFileDialog.Open();

	// The file browser needs a PluginWindow to render inside.
	// Only open it if it is not already open.
	if (!importWindowOpen) {
		cvarManager->executeCommand("togglemenu " + GetMenuName());
	}
}

void CheckpointPlugin::migrateLegacyPresets() {
	auto dataDirectory = gameWrapper->GetDataFolder();
	auto presetDirectory = getPresetDirectory();

	migrateFileIfNeeded(
		dataDirectory / DEFAULT_PRESET_FILE_NAME,
		presetDirectory / DEFAULT_PRESET_FILE_NAME
	);

	auto filenameCvar = cvarManager->getCvar("cpt_filename");

	if (!filenameCvar.IsNull()) {
		auto configuredFilename =
			std::filesystem::path(filenameCvar.getStringValue()).filename();

		if (
			!configuredFilename.empty()
			&& configuredFilename != DEFAULT_PRESET_FILE_NAME
		) {
			migrateFileIfNeeded(
				dataDirectory / configuredFilename,
				presetDirectory / configuredFilename
			);
		}
	}
}

void CheckpointPlugin::ensureDefaultPreset() {
	auto presets = getPresetFiles();

	if (!presets.empty()) {
		auto currentFilename = std::filesystem::path(
				cvarManager
					->getCvar("cpt_filename")
					.getStringValue()
			).filename().string();
		auto currentPath = getPresetPath(currentFilename);
		std::error_code ec;
		if (
			currentFilename.empty()
			|| !std::filesystem::exists(currentPath, ec)
			|| ec
		) {
			cvarManager->getCvar("cpt_filename").setValue(presets.front());
		}
		return;
	}

	const auto defaultPath = getPresetPath(DEFAULT_PRESET_FILE_NAME);
	// Create a valid empty preset before selecting it.
	if (!createEmptyPresetFile(defaultPath)) {
		return;
	}

	cvarManager->getCvar("cpt_filename").setValue(DEFAULT_PRESET_FILE_NAME);
}

void CheckpointPlugin::clonePreset(
	std::vector<std::string> command
) {
	const auto sourcePath = getCurrentPresetPath();
	if (!isValidPresetFile(sourcePath)) {
		cvarManager->log("Freeplay Checkpoint: current preset cannot be cloned because its file is invalid");
		return;
	}
	std::string sourceStem = sourcePath.stem().string();

	// Leave room for "_copy", numeric suffixes, and the extension.
	if (sourceStem.size() > MAX_PRESET_NAME_LENGTH - 32) {
		sourceStem.resize(MAX_PRESET_NAME_LENGTH - 32);
	}

	std::string filename = sanitizePresetName(sourceStem + "_copy");
	auto destinationPath = getPresetPath(filename);
	std::error_code ec;
	int suffix = 2;
	while (true) {
		ec.clear();
		const bool exists = std::filesystem::exists(destinationPath, ec);
		if (ec) {
			cvarManager->log("Freeplay Checkpoint: could not check clone destination: " + ec.message());
			return;
		}
		if (!exists) {
			break;
		}
		filename = sanitizePresetName(sourceStem + "_copy_" + std::to_string(suffix++));
		destinationPath = getPresetPath(filename);
	}
	ec.clear();
	std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::none,ec);
	if (ec) {
		cvarManager->log("Freeplay Checkpoint: could not clone preset: " + ec.message());
		return;
	}
	// The clone now exists, so it is safe for the filename callback to load it.
	cvarManager->getCvar("cpt_filename").setValue(filename);
	writeSettingsFile();
	cvarManager->log("Freeplay Checkpoint: cloned preset as " + filename);
}
