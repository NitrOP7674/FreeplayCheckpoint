/*
 * Copyright (c) 2021, Douglas Fawley
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "pch.h"
#include "CheckpointPlugin.h"

#include "bakkesmod/wrappers/GameEvent/TutorialWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarComponent/BoostWrapper.h"
#include "bakkesmod/wrappers/GameObject/CarWrapper.h"
#include "bakkesmod/wrappers/GameObject/BallWrapper.h"

#include "bakkesmod/wrappers/ArrayWrapper.h"

using namespace std::placeholders;

BAKKESMOD_PLUGIN(CheckpointPlugin, "Freeplay Checkpoint", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

// TODO: make these configurable?
static const float snapshotInterval = 0.010f; //time (s) between updates
static const int maxHistory = 3000; // 3000*0.010s = 30s
static const std::string SAVE_FILE_NAME = "freeplaycheckpoint.data";

// Prevent loading an unknown version's save file.
static const uint32_t SAVE_FILE_VERSION = 1;

void CheckpointPlugin::loadCheckpointFile() {
	std::ifstream in(gameWrapper->GetDataFolder() / SAVE_FILE_NAME, std::ios::binary);
	uint32_t version;
	readPOD(in, version);
	if (version != SAVE_FILE_VERSION) {
		in.close();
		return;
	}
	int32_t numSaves;
	readPOD(in, numSaves);
	for (int32_t i = 0; i < numSaves; i++) {
		checkpoints.emplace_back(in);
	}
	in.close();
}

void CheckpointPlugin::saveCheckpointFile() {
	std::ofstream out(gameWrapper->GetDataFolder() / SAVE_FILE_NAME, std::ios::binary | std::ios::out | std::ios::trunc);
	auto ver = SAVE_FILE_VERSION;
	writePOD(out, ver);
	auto size = int32_t(checkpoints.size());
	writePOD(out, size);
	for (auto& fav : checkpoints) {
		fav.write(out);
	}
	out.close();
}

void CheckpointPlugin::log(std::string s) {
	if (debug) {
		cvarManager->log(s);
	}
}

void CheckpointPlugin::onLoad()
{
	loadCheckpointFile();

	auto cleanHistory = cvarManager->registerCvar(
		"cpt_clean_history", "0", "If set, deletes history after the current point when exiting rewind mode", true, true, 0, true, 1, true);
	cleanHistory.addOnValueChanged([this](std::string old, CVarWrapper now) { deleteFutureHistory = now.getBoolValue(); });
	cleanHistory.notify();

	auto debugCV = cvarManager->registerCvar(
		"cpt_debug", "0", "If set, render debugging info", true, true, 0, true, 1, true);
	debugCV.addOnValueChanged([this](std::string old, CVarWrapper now) { debug = now.getBoolValue(); });
	debugCV.notify();

	registerVarianceCVars();

	// Continually call OnPreAsync.
	gameWrapper->HookEvent("Function PlayerController_TA.Driving.PlayerMove", bind(&CheckpointPlugin::OnPreAsync, this, _1));

	// Disable rewind mode if the user resets freeplay.
	gameWrapper->HookEvent("Function GameEvent_TA.Countdown.BeginState", [this](std::string eventName) {
		rewindMode = false;
		dodgeExpiration = 0.0;
	});

	// Enter rewind mode.
	cvarManager->registerNotifier("cpt_freeze", [this](std::vector<std::string> command) {
		if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused() || history.size() == 0 || rewindMode) {
			return;
		}
		latest = history.back();
		loadGameState(latest);
	}, "Activates rewind mode or saves the current position while in rewind mode", PERMISSION_FREEPLAY);

	// If in play mode, load the latest checkpoint / quick checkpoint.
	// If in rewind mode, add a checkpoint or delete the current checkpoint.
	// TODO: different button for delete?
	cvarManager->registerNotifier("cpt_do_checkpoint", [this](std::vector<std::string> command) {
		if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused()) {
			return;
		}
		if (!rewindMode) {
			loadLatestCheckpoint();
			return;
		}
		hasQuickCheckpoint = false;
		if (atCheckpoint) { // Delete the current checkpoint we are at.
			log("at cpt; erasing: " + std::to_string(curCheckpoint));
			checkpoints.erase(checkpoints.begin() + curCheckpoint);
			curCheckpoint = std::min(curCheckpoint, checkpoints.size() - 1);
			atCheckpoint = false;
			justDeletedCheckpoint = true;
			return;
		}
		// Add a new checkpoint here.
		log("adding checkpoint " + std::to_string(checkpoints.size()));
		checkpoints.push_back(latest);
		curCheckpoint = checkpoints.size() - 1;
		atCheckpoint = true;
		justDeletedCheckpoint = false;
	}, "Sets the car to the latest checkpoint", PERMISSION_FREEPLAY);

	// Go to previous checkpoint.
	cvarManager->registerNotifier("cpt_prev_checkpoint", [this](std::vector<std::string> command) {
		if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused() || checkpoints.size() == 0) {
			return;
		}
		if (!justDeletedCheckpoint) {
			// If you just deleted a checkpoint, prev should go one prior to
			// the deleted one (the current one).
			curCheckpoint = std::clamp<size_t>(curCheckpoint, 1, checkpoints.size()) - 1;
		}
		loadCurCheckpoint();
	}, "Sets the car to the previous checkpoint", PERMISSION_FREEPLAY);

	// Go to next checkpoint.
	cvarManager->registerNotifier("cpt_next_checkpoint", [this](std::vector<std::string> command) {
		if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused() || checkpoints.size() == 0) {
			return;
		}
		curCheckpoint = std::clamp<size_t>(curCheckpoint + 1, 0, checkpoints.size() - 1);
		loadCurCheckpoint();
	}, "Sets the car to the next checkpoint", PERMISSION_FREEPLAY);

	// Add default bindings.
	cvarManager->registerCvar("cpt_freeze_key", "XboxTypeS_RightThumbStick", "Key to bind cpt_freeze to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_do_checkpoint_key", "XboxTypeS_Back", "Key to bind cpt_do_checkpoint to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_prev_checkpoint_key", "XboxTypeS_DPad_Left", "Key to bind cpt_prev_checkpoint to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_next_checkpoint_key", "XboxTypeS_DPad_Right", "Key to bind cpt_next_checkpoint to on cpt_apply_bindings");
	cvarManager->registerNotifier("cpt_remove_bindings", bind(&CheckpointPlugin::removeBindKeys, this, _1),
		"Removes the configured button bindings for the Freeplay Checkpoint plugin", PERMISSION_ALL);
	cvarManager->registerNotifier("cpt_apply_bindings", bind(&CheckpointPlugin::applyBindKeys, this, _1),
		"Applys the configured button bindings for the Freeplay Checkpoint plugin", PERMISSION_ALL);
	cvarManager->registerNotifier("cpt_capture_key", bind(&CheckpointPlugin::captureBindKey, this, _1),
		"Captures currently pressed key and stores in parameter (cvar)", PERMISSION_ALL);

	// Draw the checkpoint or notification about checkpoint deletion.
	gameWrapper->RegisterDrawable(std::bind(&CheckpointPlugin::Render, this, std::placeholders::_1));
}

void CheckpointPlugin::registerVarianceCVars() {
	cvarManager->registerCvar("cpt_variance_car_dir", "0", "If set, randomly vary car's direction when resuming", true, true, 0, true, 30, true);
	cvarManager->registerCvar("cpt_variance_car_spd", "0", "If set, randomly vary car's speed when resuming", true, true, 0, true, 50, true);
	cvarManager->registerCvar("cpt_variance_ball_dir", "0", "If set, randomly vary ball's direction when resuming", true, true, 0, true, 30, true);
	cvarManager->registerCvar("cpt_variance_ball_spd", "0", "If set, randomly vary ball's speed when resuming", true, true, 0, true, 50, true);
	cvarManager->registerCvar("cpt_variance_tot", "0", "Total variance applied to all factors (range)", true, true, 0, true, 50, true);
}

void CheckpointPlugin::onUnload() {
	saveCheckpointFile();
}

void CheckpointPlugin::loadLatestCheckpoint() {
	if (hasQuickCheckpoint) {
		log("loading quick checkpoint");
		loadGameState(quickCheckpoint);
		hasQuickCheckpoint = true;
		justLoadedQuickCheckpoint = true;
		return;
	}
	if (checkpoints.size() > 0) {
		log("loading checkpoint " + std::to_string(curCheckpoint));
		loadCurCheckpoint();
		return;
	}
	log("no checkpoint to load");
	virtualTimeOffset = 0;
	holdingFor = 0;
}

void CheckpointPlugin::loadCurCheckpoint() {
	loadGameState(checkpoints.at(curCheckpoint));
	atCheckpoint = true;
}

void CheckpointPlugin::loadGameState(GameState &state) {
	latest = state;
	ServerWrapper sw = gameWrapper->GetGameEventAsServer();
	state.apply(sw);
	virtualTimeOffset = 0;
	holdingFor = 0;
	rewindMode = true;
	atCheckpoint = false;
	hasQuickCheckpoint = false;
	justDeletedCheckpoint = false;
	justLoadedQuickCheckpoint = false;
}

void CheckpointPlugin::OnPreAsync(std::string funcName)
{
	if (!gameWrapper->IsInFreeplay()) {
		return;
	}
	ServerWrapper sw = gameWrapper->GetGameEventAsServer();
	if (sw.GetBall().IsNull() || sw.GetGameCar().IsNull()) {
		return;
	}

	if (rewindMode) {
		rewind(sw);
		if (rewindMode) {
			latest.apply(sw);
		} else {
			// Exited rewind mode.
			applyVariance(latest).apply(sw);
		}
	} else {
		record(sw);
	}
}

void CheckpointPlugin::rewind(ServerWrapper sw) {
	ControllerInput ci = sw.GetCars().Get(0).GetInput();

	static float lastRewindTime = 0.0f;
	float currentTime = sw.GetSecondsElapsed();
	float elapsed = std::min(currentTime - lastRewindTime, 0.03f);
	if (elapsed < 0) {
		elapsed = 0.01f;
	}
	if (elapsed < 0.01f) {
		return;
	}
	lastRewindTime = currentTime;

	// See if we should exit rewind mode due to input.
	if ((abs(ci.Throttle) > 0.1 || abs(ci.Roll) > 0.1 || ci.Handbrake || ci.Jump || ci.ActivateBoost || ci.HoldingBoost) ||
		((atCheckpoint || justLoadedQuickCheckpoint) && abs(ci.Steer) >= .05)) {
		log("resuming...");
		rewindMode = false;
		lastRecordTime = currentTime;
		dodgeExpiration = (latest.hasDodge && latest.lastJumped != -1) ? (currentTime + MAX_DODGE_TIME - latest.lastJumped) : 0;
		if (!atCheckpoint) {
			log("quick checkpoint taken");
			hasQuickCheckpoint = true;
			quickCheckpoint = latest;
			if (deleteFutureHistory) {
				size_t current = std::clamp<size_t>(
					history.size() - 1 + size_t(ceil(virtualTimeOffset / snapshotInterval)),
					0, history.size() - 1);
				history.erase(history.begin() + current, history.end());
			}
		}
		return;
	}

	// Determine how much to rewind / advance time.
	if (abs(ci.Steer) < .05f) { // Ignore slight input; keep current game state.
		return;
	}
	if (ci.Steer < -.95 && holdingFor <= 0) {
		holdingFor -= elapsed;
	} else if (ci.Steer > .95 && holdingFor >= 0) {
		holdingFor += elapsed;
	} else {
		holdingFor = 0;
	}
	float factor = std::clamp((abs(holdingFor)-1) * 2, 2.0f, 5.0f);

	// How much (in seconds) to move "current" (positive or negative)
	float deltaElapsed = factor * elapsed * ci.Steer; // full left = 2-5 seconds/second

	virtualTimeOffset = std::clamp(
		virtualTimeOffset + deltaElapsed, -snapshotInterval * history.size(), .0f);
	size_t current = std::clamp<size_t>(
		history.size() + size_t(ceil(virtualTimeOffset / snapshotInterval)),
		0, history.size() - 1);
	latest = history.at(current);
}

void CheckpointPlugin::record(ServerWrapper sw)
{
	float currentTime = sw.GetSecondsElapsed();
	float elapsed = currentTime - lastRecordTime;
	if (elapsed < snapshotInterval) {
		return;
	}
	lastRecordTime = currentTime;
	if (dodgeExpiration != 0) {
		// If the timer expires or if the player double-jumps or gets a reset,
		// clear the jump timer so we don't take the player's dodge.
		auto c = sw.GetGameCar();
		if (currentTime > dodgeExpiration ||
			c.GetbDoubleJumped() ||
			c.GetNumWheelContacts() == 4) {
			c.SetbJumped(true);
			c.SetbDoubleJumped(true);
			dodgeExpiration = 0;
		}
	}

	// TODO: use a ring buffer?
	if (history.size() == maxHistory) {
		history.erase(history.begin());
	}
	history.emplace_back(sw);
}

void show(CanvasWrapper canvas, Vector2 *loc, std::string s) {
	static const float scale = 1.5f;
	canvas.SetPosition(*loc);
	canvas.DrawString(s, scale, scale);
	loc->Y += 20;
}

void CheckpointPlugin::Render(CanvasWrapper canvas) {
	if (!gameWrapper->IsInFreeplay() || gameWrapper->IsPaused()) {
		return;
	}
	if (!debug && (!rewindMode || (!atCheckpoint && !justDeletedCheckpoint))) {
		return;
	}
	auto screenSize = canvas.GetSize();
	if (debug) {
		canvas.SetColor('\xff', '\xff', '\xff', '\xdc');
		Vector2 loc = { (int)(screenSize.X * 0.08), (int)(screenSize.Y * 0.08) };
		show(canvas, &loc, "rewindMode: " + std::to_string(rewindMode));
		show(canvas, &loc, "atCheckpoint: " + std::to_string(atCheckpoint));
		show(canvas, &loc, "justDeletedCheckpoint: " + std::to_string(justDeletedCheckpoint));
		show(canvas, &loc, "justLoadedQuickCheckpoint: " + std::to_string(justLoadedQuickCheckpoint));
		show(canvas, &loc, "hasQuickCheckpoint: " + std::to_string(hasQuickCheckpoint));
		show(canvas, &loc, "virtualTimeOffset: " + std::to_string(virtualTimeOffset));
		size_t current = std::clamp<size_t>(
			history.size() + size_t(ceil(virtualTimeOffset / snapshotInterval)),
			0, history.size() - 1);
		show(canvas, &loc, "current: " + std::to_string(current));
		if (!rewindMode) {
			return;
		}
	}
	Vector2 loc = { (int)(screenSize.X * 0.80), (int)(screenSize.Y * 0.08) };
	if (justDeletedCheckpoint) {
		loc.X = int(screenSize.X * .70);
		canvas.SetPosition(loc);
		canvas.SetColor('\xff', '\xff', '\xff', '\xdc');
		canvas.DrawString("Checkpoint deleted!", 5, 5);
		return;
	}
	if (atCheckpoint) {
		canvas.SetPosition(loc + Vector2{ 5,5 });
		canvas.SetColor(0, 0, 0, 100);
		canvas.DrawString(std::to_string(curCheckpoint + 1) + " | " + std::to_string(checkpoints.size()), 6, 6);
		canvas.SetPosition(loc);
		canvas.SetColor('\xff', '\xff', '\xff', '\xdc');
		canvas.DrawString(std::to_string(curCheckpoint + 1) + " | " + std::to_string(checkpoints.size()), 6, 6);
	}
}
