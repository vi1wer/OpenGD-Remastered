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

#include "BaseGameLayer.h"
#include "EffectGameObject.h"
#include "GameObject.h"
#include "GJGameLevel.h"
#include "GameToolbox/conv.h"
#include "GameToolbox/log.h"
#include "external/benchmark.h"
#include "external/json.hpp"
#include <fstream>

#include "GameToolbox/getTextureString.h"
#include "platform/FileUtils.h"
#include <2d/SpriteBatchNode.h>
#include <2d/ParticleSystemQuad.h>

USING_NS_AX;

// #define USE_MULTITHREADING
// std::mutex mylock;

BaseGameLayer* BaseGameLayer::_instance = nullptr;

BaseGameLayer* BaseGameLayer::create(GJGameLevel* level)
{
	BaseGameLayer* ret = new BaseGameLayer();
	if (ret->init(level))
	{
		ret->autorelease();
		return ret;
	}
	else
	{
		delete ret;
		ret = nullptr;
		return nullptr;
	}
}

bool BaseGameLayer::init(GJGameLevel* level)
{
	if (!Layer::init())
		return false;

	_instance = this;
	_level = level;

	_effectManager = EffectManager::create();
	this->addChild(_effectManager);

	initBatchNodes();
	loadLevel();

	return true;
}

void BaseGameLayer::loadLevel()
{
	// TODO: find a modern gzip decompress library or write own gzip decompress

	std::string levelStr = _level->_levelString;
	if (levelStr.empty())
		levelStr = GJGameLevel::getLevelStrFromID(_level->_levelID);
	else if (levelStr.front() != 'k')
	{
		std::string decompressed = GJGameLevel::decompressLvlStr(levelStr);
		levelStr = decompressed.empty() ? GJGameLevel::getLevelStrFromID(_level->_levelID) : std::move(decompressed);
	}
	if (levelStr.empty() && _level->_levelID >= 1 && _level->_levelID <= 22)
		levelStr = GJGameLevel::getLevelStrFromID(_level->_levelID);
	if (levelStr.empty())
		return;
	{
		auto s = BenchmarkTimer("load level");
		setupLevel(levelStr);
		createObjectsFromSetup(levelStr);
	}

	if (_allObjects.size() != 0)
	{
		_lastObjXPos = 570.0f;

		for (GameObject* object : _allObjects)
		{
			if (_lastObjXPos < object->getPositionX())
				_lastObjXPos = object->getPositionX();
		}

		GameToolbox::log("last x: {}", _lastObjXPos);

		for (size_t i = 0; i < sectionForPos(_lastObjXPos); i++)
		{
			std::vector<GameObject*> vec;
			_sectionObjects.push_back(vec);
		}

		for (GameObject* object : _allObjects)
		{
			int section = sectionForPos(object->getPositionX());
			section = section - 1 < 0 ? 0 : section - 1;
			object->_section = section;
			while (static_cast<int>(_sectionObjects.size()) <= section)
				_sectionObjects.push_back({});
			_sectionObjects[section].push_back(object);

			object->setCascadeOpacityEnabled(false);
			object->update();
		}
	}
}

