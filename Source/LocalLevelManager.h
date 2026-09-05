/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include <string>
#include <vector>

class GJGameLevel;

class LocalLevelManager
{
public:
	static LocalLevelManager* get();

	void load();
	void save();

	const std::vector<GJGameLevel*>& getLevels() const { return _levels; }

	GJGameLevel* createUnnamedLevel();
	void upsertLevel(GJGameLevel* level);
	void removeLevel(int localId);
	void removeLevels(const std::vector<int>& localIds);

	std::string nextUnnamedName() const;
	GJGameLevel* findById(int localId) const;

	static constexpr const char* kDefaultLevelString =
		"kA1,0,kA2,0,kA3,0,kA4,0,kA6,1,kA7,1,kA8,0,kA9,0,kA10,0,kA11,0,kA13,0;";

private:
	LocalLevelManager() = default;
	bool init();

	std::vector<GJGameLevel*> _levels;
	std::string _filepath;
	int _nextId = 100000;
};
