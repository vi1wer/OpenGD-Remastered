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

#include "GameToolbox/enums.h"
#include "GameManager.h"
#include "GJGameLevel.h"
#include "ShopCatalog.h"
#include "external/hps/hps.h"
#include "ResourcesLoadingLayer.h"
#include "AppDelegate.h"
#include "GameToolbox/log.h"
#include <algorithm>
#include <platform/FileUtils.h>
#include <sstream>


struct SaveObject
{
	std::map<std::string, int> intOptions;
	std::map<std::string, bool> boolOptions;
	std::map<std::string, std::string> stringOptions;
	
	template <class B>
	void serialize(B& buf) const
	{
		buf << intOptions << boolOptions << stringOptions;
	}

	template <class B>
	void parse(B& buf)
	{
		buf >> intOptions >> boolOptions >> stringOptions;
	}

};

//cpp struct declaration and definition to avoid including hps in header
static SaveObject _options;

GameManager* GameManager::getInstance()
{
	static GameManager* _gameManager = nullptr;
	if (!_gameManager)
	{
		_gameManager = new GameManager();
		return _gameManager->init() ? _gameManager : nullptr;
	}
	return _gameManager;
	
}

bool GameManager::init()
{
	GameToolbox::log("GAME MANAGER INIT");

	auto fu = ax::FileUtils::getInstance();
	auto wp = fu->getWritablePath();
	_filepath = fmt::format("{}GameManager.opengd", wp);
	
	print();
	
	if (fu->isFileExist(_filepath))
		load();

	return true;
}


void GameManager::load()
{
	GameToolbox::log("gamemanager load");
	std::string byteStr = ax::FileUtils::getInstance()->getStringFromFile(_filepath);
	_options = hps::from_string<SaveObject>(byteStr);
	loadMembersFromMap();
	print();
}

void GameManager::save()
{
	setMembersToMap();
	GameToolbox::log("gamemanager save");
	const std::string& serialized = hps::to_string(_options);
	ax::FileUtils::getInstance()->writeStringToFile(serialized, _filepath);
}

void GameManager::print()
{
	for (const auto& [key, val] : _options.boolOptions) {
		GameToolbox::log("boolOptions[{}] = {}", key, val);
	}

	for (const auto& [key, val] : _options.intOptions) {
		GameToolbox::log("intOptions[{}] = {}", key, val);
	}

	for (const auto& [key, val] : _options.stringOptions) {
		GameToolbox::log("stringOptions[{}] = {}", key, val);
	}
}

template<>
bool GameManager::get<bool>(const std::string& key)
{
	if (_options.boolOptions.count(key))
		return _options.boolOptions.at(key);

	return false;
}

template<>
int GameManager::get<int>(const std::string& key)
{
	if (_options.intOptions.count(key))
		return _options.intOptions.at(key);

	return 0;
}

template<>
std::string GameManager::get<std::string>(const std::string& key)
{
	if (_options.stringOptions.count(key))
		return _options.stringOptions.at(key);

	return "";
}

template<>
void GameManager::set<bool>(const std::string& key, const bool& val) {
	_options.boolOptions[key] = val;
}

template<>
void GameManager::set<int>(const std::string& key, const int& val) {
	_options.intOptions[key] = val;
}

template<>
void GameManager::set<std::string>(const std::string& key, const std::string& val) {
	_options.stringOptions[key] = val;
}


void GameManager::setMembersToMap()
{
	#define SAVE_BOOL(member) set<bool>(#member, member)
	SAVE_BOOL(_openedGarage);
	SAVE_BOOL(_openedCreator);
	SAVE_BOOL(_openedPracticeMode);
	SAVE_BOOL(_mediumQuality);
	SAVE_BOOL(_playerGlowEnabled);
	SAVE_BOOL(_showHitboxes);
	SAVE_BOOL(_showHitboxesOnDeath);
	SAVE_BOOL(_showProgressBar);
	SAVE_BOOL(_showPercentage);
	SAVE_BOOL(_autoCheckpoints);
	#undef SAVE_BOOL

	#define SAVE_INT(member) set<int>(#member, member)
	SAVE_INT(_selectedCube);
	SAVE_INT(_selectedShip);
	SAVE_INT(_selectedBall);
	SAVE_INT(_selectedUfo);
	SAVE_INT(_selectedWave);
	SAVE_INT(_selectedRobot);
	SAVE_INT(_selectedSpider);
	SAVE_INT(_selectedSwing);
	SAVE_INT(_selectedJetpack);
	SAVE_INT(_selectedSpecial);
	SAVE_INT(_selectedDeathEffect);
	SAVE_INT(_playerMainColor);
	SAVE_INT(_playerSecondaryColor);
	SAVE_INT(_playerGlowColor);
	SAVE_INT(_orbs);
	SAVE_INT(_diamonds);
	SAVE_INT(_musicVolume);
	SAVE_INT(_sfxVolume);
	#undef SAVE_INT
	
	set<int>("_mainSelectedMode", static_cast<int>(_mainSelectedMode));
	set<std::string>("_unlockedShopItems", _unlockedShopItems);
}

