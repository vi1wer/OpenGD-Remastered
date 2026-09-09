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
*************************f************************************************/

#include "PlayLayer.h"
#include "AudioEngine.h"
#include "CreatorLayer.h"
#include "EffectGameObject.h"
#include "EndLevelLayer.h"

#include "LevelInfoLayer.h"
#include "LevelPage.h"
#include "LevelSelectLayer.h"
#include "LevelTools.h"
#include "MenuItemSpriteExtra.h"

#include "ImGui/ImGuiPresenter.h"
#include "ImGui/imgui/imgui.h"

#include "external/benchmark.h"
#include "external/json.hpp"
#include "external/constants.h"

#include "LevelDebugLayer.h"
#include "UILayer.h"
#include "PauseLayer.h"
#include "GJGameLevel.h"
#include "GroundLayer.h"
#include "SimpleProgressBar.h"
#include "CircleWave.h"
#include "2d/SpriteFrameCache.h"
#include "2d/ParticleSystemQuad.h"
#include "platform/FileUtils.h"
#include "GameToolbox/log.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/rand.h"
#include "GameToolbox/math.h"
#include "GameToolbox/conv.h"
#include "GameToolbox/nodes.h"
#include "GameManager.h"
#include "EffectManager.h"
#include "GameToolbox/enums.h"

#include <algorithm>

USING_NS_AX;
USING_NS_AX_EXT;

bool showDn = false, noclip = false;

float gameSpeed = 1, fps = 0;

bool fullscreen = false;
int monitorN = 0;

static PlayLayer* Instance = nullptr;

Scene* PlayLayer::scene(GJGameLevel* level)
{
	// return LevelDebugLayer::scene(level);
	auto scene = Scene::create();
	scene->addChild(PlayLayer::create(level));
	return scene;
}

void PlayLayer::showCompleteText()
{
	m_bEndAnimation = true;

	auto size = Director::getInstance()->getWinSize();

	float scale = 1.1f;
	const char* spr = "GJ_levelComplete_001.png";
	/*if (m_isPracticeMode) {
		spr = "GJ_practiceComplete_001.png";
		scale = 1;
	}*/

	auto sprite = Sprite::createWithSpriteFrameName(spr);
	sprite->setScale(0.01f);
	sprite->setPosition({size.width / 2, size.height / 2 + 35});
	m_pHudLayer->addChild(sprite);

	sprite->runAction(Sequence::create(EaseElasticOut::create(ScaleTo::create(0.66f, scale), 0.6),
									   DelayTime::create(0.88f), EaseIn::create(ScaleTo::create(0.22f, 0), 2.0f),
									   RemoveSelf::create(true), nullptr));

	auto col1 = _player1->getMainColor();
	auto col2 = _player1->getSecondaryColor();

	auto par1 = ParticleSystemQuad::create("levelComplete01.plist");
	par1->setPosition(sprite->getPosition());
	par1->setStartColor({(GLfloat)col1.r, (GLfloat)col1.g, (GLfloat)col1.b, 255});
	par1->setEndColor({(GLfloat)col1.r, (GLfloat)col1.g, (GLfloat)col1.b, 0});
	m_pHudLayer->addChild(par1, -1);

	auto par2 = ParticleSystemQuad::create("levelComplete01.plist");
	par2->setPosition(par1->getPosition());
	par2->setStartColor({(GLfloat)col2.r, (GLfloat)col2.g, (GLfloat)col2.b, 255});
	par2->setEndColor({(GLfloat)col2.r, (GLfloat)col2.g, (GLfloat)col2.b, 0});
	m_pHudLayer->addChild(par2, -1);

	auto cir = CircleWave::create(0.8f, {col1.r, col1.g, col1.b, 255}, 5.f, size.width - 10, true, false);
	cir->setPosition({size.width - 10, size.height / 2});
	m_pHudLayer->addChild(cir, -1);

	auto cir2 = CircleWave::create(0.8f, {col1.r, col1.g, col1.b, 255}, 5.f, 250.0f, true, false);
	cir2->setPosition(sprite->getPosition());
	m_pHudLayer->addChild(cir2, -1);

	// for (int i = 0; i < 9; i++)
	// 	m_pHudLayer->runAction(Sequence::createWithTwoActions(
	// 		DelayTime::create(0.16f * i), CallFunc::create([&]() { PlayLayer::spawnCircle(); })));

	// m_pHudLayer->runAction(
	// 	Sequence::createWithTwoActions(DelayTime::create(1.5f), CallFunc::create([&]() { PlayLayer::showEndLayer();
	// })));
	for (int i = 0; i < 9; i++)
	{
		scheduleOnce([&](float d) { spawnCircle(); }, 0.16f * i, "playlayer_circles");
	}
	scheduleOnce([&](float d) { showEndLayer(); }, 1.5f, "playlayer_levelend");
}			

void PlayLayer::spawnCircle()
{
	auto size = Director::getInstance()->getWinSize();
	
	auto minArea = Vec2({40, 70});
	auto maxArea = Vec2({size.width - 40, size.height - 70});

	float x = ((float)rand() / (float)RAND_MAX) * (maxArea.x - minArea.x) + minArea.x;
	float y = ((float)rand() / (float)RAND_MAX) * (maxArea.y - minArea.y) + minArea.y;

	auto col1 = _player1->getMainColor();
	auto cir = CircleWave::create(0.5f, {col1.r, col1.g, col1.b, 255}, 5.f, 50, true, false);
	cir->setPosition({x, y});
	m_pHudLayer->addChild(cir, -1);
}

void PlayLayer::spawnBounceEffect(Vec2 pos, Color4B color, bool isOrb)
{
	Node* parent = _gameLayer ? static_cast<Node*>(_gameLayer) : this;
	if (!parent)
		parent = this;

	// Official pad/orb flash: expanding additive ring at the object.
	const float maxRadius = isOrb ? 55.f : 45.f;
	if (auto* ring = CircleWave::create(0.35f, color, 4.f, maxRadius, true, false, 5.f))
	{
		ring->setPosition(pos);
		parent->addChild(ring, 100);
	}

	// Particle burst comes from the object's bumpEffect/ringEffect in triggerActivated.
	(void)isOrb;
}

void PlayLayer::showEndLayer()
{
	createLevelEnd();
}

int PlayLayer::sectionForPos(float x)
{
	int section = x / 100;
	if (section < 0)
		section = 0;
	return section;
}

PlayLayer* PlayLayer::create(GJGameLevel* level)
{
	auto ret = new (std::nothrow) PlayLayer();
	if (ret && ret->init(level))
	{
		ret->autorelease();
		return ret;
	}

	AX_SAFE_DELETE(ret);
	return nullptr;
}

void PlayLayer::loadLevel(std::string_view levelStr)
{
	std::vector<std::string_view> objData = GameToolbox::splitByDelimStringView(levelStr, ';');
	if (objData.empty())
		return;

	std::vector<std::string_view> levelData = GameToolbox::splitByDelimStringView(objData[0], ',');
	objData.erase(objData.begin());

	_colorChannels[1000] = SpriteColor(ax::Color3B::WHITE, 255, false);
	_colorChannels[1001] = SpriteColor(ax::Color3B::WHITE, 255, false);

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
			_bgID = GameToolbox::stoi(levelData[i + 1]);
			if (!_bgID)
				_bgID = 1;
			_levelSettings._bgID = _bgID;
		}
		else if (levelData[i] == "kA7")
		{
			_groundID = GameToolbox::stoi(levelData[i + 1]);
			if (!_groundID)
				_groundID = 1;
			_levelSettings._groundID = _groundID;
		}
		else if (levelData[i] == "kA2")
		{
			int mode = GameToolbox::stoi(levelData[i + 1]);
			if (mode < 0 || mode > static_cast<int>(PlayerGamemodeSwing))
				mode = 0;
			_levelSettings.gamemode = static_cast<PlayerGamemode>(mode);
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
		else if (levelData[i] == "kA22")
		{
			_levelSettings.platformer = GameToolbox::stoi(levelData[i + 1]) != 0;
		}
	}

	if (!_colorChannels.contains(1004))
		_colorChannels[1004] = {ax::Color3B::WHITE, 255, false};

	if (_player1)
	{
		_colorChannels[1005]._color = _player1->getMainColor();
		_colorChannels[1005]._blending = true;
		_colorChannels[1006]._color = _player1->getSecondaryColor();
		_colorChannels[1006]._blending = true;
	}
	_colorChannels[1010]._color = Color3B::BLACK;
	_colorChannels[1007]._color = getLightBG();
	_originalColors = _colorChannels;

	if (!objData.empty())
	{
		const auto& last = objData.back();
		if (last.empty() || last.front() != '1' || (last.size() > 1 && last[1] != ','))
			objData.pop_back();
	}

	for (std::string_view data : objData)
	{
		if (data.empty())
			continue;

		GameObject* obj = GameObject::createFromString(data);
		if (!obj)
			continue;

		if (obj->_isTrigger)
		{
			obj->setVisible(false);
			obj->setOpacity(0);
		}

		obj->_uniqueID = static_cast<int>(_pObjects.size());
		_pObjects.push_back(obj);
	}
	GameToolbox::log("PlayLayer created {} objects", _pObjects.size());

	_coinsCollected.clear();
	int coinIndex = 0;
	for (GameObject* obj : _pObjects)
	{
		if (!obj || !obj->isCoin())
			continue;
		obj->_coinIndex = coinIndex++;
	}
	_coinsCollected.assign(std::max(coinIndex, 0), false);
}

void PlayLayer::setInstance() {
	Instance = this;
    _instance = this;
}

