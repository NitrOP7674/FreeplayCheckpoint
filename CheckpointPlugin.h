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

#include "utils.h"
#include "version.h"

constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class CheckpointPlugin: public BakkesMod::Plugin::BakkesModPlugin {
	//Boilerplate
	virtual void onLoad();
	void removeBindKeys(std::vector<std::string> params);
	void applyBindKeys(std::vector<std::string> params);
	virtual void onUnload();

	private:

	std::vector<GameState> history;
	float virtualTimeOffset = 0; // Delta from end of buffer to "now"
	GameState latest;
	std::vector<GameState> checkpoints;
	size_t curCheckpoint = 0;
	bool rewindMode = false;
	bool atCheckpoint = false;
	bool justDeletedCheckpoint = false;
	float dodgeExpiration = 0;
	bool hasQuickCheckpoint = false;
	bool justLoadedQuickCheckpoint = false;
	bool debug = false;
	GameState quickCheckpoint;
	float lastRecordTime = 0;
	float holdingFor = 0;

	// Settings:
	bool deleteFutureHistory = false;
	
	void addBind(std::string key, std::string cmd);
	void removeBind(std::string key, std::string cmd);
	void OnPreAsync(std::string funcName);
	void registerVarianceCVars();
	GameState applyVariance(GameState& s);
	void rewind(ServerWrapper sw);
	void loadCheckpointFile();
	void saveCheckpointFile();
	void Render(CanvasWrapper canvas);
	void record(ServerWrapper sw);
	void loadLatestCheckpoint();
	void loadCurCheckpoint();
	void loadGameState(GameState& state);
	void log(std::string s);
};

