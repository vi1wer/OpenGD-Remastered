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

#pragma once
#include <string>

#include "base/Types.h"
#include "GameToolbox/enums.h"

class GameManager
{
public:
	bool _mediumQuality;
	bool _openedGarage;
	bool _openedCreator;
	bool _openedPracticeMode;

	int _selectedCube = 1;
	int _selectedShip = 1;
	int _selectedBall = 1;
	int _selectedUfo = 1;
	int _selectedWave = 1;
	int _selectedRobot = 1;
	int _selectedSpider = 1;
	int _selectedSwing = 1;
	int _selectedJetpack = 1;
	int _selectedSpecial = 1;
	int _selectedDeathEffect = 1;
	int _accountID = 0;
	IconType _mainSelectedMode = IconType::kIconTypeCube;
	std::string _filepath;
	int _playerMainColor = 0x7D00FF;
	int _playerSecondaryColor = 0x00FFFF;
	int _playerGlowColor = 0x00FF00;
	bool _playerGlowEnabled = true;
	int _orbs = 10000;
	int _diamonds = 0;
	int _demonKeys = 0;
	int _goldKeys = 0;
	int _orbKeyProgress = 0; // orbs toward next demon key (every 500)
	bool _treasureRoomUnlocked = false;
	std::string _openedChests; // "type:index;..."
	int _secretShopsUnlocked = 0; // bit0 Scratch, bit1 Community, bit2 Mechanic, bit3 Diamond
	int _musicVolume = 100;
	int _sfxVolume = 100;
	bool _showHitboxes = false;
	bool _showHitboxesOnDeath = false;
	bool _showProgressBar = true;
	bool _showPercentage = true;
	bool _autoCheckpoints = true;
	std::string _unlockedShopItems;

private:
	bool init();
	void load();
	
public:
	static GameManager* getInstance();
	
	template<typename T>
	T get(const std::string& key);

	template<typename T>
	void set(const std::string& key, const T& val);
	
	void save();
	void setMembersToMap();
	void loadMembersFromMap();
	bool isMedium();
	bool isHigh();
	void setQuality(bool medium);
	void setQualityMedium();
	void setQualityHigh();
	void print();
	int getSelectedIcon(IconType);
	void setSelectedIcon(IconType, int);
	bool isFollowingUser(int accountID);

	int getOrbs() const;
	int getDiamonds() const;
	int getDemonKeys() const;
	int getGoldKeys() const;
	void addOrbs(int amount);
	bool spendOrbs(int amount);
	void addDiamonds(int amount);
	bool spendDiamonds(int amount);
	void addDemonKeys(int amount);
	bool spendDemonKeys(int amount);
	void addGoldKeys(int amount);
	bool spendGoldKeys(int amount);
	bool isTreasureRoomUnlocked() const;
	bool unlockTreasureRoom(); // costs 5 keys once
	bool isChestOpened(int chestType, int index) const;
	void markChestOpened(int chestType, int index);
	int countOpenedChests() const; // types 1..6 (regular demon-key chests)
	int countOpenedChestsOfType(int chestType) const;
	int nextUnopenedGoldChest() const; // 0..19 or -1
	bool isSecretShopUnlocked(int shopIndex) const;
	void unlockSecretShop(int shopIndex);
	bool isIconUnlocked(IconType type, int id) const;
	void unlockIcon(IconType type, int id);

	int getLevelBest(int levelID, bool practice = false) const;
	int getLevelCoins(int levelID) const;
	bool recordLevelProgress(int levelID, int percent, bool practice = false);
	void recordLevelCoins(int levelID, int coinMask);
	void applySavedProgress(class GJGameLevel* level) const;

	ax::Color3B getPlayerMainColor() const;
	ax::Color3B getPlayerSecondaryColor() const;
	ax::Color3B getPlayerGlowColor() const;
	void setPlayerMainColor(ax::Color3B color);
	void setPlayerSecondaryColor(ax::Color3B color);
	void setPlayerGlowColor(ax::Color3B color);
	bool isPlayerGlowEnabled() const;
	void setPlayerGlowEnabled(bool enabled);

	int getMusicVolumePercent() const;
	int getSfxVolumePercent() const;
	float getMusicVolume() const;
	float getSfxVolume() const;
	void setMusicVolumePercent(int percent);
	void setSfxVolumePercent(int percent);
};