bool PlayLayer::init(GJGameLevel* level)
{
	if (!Layer::init())
		return false;
	
	setLevel(level);

	_effectManager = EffectManager::create();
	this->addChild(_effectManager);

	setInstance();

    // initBatchNodes();

	auto winSize = Director::getInstance()->getWinSize();

	dn = ax::DrawNode::create();
	addChild(dn, 99999);

	initBatchNodes();

	cameraFollow = ax::Node::create();
	this->addChild(cameraFollow, 100);

	auto gm = GameManager::getInstance();
	int playerIcon = gm->getSelectedIcon(IconType::kIconTypeCube);
	if (playerIcon < 1)
		playerIcon = 1;

	this->_player1 = PlayerObject::create(playerIcon, this);
	this->_player1->setPosition({-20, 105});
	this->addChild(this->_player1, 101);

	this->_player2 = PlayerObject::create(playerIcon, this);
	this->_player2->setPosition({-20, 105});
	this->addChild(this->_player2, 101);

	_player1->setMainColor(gm->getPlayerMainColor());
	_player1->setSecondaryColor(gm->getPlayerSecondaryColor());
	_player1->setGlowColor(gm->getPlayerGlowColor());
	_player1->setGlow(gm->isPlayerGlowEnabled());
	_player2->setMainColor(gm->getPlayerMainColor());
	_player2->setSecondaryColor(gm->getPlayerSecondaryColor());
	_player2->setGlowColor(gm->getPlayerGlowColor());
	_player2->setGlow(gm->isPlayerGlowEnabled());
	_player2->setVisible(false);
	_player2->setActive(false);

	// std::string levelStr = FileUtils::getInstance()->getStringFromFile("level.txt");
	std::string levelStr = level->_levelString;

	if (levelStr.empty())
		levelStr = GJGameLevel::getLevelStrFromID(level->_levelID);
	else if (levelStr.front() != 'k')
	{
		std::string decompressed = GJGameLevel::decompressLvlStr(levelStr);
		levelStr = decompressed.empty() ? GJGameLevel::getLevelStrFromID(level->_levelID) : std::move(decompressed);
	}
	if (levelStr.empty() && level->_levelID >= 1 && level->_levelID <= 22)
		levelStr = GJGameLevel::getLevelStrFromID(level->_levelID);
	if (!levelStr.empty())
		level->_levelString = levelStr;
	GameToolbox::log("PlayLayer load id={} name='{}' str={} bytes", level->_levelID, level->_levelName, levelStr.size());

	// scope based timer
	{
		auto s = BenchmarkTimer("load level");
		if (!levelStr.empty())
			loadLevel(levelStr);
	}

	this->_bottomGround = GroundLayer::create(_groundID);
	this->_ceiling = GroundLayer::create(_groundID);
	if (this->_bottomGround)
		cameraFollow->addChild(this->_bottomGround, 1);
	if (this->_ceiling)
		cameraFollow->addChild(this->_ceiling, 1);

	if (this->_ceiling)
	{
		this->_ceiling->setScaleY(-1);
		_ceiling->setVisible(false);
		if (_ceiling->_sprite)
			_ceiling->setPositionY(winSize.height + _ceiling->_sprite->getTextureRect().size.height);
	}
	if (_bottomGround)
		_bottomGround->setPositionY(-cameraFollow->getPositionY() + 12);

	this->m_pBG = Sprite::create(GameToolbox::getTextureString(fmt::format("game_bg_{:02}_001.png", _bgID)));
	if (!this->m_pBG)
	{
		this->m_pBG = Sprite::create(GameToolbox::getTextureString(fmt::format("game_bg_{:02}_001.png", 1)));
	}
	if (this->m_pBG)
	{
		m_pBG->setStretchEnabled(false);
		if (auto* tex = this->m_pBG->getTexture())
		{
			const Texture2D::TexParams texParams = {
				backend::SamplerFilter::LINEAR, backend::SamplerFilter::LINEAR,
				backend::SamplerAddressMode::REPEAT, backend::SamplerAddressMode::REPEAT};
			tex->setTexParameters(texParams);
		}
		this->m_pBG->setTextureRect(Rect(0, 0, 1024 * 5, 1024));
		this->m_pBG->setPosition(winSize.x / 2, winSize.y / 4);
		this->addChild(this->m_pBG, -100);

		if (this->_colorChannels.contains(1000))
			this->m_pBG->setColor(colorForChannel(1000));
	}
	this->_bottomGround->update(0);

	if (_pObjects.size() != 0)
	{
		this->m_lastObjXPos = 570.0f;

		for (GameObject* object : _pObjects)
		{
			// GameToolbox::log("pos: {}", object->getPositionX());
			if (this->m_lastObjXPos < object->getPositionX())
				this->m_lastObjXPos = object->getPositionX();
		}

		GameToolbox::log("last x: {}", m_lastObjXPos);

		for (size_t i = 0; i < sectionForPos(this->m_lastObjXPos); i++)
		{
			// GameToolbox::log("i = {}", i);
			std::vector<GameObject*> vec;
			_sectionObjects.push_back(vec);
		}

		for (GameObject* object : _pObjects)
		{
			int section = sectionForPos(object->getPositionX());
			section = section - 1 < 0 ? 0 : section - 1;
			while (section >= static_cast<int>(_sectionObjects.size()))
				_sectionObjects.push_back({});
			object->_section = section;
			_sectionObjects[section].push_back(object);

			if (_colorChannels.contains(object->_mainColorChannel) &&
				_colorChannels[object->_mainColorChannel]._blending)
			{
				object->setBlendFunc(GameToolbox::getBlending());
			}

			if (_colorChannels.contains(object->_secColorChannel) &&
				_colorChannels[object->_secColorChannel]._blending)
			{
				for (auto s : object->_childSprites)
					s->setBlendFunc(GameToolbox::getBlending());
			}
			object->setCascadeOpacityEnabled(false);
			object->update();
		}
	}

	m_pHudLayer = UILayer::create();

	m_pBar = SimpleProgressBar::create();
	m_pBar->setPercentage(0.f);
	m_pBar->setPosition({winSize.width / 2, winSize.height - 10});
	m_pHudLayer->addChild(m_pBar);

	m_pBar->setPosition({winSize.width / 2, winSize.height - 10});

	m_pPercentage = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "0%");
	m_pPercentage->setScale(.5f);
	m_pPercentage->setAnchorPoint({0, .5f});
	m_pPercentage->setPosition({m_pBar->getPositionX() + 110, m_pBar->getPositionY() + 1});
	m_pHudLayer->addChild(m_pPercentage);

	this->addChild(m_pHudLayer, 1000);

	// World container so mirror portals can flip gameplay without flipping the HUD.
	_gameLayer = Node::create();
	_gameLayer->setName("gameLayer");
	this->addChild(_gameLayer, 0);
	{
		std::vector<Node*> toReparent;
		for (auto* child : getChildren())
		{
			if (child && child != m_pHudLayer && child != _gameLayer)
				toReparent.push_back(child);
		}
		for (auto* child : toReparent)
		{
			const int z = child->getLocalZOrder();
			child->retain();
			child->removeFromParentAndCleanup(false);
			_gameLayer->addChild(child, z);
			child->release();
		}
	}

	setupCoinHUD();
	applyHudVisibility();

	//bool levelValid = LevelTools::verifyLevelIntegrity(levelStr, this->getLevel()->_levelID);
	constexpr bool levelValid = true;

	if (!levelValid)
	{
		auto loadfailedstr = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "Load Failed!");
		loadfailedstr->setPosition({winSize.width / 2, winSize.height / 2});
		addChild(loadfailedstr, 128);
	}
	
	updateVisibility();
	updateVisibility();

	scheduleOnce(
		[&](float d) {
			if (levelValid)
			{
				incrementTime();
				resetLevel();
			}
			else
			{
				exit();
			}
		},
		1.f, "playlayer_levelstartdelay");

	return true;
}

void PlayLayer::createLevelEnd()
{
	_jumps = _player1->_jumpedTimes;
	auto levelend = EndLevelLayer::create(this);
	addChild(levelend);
}

double lastY = 0;

void PlayLayer::incrementTime()
{
	scheduleOnce(
		[&](float d) {
			_secondsSinceStart++;
			incrementTime();
		},
		1.f, "playlayer_stopwatch");
}

void PlayLayer::update(float dt)
{

	if (m_freezePlayer)
	{
		AudioEngine::pauseAll();
		return;
	}
	else
	{
		AudioEngine::resumeAll();
	}

	float step = std::min(2.0f, dt * 60.0f);

	_player1->m_bIsPlatformer = m_platformerMode;
	_player1->noclip = noclip;
	_player2->noclip = noclip;

	auto winSize = Director::getInstance()->getWinSize();

	if (this->_colorChannels.contains(1005)) this->_colorChannels.at(1005)._color = this->_player1->getMainColor();
	if (this->_colorChannels.contains(1006)) this->_colorChannels.at(1006)._color = this->_player1->getSecondaryColor();

	_colorChannels[1007]._color = getLightBG();

	const bool gameplayActive = _player1 && !_player1->isDead() && (!_isDualMode || (_player2 && !_player2->isDead()));
	if (!m_freezePlayer && gameplayActive)
	{
		_player1->storeShipRotationPos();
		if (_isDualMode)
			_player2->storeShipRotationPos();

		step /= 4.0f;
		lastY = _player1->getYVel();
		for (int i = 0; i < 4; i++)
		{
			this->_player1->update(step);

			_player1->setOuterBounds(Rect(_player1->getPosition() - Vec2(15.f, 15.f), Vec2(30.f, 30.f)));
			_player1->setInnerBounds(Rect(_player1->getPosition() - Vec2(3.75f, 3.75f), Vec2(7.5f, 7.5f)));

			this->checkCollisions(_player1, step);

			if (this->_player1->isDead())
				break;

			if (!_isDualMode)
				continue;

			this->_player2->update(step);

			_player2->setOuterBounds(Rect(_player2->getPosition() - Vec2(15.f, 15.f), Vec2(30.f, 30.f)));
			_player2->setInnerBounds(Rect(_player2->getPosition() - Vec2(3.75f, 3.75f), Vec2(7.5f, 7.5f)));

			this->checkCollisions(_player2, step);

			if (this->_player2->isDead())
				break;
		}
		step *= 4.0f;

		auto* gm = GameManager::getInstance();
		if (gm && gm->_autoCheckpoints && _isPracticeMode && _player1 && !_player1->isDead() &&
			_player1->isOnGround() && _player1->isGroundedMode())
		{
			const float x = _player1->getPositionX();
			if (x - _lastAutoCheckpointX >= 50.f)
				markCheckpoint();
		}
	}

	if (_effectManager)
	{
		if (_player1)
		{
			const Vec2 lastP = _player1->getLastP();
			_effectManager->_xAccel = _player1->getPositionX() - lastP.x;
			_effectManager->_yAccel = _player1->getPositionY() - lastP.y;
			_player1->setLastP(_player1->getPosition());
		}
		_effectManager->prepareMoveActions(step / 60.f, false);
		processMoveActionsStep(step);
	}

	m_pBar->setPercentage(_player1->getPositionX() / this->m_lastObjXPos * 100.f);
	float val = m_pBar->getPercentage();
	m_pPercentage->setString(StringUtils::format("%.02f%%", val > 100 ? 100 : val < 0 ? 0 : val));

	if (val >= 100 && !m_bEndAnimation)
	{
		recordAttemptProgress(true);
		this->showCompleteText();
	}

	this->updateVisibility();
	this->updateCamera(step);
	if (_player1->_currentGamemode == PlayerGamemodeShip || _player1->_currentGamemode == PlayerGamemodeUFO)
		_player1->updateShipRotation(step);
	if (_isDualMode && (_player2->_currentGamemode == PlayerGamemodeShip || _player2->_currentGamemode == PlayerGamemodeUFO))
		_player2->updateShipRotation(step);

	_colorChannels[1005]._color = _player1->getMainColor();
	_colorChannels[1006]._color = _player1->getSecondaryColor();
	_colorChannels[1007]._color = getLightBG();
	if (m_pBG)
		m_pBG->setColor(colorForChannel(1000));
}

void PlayLayer::applyHudVisibility()
{
	auto* gm = GameManager::getInstance();
	const bool bar = !gm || gm->_showProgressBar;
	const bool pct = !gm || gm->_showPercentage;
	if (m_pBar)
		m_pBar->setVisible(bar);
	if (m_pPercentage)
	{
		m_pPercentage->setVisible(pct);
		if (m_pBar)
			m_pPercentage->setPosition({m_pBar->getPositionX() + (bar ? 110.f : 0.f), m_pBar->getPositionY() + 1});
	}
	showDn = gm && gm->_showHitboxes;
	if (dn && !_freezeHitboxesOnDeath)
		dn->setVisible(showDn);
}

ax::Color3B PlayLayer::getLightBG()
{
	return colorForChannel(1000);
}

void PlayLayer::destroyPlayer(PlayerObject* player)
{
	if (player->isDead() || player->noclip)
		return;

	player->setIsDead(true);
	player->playDeathEffect();
	player->stopRotation();
	player->setVisible(false);
	fireOnDeathTriggers();
	recordAttemptProgress(false);

	auto* gm = GameManager::getInstance();
	if (gm && gm->_showHitboxesOnDeath)
	{
		_freezeHitboxesOnDeath = true;
		showDn = true;
		if (dn)
			dn->setVisible(true);
	}

	scheduleOnce([&](float d) { resetLevel(); }, 1.f, "playlayer_restart");
}