void GameManager::loadMembersFromMap()
{
	#define LOAD_BOOL(member) member = get<bool>(#member)
	LOAD_BOOL(_openedGarage);
	LOAD_BOOL(_openedCreator);
	LOAD_BOOL(_openedPracticeMode);
	LOAD_BOOL(_mediumQuality);
	LOAD_BOOL(_playerGlowEnabled);
	#undef LOAD_BOOL

	auto loadBoolDefault = [](const char* key, bool fallback) {
		if (_options.boolOptions.count(key))
			return _options.boolOptions.at(key);
		return fallback;
	};
	_showHitboxes = loadBoolDefault("_showHitboxes", false);
	_showHitboxesOnDeath = loadBoolDefault("_showHitboxesOnDeath", false);
	_showProgressBar = loadBoolDefault("_showProgressBar", true);
	_showPercentage = loadBoolDefault("_showPercentage", true);
	_autoCheckpoints = loadBoolDefault("_autoCheckpoints", true);
	
	#define LOAD_INT(member) member = get<int>(#member)
	LOAD_INT(_selectedCube);
	LOAD_INT(_selectedShip);
	LOAD_INT(_selectedBall);
	LOAD_INT(_selectedUfo);
	LOAD_INT(_selectedWave);
	LOAD_INT(_selectedRobot);
	LOAD_INT(_selectedSpider);
	LOAD_INT(_selectedSwing);
	LOAD_INT(_selectedJetpack);
	LOAD_INT(_selectedSpecial);
	LOAD_INT(_selectedDeathEffect);
	LOAD_INT(_playerMainColor);
	LOAD_INT(_playerSecondaryColor);
	LOAD_INT(_playerGlowColor);
	LOAD_INT(_diamonds);
	#undef LOAD_INT

	if (_options.intOptions.count("_orbs"))
		_orbs = get<int>("_orbs");
	else
		_orbs = 10000;

	if (_options.intOptions.count("_musicVolume"))
		_musicVolume = std::clamp(get<int>("_musicVolume"), 0, 100);
	else
		_musicVolume = 100;

	if (_options.intOptions.count("_sfxVolume"))
		_sfxVolume = std::clamp(get<int>("_sfxVolume"), 0, 100);
	else
		_sfxVolume = 100;

	_unlockedShopItems = get<std::string>("_unlockedShopItems");
	
	if (_playerMainColor == 0 && _playerSecondaryColor == 0 && _playerGlowColor == 0)
	{
		_playerMainColor = 0x7D00FF;
		_playerSecondaryColor = 0x00FFFF;
		_playerGlowColor = 0x00FF00;
	}
	
	_mainSelectedMode = static_cast<IconType>(get<int>("_mainSelectedMode"));
}

namespace
{
ax::Color3B unpackPlayerColor(int value)
{
	return {
		static_cast<uint8_t>((value >> 16) & 0xFF),
		static_cast<uint8_t>((value >> 8) & 0xFF),
		static_cast<uint8_t>(value & 0xFF),
	};
}

int packPlayerColor(ax::Color3B color)
{
	return (color.r << 16) | (color.g << 8) | color.b;
}
}

ax::Color3B GameManager::getPlayerMainColor() const
{
	return unpackPlayerColor(_playerMainColor);
}

ax::Color3B GameManager::getPlayerSecondaryColor() const
{
	return unpackPlayerColor(_playerSecondaryColor);
}

ax::Color3B GameManager::getPlayerGlowColor() const
{
	return unpackPlayerColor(_playerGlowColor);
}

void GameManager::setPlayerMainColor(ax::Color3B color)
{
	_playerMainColor = packPlayerColor(color);
}

void GameManager::setPlayerSecondaryColor(ax::Color3B color)
{
	_playerSecondaryColor = packPlayerColor(color);
}

void GameManager::setPlayerGlowColor(ax::Color3B color)
{
	_playerGlowColor = packPlayerColor(color);
}

bool GameManager::isPlayerGlowEnabled() const
{
	return _playerGlowEnabled;
}

void GameManager::setPlayerGlowEnabled(bool enabled)
{
	_playerGlowEnabled = enabled;
}

bool GameManager::isMedium() { return _mediumQuality; }
bool GameManager::isHigh() { return !_mediumQuality; }


void GameManager::setQuality(bool medium)
{
	_mediumQuality = medium;
	this->save();
	ax::Director::getInstance()->replaceScene(ResourcesLoadingLayer::scene());
}

void GameManager::setQualityMedium() { setQuality(true); }
void GameManager::setQualityHigh() { setQuality(false); }

