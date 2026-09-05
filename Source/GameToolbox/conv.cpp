/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License    
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*************************************************************************/

#include "conv.h"
#include "GameObject.h"
#include "external/fast_float.h"
#include "math/MathUtil.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <utility>
#include "log.h"

bool _showDebugImgui = false;

USING_NS_AX;

ax::BlendFunc GameToolbox::getBlending()
{
	return BlendFunc::ADDITIVE;
}

std::string GameToolbox::xorFunction(const std::string& str, int key) {
    std::string result;
    result.reserve(str.length());

    for (char c : str) {
        result += c ^ key;
    }

    return result;
}

int GameToolbox::stoi(const std::string_view s)
{
	int ret = 0;
	std::from_chars(s.data(), s.data() + s.size(), ret);
	return ret;
}

float GameToolbox::stof(const std::string_view s)
{
	float ret = 0.0f;
	fast_float::from_chars(s.data(), s.data() + s.size(), ret);
	return ret;
}

std::vector<std::string> GameToolbox::splitByDelim(const std::string& str, char delim)
{
	std::vector<std::string> tokens;
	size_t pos = 0;
	size_t len = str.length();
	tokens.reserve(len / 2); // allocate memory for expected number of tokens

	while (pos < len)
	{
		size_t end = str.find_first_of(delim, pos);
		if (end == std::string::npos)
		{
			tokens.emplace_back(str.substr(pos));
			break;
		}
		tokens.emplace_back(str.substr(pos, end - pos));
		pos = end + 1;
	}

	return tokens;
}

std::vector<std::string_view> GameToolbox::splitByDelimStringView(std::string_view str, char delim)
{
	std::vector<std::string_view> tokens;
	size_t pos = 0;
	size_t len = str.length();

	while (pos < len)
	{
		size_t end = str.find(delim, pos);
		if (end == std::string_view::npos)
		{
			tokens.emplace_back(str.substr(pos));
			break;
		}
		tokens.emplace_back(str.substr(pos, end - pos));
		pos = end + 1;
	}

	return tokens;
}

void GameToolbox::applyHSV(GDHSV const& hsv, Color3B* color)
{
	ax::HSV objhsv = ax::HSV(*color);
	objhsv.h += hsv.h;
	if (hsv.sChecked)
		objhsv.s += hsv.s;
	else
		objhsv.s *= hsv.s;
	if (hsv.vChecked)
		objhsv.v += hsv.v;
	else
		objhsv.v *= hsv.v;

	*color = GameToolbox::hsvToRgb(objhsv);
}

ax::Color3B GameToolbox::hsvToRgb(const ax::HSV& hsv)
{
	float h, s, v;
	h = hsv.h;
	s = hsv.s;
	v = hsv.v;

	if (std::isnan(h)) {
		return { (uint8_t)(v * 255.0f), (uint8_t)(v * 255.0f), (uint8_t)(v * 255.0f) };
	}

	h = std::fmod((360.0f + std::fmod(h, 360)), 360.0f);
	s = std::clamp(s, 0.0f, 1.0f);
	v = std::clamp(v, 0.0f, 1.0f);

	h /= 60.0;
	float p = v * (1.0f - s);
	float q = v * (1.0f - (s * std::fmod(h, 1.0f)));
	float t = v * (1.0f - (s * (1.0f - std::fmod(h, 1.0f))));

	switch (static_cast<int>(std::floor(h)) % 6) {
	case 0:
		return { (uint8_t)(v * 255.0f), (uint8_t)(t * 255.0f), (uint8_t)(p * 255.0f) };
	case 1:
		return { (uint8_t)(q * 255.0f), (uint8_t)(v * 255.0f), (uint8_t)(p * 255.0f) };
	case 2:
		return { (uint8_t)(p * 255.0f), (uint8_t)(v * 255.0f), (uint8_t)(t * 255.0f) };
	case 3:
		return { (uint8_t)(p * 255.0f), (uint8_t)(q * 255.0f), (uint8_t)(v * 255.0f) };
	case 4:
		return { (uint8_t)(t * 255.0f), (uint8_t)(p * 255.0f), (uint8_t)(v * 255.0f) };
	case 5:
		return { (uint8_t)(v * 255.0f), (uint8_t)(p * 255.0f), (uint8_t)(q * 255.0f) };
	default:
		throw std::logic_error("Unreachable!");
	}
}

const char* GameToolbox::levelLengthString(int len)
{
	switch (len)
	{
	case 1:
		return "Short";
	case 2:
		return "Medium";
	case 3:
		return "Long";
	case 4:
		return "XL";
	case 5:
		return "Plat.";
	default:
		return "Tiny";
	}
}

namespace
{
	constexpr float kSpeedNormal = 311.58f;
	constexpr float kSpeedSlow = 251.16f;
	constexpr float kSpeedMedium = 387.42f;
	constexpr float kSpeedFast = 468.0f;
	constexpr float kSpeedSuperFast = 576.0f;

	float speedFromIndex(int index)
	{
		switch (index)
		{
		case 1:
			return kSpeedSlow;
		case 2:
			return kSpeedMedium;
		case 3:
			return kSpeedFast;
		case 4:
			return kSpeedSuperFast;
		default:
			return kSpeedNormal;
		}
	}