int PlayLayer::currentPercent() const
{
	if (m_lastObjXPos <= 0.f || !_player1)
		return 0;
	const float val = _player1->getPositionX() / m_lastObjXPos * 100.f;
	return std::clamp(static_cast<int>(val), 0, 100);
}

void PlayLayer::recordAttemptProgress(bool completed)
{
	auto* level = getLevel();
	auto* gm = GameManager::getInstance();
	if (!level || !gm)
		return;

	const int percent = completed ? 100 : currentPercent();
	const int previousNormalBest = !_isPracticeMode ? gm->getLevelBest(level->_levelID, false) : 0;
	if (gm->recordLevelProgress(level->_levelID, percent, _isPracticeMode))
	{
		if (_isPracticeMode)
			level->_practicePercent = static_cast<float>(percent);
		else
		{
			level->_normalPercent = static_cast<float>(percent);
			if (percent > 0)
				showNewBest(percent);
		}
	}

	if (completed)
	{
		int mask = 0;
		for (size_t i = 0; i < _coinsCollected.size() && i < 8; i++)
		{
			if (_coinsCollected[i])
				mask |= (1 << static_cast<int>(i));
		}
		if (mask)
			gm->recordLevelCoins(level->_levelID, mask);

		// First normal clear → mana orbs (keys drop every 500 orbs) + bonus key for demons.
		if (!_isPracticeMode && !_testMode && previousNormalBest < 100)
		{
			const int orbGain = std::max(10, level->_stars * 20);
			gm->addOrbs(orbGain);
			if (level->_demon)
				gm->addDemonKeys(1);
			gm->save();
		}
	}
}

void PlayLayer::showNewBest(int percent)
{
	if (!m_pHudLayer)
		return;

	if (_newBestBanner)
	{
		_newBestBanner->stopAllActions();
		_newBestBanner->removeFromParent();
		_newBestBanner = nullptr;
	}

	const auto winSize = Director::getInstance()->getWinSize();
	auto* banner = Node::create();
	banner->setCascadeOpacityEnabled(true);
	banner->setPosition({winSize.width / 2.f, winSize.height / 2.f + 36.f});
	m_pHudLayer->addChild(banner, 80);
	_newBestBanner = banner;

	float percentY = -28.f;
	if (auto* spr = Sprite::createWithSpriteFrameName("GJ_newBest_001.png"))
	{
		spr->setPosition({0.f, 0.f});
		banner->addChild(spr);
		percentY = -spr->getContentSize().height * 0.5f - 10.f;
	}

	auto* label = Label::createWithBMFont(
		GameToolbox::getTextureString("goldFont.fnt"),
		StringUtils::format("%i%%", percent));
	label->setScale(0.58f);
	label->setAnchorPoint({0.5f, 1.f});
	label->setPosition({0.f, percentY});
	banner->addChild(label, 1);

	banner->setScale(0.01f);
	banner->runAction(Sequence::create(
		EaseElasticOut::create(ScaleTo::create(0.55f, 1.f), 0.6f),
		DelayTime::create(1.25f),
		FadeTo::create(0.35f, 0),
		CallFunc::create([this, banner]() {
			if (_newBestBanner == banner)
				_newBestBanner = nullptr;
			if (banner->getParent())
				banner->removeFromParent();
		}),
		nullptr));
}

void PlayLayer::setupCoinHUD()
{
	for (auto* spr : _coinHUD)
	{
		if (spr)
			spr->removeFromParent();
	}
	_coinHUD.clear();
	if (!m_pHudLayer || _coinsCollected.empty())
		return;

	const auto winSize = Director::getInstance()->getWinSize();
	int userCount = 0;
	int secretCount = 0;
	for (GameObject* obj : _pObjects)
	{
		if (!obj || !obj->isCoin())
			continue;
		if (obj->getID() == 1329)
			userCount++;
		else
			secretCount++;
	}

	const char* frame = (userCount > 0 && secretCount == 0) ? "secretCoinUI2_001.png" : "secretCoinUI_001.png";
	if (!SpriteFrameCache::getInstance()->getSpriteFrameByName(frame))
		frame = (userCount > 0 && secretCount == 0) ? "GJ_coinsIcon2_001.png" : "GJ_coinsIcon_001.png";

	const int count = static_cast<int>(_coinsCollected.size());
	// Secret / user coin icons sit in the bottom-right corner of the HUD.
	const float startX = winSize.width - 26.f - (count - 1) * 26.f;
	const float coinY = 32.f;
	for (int i = 0; i < count; i++)
	{
		auto* icon = Sprite::createWithSpriteFrameName(frame);
		if (!icon)
			continue;
		icon->setScale(0.5f);
		icon->setPosition({startX + i * 26.f, coinY});
		m_pHudLayer->addChild(icon, 20);
		_coinHUD.push_back(icon);
	}
	refreshCoinHUD();
}

void PlayLayer::refreshCoinHUD()
{
	for (size_t i = 0; i < _coinHUD.size(); i++)
	{
		if (!_coinHUD[i])
			continue;
		const bool got = i < _coinsCollected.size() && _coinsCollected[i];
		_coinHUD[i]->setColor(got ? Color3B::WHITE : Color3B(70, 70, 70));
		_coinHUD[i]->setOpacity(got ? 255 : 160);
	}
}

void PlayLayer::pickupCoin(GameObject* obj, PlayerObject* player)
{
	if (!obj || !player || obj->hasBeenActivatedByPlayer(player))
		return;

	obj->triggerActivated(player);
	obj->stopAllActions();
	obj->setVisible(false);
	obj->setOpacity(0);

	if (obj->_coinIndex >= 0 && obj->_coinIndex < static_cast<int>(_coinsCollected.size()))
		_coinsCollected[obj->_coinIndex] = true;
	refreshCoinHUD();

	if (obj->_coinIndex >= 0 && obj->_coinIndex < static_cast<int>(_coinHUD.size()) && _coinHUD[obj->_coinIndex])
	{
		auto* icon = _coinHUD[obj->_coinIndex];
		icon->stopAllActions();
		icon->setScale(0.2f);
		icon->runAction(EaseElasticOut::create(ScaleTo::create(0.45f, 0.5f), 0.6f));
	}

	const Vec2 pos = obj->getPosition();
	if (auto* burst = ParticleSystemQuad::create("coinPickupEffect.plist"))
	{
		burst->setPosition(pos);
		burst->setAutoRemoveOnFinish(true);
		burst->setScale(1.6f);
		addChild(burst, 120);
	}
	if (auto* burst2 = ParticleSystemQuad::create("coinEffect.plist"))
	{
		burst2->setPosition(pos);
		burst2->setAutoRemoveOnFinish(true);
		addChild(burst2, 120);
	}

	if (auto* flash = Sprite::createWithSpriteFrameName(
			obj->getID() == 1329 ? "secretCoin_2_01_001.png" : "secretCoin_01_001.png"))
	{
		flash->setPosition(pos);
		flash->setBlendFunc(GameToolbox::getBlending());
		addChild(flash, 121);
		flash->runAction(Sequence::create(
			Spawn::createWithTwoActions(ScaleTo::create(0.28f, 1.8f), FadeOut::create(0.28f)),
			RemoveSelf::create(true),
			nullptr));
	}

	if (auto* wave = CircleWave::create(0.35f, {255, 220, 60, 255}, 8.f, 70.f, true, false))
	{
		wave->setPosition(pos);
		addChild(wave, 119);
	}

	const char* sounds[] = {"gold01.ogg", "coin01.ogg", "secretCoin01.ogg", "endStart_02.ogg", "playSound_01.ogg"};
	for (const char* sound : sounds)
	{
		if (FileUtils::getInstance()->isFileExist(sound) ||
			FileUtils::getInstance()->isFileExist(GameToolbox::getTextureString(sound)))
		{
			AudioEngine::play2d(sound, false, 0.55f);
			break;
		}
	}
}

void PlayLayer::updateCamera(float dt)
{
	auto winSize = Director::getInstance()->getWinSize();
	Vec2 cam = m_obCamPos;

	PlayerObject* player = _player1;
	Vec2 pPos = player->getPosition();

	if (player->_currentGamemode != PlayerGamemodeCube || _isDualMode)
	{
		cam.y = (winSize.height * -0.5f) + m_fCameraYCenter;
		if (cam.y <= 0.0f)
			cam.y = 0.0f;
	}
	else
	{
		float unk2 = 90.0f;
		float unk3 = 120.0f;
		if (player->isGravityFlipped())
		{
			unk2 = 120.0f;
			unk3 = 90.0f;
		}
		if (pPos.y <= winSize.height + cam.y - unk2)
		{
			if (pPos.y < unk3 + cam.y)
				cam.y = pPos.y - unk3;
		}
		else
			cam.y = pPos.y - winSize.height + unk2;
		if (!player->isGravityFlipped())
		{
			Vec2 lastGroundPos = player->getLastGroundPos();

			if (lastGroundPos.y == 105.f)
				if (pPos.y <= cam.y + winSize.height - unk2)
					cam.y = 0.0f;
		}
	}

	cam.y = clampf(cam.y, 0.0f, 1140.f - winSize.height);

	if (pPos.x >= winSize.width / 2.5f && !_player1->isDead() && !_player2->isDead() &&
		!player->m_bIsPlatformer) // wrong but works for now
	{
		this->m_pBG->setPositionX(this->m_pBG->getPositionX() -
								  dt * player->getPlayerSpeed() * _bottomGround->getSpeed() * 0.1175f);
		if (_bottomGround)
			_bottomGround->update(dt * player->getPlayerSpeed());
		if (_ceiling)
			_ceiling->update(dt * player->getPlayerSpeed());
		cam.x = pPos.x - (winSize.width / 2.5f);
	}
	else if (player->m_bIsPlatformer)
		cam.x = pPos.x - winSize.width / 2.f;

	if (this->m_pBG->getPosition().x <= cam.x - 1024.f)
		this->m_pBG->setPositionX(this->m_pBG->getPositionX() + 1024.f);

	this->m_pBG->setPositionX(this->m_pBG->getPositionX() + (cam.x - m_obCamPos.x));

	if (!this->m_bMoveCameraX)
		m_obCamPos.x = cam.x;

	// if camera reset then do not lerp
	if (!this->m_bMoveCameraY && cam.x != 0)
	{
		m_obCamPos.y = GameToolbox::iLerp(m_obCamPos.y, cam.y, 0.1f, dt / 60.f);
	}
	else
	{
		m_obCamPos.y = cam.y;
	}

	Vec2 camShake{0.f, 0.f};
	if (_shakeTime > 0.f)
	{
		_shakeTime -= dt / 60.f;
		const float mag = _shakeStrength > 0.f ? _shakeStrength : 4.f;
		camShake.x = GameToolbox::randomFloat(static_cast<int>(-mag), static_cast<int>(mag));
		camShake.y = GameToolbox::randomFloat(static_cast<int>(-mag), static_cast<int>(mag));
	}

	Camera::getDefaultCamera()->setPosition(this->m_obCamPos + winSize / 2 + camShake);

	cameraFollow->setPosition(m_obCamPos);
	const bool flying = _player1->isFlying() || _player1->_currentGamemode == PlayerGamemodeBall;
	if (_ceiling)
		_ceiling->setVisible(flying || _isDualMode);
	if (_player1->_currentGamemode == PlayerGamemodeCube && !_isDualMode)
		_bottomGround->setPositionY(-cameraFollow->getPositionY() + 12);

	if (m_pHudLayer)
		m_pHudLayer->setPosition(this->m_obCamPos);

	// Animate mirror portal: slide objects across, flip at midpoint (like official GD).
	const float mirrorTarget = _isMirror ? 1.f : 0.f;
	const float mirrorStep = (dt / 60.f) / kMirrorAnimDuration;
	if (_mirrorVisual < mirrorTarget)
		_mirrorVisual = std::min(mirrorTarget, _mirrorVisual + mirrorStep);
	else if (_mirrorVisual > mirrorTarget)
		_mirrorVisual = std::max(mirrorTarget, _mirrorVisual - mirrorStep);

	applyMirrorVisual(winSize.width);
}