int GameManager::getSelectedIcon(IconType mode)
{
	switch (mode)
	{
		case IconType::kIconTypeCube: return _selectedCube;
		case IconType::kIconTypeShip: return _selectedShip;
		case IconType::kIconTypeBall: return _selectedBall;
		case IconType::kIconTypeUfo: return _selectedUfo;
		case IconType::kIconTypeWave: return _selectedWave;
		case IconType::kIconTypeRobot: return _selectedRobot;
		case IconType::kIconTypeSpider: return _selectedSpider;
		case IconType::kIconTypeSwing: return _selectedSwing;
		case IconType::kIconTypeJetpack: return _selectedJetpack;
		case IconType::kIconTypeSpecial: return _selectedSpecial;
		case IconType::kIconTypeDeathEffect: return _selectedDeathEffect;
		default: return 1;
	}
}

void GameManager::setSelectedIcon(IconType mode, int id) {
	switch (mode)
	{
		case IconType::kIconTypeCube: _selectedCube = id; break;
		case IconType::kIconTypeShip: _selectedShip = id; break;
		case IconType::kIconTypeBall: _selectedBall = id; break;
		case IconType::kIconTypeUfo: _selectedUfo = id; break;
		case IconType::kIconTypeWave: _selectedWave = id; break;
		case IconType::kIconTypeRobot: _selectedRobot = id; break;
		case IconType::kIconTypeSpider: _selectedSpider = id; break;
		case IconType::kIconTypeSwing: _selectedSwing = id; break;
		case IconType::kIconTypeJetpack: _selectedJetpack = id; break;
		case IconType::kIconTypeSpecial: _selectedSpecial = id; break;
		case IconType::kIconTypeDeathEffect: _selectedDeathEffect = id; break;
		default: break;
	}
}

bool GameManager::isFollowingUser(int accountID)
{
	return false;
}

int GameManager::getOrbs() const
{
	return _orbs;
}

int GameManager::getDiamonds() const
{
	return _diamonds;
}

void GameManager::addOrbs(int amount)
{
	if (amount > 0)
		_orbs += amount;
}

bool GameManager::spendOrbs(int amount)
{
	if (amount <= 0 || _orbs < amount)
		return false;
	_orbs -= amount;
	return true;
}

namespace
{
	std::string unlockToken(IconType type, int id)
	{
		return fmt::format("{},{}", static_cast<int>(type), id);
	}

	bool csvContainsToken(const std::string& csv, const std::string& token)
	{
		std::stringstream ss(csv);
		std::string part;
		while (std::getline(ss, part, ';'))
		{
			if (part == token)
				return true;
		}
		return false;
	}
}

bool GameManager::isIconUnlocked(IconType type, int id) const
{
	if (!ShopCatalog::isShopLockedItem(type, id))
		return true;
	return csvContainsToken(_unlockedShopItems, unlockToken(type, id));
}

void GameManager::unlockIcon(IconType type, int id)
{
	const auto token = unlockToken(type, id);
	if (csvContainsToken(_unlockedShopItems, token))
		return;
	if (!_unlockedShopItems.empty())
		_unlockedShopItems += ';';
	_unlockedShopItems += token;
}

namespace
{
	std::string levelBestKey(int levelID, bool practice)
	{
		return practice ? fmt::format("best_p_{}", levelID) : fmt::format("best_n_{}", levelID);
	}

	std::string levelCoinKey(int levelID)
	{
		return fmt::format("coins_{}", levelID);
	}
}

int GameManager::getLevelBest(int levelID, bool practice) const
{
	return const_cast<GameManager*>(this)->get<int>(levelBestKey(levelID, practice));
}

int GameManager::getLevelCoins(int levelID) const
{
	return const_cast<GameManager*>(this)->get<int>(levelCoinKey(levelID));
}

bool GameManager::recordLevelProgress(int levelID, int percent, bool practice)
{
	percent = std::clamp(percent, 0, 100);
	const int previous = getLevelBest(levelID, practice);
	if (percent <= previous)
		return false;
	const_cast<GameManager*>(this)->set<int>(levelBestKey(levelID, practice), percent);
	save();
	return true;
}

void GameManager::recordLevelCoins(int levelID, int coinMask)
{
	const int previous = getLevelCoins(levelID);
	if ((coinMask | previous) == previous)
		return;
	const_cast<GameManager*>(this)->set<int>(levelCoinKey(levelID), coinMask | previous);
	save();
}

void GameManager::applySavedProgress(GJGameLevel* level) const
{
	if (!level)
		return;
	level->_normalPercent = static_cast<float>(getLevelBest(level->_levelID, false));
	level->_practicePercent = static_cast<float>(getLevelBest(level->_levelID, true));
}

int GameManager::getMusicVolumePercent() const
{
	return std::clamp(_musicVolume, 0, 100);
}

int GameManager::getSfxVolumePercent() const
{
	return std::clamp(_sfxVolume, 0, 100);
}

float GameManager::getMusicVolume() const
{
	return getMusicVolumePercent() / 100.f;
}

float GameManager::getSfxVolume() const
{
	return getSfxVolumePercent() / 100.f;
}

void GameManager::setMusicVolumePercent(int percent)
{
	_musicVolume = std::clamp(percent, 0, 100);
}

void GameManager::setSfxVolumePercent(int percent)
{
	_sfxVolume = std::clamp(percent, 0, 100);
}