void BaseGameLayer::initBatchNodes()
{
	_blendingBatchNodeB4 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_blendingBatchNodeB4, -23);
	_blendingBatchNodeB4->setBlendFunc(GameToolbox::getBlending());
	_blendingBatchNodeB4->setName("_blendingBatchNodeB4");

	_mainBatchNodeB4 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_mainBatchNodeB4, -22);
	_mainBatchNodeB4->setName("_mainBatchNodeB4");

	_blendingBatchNodeB3 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_blendingBatchNodeB3, -16);
	_blendingBatchNodeB3->setBlendFunc(GameToolbox::getBlending());
	_blendingBatchNodeB3->setName("_blendingBatchNodeB3");

	_mainBatchNodeB3 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_mainBatchNodeB3, -15);
	_mainBatchNodeB3->setName("_mainBatchNodeB3");

	_blendingBatchNodeB2 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_blendingBatchNodeB2, -9);
	_blendingBatchNodeB2->setBlendFunc(GameToolbox::getBlending());
	_blendingBatchNodeB2->setName("_blendingBatchNodeB2");

	_mainBatchNodeB2 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_mainBatchNodeB2, -8);
	_mainBatchNodeB2->setName("_mainBatchNodeB2");

	_blendingBatchNodeB1 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_blendingBatchNodeB1, -2);
	_blendingBatchNodeB1->setBlendFunc(GameToolbox::getBlending());
	_blendingBatchNodeB1->setName("_blendingBatchNodeB1");

	_mainBatchNodeB1 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_mainBatchNodeB1, -1);
	_mainBatchNodeB1->setName("_mainBatchNodeB1");

	_blendingBatchNodeT1 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_blendingBatchNodeT1, 2);
	_blendingBatchNodeT1->setBlendFunc(GameToolbox::getBlending());
	_blendingBatchNodeT1->setName("_blendingBatchNodeT1");

	_mainBatchNodeT1 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_mainBatchNodeT1, 3);
	_mainBatchNodeT1->setName("_mainBatchNodeT1");

	_blendingBatchNodeT2 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_blendingBatchNodeT2, 9);
	_blendingBatchNodeT2->setBlendFunc(GameToolbox::getBlending());
	_blendingBatchNodeT2->setName("_blendingBatchNodeT2");

	_mainBatchNodeT2 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_mainBatchNodeT2, 10);
	_mainBatchNodeT2->setName("_mainBatchNodeT2");

	_blendingBatchNodeT3 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_blendingBatchNodeT3, 24);
	_blendingBatchNodeT3->setBlendFunc(GameToolbox::getBlending());
	_blendingBatchNodeT3->setName("_blendingBatchNodeT3");

	_mainBatchNodeT3 = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_mainBatchNodeTexture), 50);
	this->addChild(_mainBatchNodeT3, 25);
	_mainBatchNodeT3->setName("_mainBatchNodeT3");

	_glowBatchNode = ax::SpriteBatchNode::create(GameToolbox::getTextureString("GJ_GameSheetGlow.png"), 150);
	this->addChild(_glowBatchNode);
	_glowBatchNode->setBlendFunc(GameToolbox::getBlending());
	_glowBatchNode->setName("_glowBatchNode");

	_main2BatchNode = ax::SpriteBatchNode::create(GameToolbox::getTextureString(_main2BatchNodeTexture), 150);
	this->addChild(_main2BatchNode);
	_main2BatchNode->setName("_main2BatchNode");

	//_particleBatchNode = ax::ParticleBatchNode::create("square.png", 30);
	// addChild(_particleBatchNode);

	_mainBatchNodeTexture = _mainBatchNodeT3 && _mainBatchNodeT3->getTexture()
								? _mainBatchNodeT3->getTexture()->getPath()
								: std::string{};
	_main2BatchNodeTexture = _main2BatchNode && _main2BatchNode->getTexture()
								 ? _main2BatchNode->getTexture()->getPath()
								 : std::string{};
}

bool BaseGameLayer::isObjectBlending(GameObject* obj)
{
	if (!obj)
		return false;
	const bool mainBlend = _colorChannels.contains(obj->_mainColorChannel) && _colorChannels[obj->_mainColorChannel]._blending;
	const bool secBlend = _colorChannels.contains(obj->_secColorChannel) && _colorChannels[obj->_secColorChannel]._blending;
	if (_colorChannels.contains(obj->_mainColorChannel) && _colorChannels.contains(obj->_secColorChannel))
		return mainBlend && secBlend;
	return mainBlend || secBlend;
}

namespace
{
bool spriteMatchesBatch(ax::Sprite* sprite, ax::SpriteBatchNode* batch)
{
	if (!sprite || !batch)
		return false;
	auto* st = sprite->getTexture();
	auto* bt = batch->getTexture();
	if (!st || !bt)
		return false;
	if (st == bt)
		return true;
	return st->getBackendTexture() && st->getBackendTexture() == bt->getBackendTexture();
}

bool spriteTreeMatchesBatch(ax::Sprite* sprite, ax::SpriteBatchNode* batch)
{
	if (!spriteMatchesBatch(sprite, batch))
		return false;
	for (auto* child : sprite->getChildren())
	{
		auto* childSprite = dynamic_cast<ax::Sprite*>(child);
		if (!childSprite || !spriteTreeMatchesBatch(childSprite, batch))
			return false;
	}
	return true;
}
} // namespace

