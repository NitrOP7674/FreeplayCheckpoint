/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "pch.h"
#include "CheckpointPlugin.h"
#include "utils/parser.h"
#include <functional>

using namespace std::placeholders;

void CheckpointPlugin::removeBindKeys(std::vector<std::string> params) {
	removeBind(cvarManager->getCvar("cpt_freeze_key").getStringValue(), "cpt_freeze");
	removeBind(cvarManager->getCvar("cpt_do_checkpoint_key").getStringValue(), "cpt_do_checkpoint");
	removeBind(cvarManager->getCvar("cpt_prev_checkpoint_key").getStringValue(), "cpt_prev_checkpoint");
	removeBind(cvarManager->getCvar("cpt_next_checkpoint_key").getStringValue(), "cpt_next_checkpoint");
	removeBind(cvarManager->getCvar("cpt_freeze_ball_key").getStringValue(), "cpt_freeze_ball");
	removeBind(cvarManager->getCvar("cpt_mirror_state_key").getStringValue(), "cpt_mirror_state");
}

void CheckpointPlugin::applyBindKeys(std::vector<std::string> params) {
	addBind(cvarManager->getCvar("cpt_freeze_key").getStringValue(), "cpt_freeze");
	addBind(cvarManager->getCvar("cpt_do_checkpoint_key").getStringValue(), "cpt_do_checkpoint");
	addBind(cvarManager->getCvar("cpt_prev_checkpoint_key").getStringValue(), "cpt_prev_checkpoint");
	addBind(cvarManager->getCvar("cpt_next_checkpoint_key").getStringValue(), "cpt_next_checkpoint");
	addBind(cvarManager->getCvar("cpt_freeze_ball_key").getStringValue(), "cpt_freeze_ball");
	addBind(cvarManager->getCvar("cpt_mirror_state_key").getStringValue(), "cpt_mirror_state");
}

void CheckpointPlugin::resetDefaultBindKeys(std::vector<std::string> params) {
	cvarManager->getCvar("cpt_freeze_key").setValue("XboxTypeS_RightThumbStick");
	cvarManager->getCvar("cpt_do_checkpoint_key").setValue("XboxTypeS_Back");
	cvarManager->getCvar("cpt_prev_checkpoint_key").setValue("XboxTypeS_DPad_Left");
	cvarManager->getCvar("cpt_next_checkpoint_key").setValue("XboxTypeS_DPad_Right");
	cvarManager->getCvar("cpt_freeze_ball_key").setValue("XboxTypeS_DPad_Up");
	cvarManager->getCvar("cpt_mirror_state_key").setValue("XboxTypeS_DPad_Down");
}

static const std::vector<std::string> KEY_LIST = {
	"XboxTypeS_A", "XboxTypeS_B", "XboxTypeS_X", "XboxTypeS_Y", "XboxTypeS_RightShoulder", "XboxTypeS_RightTrigger",
	"XboxTypeS_RightThumbStick", "XboxTypeS_LeftShoulder", "XboxTypeS_LeftTrigger", "XboxTypeS_LeftThumbStick", "XboxTypeS_Start",
	"XboxTypeS_Back", "XboxTypeS_DPad_Up", "XboxTypeS_DPad_Left", "XboxTypeS_DPad_Right", "XboxTypeS_DPad_Down" };

void CheckpointPlugin::registerBindingCVars() {
	cvarManager->registerCvar("cpt_freeze_key", "XboxTypeS_RightThumbStick", "Key to bind cpt_freeze to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_do_checkpoint_key", "XboxTypeS_Back", "Key to bind cpt_do_checkpoint to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_prev_checkpoint_key", "XboxTypeS_DPad_Left", "Key to bind cpt_prev_checkpoint to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_next_checkpoint_key", "XboxTypeS_DPad_Right", "Key to bind cpt_next_checkpoint to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_freeze_ball_key", "XboxTypeS_DPad_Up", "Key to bind cpt_freeze_ball to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_mirror_state_key", "XboxTypeS_DPad_Down", "Key to bind cpt_mirror_state to on cpt_apply_bindings");
	cvarManager->registerNotifier("cpt_remove_bindings", bind(&CheckpointPlugin::removeBindKeys, this, _1),
		"Removes the configured button bindings for the Freeplay Checkpoint plugin", PERMISSION_ALL);
	cvarManager->registerNotifier("cpt_apply_bindings", bind(&CheckpointPlugin::applyBindKeys, this, _1),
		"Applys the configured button bindings for the Freeplay Checkpoint plugin", PERMISSION_ALL);
	cvarManager->registerNotifier("cpt_reset_default_bindings", bind(&CheckpointPlugin::resetDefaultBindKeys, this, _1),
		"Resets bindings to the default values", PERMISSION_ALL);
	cvarManager->registerNotifier("cpt_capture_key", bind(&CheckpointPlugin::captureBindKey, this, _1),
		"Captures currently pressed key and stores in parameter (cvar)", PERMISSION_ALL);
}


void CheckpointPlugin::captureBindKey(std::vector<std::string> params) {
	if (params.size() != 2) {
		cvarManager->log("cpt_capture_key: error: requires exactly 1 param.");
		return;
	}
	std::string command = params.back();
	auto cvar = cvarManager->getCvar(command + "_key");
	if (cvar.IsNull()) {
		cvarManager->log("cpt_capture_key: error: unknown cvar to capture to.");
		return;
	}
	auto oldKey = cvar.getStringValue();
	removeBind(oldKey, command);
	for (auto key : KEY_LIST) {
		if (gameWrapper->IsKeyPressed(gameWrapper->GetFNameIndexByString(key))) {
			cvar.setValue(key);
			log("cpt_capture_key: " + command + " = " + key);
			break;
		}
	}
	applyBindKeys(std::vector<std::string>());
}

void CheckpointPlugin::addBind(std::string key, std::string cmd) {
	std::string old = cvarManager->getBindStringForKey(key);
	std::vector<std::string> cmds;

	for (char* token = strtok(const_cast<char*>(old.c_str()), ";");
		token != nullptr;
		token = strtok(nullptr, ";"))
	{
		auto tok = std::string(token);
		trim(tok);
		if (tok != "") {
			cmds.push_back(std::string(tok));
		}
	}
	if (std::find(cmds.begin(), cmds.end(), cmd) == cmds.end()) {
		cmds.push_back(trim(cmd));
	}
	std::stringstream s;
	std::copy(cmds.begin(), cmds.end() - 1, std::ostream_iterator<std::string>(s, ";"));
	s << cmds.back();
	cvarManager->setBind(key, s.str());
}

void CheckpointPlugin::removeBind(std::string key, std::string cmd) {
	std::string old = cvarManager->getBindStringForKey(key);
	std::vector<std::string> cmds;
	log("removing " + cmd + " from " + key);
	for (char* token = strtok(const_cast<char*>(old.c_str()), ";");
		token != nullptr;
		token = strtok(nullptr, ";"))
	{
		auto tok = std::string(token);
		trim(tok);
		if (tok != "" && tok != cmd) {
			log("pushing " + tok);
			cmds.push_back(std::string(tok));
		}
	}
	if (cmds.size() == 0) {
		cvarManager->executeCommand("unbind " + key);
		return;
	}
	std::stringstream s;
	std::copy(cmds.begin(), cmds.end() - 1, std::ostream_iterator<std::string>(s, ";"));
	s << cmds.back();
	log("setting " + key + " to " + s.str());
	cvarManager->setBind(key, s.str());
}