float PlayLayer::getRelativeMod(Vec2 pos, float v1, float v2, float v3)
{
	auto winSize = ax::Director::getInstance()->getWinSize();
	float camX = m_obCamPos.x;
	float centerX = winSize.width / 2.f;
	float camXCenter = camX + centerX;
	float posX = pos.x;

	float vv1;
	float vv2;
	float vv3;
	float result;

	if (posX <= camXCenter)
	{
		vv2 = v2;
		vv3 = (camXCenter - posX) - v3;
	}
	else
	{
		vv1 = ((posX - v3) - camX) - centerX;
		vv2 = v1;
		vv3 = vv1;
	}
	if (vv2 < 1.f)
		vv2 = 1.f;

	result = (centerX - vv3) / vv2;

	return result;
}

void PlayLayer::applyEnterEffect(GameObject* obj)
{
	if (obj->getEnterEffectID() != _enterEffectID)
		obj->setEnterEffectID(_enterEffectID);
	Vec2 objStartPos = obj->getStartPosition();
	Vec2 objStartScale = obj->getStartScale();
	float rModn = getRelativeMod(objStartPos, 60.f, 60.f, 0.f);
	float rMod = clampf(rModn, 0.f, 1.f);

	switch (obj->getEnterEffectID())
	{
	case 2:
		if (obj->getGameObjectType() != kGameObjectTypeYellowJumpPad)
		{
			obj->setScaleX(rMod * objStartScale.x);
			obj->setScaleY(rMod * objStartScale.y);
		}
		break;
	case 3:
		if (obj->getGameObjectType() != kGameObjectTypeYellowJumpPad)
		{
			obj->setScaleX((2.f - rMod) * objStartScale.x);
			obj->setScaleY((2.f - rMod) * objStartScale.y);
		}
		break;
	case 4:
		if (obj->getGameObjectType() != kGameObjectTypeYellowJumpPad)
			obj->setPositionY((1.0 - rMod) * 100.f + objStartPos.y);
		break;
	case 5:
		if (obj->getGameObjectType() != kGameObjectTypeYellowJumpPad)
			obj->setPositionY((1.0 - rMod) * -100.f + objStartPos.y);
		break;
	case 6:
		if (obj->getGameObjectType() != kGameObjectTypeYellowJumpPad)
			obj->setPositionX((1.0 - rMod) * -100.f + objStartPos.x);
		break;
	case 7:
		if (obj->getGameObjectType() != kGameObjectTypeYellowJumpPad)
			obj->setPositionX((1.0 - rMod) * 100.f + objStartPos.x);
		break;
	default:
		obj->setPosition(objStartPos);
		break;
	}
	obj->setEnterEffectID(0);
	obj->refreshCollisionBounds();
}

void PlayLayer::updateVisibility()
{
	auto winSize = ax::Director::getInstance()->getWinSize();

	float unk = 70.0f;

	int prevSection;
	int nextSection;
	if (_editorVisibilityAllSections)
	{
		prevSection = 0;
		nextSection = static_cast<int>(_sectionObjects.size());
	}
	else
	{
		prevSection = floorf(this->m_obCamPos.x / 100) - 1.0f;
		nextSection = ceilf((this->m_obCamPos.x + winSize.width) / 100) + 1.0f;
	}

	for (int i = prevSection; i < nextSection; i++)
	{
		if (i >= 0)
		{
			if (i < _sectionObjects.size())
			{
				auto section = _sectionObjects[i];
				for (size_t j = 0; j < section.size(); j++)
				{
					GameObject* obj = section[j];
					if (!obj)
						continue;

					if (obj->getParent() == nullptr)
						attachGameObject(obj);

					obj->setActive(true);
					obj->update();

					// if (obj->getType() == kBallFrame || obj->getType() ==
					// kYellowJumpRing)
					//	 obj->setScale(this->getAudioEffectsLayer()->getAudioScale())

					// auto pos = obj->getPosition();

					float unk2 = 0.0f;
					if (obj->getGameObjectType() == kGameObjectTypeDecoration)
						unk2 = obj->getTextureRect().origin.x * abs(obj->getScaleX()) * 0.4f;

					float opacity = clampf(getRelativeMod(obj->getPosition(), 70.f, 70.f, unk2), 0.f, 1.f);
					if (!obj->getDontTransform())
					{
						obj->_effectOpacityMultipler = opacity;
						this->applyEnterEffect(obj);
					}
				}
			}
		}
	}

	if (!_editorVisibilityAllSections && _prevSection - 1 >= 0 && _sectionObjects.size() != 0 && _prevSection <= _sectionObjects.size())
	{
		auto section = _sectionObjects[_prevSection - 1];
		for (size_t j = 0; j < section.size(); j++)
		{
			if (!section[j])
				continue;
			section[j]->setActive(false);
			detachGameObject(section[j]);
		}
	}

	this->_prevSection = prevSection;
	this->_nextSection = nextSection;
}

void PlayLayer::applyLevelStartGamemode(PlayerObject* player, PlayerGamemode gameMode)
{
	player->setRotation(0.f);
	player->setGamemode(gameMode);

	switch (gameMode)
	{
	case PlayerGamemodeShip:
	case PlayerGamemodeUFO:
	case PlayerGamemodeWave:
	case PlayerGamemodeSwing:
		m_fCameraYCenter = 240.0f;
		tweenBottomGround(-68);
		tweenCeiling(388);
		break;
	case PlayerGamemodeBall:
		m_fCameraYCenter = 210.0f;
		tweenBottomGround(-38);
		tweenCeiling(358);
		break;
	default:
		break;
	}
}

void PlayLayer::setDualMode(bool dual)
{
	_isDualMode = dual;

	if (dual)
	{
		if (m_fCameraYCenter <= 0.f)
			m_fCameraYCenter = 240.0f;

		_player2->setPosition(_player1->getPosition());
		_player2->setVisible(true);
		_player2->setActive(true);
		_player2->reset();
		_player2->flipGravity(!_player1->isGravityFlipped());
		_player2->setGamemode(_player1->_currentGamemode);
		_player2->toggleMini(_player1->_mini);
		_player2->m_dXVel = _player1->m_dXVel;
		_player2->setPlayerSpeed(_player1->getPlayerSpeed());

		if (_player1->_currentGamemode == PlayerGamemodeBall)
		{
			if (_bottomGround)
				_bottomGround->setPositionY(-38.f);
			if (_ceiling)
				_ceiling->setPositionY(358.f);
			tweenBottomGround(-38);
			tweenCeiling(358);
		}
		else
		{
			if (_bottomGround)
				_bottomGround->setPositionY(-68.f);
			if (_ceiling)
				_ceiling->setPositionY(388.f);
			tweenBottomGround(-68);
			tweenCeiling(388);
		}
		if (_ceiling)
			_ceiling->setVisible(true);

		// Same as official GD: spawn on P1 and let flipped gravity carry P2 up to the ceiling.
		_player2->setIsOnGround(false);
		_player2->setYVel(0.f);
	}
	else
	{
		_player2->setVisible(false);
		_player2->setActive(false);
		_player2->flipGravity(false);
		const bool keepFlyBox = _player1 && (_player1->isFlying() || _player1->_currentGamemode == PlayerGamemodeBall);
		if (_ceiling && !keepFlyBox)
			_ceiling->setVisible(false);
		if (!keepFlyBox && _bottomGround && cameraFollow)
			_bottomGround->setPositionY(-cameraFollow->getPositionY() + 12);
	}
}

void PlayLayer::changeGameMode(GameObject* obj, PlayerObject* player, PlayerGamemode gameMode)
{
	if (!obj || !player)
		return;
	obj->triggerActivated(player);
	switch (gameMode)
	{
	case PlayerGamemodeShip:
	case PlayerGamemodeUFO:
	case PlayerGamemodeWave:
	case PlayerGamemodeSwing:
		if (obj->getPositionY() < 270)
			m_fCameraYCenter = 240.0f;
		else
			m_fCameraYCenter = (floorf(obj->getPositionY() / 30.0f) * 30.0f);

		tweenBottomGround(-68);
		tweenCeiling(388);
		break;
	case PlayerGamemodeBall:
		if (obj->getPositionY() < 240.0f)
			m_fCameraYCenter = 210.0f;
		else
			m_fCameraYCenter = (floorf(obj->getPositionY() / 30.0f) * 30.0f);

		tweenBottomGround(-38);
		tweenCeiling(358);
		break;
	case PlayerGamemodeCube:
	case PlayerGamemodeRobot:
	case PlayerGamemodeSpider:
		if (_ceiling)
			_ceiling->setVisible(_isDualMode);
		if (_isDualMode)
		{
			if (_bottomGround)
				_bottomGround->setPositionY(-68.f);
			if (_ceiling)
				_ceiling->setPositionY(388.f);
		}
		else if (_bottomGround && cameraFollow)
			_bottomGround->setPositionY(-cameraFollow->getPositionY() + 12);
		break;
	default:
		break;
	}

	player->setRotation(0.f);
	player->setGamemode(gameMode);
}

void PlayLayer::moveCameraToPos(Vec2 pos)
{
	auto moveX = [this](float a, float b, float c) -> void {
		this->stopActionByTag(0);
		auto tweenAction = ActionTween::create(b, "cTX", m_obCamPos.x, a);
		auto easeAction = EaseInOut::create(tweenAction, c);
		easeAction->setTag(0);
		this->runAction(easeAction);
	};
	auto moveY = [this](float a, float b, float c) -> void {
		this->stopActionByTag(1);
		auto tweenAction = ActionTween::create(b, "cTY", m_obCamPos.y, a);
		auto easeAction = EaseInOut::create(tweenAction, c);
		easeAction->setTag(1);
		this->runAction(easeAction);
	};
	moveX(pos.x, 1.2f, 1.8f);
	moveY(pos.y, 1.2f, 1.8f);
}