void BaseGameLayer::attachGameObject(GameObject* obj)
{
	if (!obj)
		return;

	if (obj->_particle)
	{
		if (!obj->_particle->getParent())
		{
			addChild(obj->_particle, 6);
			AX_SAFE_RELEASE(obj->_particle);
		}
		obj->_particle->setPosition(obj->getPosition());
		if (!obj->_animateOnTrigger)
			obj->_particle->resumeEmissions();
	}

	if (obj->_glowSprite)
	{
		if (spriteMatchesBatch(obj->_glowSprite, _glowBatchNode))
			_glowBatchNode->addChild(obj->_glowSprite);
		else
			addChild(obj->_glowSprite, -1);
		AX_SAFE_RELEASE(obj->_glowSprite);
	}

	const bool blending = isObjectBlending(obj);
	ax::SpriteBatchNode* batch = nullptr;

	if (spriteTreeMatchesBatch(obj, _mainBatchNodeT1))
	{
		switch (obj->_zLayer)
		{
		case -3:
			batch = blending ? _blendingBatchNodeB4 : _mainBatchNodeB4;
			break;
		case -1:
			batch = blending ? _blendingBatchNodeB3 : _mainBatchNodeB3;
			break;
		case 1:
			batch = blending ? _blendingBatchNodeB2 : _mainBatchNodeB2;
			break;
		case 3:
			batch = blending ? _blendingBatchNodeB1 : _mainBatchNodeB1;
			break;
		case 7:
			batch = blending ? _blendingBatchNodeT2 : _mainBatchNodeT2;
			break;
		case 9:
			batch = blending ? _blendingBatchNodeT3 : _mainBatchNodeT3;
			break;
		default:
			batch = blending ? _blendingBatchNodeT1 : _mainBatchNodeT1;
			break;
		}
	}
	else if (spriteTreeMatchesBatch(obj, _main2BatchNode))
	{
		batch = _main2BatchNode;
		if (blending)
			obj->setBlendFunc(GameToolbox::getBlending());
	}

	if (batch)
		batch->addChild(obj);
	else
		addChild(obj, 3);

	AX_SAFE_RELEASE(obj);

	std::string_view frame = GameObject::getBlockFrame(obj->getID());
	if (obj->_isTrigger || frame.rfind("edit_e", 0) == 0 || frame.rfind("edit_ee", 0) == 0)
	{
		obj->setVisible(false);
		obj->setOpacity(0);
	}
	else if (!obj->getActionByTag(80) && !obj->getActionByTag(81))
		obj->startIdleAnimation();
}

void BaseGameLayer::detachGameObject(GameObject* obj)
{
	if (!obj)
		return;

	auto detachNode = [](ax::Node* node) {
		if (!node)
			return;
		if (auto* parent = node->getParent())
		{
			AX_SAFE_RETAIN(node);
			parent->removeChild(node, false);
		}
	};

	detachNode(obj->_particle);
	detachNode(obj->_glowSprite);
	detachNode(obj);
}

void BaseGameLayer::createObjectsFromSetup(std::string_view uncompressedLevelString)
{
	std::vector<std::string_view> objData = GameToolbox::splitByDelimStringView(uncompressedLevelString, ';');
	if (objData.empty())
		return;

	_allObjects.reserve(objData.size());
	objData.erase(objData.begin());

	if (!objData.empty())
	{
		const auto& last = objData.back();
		if (last.empty() || last.front() != '1' || (last.size() > 1 && last[1] != ','))
			objData.pop_back();
	}

	GameToolbox::log("creating & pushing");

	for (const auto& objectDataSpecific : objData)
	{
		if (objectDataSpecific.empty())
			continue;
		GameObject* obj = GameObject::createFromString(objectDataSpecific);
		if (obj)
		{
			obj->_uniqueID = static_cast<int>(_allObjects.size());
			_allObjects.push_back(obj);
		}
	}
}

ax::Color3B BaseGameLayer::getLightBG(ax::Color3B bg, ax::Color3B p1)
{
	ax::HSV hsv = ax::HSV(bg);
	hsv.s -= 0.07843137255f;

	return GameToolbox::hsvToRgb(hsv); GameToolbox::blendColor(p1, GameToolbox::hsvToRgb(hsv), hsv.v / 100);
}

