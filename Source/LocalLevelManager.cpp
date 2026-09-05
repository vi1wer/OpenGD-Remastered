/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "LocalLevelManager.h"

#include "GJGameLevel.h"
#include "GameToolbox/log.h"
#include "external/json.hpp"
#include "platform/FileUtils.h"

#include <algorithm>
#include <cctype>
#include <fmt/format.h>

using json = nlohmann::json;

LocalLevelManager* LocalLevelManager::get()
{
	static LocalLevelManager* s_instance = nullptr;
	if (!s_instance)
	{
		s_instance = new LocalLevelManager();
		s_instance->init();
	}
	return s_instance;
}

bool LocalLevelManager::init()
{
	auto* fu = ax::FileUtils::getInstance();
	_filepath = fmt::format("{}LocalLevels.opengd.json", fu->getWritablePath());
	load();
	return true;
}

void LocalLevelManager::load()
{
	for (auto* level : _levels)
		delete level;
	_levels.clear();
	_nextId = 100000;

	auto* fu = ax::FileUtils::getInstance();
	if (!fu->isFileExist(_filepath))
		return;

	const std::string text = fu->getStringFromFile(_filepath);
	if (text.empty())
		return;

	json root = json::parse(text, nullptr, false);
	if (!root.is_object() || !root.contains("levels") || !root["levels"].is_array())
		return;

	if (root.contains("nextId") && root["nextId"].is_number_integer())
		_nextId = root["nextId"].get<int>();

	for (const auto& entry : root["levels"])
	{
		if (!entry.is_object())
			continue;

		const std::string name = entry.value("name", "UNNAMED 0");
		const int id = entry.value("id", _nextId++);
		auto* level = GJGameLevel::createWithMinimumData(name, entry.value("creator", "You"), id);
		if (!level)
			continue;

		level->_description = entry.value("description", "");
		level->_levelString = entry.value("levelString", kDefaultLevelString);
		level->_musicID = entry.value("musicID", 0);
		level->_officialSongID = entry.value("officialSongID", 0);
		level->_songID = entry.value("songID", 0);
		level->_songName = entry.value("songName", "");
		level->_length = entry.value("length", 0);
		level->_version = entry.value("version", 1);
		level->_objects = entry.value("objects", 0);
		level->_setCompletes = entry.value("verified", 0);

		_levels.push_back(level);
		if (id >= _nextId)
			_nextId = id + 1;
	}

	GameToolbox::log("LocalLevelManager loaded {} levels", _levels.size());
}

void LocalLevelManager::save()
{
	json root;
	root["nextId"] = _nextId;
	root["levels"] = json::array();

	for (auto* level : _levels)
	{
		if (!level)
			continue;
		json entry;
		entry["id"] = level->_levelID;
		entry["name"] = level->_levelName;
		entry["creator"] = level->_levelCreator;
		entry["description"] = level->_description;
		entry["levelString"] = level->_levelString;
		entry["musicID"] = level->_musicID;
		entry["officialSongID"] = level->_officialSongID;
		entry["songID"] = level->_songID;
		entry["songName"] = level->_songName;
		entry["length"] = level->_length;
		entry["version"] = level->_version;
		entry["objects"] = level->_objects;
		entry["verified"] = level->_setCompletes;
		root["levels"].push_back(std::move(entry));
	}

	ax::FileUtils::getInstance()->writeStringToFile(root.dump(2), _filepath);
}

std::string LocalLevelManager::nextUnnamedName() const
{
	int maxIndex = -1;
	for (auto* level : _levels)
	{
		if (!level)
			continue;
		std::string name = level->_levelName;
		for (char& c : name)
			c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

		constexpr std::string_view prefix = "UNNAMED ";
		if (name.rfind(prefix, 0) != 0)
			continue;

		try
		{
			const int idx = std::stoi(name.substr(prefix.size()));
			maxIndex = std::max(maxIndex, idx);
		}
		catch (...)
		{
		}
	}
	return fmt::format("UNNAMED {}", maxIndex + 1);
}

GJGameLevel* LocalLevelManager::createUnnamedLevel()
{
	const std::string name = nextUnnamedName();
	auto* level = GJGameLevel::createWithMinimumData(name, "You", _nextId++);
	if (!level)
		return nullptr;

	level->_levelString = kDefaultLevelString;
	level->_musicID = 0;
	level->_officialSongID = 0;
	level->_length = 0;
	level->_version = 1;
	level->_levelCreator = "You";
	level->_description = "";
	level->_setCompletes = 0;

	_levels.insert(_levels.begin(), level);
	save();
	return level;
}

void LocalLevelManager::upsertLevel(GJGameLevel* level)
{
	if (!level)
		return;

	auto it = std::find_if(_levels.begin(), _levels.end(), [level](GJGameLevel* existing) {
		return existing && existing->_levelID == level->_levelID;
	});

	if (it == _levels.end())
		_levels.insert(_levels.begin(), level);
	else if (*it != level)
	{
		// Keep the manager's pointer; copy fields from the edited instance.
		GJGameLevel* dst = *it;
		dst->_levelName = level->_levelName;
		dst->_description = level->_description;
		dst->_levelString = level->_levelString;
		dst->_musicID = level->_musicID;
		dst->_officialSongID = level->_officialSongID;
		dst->_songID = level->_songID;
		dst->_songName = level->_songName;
		dst->_length = level->_length;
		dst->_version = level->_version;
		dst->_objects = level->_objects;
		dst->_setCompletes = level->_setCompletes;
		dst->_levelCreator = level->_levelCreator;
	}

	save();
}

void LocalLevelManager::removeLevel(int localId)
{
	removeLevels({localId});
}

void LocalLevelManager::removeLevels(const std::vector<int>& localIds)
{
	if (localIds.empty())
		return;

	_levels.erase(
		std::remove_if(
			_levels.begin(), _levels.end(),
			[&](GJGameLevel* level) {
				if (!level)
					return true;
				if (std::find(localIds.begin(), localIds.end(), level->_levelID) == localIds.end())
					return false;
				delete level;
				return true;
			}),
		_levels.end());
	save();
}

GJGameLevel* LocalLevelManager::findById(int localId) const
{
	for (auto* level : _levels)
	{
		if (level && level->_levelID == localId)
			return level;
	}
	return nullptr;
}