void PlayLayer::checkCollisions(PlayerObject* player, float dt)
{
	player->beginSlopePass();

	auto playerOuterBounds = player->_mini ? player->getOuterBounds(0.6f, 0.6f) : player->getOuterBounds();
	player->setTouchedRing(nullptr);
	if (player->getPositionY() < (player->_mini ? 99.f : 105.0f) && player->isGroundedMode())
	{
		if (player->isGravityFlipped())
		{
			this->destroyPlayer(player);
			return;
		}

		player->setPositionY((player->_mini ? 99.f : 105.0f));

		player->hitGround(false);
	}

	else if (player->getPositionY() > 1290.0f)
	{
		this->destroyPlayer(player);
		return;
	}

	if (_isDualMode && player->isGroundedMode())
	{
		const float shift = m_fCameraYCenter > 0.f ? (m_fCameraYCenter - 240.f) : 0.f;
		const float dualCeil = (player->_mini ? 249.f : 255.f) + shift;
		if (player->getPositionY() > dualCeil)
		{
			if (!player->isGravityFlipped())
			{
				this->destroyPlayer(player);
				return;
			}
			player->setPositionY(dualCeil);
			player->hitGround(true);
		}
	}

	if (player->isFlying() || player->_currentGamemode == PlayerGamemodeBall)
	{
		const float worldFloor = flyingFloorY(player);
		const float worldCeil = flyingCeilY(player);
		if (player->getPositionY() < worldFloor)
		{
			player->setPositionY(worldFloor);

			if (!player->isGravityFlipped())
				player->hitGround(false);

			player->setYVel(0.f);
		}
		if (player->getPositionY() > worldCeil)
		{
			player->setPositionY(worldCeil);

			if (player->isGravityFlipped())
				player->hitGround(true);

			player->setYVel(0.f);
		}
	}

	// Ground/ceiling snap moves the player; collision must use the new box
	// or the ship visually sits in spikes while the hitbox is still below them.
	player->setOuterBounds(Rect(player->getPosition() - Vec2(15.f, 15.f), Vec2(30.f, 30.f)));
	player->setInnerBounds(Rect(player->getPosition() - Vec2(3.75f, 3.75f), Vec2(7.5f, 7.5f)));
	playerOuterBounds = player->_mini ? player->getOuterBounds(0.6f, 0.6f) : player->getOuterBounds();

	auto* gm = GameManager::getInstance();
	const bool drawBoxes = (gm && gm->_showHitboxes) || _freezeHitboxesOnDeath;
	showDn = drawBoxes;

	dn->setVisible(drawBoxes);

	if (showDn)
	{
		dn->clear();
		renderRect(playerOuterBounds, ax::Color4B::RED);
		renderRect(player->getInnerBounds(), ax::Color4B::GREEN);
	}

	int current_section = this->sectionForPos(player->getPositionX());

	std::deque<GameObject*> m_pHazards;

	player->clearLetterBlockFlags();

	for (int i = current_section - 2; i <= current_section + 1; i++)
	{
		if (i < _sectionObjects.size() && i >= 0)
		{
			std::vector<GameObject*> section = _sectionObjects[i];

			for (int j = 0; j < section.size(); j++)
			{
				GameObject* obj = section[j];

				if (!obj)
					continue;

				if (obj->wantsCollisionBounds())
					obj->refreshCollisionBounds();

				if (obj->isCoin())
				{
					if (obj->hasBeenActivatedByPlayer(player))
						continue;
					Rect coinBounds = obj->getOuterBounds();
					if (coinBounds.size.width <= 0.f || coinBounds.size.height <= 0.f)
						coinBounds = Rect(obj->getPosition() - Vec2(22.f, 22.f), Vec2(44.f, 44.f));
					else
					{
						coinBounds.origin -= Vec2(6.f, 6.f);
						coinBounds.size += Vec2(12.f, 12.f);
					}
					if (playerOuterBounds.intersectsRect(coinBounds))
						pickupCoin(obj, player);
					continue;
				}

				if (obj->isLetterBlock())
				{
					if (playerOuterBounds.intersectsRect(obj->getLetterBlockBounds()))
						player->applyLetterBlock(obj);
					continue;
				}

				const GameObjectType earlyType = obj->getGameObjectType();
				if (earlyType == kGameObjectTypeCubePortal || earlyType == kGameObjectTypeShipPortal ||
					earlyType == kGameObjectTypeBallPortal || earlyType == kGameObjectTypeUfoPortal ||
					earlyType == kGameObjectTypeWavePortal || earlyType == kGameObjectTypeRobotPortal ||
					earlyType == kGameObjectTypeSpiderPortal || earlyType == kGameObjectTypeSwingPortal ||
					earlyType == kGameObjectTypeInverseMirrorPortal || earlyType == kGameObjectTypeNormalMirrorPortal ||
					earlyType == kGameObjectTypeDualPortal || earlyType == kGameObjectTypeSoloPortal)
				{
					if (obj->hasBeenActivatedByPlayer(player))
						continue;
					Rect portalBounds = obj->getOuterBounds();
					if (portalBounds.size.width <= 0.f || portalBounds.size.height <= 0.f)
						portalBounds = Rect(obj->getPosition() - Vec2(20.f, 50.f), Vec2(40.f, 100.f));
					// A ship on the floor (y=105) misses the official 86px portal by ~2px.
					portalBounds.origin -= Vec2(8.f, 16.f);
					portalBounds.size += Vec2(16.f, 32.f);
					if (playerOuterBounds.intersectsRect(portalBounds))
					{
						player->setPortalP(obj->getPosition());
						player->setPortalObject(obj);
						switch (earlyType)
						{
						case kGameObjectTypeInverseMirrorPortal:
							obj->triggerActivated(player);
							setMirror(true);
							break;
						case kGameObjectTypeNormalMirrorPortal:
							obj->triggerActivated(player);
							setMirror(false);
							break;
						case kGameObjectTypeShipPortal:
							changeGameMode(obj, player, PlayerGamemodeShip);
							break;
						case kGameObjectTypeBallPortal:
							changeGameMode(obj, player, PlayerGamemodeBall);
							break;
						case kGameObjectTypeUfoPortal:
							changeGameMode(obj, player, PlayerGamemodeUFO);
							break;
						case kGameObjectTypeCubePortal:
							changeGameMode(obj, player, PlayerGamemodeCube);
							break;
						case kGameObjectTypeWavePortal:
							changeGameMode(obj, player, PlayerGamemodeWave);
							break;
						case kGameObjectTypeRobotPortal:
							changeGameMode(obj, player, PlayerGamemodeRobot);
							break;
						case kGameObjectTypeSpiderPortal:
							changeGameMode(obj, player, PlayerGamemodeSpider);
							break;
						case kGameObjectTypeSwingPortal:
							changeGameMode(obj, player, PlayerGamemodeSwing);
							break;
						case kGameObjectTypeDualPortal:
							obj->triggerActivated(player);
							setDualMode(true);
							break;
						case kGameObjectTypeSoloPortal:
							obj->triggerActivated(player);
							setDualMode(false);
							break;
						default:
							break;
						}
					}
					continue;
				}

				auto objBounds = obj->getOuterBounds();
				const GameObjectType objType = obj->getGameObjectType();

				if (objType == kGameObjectTypeDecoration || objType == kGameObjectTypeSpecial ||
					objType == kGameObjectTypeLetterD || objType == kGameObjectTypeLetterJ ||
					objType == kGameObjectTypeLetterS || objType == kGameObjectTypeLetterH ||
					objType == kGameObjectTypeLetterF || !obj->wantsCollisionBounds())
					continue;

				if (obj->_isTrigger)
				{
					if (objBounds.size.width <= 0.f || objBounds.size.height <= 0.f)
						continue;
					if (playerOuterBounds.intersectsRect(objBounds))
					{
						auto* touchTrigger = dynamic_cast<EffectGameObject*>(obj);
						if (touchTrigger && touchTrigger->_touchTriggered)
							touchTrigger->triggerActivated(dt);
					}
					continue;
				}

				if (objType == kGameObjectTypeHazard)
				{
					if (objBounds.size.width <= 0.f || objBounds.size.height <= 0.f)
					{
						const Hitbox hb = GameObject::resolveHitbox(obj->getID());
						if (hb.w > 0.f && hb.h > 0.f)
							objBounds = Rect(obj->getPosition() + Vec2(hb.x, hb.y), Vec2(hb.w, hb.h));
						else
							objBounds = Rect(obj->getPosition() + Vec2(-3.f, -6.f), Vec2(6.f, 12.f));
						obj->setOuterBounds(objBounds);
					}
				}

				if ((objBounds.size.width <= 0 || objBounds.size.height <= 0) && obj->_radius <= 0)
					continue;

				if (objType == kGameObjectTypeHazard)
				{
					m_pHazards.push_back(obj);
					if (showDn)
					{
						if (obj->_radius <= 0)
							renderRect(objBounds, ax::Color4B::RED);
						else
							dn->drawCircle(obj->getPosition(), obj->_radius, 0, 20, 0, ax::Color4B::RED);
					}
				}
				else if (objType == kGameObjectTypeSolid || objType == kGameObjectTypeSlope ||
						 objType == kGameObjectTypeCollisionObject)
				{
					if (showDn)
						renderRect(objBounds, ax::Color4B::BLUE);

					if (playerOuterBounds.intersectsRect(objBounds))
					{
						if (objType == kGameObjectTypeSlope)
							player->collidedWithSlope(dt, obj);
						else
							player->collidedWithObject(dt, obj);
					}
				}
				else if (playerOuterBounds.intersectsRect(objBounds))
				{
					if (objType != kGameObjectTypeSolid && objType != kGameObjectTypeSlope &&
						obj->hasBeenActivatedByPlayer(player))
						continue;

					switch (objType)
					{
						case kGameObjectTypeInverseGravityPortal:
							// if (!player->isGravityFlipped())
							//	 this->playGravityEffect(true);
							obj->triggerActivated(player);
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);

							changeGravity(true);
							break;

						case kGameObjectTypeNormalGravityPortal:
							// if (player->isGravityFlipped())
							//	 this->playGravityEffect(false);
							obj->triggerActivated(player);
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);

							changeGravity(false);
							break;

						case kGameObjectTypeShipPortal:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);

							this->changeGameMode(obj, player, PlayerGamemodeShip);
							break;

						case kGameObjectTypeBallPortal:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);

							this->changeGameMode(obj, player, PlayerGamemodeBall);
							break;

						case kGameObjectTypeUfoPortal:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);

							this->changeGameMode(obj, player, PlayerGamemodeUFO);
							break;

						case kGameObjectTypeCubePortal:

							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);

							this->changeGameMode(obj, player, PlayerGamemodeCube);
							break;

						case kGameObjectTypeWavePortal:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							this->changeGameMode(obj, player, PlayerGamemodeWave);
							break;

						case kGameObjectTypeRobotPortal:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							this->changeGameMode(obj, player, PlayerGamemodeRobot);
							break;

						case kGameObjectTypeSpiderPortal:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							this->changeGameMode(obj, player, PlayerGamemodeSpider);
							break;

						case kGameObjectTypeSwingPortal:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							this->changeGameMode(obj, player, PlayerGamemodeSwing);
							break;

						case kGameObjectTypeTeleportPortal:
							obj->triggerActivated(player);
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							teleportPlayer(player, obj);
							break;

						case kGameObjectTypeDualPortal:
							obj->triggerActivated(player);
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							setDualMode(true);
							break;

						case kGameObjectTypeSoloPortal:
							obj->triggerActivated(player);
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							setDualMode(false);
							break;

						case kGameObjectTypeYellowJumpPad:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							obj->triggerActivated(player);
							spawnBounceEffect(obj->getPosition(), Color4B(255, 255, 0, 255), false);
							player->propellPlayer(1, Color4B(255, 255, 0, 255));
							player->_touchedPadObject = obj;
							break;

						case kGameObjectTypeGravityPad: {
							if (player->_touchedPadObject)
								break;
							auto pos = obj->getPosition();
							pos.y -= 10;
							player->setPortalP(pos);
							player->setPortalObject(obj);
							obj->triggerActivated(player);
							spawnBounceEffect(obj->getPosition(), Color4B(0, 255, 255, 255), false);
							player->propellPlayer(0.8, Color4B(0, 255, 255, 255));
							player->_touchedPadObject = obj;
							changeGravity(!player->isGravityFlipped());
							break;
						}

						case kGameObjectTypePinkJumpPad:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							obj->triggerActivated(player);
							spawnBounceEffect(obj->getPosition(), Color4B(255, 0, 255, 255), false);
							player->propellPlayer(0.65, Color4B(255, 0, 255, 255));
							player->_touchedPadObject = obj;
							break;

						case kGameObjectTypeRedJumpPad:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							obj->triggerActivated(player);
							spawnBounceEffect(obj->getPosition(), Color4B(255, 40, 40, 255), false);
							player->propellPlayer(1.25, Color4B(255, 40, 40, 255));
							player->_touchedPadObject = obj;
							break;

						case kGameObjectTypeSpiderPad:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							obj->triggerActivated(player);
							player->_touchedPadObject = obj;
							spiderTeleport(player);
							break;

						case kGameObjectTypeYellowJumpRing:
						case kGameObjectTypeDashRing:
						case kGameObjectTypeGravityDashRing:
						case kGameObjectTypeGravityRing:
						case kGameObjectTypeRedJumpRing:
						case kGameObjectTypePinkJumpRing:
						case kGameObjectTypeDropRing:
						case kGameObjectTypeGreenRing:
						case kGameObjectTypeSpiderRing:
						case kGameObjectTypeCustomRing:
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);

							player->setTouchedRing(obj);
							// Holding through a jump clears _queuedHold; restore it so orbs still fire.
							if (player->m_bIsHolding)
								player->_queuedHold = true;

							player->ringJump(obj);
							if (obj->getGameObjectType() == kGameObjectTypeCustomRing &&
								obj->hasBeenActivatedByPlayer(player))
								teleportPlayer(player, obj);
							break;
						case kGameObjectTypeModifier:
							obj->triggerActivated(player);
							switch (obj->getID())
							{
							case 201:
								changePlayerSpeed(0);
								break;
							case 200:
								changePlayerSpeed(1);
								break;
							case 202:
								changePlayerSpeed(2);
								break;
							case 203:
								changePlayerSpeed(3);
								break;
							case 1334:
								changePlayerSpeed(4);
								break;
							}
							break;
						case kGameObjectTypeSpecial:
							break;
						case kGameObjectTypeNormalMirrorPortal:
							obj->triggerActivated(player);
							setMirror(false);
							break;
						case kGameObjectTypeInverseMirrorPortal:
							obj->triggerActivated(player);
							setMirror(true);
							break;
						case kGameObjectTypeMiniSizePortal:
							obj->triggerActivated(player);
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							player->toggleMini(true);
							break;
						case kGameObjectTypeRegularSizePortal:
							obj->triggerActivated(player);
							player->setPortalP(obj->getPosition());
							player->setPortalObject(obj);
							player->toggleMini(false);
							break;
						case kGameObjectTypeSlope:
							player->collidedWithSlope(dt, obj);
							break;
						case kGameObjectTypeSecretCoin:
						case kGameObjectTypeUserCoin:
						case kGameObjectTypeCollectible:
							pickupCoin(obj, player);
							break;
						default:
							if (obj->isCoin())
								pickupCoin(obj, player);
							break;
						}
					}
				}
			}
		}
	for (unsigned int i = 0; i < m_pHazards.size(); ++i)
	{
		GameObject* hazard = m_pHazards[i];
		if (hazard->_radius > 0)
		{
			if (playerOuterBounds.intersectsCircle(hazard->getPosition(), hazard->_radius))
				destroyPlayer(player);
		}
		else
		{
			Rect hazardBounds = hazard->getOuterBounds();
			if (hazardBounds.size.width <= 0.f || hazardBounds.size.height <= 0.f)
				hazardBounds = Rect(hazard->getPosition() - Vec2(9.f, 10.f), Vec2(18.f, 20.f));
			if (playerOuterBounds.intersectsRect(hazardBounds))
				destroyPlayer(player);
		}
	}
	m_pHazards.clear();

	player->endSlopePass(dt);

	if (player->_spiderTeleportQueued)
	{
		player->_spiderTeleportQueued = false;
		spiderTeleport(player);
	}

	if (player->_currentGamemode == PlayerGamemodeShip)
		player->_queuedHold = false;

	if (player == _player1)
	{
		const float fromX = _lastTriggerScanX;
		const float toX = player->getPositionX();
		if (fromX != toX)
		{
			for (GameObject* obj : _pObjects)
			{
				if (!obj || !obj->_isTrigger)
					continue;
				auto* trigger = dynamic_cast<EffectGameObject*>(obj);
				if (!trigger || trigger->_spawnTriggered || trigger->_touchTriggered)
					continue;
				const int tid = trigger->getID();
				if (tid == 1812 || tid == 1815)
					continue;
				const float tx = obj->getPositionX();
				const bool crossed = (toX >= fromX) ? (tx > fromX && tx <= toX) : (tx < fromX && tx >= toX);
				if (crossed)
					trigger->triggerActivated(dt);
			}
		}
		_lastTriggerScanX = toX;
	}
}