void BaseGameLayer::setupLevel(std::string_view uncompressedLevelString)
{
	auto chunks = GameToolbox::splitByDelimStringView(uncompressedLevelString, ';');
	if (chunks.empty())
		return;

	std::vector<std::string_view> levelData = GameToolbox::splitByDelimStringView(chunks[0], ',');

	_colorChannels[1000] = SpriteColor(ax::Color3B::WHITE, 255, false);
	_colorChannels[1001] = SpriteColor(ax::Color3B::WHITE, 255, false);

	if (levelData.size() < 2)
		return;

	for (size_t i = 0; i + 1 < levelData.size(); i += 2)
	{
		if (levelData[i] == "kS1")
		{
			_colorChannels[1000]._color.r = static_cast<uint8_t>(GameToolbox::stof(levelData[i + 1]));
		}
		else if (levelData[i] == "kS2")
		{
			_colorChannels[1000]._color.g = static_cast<uint8_t>(GameToolbox::stof(levelData[i + 1]));
		}
		else if (levelData[i] == "kS3")
		{
			_colorChannels[1000]._color.b = static_cast<uint8_t>(GameToolbox::stof(levelData[i + 1]));
		}
		else if (levelData[i] == "kS4")
		{
			_colorChannels[1001]._color.r = static_cast<uint8_t>(GameToolbox::stof(levelData[i + 1]));
		}
		else if (levelData[i] == "kS5")
		{
			_colorChannels[1001]._color.g = static_cast<uint8_t>(GameToolbox::stof(levelData[i + 1]));
		}
		else if (levelData[i] == "kS6")
		{
			_colorChannels[1001]._color.b = static_cast<uint8_t>(GameToolbox::stof(levelData[i + 1]));
		}
		else if (levelData[i] == "kS29")
		{
			auto colorString = GameToolbox::splitByDelimStringView(levelData[i + 1], '_');
			fillColorChannel(colorString, 1000);
		}
		else if (levelData[i] == "kS30")
		{
			auto colorString = GameToolbox::splitByDelimStringView(levelData[i + 1], '_');
			fillColorChannel(colorString, 1001);
		}
		else if (levelData[i] == "kS31")
		{
			auto colorString = GameToolbox::splitByDelimStringView(levelData[i + 1], '_');
			fillColorChannel(colorString, 1002);
		}
		else if (levelData[i] == "kS32")
		{
			auto colorString = GameToolbox::splitByDelimStringView(levelData[i + 1], '_');
			fillColorChannel(colorString, 1004);
		}
		else if (levelData[i] == "kS37")
		{
			auto colorString = GameToolbox::splitByDelimStringView(levelData[i + 1], '_');
			fillColorChannel(colorString, 1003);
		}
		else if (levelData[i] == "kS38")
		{
			parseColorChannelList(levelData[i + 1]);
		}
		else if (levelData[i] == "kA6")
		{
			_levelSettings._bgID = GameToolbox::stoi(levelData[i + 1]);
			if (!_levelSettings._bgID)
				_levelSettings._bgID = 1;
		}
		else if (levelData[i] == "kA7")
		{
			_levelSettings._groundID = GameToolbox::stoi(levelData[i + 1]);
			if (!_levelSettings._groundID)
				_levelSettings._groundID = 1;
		}
		else if (levelData[i] == "kA2")
		{
			_levelSettings.gamemode = (PlayerGamemode)GameToolbox::stoi(levelData[i + 1]);
		}
		else if (levelData[i] == "kA3")
		{
			_levelSettings.mini = GameToolbox::stoi(levelData[i + 1]);
		}
		else if (levelData[i] == "kA4")
		{
			_levelSettings.speed = GameToolbox::stoi(levelData[i + 1]);
		}
		else if (levelData[i] == "kA8")
		{
			_levelSettings.dual = GameToolbox::stoi(levelData[i + 1]);
		}
		else if (levelData[i] == "kA10")
		{
			_levelSettings.twoPlayer = GameToolbox::stoi(levelData[i + 1]);
		}
		else if (levelData[i] == "kA11")
		{
			_levelSettings.flipGravity = GameToolbox::stoi(levelData[i + 1]);
		}
		else if (levelData[i] == "kA13")
		{
			_levelSettings.songOffset = GameToolbox::stof(levelData[i + 1]);
		}
		else if (levelData[i] == "kA14")
		{
			// guidelines / unused in OpenGD
		}
		else if (levelData[i] == "kA15")
		{
			_levelSettings._fontID = GameToolbox::stoi(levelData[i + 1]);
		}
		else if (levelData[i] == "kA16")
		{
			_levelSettings._mgID = GameToolbox::stoi(levelData[i + 1]);
			if (!_levelSettings._mgID)
				_levelSettings._mgID = 1;
		}
		else if (levelData[i] == "kA17")
		{
			// unused / fade-in related in some versions
		}
		else if (levelData[i] == "kA18")
		{
			// ground line related
		}
		else if (levelData[i] == "kA22")
		{
			_levelSettings.platformer = GameToolbox::stoi(levelData[i + 1]) != 0;
		}
	}

	if (!_colorChannels.contains(1002))
		_colorChannels[1002] = SpriteColor(Color3B::WHITE, 255, false);
	if (!_colorChannels.contains(1003))
		_colorChannels[1003] = SpriteColor(Color3B(80, 80, 80), 255, false); // G2
	if (!_colorChannels.contains(1009))
		_colorChannels[1009] = SpriteColor(Color3B::WHITE, 255, false); // MG
	if (!_colorChannels.contains(1014))
		_colorChannels[1014] = SpriteColor(Color3B(120, 120, 120), 255, false); // MG2

	// change to get the player color not from player
	_colorChannels[1005]._color = Color3B::WHITE;
	_colorChannels[1005]._blending = true;
	_colorChannels[1006]._color = Color3B::WHITE;
	_colorChannels[1006]._blending = true;
	_colorChannels[1010]._color = Color3B::BLACK;
	_colorChannels[1007]._color = _colorChannels[1000]._color;
	_colorChannels[1007]._blending = true;

	_originalColors = _colorChannels;
}