	float speedFromPortalId(int id)
	{
		switch (id)
		{
		case 200:
			return kSpeedSlow;
		case 201:
			return kSpeedNormal;
		case 202:
			return kSpeedMedium;
		case 203:
			return kSpeedFast;
		case 1334:
			return kSpeedSuperFast;
		default:
			return kSpeedNormal;
		}
	}

	bool isSpeedPortalId(int id) { return id == 200 || id == 201 || id == 202 || id == 203 || id == 1334; }

	int lengthCategoryFromSeconds(float seconds, bool platformer)
	{
		if (platformer)
			return 5;
		if (seconds < 10.f)
			return 0;
		if (seconds < 30.f)
			return 1;
		if (seconds < 60.f)
			return 2;
		if (seconds < 120.f)
			return 3;
		return 4;
	}

	float secondsFromXPos(float levelLength, float startSpeed, const std::vector<std::pair<float, float>>& portals)
	{
		float speed = startSpeed;
		if (portals.empty())
			return levelLength / speed;

		float lastObjPos = 0.f;
		float totalTime = 0.f;
		for (const auto& [x, portalSpeed] : portals)
		{
			const float currentSegment = x - lastObjPos;
			if (levelLength <= currentSegment)
				break;

			totalTime += currentSegment / speed;
			speed = portalSpeed;
			lastObjPos = x;
		}
		return (levelLength - lastObjPos) / speed + totalTime;
	}

	int parseHeaderKeyInt(std::string_view header, std::string_view key)
	{
		const size_t pos = header.find(key);
		if (pos == std::string_view::npos)
			return 0;
		size_t cursor = pos + key.size();
		if (cursor >= header.size() || header[cursor] != ',')
			return 0;
		++cursor;
		while (cursor < header.size() && header[cursor] == ' ')
			++cursor;
		const size_t start = cursor;
		while (cursor < header.size() && header[cursor] != ',')
			++cursor;
		if (start == cursor)
			return 0;
		return GameToolbox::stoi(header.substr(start, cursor - start));
	}
} // namespace