void PlayLayer::onDrawImGui()
{
	extern bool _showDebugImgui;
	if (!_showDebugImgui)
		return;
	ImGui::SetNextWindowPos({1000.0f, 200.0f}, ImGuiCond_FirstUseEver);

	ImGui::Begin("PlayLayer Debug");

	ImGui::Text("%s", std::to_string(_player1->_queuedHold).c_str());

	ImGui::Checkbox("Freeze Player", &m_freezePlayer);
	ImGui::Checkbox("Platformer Mode (Basic)", &m_platformerMode);

#ifdef AX_PLATFORM_PC
	if (ImGui::Checkbox("Fullscreen", &fullscreen))
	{
		int a;
		auto monitor = glfwGetMonitors(&a)[monitorN];
		auto mode = glfwGetVideoMode(monitor);

		if (fullscreen)
			glfwSetWindowMonitor(static_cast<GLViewImpl*>(ax::Director::getInstance()->getGLView())->getWindow(),
								 monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
		else
		{
			glfwSetWindowMonitor(static_cast<GLViewImpl*>(ax::Director::getInstance()->getGLView())->getWindow(),
								 NULL, 0, 0, 1280, 720, 0);
			glfwWindowHint(GLFW_DECORATED, true);
		}
	}
#endif

	ImGui::SameLine();

	if (ImGui::ArrowButton("full", ImGuiDir_Right))
		ImGui::OpenPopup("Fullscreen Settings");

	if (ImGui::BeginPopupModal("Fullscreen Settings", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::InputInt("Monitor", &monitorN);
		if (ImGui::Button("Close"))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}

	if (ImGui::Button("Exit"))
	{
		this->exit();
	}

	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
				ImGui::GetIO().Framerate);

	ImGui::Text("yVel %.3f", _player1->getYVel());

	if (auto* gm = GameManager::getInstance())
		ImGui::Checkbox("Show Hitboxes", &gm->_showHitboxes);
	ImGui::Checkbox("Gain the power of invincibility", &noclip);

	if (ImGui::InputFloat("Speed", &gameSpeed))
		Director::getInstance()->getScheduler()->setTimeScale(gameSpeed);

	if (ImGui::InputFloat("FPS", &fps))
		Director::getInstance()->setAnimationInterval(1.0f / fps);

	ImGui::Text("Sections: %zu", _sectionObjects.size());
	if (_sectionObjects.size() > 0 && sectionForPos(_player1->getPositionX()) - 1 < _sectionObjects.size())
		ImGui::Text("Current Section Size: %zu", _sectionObjects[sectionForPos(_player1->getPositionX()) <= 0
																	  ? 0
																	  : sectionForPos(_player1->getPositionX()) - 1]
													.size());

	if (ImGui::Button("Reset"))
	{
		this->resetLevel();
	}
	ImGui::End();
}

void PlayLayer::resetLevel()
{
	unschedule("playlayer_restart");
	_attempts++;
	_freezeHitboxesOnDeath = false;
	if (auto* gm = GameManager::getInstance())
		showDn = gm->_showHitboxes;
	if (dn)
		dn->setVisible(showDn);
	auto dir = Director::getInstance();
	_player1->setPosition({2, 105});
	_lastTriggerScanX = -30.f;
	_player1->setRotation(0);
	_player1->setVisible(true);
	_player2->setPosition({2, 105});
	_player2->setRotation(0);
	_player2->setVisible(false);
	_player2->setActive(false);
	m_obCamPos.x = 0;
	m_obCamPos.y = 0;
	_bottomGround->setPositionX(0);
	_ceiling->setPositionX(0);
	_player1->reset();
	_player2->reset();
	m_pBG->setPositionX(dir->getWinSize().x / 2);
	_enterEffectID = 0;
	m_bEndAnimation = false;
	_isDualMode = false;
	_secondsSinceStart = 0;
	_itemCounts.clear();
	_shakeTime = 0.f;
	_shakeStrength = 0.f;
	_isMirror = false;
	_mirrorVisual = 0.f;
	applyMirrorVisual(dir->getWinSize().width);
	if (_effectManager)
	{
		_effectManager->_groupActions.clear();
		_effectManager->_activeMoveActions.clear();
		_effectManager->_completedMoveActions.clear();
	}

	for (auto& [id, group] : _groups)
	{
		group._alpha = 1.f;
		group.groupState = GroupProperties::GroupState::NOT_CHANGING;
	}

	for (auto obj : this->_pObjects)
	{
		if (!obj)
			continue;
		if (obj->_isTrigger)
		{
			if (auto* trigger = dynamic_cast<EffectGameObject*>(obj))
				trigger->_wasTriggerActivated = false;
		}
		obj->_hasBeenActivatedP1 = false;
		obj->_hasBeenActivatedP2 = false;
		obj->_effectOpacityMultipler = 1.f;
		obj->_startPosOffset = {0.f, 0.f};
		obj->_unkbool = false;
		obj->_toggledOn = true;
		obj->setPosition(obj->getStartPosition());
		if (obj->_isTrigger)
		{
			obj->setVisible(false);
			obj->setOpacity(0);
		}
		else
		{
			obj->setVisible(true);
			obj->setOpacity(obj->getID() == 1586 ? 0 : 255);
		}
		if (obj->_particle)
		{
			obj->_particle->setPosition(obj->getStartPosition());
			if (obj->_animateOnTrigger)
				obj->_particle->stopSystem();
			else
				obj->_particle->resetSystem();
		}
		obj->setActive(false);
		detachGameObject(obj);
	}
	std::fill(_coinsCollected.begin(), _coinsCollected.end(), false);
	refreshCoinHUD();

	if (_newBestBanner)
	{
		_newBestBanner->stopAllActions();
		_newBestBanner->removeFromParent();
		_newBestBanner = nullptr;
	}

	_player1->stopAllActions();
	_player2->stopAllActions();

	_colorChannels = _originalColors;

	_prevSection = -1;
	_nextSection = -1;

	if (this->_colorChannels.contains(1000) && this->m_pBG)
		this->m_pBG->setColor(colorForChannel(1000));
	else if (this->m_pBG) {
		this->m_pBG->setColor(ax::Color3B::GRAY);
		this->_colorChannels[1000]._color = ax::Color3B::GRAY;
	}
	this->_bottomGround->update(0);
	this->_ceiling->update(0);

	AudioEngine::stopAll();
	{
		float musicVol = 0.1f;
		if (auto* gm = GameManager::getInstance())
			musicVol *= gm->getMusicVolume();
		_musicAudioId = AudioEngine::play2d(LevelTools::resolveAudioPath(getLevel()), false, musicVol);
		AudioEngine::setCurrentTime(_musicAudioId, _levelSettings.songOffset);
	}

	applyLevelStartGamemode(_player1, _levelSettings.gamemode);
	_player2->setRotation(0.f);
	_player2->setGamemode(_levelSettings.gamemode);
	_player1->toggleMini(_levelSettings.mini);
	_player2->toggleMini(_levelSettings.mini);
	changePlayerSpeed(_levelSettings.speed);
	m_platformerMode = _levelSettings.platformer;
	_player1->m_bIsPlatformer = m_platformerMode;
	_player2->m_bIsPlatformer = m_platformerMode;
	_player1->direction = m_platformerMode ? 0.f : 1.f;
	_player2->direction = m_platformerMode ? 0.f : 1.f;
	m_obCamPos.y = m_fCameraYCenter;
	setDualMode(_levelSettings.dual);
	scheduleUpdate();

	if (_isPracticeMode && !_checkpoints.empty())
		applyCheckpoint(_checkpoints.back());
}

void PlayLayer::renderRect(ax::Rect rect, ax::Color4B col)
{
	dn->drawRect({rect.getMinX(), rect.getMinY()}, {rect.getMaxX(), rect.getMaxY()}, col);
	dn->drawSolidRect({rect.getMinX(), rect.getMinY()}, {rect.getMaxX(), rect.getMaxY()},
					  Color4B(col.r, col.g, col.b, 100));
}

void PlayLayer::onEnter()
{
	Layer::onEnter();

	auto listener = EventListenerKeyboard::create();
	auto dir = Director::getInstance();

	listener->onKeyPressed = AX_CALLBACK_2(PlayLayer::onKeyPressed, this);
	listener->onKeyReleased = AX_CALLBACK_2(PlayLayer::onKeyReleased, this);
	dir->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);

	auto current = dir->getRunningScene();
#if SHOW_IMGUI == true
	ImGuiPresenter::getInstance()->addRenderLoop("#playlayer", AX_CALLBACK_0(PlayLayer::onDrawImGui, this), current);
#endif
}

