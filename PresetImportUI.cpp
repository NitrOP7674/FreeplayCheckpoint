/*
* Copyright (c) 2026
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "pch.h"
#include "CheckpointPlugin.h"

std::string CheckpointPlugin::GetMenuName() {
    return "freeplaycheckpoint_import";
}

std::string CheckpointPlugin::GetMenuTitle() {
    return "Import Freeplay Checkpoint Preset";
}

void CheckpointPlugin::SetImGuiContext(uintptr_t ctx) {
    ImGui::SetCurrentContext(reinterpret_cast<ImGuiContext*>(ctx));
}

bool CheckpointPlugin::ShouldBlockInput() {
    return ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
}

bool CheckpointPlugin::IsActiveOverlay() {
    return true;
}

void CheckpointPlugin::OnOpen() {
    importWindowOpen = true;
}

void CheckpointPlugin::OnClose() {
    importWindowOpen = false;
}

void CheckpointPlugin::Render() {
    presetFileDialog.Display();
    if (presetFileDialog.HasSelected()) {
        auto selected = presetFileDialog.GetSelected();
        presetFileDialog.ClearSelected();
        importPresetFile(selected);
        cvarManager->executeCommand("togglemenu " + GetMenuName());
    }
}