#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include "bakkesmod/plugin/bakkesmodplugin.h"

#include <Windows.h>
#include <shellapi.h>

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>

#include "imgui/imgui.h"

template<typename S, typename... Args>
void LOG(const S& format_str, Args&&... args);

void close_opened_menus(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gw);
void reopen_closed_menus(std::shared_ptr<CVarManagerWrapper>  cvarManager);
void AlignForWidth(float width, float alignment = 0.5f);
void AddUnderline(ImColor col_);
void TextURL(const char* text_, const char* URL_, uint8_t SameLineBefore_, uint8_t SameLineAfter_);