void PlayLayer::onExit()
{

#if SHOW_IMGUI == true
	Director::getInstance()->getEventDispatcher()->removeEventListenersForTarget(this);
	ImGuiPresenter::getInstance()->removeRenderLoop("#playlayer");
#endif
	LevelPage::replacingScene = false;
	Layer::onExit();
}

void PlayLayer::pauseGame()
{
	if (_isPaused || m_bEndAnimation)
		return;
	if (_player1 && _player1->isDead())
		return;

	_isPaused = true;
	m_freezePlayer = true;

	if (_player1 && _player1->m_bIsHolding)
		_player1->releaseButton();
	if (_isDualMode && _player2 && _player2->m_bIsHolding)
		_player2->releaseButton();

	if (m_pHudLayer && m_pHudLayer->_listener)
		m_pHudLayer->_listener->setEnabled(false);

	if (_pauseLayer)
	{
		_pauseLayer->removeFromParent();
		_pauseLayer = nullptr;
	}

	_pauseLayer = PauseLayer::create(this);
	if (_pauseLayer && m_pHudLayer)
	{
		m_pHudLayer->addChild(_pauseLayer, 500);
		return;
	}

	_isPaused = false;
	m_freezePlayer = false;
	if (m_pHudLayer && m_pHudLayer->_listener)
		m_pHudLayer->_listener->setEnabled(true);
	_pauseLayer = nullptr;
}

void PlayLayer::resumeGame()
{
	if (!_isPaused)
		return;

	_isPaused = false;
	m_freezePlayer = false;

	if (m_pHudLayer && m_pHudLayer->_listener)
		m_pHudLayer->_listener->setEnabled(true);

	if (_pauseLayer)
	{
		_pauseLayer->removeFromParent();
		_pauseLayer = nullptr;
	}

	AudioEngine::resumeAll();
}

void PlayLayer::togglePracticeMode()
{
	_isPracticeMode = !_isPracticeMode;
	if (!_isPracticeMode)
		clearCheckpoints();
	else if (_player1)
		_lastAutoCheckpointX = _player1->getPositionX();
	if (m_pHudLayer)
		m_pHudLayer->setPracticeHintVisible(_isPracticeMode);
}

void PlayLayer::markCheckpoint()
{
	if (!_isPracticeMode || !_player1 || _player1->isDead() || _isPaused)
		return;

	PracticeCheckpoint cp;
	cp.pos1 = _player1->getPosition();
	cp.rot1 = _player1->getRotation();
	cp.yVel1 = _player1->getYVel();
	cp.xVel1 = _player1->m_dXVel;
	cp.speed1 = _player1->getPlayerSpeed();
	cp.gravity1 = _player1->isGravityFlipped();
	cp.mini1 = _player1->_mini;
	cp.gamemode1 = _player1->_currentGamemode;
	cp.dual = _isDualMode;
	if (_player2 && _isDualMode)
	{
		cp.pos2 = _player2->getPosition();
		cp.rot2 = _player2->getRotation();
		cp.yVel2 = _player2->getYVel();
		cp.xVel2 = _player2->m_dXVel;
		cp.speed2 = _player2->getPlayerSpeed();
		cp.gravity2 = _player2->isGravityFlipped();
		cp.mini2 = _player2->_mini;
		cp.gamemode2 = _player2->_currentGamemode;
	}
	cp.camPos = m_obCamPos;
	if (_musicAudioId >= 0)
		cp.songTime = AudioEngine::getCurrentTime(_musicAudioId);

	if (SpriteFrameCache::getInstance()->getSpriteFrameByName("checkpoint_01_001.png"))
	{
		auto* spr = Sprite::createWithSpriteFrameName("checkpoint_01_001.png");
		if (spr)
		{
			spr->setStretchEnabled(false);
			spr->setPosition(cp.pos1);
			addChild(spr, 90);
			cp.sprite = spr;
		}
	}

	_checkpoints.push_back(cp);
	_lastAutoCheckpointX = cp.pos1.x;
}

void PlayLayer::removeCheckpoint()
{
	if (!_isPracticeMode || _checkpoints.empty() || _isPaused)
		return;

	auto& cp = _checkpoints.back();
	if (cp.sprite)
		cp.sprite->removeFromParent();
	_checkpoints.pop_back();
	_lastAutoCheckpointX = _checkpoints.empty() ? -9999.f : _checkpoints.back().pos1.x;
}

void PlayLayer::clearCheckpoints()
{
	for (auto& cp : _checkpoints)
	{
		if (cp.sprite)
			cp.sprite->removeFromParent();
	}
	_checkpoints.clear();
	_lastAutoCheckpointX = -9999.f;
}

void PlayLayer::applyCheckpoint(const PracticeCheckpoint& checkpoint)
{
	if (!_player1)
		return;

	applyLevelStartGamemode(_player1, checkpoint.gamemode1);
	_player1->toggleMini(checkpoint.mini1);
	_player1->flipGravity(checkpoint.gravity1);
	_player1->setPosition(checkpoint.pos1);
	_player1->setRotation(checkpoint.rot1);
	_player1->m_dXVel = checkpoint.xVel1;
	_player1->setPlayerSpeed(checkpoint.speed1);
	_player1->setYVel(checkpoint.yVel1);
	_player1->setIsDead(false);
	_player1->setVisible(true);

	setDualMode(checkpoint.dual);
	if (checkpoint.dual && _player2)
	{
		_player2->setGamemode(checkpoint.gamemode2);
		_player2->toggleMini(checkpoint.mini2);
		_player2->flipGravity(checkpoint.gravity2);
		_player2->setPosition(checkpoint.pos2);
		_player2->setRotation(checkpoint.rot2);
		_player2->m_dXVel = checkpoint.xVel2;
		_player2->setPlayerSpeed(checkpoint.speed2);
		_player2->setYVel(checkpoint.yVel2);
		_player2->setIsDead(false);
		_player2->setVisible(true);
		_player2->setActive(true);
	}

	m_obCamPos = checkpoint.camPos;
	if (m_pBG)
		m_pBG->setPositionX(Director::getInstance()->getWinSize().width / 2.f);
	if (_musicAudioId >= 0)
		AudioEngine::setCurrentTime(_musicAudioId, checkpoint.songTime);
}

void PlayLayer::applyMusicVolume()
{
	if (_musicAudioId < 0)
		return;
	float musicVol = 0.1f;
	if (auto* gm = GameManager::getInstance())
		musicVol *= gm->getMusicVolume();
	AudioEngine::setVolume(_musicAudioId, musicVol);
}

void PlayLayer::exit()
{
	clearCheckpoints();

	_player1->deactivateStreak();
	_player2->deactivateStreak();
	unscheduleAllCallbacks();
	_player1->unscheduleAllCallbacks();
	_player2->unscheduleAllCallbacks();
	_bottomGround->unscheduleUpdate();
	if (_ceiling)
	{
		_ceiling->unscheduleUpdate();
	}

	int size = _pObjects.size();
	for (int i = 0; i < size; i++)
	{
		GameObject* obj = _pObjects.at(i);
		if (obj && !obj->getParent())
		{
			if (obj->_particle)
			{
				obj->_particle->onExit();
				AX_SAFE_RELEASE_NULL(obj->_particle);
			}
			if (obj->_glowSprite)
			{
				obj->_glowSprite->onExit();
				AX_SAFE_RELEASE_NULL(obj->_glowSprite);
			}
			obj->unscheduleAllCallbacks();
			obj->onExit();
			obj->setActive(false);
			AX_SAFE_RELEASE(obj);
		}
	}

	Instance = nullptr;
	BaseGameLayer::_instance = nullptr;

	AudioEngine::stopAll();
	AudioEngine::play2d("quitSound_01.ogg", false, 0.1f);
	AudioEngine::play2d("menuLoop.mp3", true, 0.2f);

	int id = 1;
	if (auto* level = getLevel())
		id = level->_levelID;
	if (id < 1 || id > 22)
	{
		GameToolbox::popSceneWithTransition(0.5f);
		return;
	}

	Director::getInstance()->replaceScene(
		TransitionFade::create(0.5f, LevelSelectLayer::scene(id - 1)));
}

void PlayLayer::onKeyPressed(EventKeyboard::KeyCode keyCode, Event* event)
{
	GameToolbox::log("Key with keycode {} pressed", static_cast<int>(keyCode));

	if (keyCode == EventKeyboard::KeyCode::KEY_ESCAPE || keyCode == EventKeyboard::KeyCode::KEY_BACK)
	{
		if (_isPaused)
			resumeGame();
		else
			pauseGame();
		return;
	}

	if (_isPaused)
		return;

	switch (keyCode)
	{
	case EventKeyboard::KeyCode::KEY_R: {
		resetLevel();
	}
	break;
	case EventKeyboard::KeyCode::KEY_Z: {
		markCheckpoint();
	}
	break;
	case EventKeyboard::KeyCode::KEY_X: {
		removeCheckpoint();
	}
	break;
	case EventKeyboard::KeyCode::KEY_F: {
		extern bool _showDebugImgui;
		_showDebugImgui = !_showDebugImgui;
	}
	break;
	case EventKeyboard::KeyCode::KEY_SPACE: {
		if (!_player1->m_bIsHolding)
			_player1->pushButton();
		if (_isDualMode && !_player2->m_bIsHolding)
			_player2->pushButton();
	}
	break;
	case EventKeyboard::KeyCode::KEY_UP_ARROW: {
		if (!_player1->m_bIsHolding)
			_player1->pushButton();
		if (_isDualMode && !_player2->m_bIsHolding)
			_player2->pushButton();
	}
	break;
	default:
		break;
	}
	if (_player1->m_bIsPlatformer)
	{
		if (keyCode == EventKeyboard::KeyCode::KEY_A || keyCode == EventKeyboard::KeyCode::KEY_LEFT_ARROW)
			_player1->direction = -1.f;
		else if (keyCode == EventKeyboard::KeyCode::KEY_D || keyCode == EventKeyboard::KeyCode::KEY_RIGHT_ARROW)
			_player1->direction = 1.f;
	}
}

