/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once


#include "utils/parser.h"
#include "state.h"
#include "IMGUI/imgui.h"
#include "IMGUI/imfilebrowser.h"
#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"

#include "version.h"

#include <filesystem>

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);
inline constexpr uint32_t SAVE_FILE_VERSION = 1;
constexpr float MAX_DODGE_TIME = 1.2f;

template<typename T>
void writePOD(std::ostream& out, const T& t) {
	out.write(reinterpret_cast<const char*>(&t), sizeof(T));
}

template<typename T>
void readPOD(std::istream& in, T& t) {
	T temp;
	in.read(reinterpret_cast<char*>(&temp), sizeof(T));
	if (in.eof()) {
		return;
	}
	t = temp;
}

// Rotator uses ints instead of floats.  Floats are better.
struct Rot {
	float Pitch, Yaw, Roll;
};

// TODO: make this a full-on "RewindMode" class with functions for operations
struct RewindState {
	bool atCheckpoint = false;
	float virtualTimeOffset = 0; // Delta from end of buffer to "now"
	bool justDeletedCheckpoint = false;
	bool justLoadedQuickCheckpoint = false;
	float holdingFor = 0;
	bool deleting = false;
	int buttonsDown = 0x7f;
};

class CheckpointPlugin
	: public BakkesMod::Plugin::BakkesModPlugin,
      public BakkesMod::Plugin::PluginWindow
{
	//Boilerplate
	virtual void onLoad();
	void copyShot(std::vector<std::string> command);
	void mirrorState(std::vector<std::string> command);
	void deleteAllCheckpoints(std::vector<std::string> command);
	void randCheckpoint(std::vector<std::string> command);
	void pasteShot(std::vector<std::string> command);
	void freezeBallUnfreezeCar(std::vector<std::string> command);
	virtual void onUnload();
	void doCheckpoint(std::vector<std::string> command);
	void lockCheckpoint(std::vector<std::string> command);
	void prevCheckpoint(std::vector<std::string> command);
	void nextCheckpoint(std::vector<std::string> command);

	void Render() override;

	std::string GetMenuName() override;
	std::string GetMenuTitle() override;

	void SetImGuiContext(uintptr_t ctx) override;

	bool ShouldBlockInput() override;
	bool IsActiveOverlay() override;

	void OnOpen() override;
	void OnClose() override;

private:
	RewindState rewindState;
	std::vector<GameState> history;
	GameState latest;
	std::vector<GameState> checkpoints;
	std::vector<bool> locks;
	size_t curCheckpoint = 0;
	bool rewindMode = false;
	bool freezeBall = false;
	float dodgeExpiration = 0;
	bool hasQuickCheckpoint = false;
	GameState quickCheckpoint;
	float lastRecordTime = 0;
	float lastRewindTime = 0;
	std::vector<GameState> gameHistory;
	int carNum = 0;
	bool playingFromCheckpoint = false;

	// Settings:
	bool deleteFutureHistory = false;
	bool ignorePNNotFrozen = false;
	bool ignorePrev = false;
	bool ignoreNext = false;
	bool ignoreFreezeBall = false;
	bool disableTraining = false;
	bool disableWorkshop = false;
	bool debug = false;
	bool resetOnGoal = false;
	bool resetOnBallGround = false;
	bool nextInsteadOfReset = false;
	bool mirrorLoads = false;
	bool randomizeLoads = false;
	bool showBoost = false;
	bool disableDeleteOnDpadDown = false;

	void addBind(std::string key, std::string cmd);
	void removeBind(std::string key, std::string cmd);
	void OnPreAsync(std::string funcName);
	void registerVarianceCVars();
	void registerBindingCVars();
	void captureBindKey(std::vector<std::string> params);
	void removeBindKeys(std::vector<std::string> params);
	void applyBindKeys(std::vector<std::string> params);
	void resetDefaultBindKeys(std::vector<std::string> params);
	GameState applyVariance(GameState& s);
	bool rewind(ServerWrapper sw);

	// Presets
	std::filesystem::path getPresetDirectory();
	std::filesystem::path getPresetPath(const std::string& filename);
	std::filesystem::path getCurrentPresetPath();

	std::vector<std::string> getPresetFiles();
	bool createEmptyPresetFile(const std::filesystem::path& path);
	std::string sanitizePresetName(const std::string& rawName);

	void createPreset(std::vector<std::string> command);
	void clonePreset(std::vector<std::string> command);
	void renamePreset(std::vector<std::string> command);
	void deletePreset(std::vector<std::string> command);
	void importPreset(std::vector<std::string> command);
	void importPresetFile(const std::filesystem::path& source);

	ImGui::FileBrowser presetFileDialog{
		ImGuiFileBrowserFlags_CloseOnEsc |
		ImGuiFileBrowserFlags_SingleClickDir |
		ImGuiFileBrowserFlags_SortIgnoreCase
	};
	bool importWindowOpen = false;

	void migrateLegacyPresets();
	void ensureDefaultPreset();

	// Checkpoint persistence
	void loadCheckpointFile();
	void saveCheckpointFile();

	void Render(CanvasWrapper canvas);
	void record(ServerWrapper sw);
	void loadLatestCheckpoint();
	void loadCurCheckpoint();
	void loadRandomCheckpoint();
	void loadGameState(const GameState&);
	void log(std::string s);
	void boolvar(std::string name, std::string desc, bool* var);
	std::unique_ptr<GameState> getReplayGameState();
	void setFrozen(bool car, bool ball);
	void writeSettingsFile();
	bool enabled();
	bool enabledLoads();
};

bool isValidPresetFile(const std::filesystem::path& path);