void BaseGameLayer::fillColorChannel(std::span<std::string_view> colorString, int id)
{
	if (!_colorChannels.contains(id))
		_colorChannels[id] = SpriteColor(ax::Color3B::WHITE, 255, false);

	SpriteColor& col = _colorChannels[id];
	for (size_t j = 0; j + 1 < colorString.size(); j += 2)
	{
		switch (GameToolbox::stoi(colorString[j]))
		{
		case 1:
			col._color.r = static_cast<uint8_t>(GameToolbox::stof(colorString[j + 1]));
			break;
		case 2:
			col._color.g = static_cast<uint8_t>(GameToolbox::stof(colorString[j + 1]));
			break;
		case 3:
			col._color.b = static_cast<uint8_t>(GameToolbox::stof(colorString[j + 1]));
			break;
		case 4:
		{
			const int playerColor = GameToolbox::stoi(colorString[j + 1]);
			if (playerColor == 1)
				col._copyingColorID = 1005;
			else if (playerColor == 2)
				col._copyingColorID = 1006;
			break;
		}
		case 5:
			col._blending = GameToolbox::stoi(colorString[j + 1]) != 0;
			break;
		case 7:
			col._opacity = GameToolbox::stof(colorString[j + 1]) * 255.f;
			break;
		}
	}
}

void BaseGameLayer::parseColorChannelList(std::string_view ks38)
{
	auto colorString = GameToolbox::splitByDelimStringView(ks38, '|');
	for (std::string_view colorData : colorString)
	{
		if (colorData.empty())
			continue;

		auto innerData = GameToolbox::splitByDelimStringView(colorData, '_');
		int key = 0;
		SpriteColor col(ax::Color3B::WHITE, 255.f, false);
		col._copyingColorID = -1;
		col._applyHsv = false;

		for (size_t j = 0; j + 1 < innerData.size(); j += 2)
		{
			switch (GameToolbox::stoi(innerData[j]))
			{
			case 1:
				col._color.r = static_cast<uint8_t>(GameToolbox::stof(innerData[j + 1]));
				break;
			case 2:
				col._color.g = static_cast<uint8_t>(GameToolbox::stof(innerData[j + 1]));
				break;
			case 3:
				col._color.b = static_cast<uint8_t>(GameToolbox::stof(innerData[j + 1]));
				break;
			case 4:
			{
				const int playerColor = GameToolbox::stoi(innerData[j + 1]);
				if (playerColor == 1)
					col._copyingColorID = 1005;
				else if (playerColor == 2)
					col._copyingColorID = 1006;
				break;
			}
			case 5:
				col._blending = GameToolbox::stoi(innerData[j + 1]) != 0;
				break;
			case 6:
				key = GameToolbox::stoi(innerData[j + 1]);
				break;
			case 7:
				col._opacity = GameToolbox::stof(innerData[j + 1]) * 255.f;
				break;
			case 9:
			{
				const int copyId = GameToolbox::stoi(innerData[j + 1]);
				if (copyId > 0)
					col._copyingColorID = copyId;
				break;
			}
			case 10:
			{
				auto hsv = GameToolbox::splitByDelimStringView(innerData[j + 1], 'a');
				if (hsv.size() >= 5)
				{
					col._hsvModifier.h = GameToolbox::stof(hsv[0]);
					col._hsvModifier.s = GameToolbox::stof(hsv[1]);
					col._hsvModifier.v = GameToolbox::stof(hsv[2]);
					col._hsvModifier.sChecked = GameToolbox::stoi(hsv[3]) != 0;
					col._hsvModifier.vChecked = GameToolbox::stoi(hsv[4]) != 0;
					col._applyHsv = true;
				}
				break;
			}
			}
		}

		if (key != 0)
			_colorChannels[key] = col;
	}
}