void PlayLayer::onKeyReleased(EventKeyboard::KeyCode keyCode, Event* event)
{
	GameToolbox::log("Key with keycode {} released", static_cast<int>(keyCode));
	if (_player1->m_bIsPlatformer)
	{
		const bool left = keyCode == EventKeyboard::KeyCode::KEY_A || keyCode == EventKeyboard::KeyCode::KEY_LEFT_ARROW;
		const bool right = keyCode == EventKeyboard::KeyCode::KEY_D || keyCode == EventKeyboard::KeyCode::KEY_RIGHT_ARROW;
		if ((left && _player1->direction < 0.f) || (right && _player1->direction > 0.f))
			_player1->direction = 0.f;
	}
	switch (keyCode)
	{
	case EventKeyboard::KeyCode::KEY_SPACE: {
		if (_isPaused)
			break;
		if (_player1->m_bIsHolding)
			_player1->releaseButton();
		if (_isDualMode && _player2->m_bIsHolding)
			_player2->releaseButton();
	}
	break;
	case EventKeyboard::KeyCode::KEY_UP_ARROW: {
		if (_isPaused)
			break;
		if (_player1->m_bIsHolding)
			_player1->releaseButton();
		if (_isDualMode && _player2->m_bIsHolding)
			_player2->releaseButton();
	}
	default:
		break;
	}
}

void PlayLayer::tweenBottomGround(float y)
{
	if (!_bottomGround)
		return;
	_bottomGround->runAction(EaseInOut::create(ActionTween::create(0.1f, "y", _bottomGround->getPositionY(), y), 2.f));
}

void PlayLayer::tweenCeiling(float y)
{
	if (!_ceiling)
		return;
	_ceiling->runAction(EaseInOut::create(ActionTween::create(0.1f, "y", _ceiling->getPositionY(), y), 2.f));
}

float PlayLayer::flyingFloorY(const PlayerObject* player) const
{
	const float camY = cameraFollow ? cameraFollow->getPositionY() : m_obCamPos.y;
	const float groundY = _bottomGround ? _bottomGround->getPositionY() : 0.f;
	return groundY + camY + (player && player->_mini ? 87.f : 93.f);
}

float PlayLayer::flyingCeilY(const PlayerObject* player) const
{
	if (_ceiling)
		return _ceiling->getPositionY() - (player && player->_mini ? 234.f : 240.f) + m_fCameraYCenter - 12.f;
	return m_fCameraYCenter + (player && player->_mini ? 69.f : 75.f);
}

void PlayLayer::changePlayerSpeed(int speed)
{
	switch (speed)
	{
	case 0:
		_player1->m_dXVel = 5.77;
		_player1->setPlayerSpeed(0.9);
		_player2->m_dXVel = 5.77;
		_player2->setPlayerSpeed(0.9);
		break;
	case 1:
		_player1->m_dXVel = 5.98;
		_player1->setPlayerSpeed(0.7);
		_player2->m_dXVel = 5.98;
		_player2->setPlayerSpeed(0.7);
		break;
	case 2:
		_player1->m_dXVel = 5.87;
		_player1->setPlayerSpeed(1.1);
		_player2->m_dXVel = 5.87;
		_player2->setPlayerSpeed(1.1);
		break;
	case 3:
		_player1->m_dXVel = 6;
		_player1->setPlayerSpeed(1.3);
		_player2->m_dXVel = 6;
		_player2->setPlayerSpeed(1.3);
		break;
	case 4:
		_player1->m_dXVel = 6;
		_player1->setPlayerSpeed(1.6);
		_player2->m_dXVel = 6;
		_player2->setPlayerSpeed(1.6);
		break;
	}
}

void PlayLayer::changeGravity(bool gravityFlipped)
{
	_player1->flipGravity(gravityFlipped);
	if (_isDualMode)
		_player2->flipGravity(!gravityFlipped);
}

void PlayLayer::setMirror(bool mirror)
{
	if (_isMirror == mirror)
		return;

	_isMirror = mirror;
	// Visual tween runs in updateCamera → applyMirrorVisual.
}

void PlayLayer::applyMirrorVisual(float screenWidth)
{
	if (!_gameLayer)
		return;

	// Ease-in-out (smoothstep), then a horizontal whoosh like official GD:
	// slide off-screen → flip → slide in from the other side (~0.5s).
	const float t = _mirrorVisual;
	const float te = t * t * (3.f - 2.f * t);
	const float centerX = m_obCamPos.x + screenWidth * 0.5f;
	const bool flipped = te >= 0.5f;

	_gameLayer->setScaleX(flipped ? -1.f : 1.f);

	if (!flipped)
	{
		// First half: leave normal pose, slide toward +X
		const float u = te * 2.f;
		const float ease = u * u * (3.f - 2.f * u);
		_gameLayer->setPositionX(ease * screenWidth);
	}
	else
	{
		// Second half: enter mirrored pose from the opposite side
		const float u = (te - 0.5f) * 2.f;
		const float ease = u * u * (3.f - 2.f * u);
		_gameLayer->setPositionX(2.f * centerX + screenWidth * (1.f - ease));
	}
}

GameObject* PlayLayer::findTeleportDestination(GameObject* src)
{
	if (!src)
		return nullptr;

	// 2.2-style: target group ID on the portal
	const int targetGroup = src->_teleportTargetGroupId;
	if (targetGroup > 0 && _groups.contains(targetGroup) && !_groups[targetGroup]._objects.empty())
	{
		for (GameObject* obj : _groups[targetGroup]._objects)
		{
			if (obj && obj != src)
				return obj;
		}
	}

	GameObject* best = nullptr;
	float bestDist = 1e12f;
	const int link = src->_linkedGroupId;

	for (GameObject* obj : _pObjects)
	{
		if (!obj || obj == src)
			continue;

		const bool isPortal = obj->getGameObjectType() == kGameObjectTypeTeleportPortal;
		const bool isDestPad = obj->getID() == 749 || obj->getID() == 747;
		if (!isPortal && !isDestPad)
			continue;

		bool linked = false;
		if (link > 0)
		{
			if (obj->_linkedGroupId == link)
				linked = true;
			for (int g : obj->_groups)
			{
				if (g == link)
				{
					linked = true;
					break;
				}
			}
		}
		if (!linked)
		{
			for (int g : src->_groups)
			{
				for (int g2 : obj->_groups)
				{
					if (g == g2 && g > 0)
					{
						linked = true;
						break;
					}
				}
				if (linked)
					break;
			}
		}
		if (!linked)
			continue;

		const float dist = obj->getPosition().distance(src->getPosition());
		if (dist > 1.f && dist < bestDist)
		{
			bestDist = dist;
			best = obj;
		}
	}
	return best;
}

void PlayLayer::teleportPlayer(PlayerObject* player, GameObject* from)
{
	if (!player || !from)
		return;

	// Official GJBaseGameLayer::getPortalTargetPos:
	// For classic portal 747 → (player.x, portal.y + teleportYOffset)
	// Otherwise → target object position when available.
	Vec2 dest;
	GameObject* target = findTeleportDestination(from);

	if (from->getID() == 747 || (!target && (from->_teleportYOffset != 0.f || from->getID() == 749)))
	{
		dest.x = player->getPositionX();
		dest.y = from->getPositionY() + from->_teleportYOffset;
	}
	else if (target)
	{
		dest = target->getPosition();
	}
	else
	{
		return;
	}

	if (from->_teleportIgnoreX)
		dest.x = player->getPositionX();
	if (from->_teleportIgnoreY)
		dest.y = player->getPositionY();

	player->setPosition(dest);
	player->setYVel(0);
	player->setIsOnGround(false);
}

void PlayLayer::spiderTeleport(PlayerObject* player)
{
	if (!player)
		return;

	const bool searchUp = !player->isGravityFlipped();
	const Rect playerBounds = player->_mini ? player->getOuterBounds(0.6f, 0.6f) : player->getOuterBounds();
	const float playerMidX = playerBounds.getMidX();
	const float playerTop = playerBounds.getMaxY();
	const float playerBot = playerBounds.getMinY();

	float bestY = searchUp ? 1e12f : -1e12f;
	bool found = false;

	const int currentSection = sectionForPos(player->getPositionX());
	for (int i = currentSection - 2; i <= currentSection + 1; ++i)
	{
		if (i < 0 || i >= static_cast<int>(_sectionObjects.size()))
			continue;
		for (GameObject* obj : _sectionObjects[i])
		{
			if (!obj || !obj->isActive())
				continue;
			const GameObjectType type = obj->getGameObjectType();
			if (type != kGameObjectTypeSolid && type != kGameObjectTypeSlope)
				continue;
			const Rect box = obj->getOuterBounds();
			if (box.size.width <= 0.f || box.size.height <= 0.f)
				continue;
			if (playerBounds.getMaxX() <= box.getMinX() || playerBounds.getMinX() >= box.getMaxX())
				continue;

			if (searchUp)
			{
				if (box.getMinY() >= playerTop - 2.f && box.getMinY() < bestY)
				{
					bestY = box.getMinY();
					found = true;
				}
			}
			else if (box.getMaxY() <= playerBot + 2.f && box.getMaxY() > bestY)
			{
				bestY = box.getMaxY();
				found = true;
			}
		}
	}

	const float halfH = playerBounds.size.height * 0.5f;
	if (found)
	{
		float newY = searchUp ? (bestY - halfH) : (bestY + halfH);
		player->setPositionY(newY);
	}
	else
	{
		if (searchUp)
			player->setPositionY(flyingCeilY(player));
		else
			player->setPositionY(flyingFloorY(player));
	}

	player->setYVel(0);
	player->flipGravity(!player->isGravityFlipped());
	player->hitGround(player->isGravityFlipped());
}

bool PlayLayer::tryActivateCountTrigger(EffectGameObject* trigger)
{
	if (!trigger)
		return false;
	const int value = _itemCounts[trigger->_itemID];
	const int target = trigger->_countTarget;
	bool pass = value == target;
	if (trigger->_countCompare == 1)
		pass = value > target;
	else if (trigger->_countCompare == 2)
		pass = value < target;
	if (!pass)
		return false;

	if (_groups.contains(trigger->_targetGroupId))
	{
		for (GameObject* gameObj : _groups[trigger->_targetGroupId]._objects)
		{
			if (gameObj && gameObj->_isTrigger)
				static_cast<EffectGameObject*>(gameObj)->triggerActivated(0.f);
		}
	}
	return true;
}

void PlayLayer::fireOnDeathTriggers()
{
	for (GameObject* obj : _pObjects)
	{
		if (!obj || !obj->_isTrigger)
			continue;
		if (obj->getID() != 1812)
			continue;
		auto* trigger = static_cast<EffectGameObject*>(obj);
		if (trigger->_spawnTriggered)
			continue;
		trigger->triggerActivated(0.f);
	}
}

PlayLayer* PlayLayer::getInstance()
{
	return Instance;
}
