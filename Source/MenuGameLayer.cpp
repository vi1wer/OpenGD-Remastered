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

#include "MenuGameLayer.h"

#include "GroundLayer.h"
#include "GameManager.h"
#include "2d/Menu.h"
#include "2d/Node.h"
#include "PlayerObject.h"
#include "base/Director.h"
#include "EventListenerTouch.h"
#include "EventDispatcher.h"
#include "GameToolbox/enums.h"
#include "GameToolbox/getTextureString.h"
#include "GameToolbox/rand.h"
#include <algorithm>

USING_NS_AX;

MenuGameLayer* MenuGameLayer::create()
{
	MenuGameLayer* pRet = new MenuGameLayer();
	if (pRet->init())
	{
		pRet->autorelease();
		return pRet;
	}
	else
	{
		delete pRet;
		pRet = nullptr;
		return nullptr;
	}
}

Scene* MenuGameLayer::scene() {
	auto scene = Scene::create();
	scene->addChild(MenuGameLayer::create());
	return scene;
}

void MenuGameLayer::onEnter()
{
	Layer::onEnter();
	// Editor/play can leave BaseGameLayer colors/textures on shared ground look — restore menu default.
	if (groundLayer)
		groundLayer->resetMenuAppearance();
}

bool MenuGameLayer::init(){
	if (!Layer::init()) return false;

	this->startPos = Vec2(0, 105);

	auto dir = Director::getInstance();
	const auto& winSize = dir->getWinSize();

	groundLayer = GroundLayer::create(1);
	if (groundLayer)
	{
		groundLayer->_followPlayLayerColors = false;
		// PlayLayer places the ground at +12 when the camera is at Y = 0 so a
		// player at y = 105 stands on the floor line.
		groundLayer->setPositionY(12.f);
		addChild(groundLayer, 2);
	}

	const Texture2D::TexParams texParams = {
		backend::SamplerFilter::LINEAR,
		backend::SamplerFilter::LINEAR,
		backend::SamplerAddressMode::REPEAT,
		backend::SamplerAddressMode::REPEAT
	};

	// Fullscreen scrolling strip: tiles side-by-side, scaled to cover win height.
	bgSprites = Node::create();
	bgSprites->setAnchorPoint({0.f, 0.f});
	addChild(bgSprites, -3);

	for (int i = 0; i < 4; i++)
	{
		auto* gr = Sprite::create(GameToolbox::getTextureString("game_bg_01_001.png"));
		if (!gr)
			continue;
		gr->setStretchEnabled(false);
		if (auto* tex = gr->getTexture())
			tex->setTexParameters(texParams);
		_bgTileW = gr->getContentSize().width;
		_bgTileH = gr->getContentSize().height;
		gr->setTextureRect(Rect(0, 0, _bgTileW, _bgTileH));
		gr->setAnchorPoint({0.f, 0.f});
		gr->setPosition({_bgTileW * static_cast<float>(i), 0.f});
		gr->setColor({0, 102, 255});
		bgSprites->addChild(gr);
	}
	sep = 0.3f;
	updateForWinSize();

	scheduleUpdate();

	auto listener = EventListenerTouchOneByOne::create();

	listener->setEnabled(true);
	listener->setSwallowTouches(false);
	listener->onTouchBegan = AX_CALLBACK_2(MenuGameLayer::onTouchBegan, this);
	dir->getEventDispatcher()->addEventListenerWithSceneGraphPriority(listener, this);

	this->resetPlayer(false);

	return true;
}

void MenuGameLayer::updateForWinSize()
{
	const auto winSize = Director::getInstance()->getWinSize();
	if (bgSprites && _bgTileH > 0.f)
	{
		// Fill height; keep aspect so tiles don't stretch. Extra width scrolls.
		const float scale = winSize.height / _bgTileH;
		bgSprites->setScale(scale);
		bgSprites->setPosition({0.f, 0.f});
		bsizeX = _bgTileW * scale;
		bgStartPos = bgSprites->getPositionX();
	}
	if (groundLayer)
		groundLayer->updateForWinSize();
}
void MenuGameLayer::spawnMenuPlayer()
{
	if (player)
	{
		player->removeFromParent();
		player = nullptr;
	}

	player = PlayerObject::createForMenu(this);
	if (!player)
		return;

	player->setMainColor(GameToolbox::randomColor3B());
	player->setSecondaryColor(GameToolbox::randomColor3B());
	player->setGlow(GameToolbox::randomInt(0, 3) == 0);
	player->setPosition({-100.f, _groundY});
	addChild(player, 16);
}

void MenuGameLayer::tryJump(float delta)
{
	if (!player)
		return;

	_jumpTimer -= delta;
	if (_jumpTimer > 0.f)
		return;

	if (player->isFlying())
	{
		if (player->m_bIsHolding)
			player->releaseButton();
		else
			player->pushButton();
		_jumpTimer = GameToolbox::randomFloat(2, 8) / 10.f + 0.15f;
		return;
	}

	if (player->_currentGamemode == PlayerGamemodeBall || player->isOnGround())
	{
		player->pushButton();
		_pendingRelease = true;
		_jumpTimer = GameToolbox::randomFloat(5, 14) / 10.f;
	}
}

void MenuGameLayer::spiderTeleportMenu()
{
	if (!player || !player->_spiderTeleportQueued)
		return;

	player->_spiderTeleportQueued = false;

	const float halfH = 15.f;
	if (!player->isGravityFlipped())
		player->setPositionY(_ceilY - halfH);
	else
		player->setPositionY(_groundY + halfH);

	player->setYVel(0);
	player->flipGravity(!player->isGravityFlipped());
	player->hitGround(player->isGravityFlipped());
}

