/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "utils/parser.h"
#include "state.h"

#include "version.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);
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

class CheckpointPlugin : public BakkesMod::Plugin::BakkesModPlugin {
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