ax::Color3B BaseGameLayer::colorForChannel(int id) const
{
	auto it = _colorChannels.find(id);
	if (it == _colorChannels.end())
		return ax::Color3B::WHITE;

	const SpriteColor* col = &it->second;
	ax::Color3B result = col->_color;
	int copyId = col->_copyingColorID;
	int guard = 0;
	while (copyId != -1 && guard++ < 8)
	{
		auto copyIt = _colorChannels.find(copyId);
		if (copyIt == _colorChannels.end() || &copyIt->second == col)
			break;
		result = copyIt->second._color;
		if (copyIt->second._copyingColorID == -1 || copyIt->second._copyingColorID == copyId)
			break;
		copyId = copyIt->second._copyingColorID;
	}

	if (col->_applyHsv)
		GameToolbox::applyHSV(col->_hsvModifier, &result);

	return result;
}

int BaseGameLayer::sectionForPos(float x)
{
	int section = x / 100;
	if (section < 0)
		section = 0;
	return section;
}

void BaseGameLayer::addObject(GameObject* obj)
{
}

void BaseGameLayer::loadLevelData(std::string_view levelDataString)
{
	// std::vector<std::string_view> levelData = GameToolbox::splitByDelimStringView(levelDataString, ',');
}

void BaseGameLayer::processMoveActions(float dt)
{
	for (auto dictObject : this->_effectManager->_activeMoveActions)
	{
		GroupProperties* currentGroup;
		float x, y;
		if (!this->_groups.contains(dictObject.first))
		{
			this->_groups.insert({dictObject.first, GroupProperties{}});
		}
		currentGroup = &this->_groups[dictObject.first];
		x = dictObject.second->_newPosOptimized.x;
		y = dictObject.second->_newPosOptimized.y;

		if (currentGroup && (x != 0 || y != 0))
		{
			for (auto obj : currentGroup->_objects)
			{
				if (!obj)
					continue;
				if (!obj->_unkbool)
				{
					obj->_firstPosition.x = obj->_startPosition.x + obj->_startPosOffset.x;
					obj->_firstPosition.y = obj->_startPosition.y + obj->_startPosOffset.y;
					obj->_unkbool = true;
				}

				if (y != 0)
					obj->_startPosOffset.y += y;
				if (x != 0)
					obj->_startPosOffset.x += x;
				if (!obj->_isTrigger)
					obj->setPosition(obj->getStartPosition() + obj->_startPosOffset);

				if (x != 0)
				{

					int sectionSize = this->_sectionObjects.size();
					auto section = BaseGameLayer::sectionForPos(obj->_startPosition.x + obj->_startPosOffset.x);
					section = section - 1 < 0 ? 0 : section - 1;
					if (obj->_section != section)
					{
						if (obj->_section >= 0 && obj->_section < sectionSize)
						{
							auto vec = &this->_sectionObjects[obj->_section];
							auto newEnd = std::partition(vec->begin(), vec->end(), [&](GameObject* a) { return a != obj; });
							vec->resize(newEnd - vec->begin());
						}
						while (section >= sectionSize)
						{
							this->_sectionObjects.push_back({});
							sectionSize++;
						}
						this->_sectionObjects[section].push_back(obj);
						obj->_section = section;
					}
				}
			}
		}
	}
}

void BaseGameLayer::runMoveCommand(float duration, ax::Point offsetPos, int easeType, float easeAmt, int groupID,
								  bool lockToPlayerX, bool lockToPlayerY)
{
	if (_effectManager)
		this->_effectManager->runMoveCommand(duration, offsetPos, easeType, easeAmt, groupID, lockToPlayerX,
											 lockToPlayerY);
}

void BaseGameLayer::runFollowCommand(float duration, int groupID, bool followX, bool followY)
{
	if (_effectManager)
		_effectManager->runFollowCommand(duration, groupID, followX, followY);
}

void BaseGameLayer::stopGroupActions(int groupID)
{
	if (_effectManager)
		_effectManager->stopGroupActions(groupID);
}

void BaseGameLayer::processMoveActionsStep(float dt)
{
	this->processMoveActions(dt);
}