void MenuGameLayer::processPlayerMovement(float delta)
{
	if (!this->player)
		return;

	// Same order as PlayLayer: input, then 4 physics substeps with collision.
	tryJump(delta);
	player->storeShipRotationPos();

	float step = std::min(2.0f, delta * 60.0f);
	step /= 4.0f;
	for (int i = 0; i < 4; i++)
	{
		this->player->update(step);
		collideMenuWorld();
		spiderTeleportMenu();
	}
	step *= 4.0f;

	if (_pendingRelease)
	{
		player->releaseButton();
		_pendingRelease = false;
	}

	if (player->_currentGamemode == PlayerGamemodeShip || player->_currentGamemode == PlayerGamemodeUFO)
		player->updateShipRotation(step);

	const auto& winSize = Director::getInstance()->getWinSize();
	if (this->player->getPositionX() >= winSize.width + player->getContentSize().width)
		this->resetPlayer(false);
}

void MenuGameLayer::collideMenuWorld()
{
	if (!player)
		return;

	const float y = player->getPositionY();

	// PlayLayer::checkCollisions floor snap for cube/robot/spider.
	if (y < _groundY && player->isGroundedMode())
	{
		if (player->isGravityFlipped())
			player->flipGravity(false);
		player->setPositionY(_groundY);
		player->hitGround(false);
	}
	else if (y > _ceilY && player->isGroundedMode() && player->isGravityFlipped())
	{
		player->setPositionY(_ceilY);
		player->hitGround(true);
	}

	if (player->isFlying() || player->_currentGamemode == PlayerGamemodeBall)
	{
		if (player->getPositionY() < _groundY)
		{
			player->setPositionY(_groundY);
			if (!player->isGravityFlipped())
				player->hitGround(false);
			player->setYVel(0.f);
		}
		if (player->getPositionY() > _ceilY)
		{
			player->setPositionY(_ceilY);
			if (player->isGravityFlipped())
				player->hitGround(true);
			player->setYVel(0.f);
		}
	}
}

void MenuGameLayer::applyMenuBounds()
{
	collideMenuWorld();
}

void MenuGameLayer::pickRandomIcon()
{
	static constexpr PlayerGamemode kModes[] = {
		PlayerGamemodeCube,  PlayerGamemodeShip,  PlayerGamemodeBall, PlayerGamemodeUFO,
		PlayerGamemodeRobot, PlayerGamemodeSpider, PlayerGamemodeSwing,
	};
	player->setGamemode(kModes[GameToolbox::randomInt(0, static_cast<int>(std::size(kModes)) - 1)]);
}

void MenuGameLayer::resetPlayer(bool touched)
{
	spawnMenuPlayer();
	if (!player)
		return;

	player->reset();
	player->stopRotation();
	player->setRotation(0.f);
	if (player->isGravityFlipped())
		player->flipGravity(false);
	player->releaseButton();
	pickRandomIcon();

	const float x = touched ? -600.0f : -300.0f;
	float y = _groundY;
	if (player->isFlying())
		y = _groundY + static_cast<float>(GameToolbox::randomInt(20, 70));

	player->setPosition({x, y});
	player->setYVel(0);
	player->setVisible(true);

	if (!player->isFlying())
	{
		player->hitGround(false);
		player->setPositionY(_groundY);
	}

	_jumpTimer = GameToolbox::randomFloat(2, 8) / 10.f;
	_pendingRelease = false;
	if (player->isFlying() && GameToolbox::randomInt(0, 1) == 1)
		player->pushButton();
}

void MenuGameLayer::processBackground(float delta) {
	if (!bgSprites || bsizeX <= 0.f)
		return;
	float xpos = bgSprites->getPositionX() - sep;
	if (xpos <= -bsizeX)
		xpos += bsizeX;
	bgSprites->setPositionX(xpos);
}

void MenuGameLayer::update(float delta) {
	processBackground(delta);
	processPlayerMovement(delta);
	if (!groundLayer)
		return;
	if (player)
		groundLayer->update(std::min(2.0f, delta * 60.0f) * player->getPlayerSpeed());
	else
		groundLayer->update(delta * 60.0f);
}

//void MenuGameLayer::renderRect(ax::Rect rect, ax::Color4B col)
//{
//	_dnHitbox->drawRect({rect.getMinX(), rect.getMinY()}, {rect.getMaxX(), rect.getMaxY()}, col);
//	_dnHitbox->drawSolidRect({rect.getMinX(), rect.getMinY()}, {rect.getMaxX(), rect.getMaxY()},
//					  Color4B(col.r, col.g, col.b, 100));
//}

bool MenuGameLayer::onTouchBegan(ax::Touch* touch, ax::Event* event)
{
	auto touchPos = touch->getLocation();
	ax::Rect hitbox = player->getBoundingBox();

	//hitbox probably needs to be adjusted because it feels a bit off atm
	//constexpr float hitboxMult = 2.5f;
	//hitbox.origin -= {hitboxMult, hitboxMult};
	//hitbox.size += {hitboxMult * 2, hitboxMult * 2};
	
	if(hitbox.containsPoint(touchPos)) {
		player->playDeathEffect(false);
		this->resetPlayer(true);
		return true;
	}
	return false;
}