float GameToolbox::calculateLevelLengthSeconds(int startSpeed, const std::vector<GameObject*>& objects)
{
	float furthestX = 0.f;
	std::vector<std::pair<float, float>> portals;
	portals.reserve(objects.size());

	for (GameObject* obj : objects)
	{
		if (!obj)
			continue;

		const float x = obj->getPositionX();
		furthestX = std::max(furthestX, x);

		const int id = obj->getID();
		if (isSpeedPortalId(id))
			portals.emplace_back(x, speedFromPortalId(id));
	}

	std::sort(portals.begin(), portals.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	return secondsFromXPos(furthestX, speedFromIndex(startSpeed), portals);
}

int GameToolbox::calculateLevelLengthCategory(int startSpeed, bool platformer, const std::vector<GameObject*>& objects)
{
	if (platformer)
		return 5;
	return lengthCategoryFromSeconds(calculateLevelLengthSeconds(startSpeed, objects), false);
}

int GameToolbox::calculateLevelLengthCategoryFromString(std::string_view levelString)
{
	if (levelString.empty())
		return 0;

	const auto parts = GameToolbox::splitByDelimStringView(levelString, ';');
	if (parts.empty())
		return 0;

	const std::string_view header = parts[0];
	const int startSpeed = parseHeaderKeyInt(header, "kA4");
	const bool platformer = parseHeaderKeyInt(header, "kA22") != 0;
	if (platformer)
		return 5;

	float furthestX = 0.f;
	std::vector<std::pair<float, float>> portals;

	for (size_t partIndex = 1; partIndex < parts.size(); ++partIndex)
	{
		const std::string_view objectData = parts[partIndex];
		if (objectData.empty())
			continue;

		const auto props = GameToolbox::splitByDelimStringView(objectData, ',');
		if (props.size() < 2)
			continue;

		const int id = GameToolbox::stoi(props[1]);
		float x = 0.f;
		bool portalChecked = true;
		for (size_t i = 0; i + 1 < props.size(); i += 2)
		{
			const int key = GameToolbox::stoi(props[i]);
			if (key == 2)
				x = GameToolbox::stof(props[i + 1]);
			else if (key == 13)
				portalChecked = GameToolbox::stoi(props[i + 1]) != 0;
		}

		furthestX = std::max(furthestX, x);
		if (portalChecked && isSpeedPortalId(id))
			portals.emplace_back(x, speedFromPortalId(id));
	}

	std::sort(portals.begin(), portals.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
	const float seconds = secondsFromXPos(furthestX, speedFromIndex(startSpeed), portals);
	return lengthCategoryFromSeconds(seconds, false);
}

std::string GameToolbox::xorCipher(const std::string& message, const std::string& key)
{
	std::string encryptedMessage;
	for (size_t i = 0; i < message.size(); ++i)
	{
		encryptedMessage += message[i] ^ key[i % key.size()];
	}
	return encryptedMessage;
}

ax::Color3B GameToolbox::blendColor(const ax::Color3B& color1, const ax::Color3B& color2, float ratio)
{
	uint8_t r = color1.r * (1 - ratio) + color2.r * ratio;
	uint8_t g = color1.g * (1 - ratio) + color2.g * ratio;
	uint8_t b = color1.b * (1 - ratio) + color2.b * ratio;

	return ax::Color3B(r, g, b);
}

void GameToolbox::drawFromRect(ax::Rect const& rect, ax::Color4B color, ax::DrawNode* drawNode)
{
	drawNode->drawSolidRect({rect.getMinX(), rect.getMinY()}, {rect.getMaxX(), rect.getMaxX()}, color);
}

int GameToolbox::getValueForGamemode(IconType mode)
{
	switch (mode)
	{
	case kIconTypeCube:
		return 485;
	case kIconTypeShip:
		return 169;
	case kIconTypeBall:
		return 118;
	case kIconTypeUfo:
		return 149;
	case kIconTypeWave:
		return 96;
	case kIconTypeRobot:
		return 68;
	case kIconTypeSpider:
		return 69;
	case kIconTypeSwing:
		return 43;
	case kIconTypeJetpack:
		return 8;
	case kIconTypeDeathEffect:
		return 20;
	case kIconTypeSpecial:
		return 7;
	default:
		return 0;
	}
}
const char* GameToolbox::getNameGamemode(IconType mode)
{
	switch (mode)
	{
	case kIconTypeShip:
		return "ship";
	case kIconTypeBall:
		return "player_ball";
	case kIconTypeUfo:
		return "bird";
	case kIconTypeWave:
		return "dart";
	case kIconTypeRobot:
		return "robot";
	case kIconTypeSpider:
		return "spider";
	case kIconTypeSwing:
		return "swing";
	case kIconTypeJetpack:
		return "jetpack";
	default:
		return "player";
	}
}

Color3B GameToolbox::colorForIdx(int col)
{
	// Official GameManager::colorForIdx palette dumped from Geometry Dash 2.2074.
	static const Color3B kOfficialColors[] = {
		{125, 255, 0},	 {0, 255, 0},	   {0, 255, 125},	{0, 255, 255},	{0, 125, 255},
		{0, 0, 255},	   {125, 0, 255},	{255, 0, 255},	{255, 0, 125},	{255, 0, 0},
		{255, 125, 0},	 {255, 255, 0},	{255, 255, 255},  {185, 0, 255},	{255, 185, 0},
		{0, 0, 0},		 {0, 200, 255},	{175, 175, 175},  {90, 90, 90},	  {255, 125, 125},
		{0, 175, 75},	  {0, 125, 125},	{0, 75, 175},	 {75, 0, 175},	 {125, 0, 125},
		{175, 0, 75},	  {175, 75, 0},	 {125, 125, 0},	{75, 175, 0},	 {255, 75, 0},
		{150, 50, 0},	  {150, 100, 0},	{100, 150, 0},	{0, 150, 100},	{0, 100, 150},
		{100, 0, 150},	 {150, 0, 100},	{150, 0, 0},	  {0, 150, 0},	  {0, 0, 150},
		{125, 255, 175},   {125, 125, 255},  {255, 250, 127},  {250, 127, 255},  {0, 255, 192},
		{80, 50, 14},	  {205, 165, 118},  {182, 128, 255},  {255, 58, 58},	{77, 77, 143},
		{0, 10, 76},	   {253, 212, 206},  {190, 181, 255},  {112, 0, 0},	  {82, 2, 0},
		{56, 1, 6},		{128, 79, 79},	{122, 53, 53},	{81, 36, 36},	 {163, 98, 70},
		{117, 73, 54},	 {86, 53, 40},	 {255, 185, 114},  {255, 160, 64},   {102, 49, 30},
		{91, 39, 0},	   {71, 32, 0},	  {167, 123, 77},   {109, 83, 57},	{81, 62, 42},
		{255, 255, 192},   {253, 224, 160},  {192, 255, 160},  {177, 255, 109},  {192, 255, 224},
		{148, 255, 228},   {67, 161, 138},   {49, 109, 95},	{38, 84, 73},	 {0, 96, 0},
		{0, 64, 0},		{0, 96, 96},	  {0, 64, 64},	  {160, 255, 255},  {1, 7, 112},
		{0, 73, 109},	  {0, 50, 76},	  {0, 38, 56},	  {80, 128, 173},   {51, 83, 117},
		{35, 60, 86},	  {224, 224, 224},  {61, 6, 140},	 {55, 8, 96},	  {64, 64, 64},
		{111, 73, 164},	{84, 54, 127},	{66, 42, 99},	 {252, 181, 255},  {175, 87, 175},
		{130, 67, 130},	{94, 49, 94},	 {128, 128, 128},  {102, 3, 62},	 {71, 1, 52},
		{210, 255, 50},	{118, 189, 255},
	};
	constexpr int kCount = static_cast<int>(sizeof(kOfficialColors) / sizeof(kOfficialColors[0]));
	if (col < 0 || col >= kCount)
		return {255, 255, 255};
	return kOfficialColors[col];
}
