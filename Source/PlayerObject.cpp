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

#include "PlayerObject.h"
#include "AnimatedIconSprite.h"
#include "HardStreak.h"
#include "AudioEngine.h"

#include "PlayLayer.h"
#include "GameManager.h"
#include "2d/ParticleSystem.h"
#include "2d/ParticleSystemQuad.h"
#include "2d/ActionInstant.h"
#include "2d/ActionInterval.h"
#include "2d/ActionEase.h"
#include "base/Director.h"
#include "renderer/TextureCache.h"
#include "CircleWave.h"
#include "UTF8.h"
#include "GameToolbox/conv.h"
#include "GameToolbox/rand.h"
#include "GameToolbox/enums.h"
#include "GameToolbox/log.h"
#include "GameToolbox/math.h"
#include <algorithm>
#include <cmath>

USING_NS_AX;

Texture2D* PlayerObject::motionStreakTex = nullptr;

void PlayerObject::reset()
{
	m_dYVel = 0.f;
	setIsDead(false);
	flipGravity(false);
	m_snappedObject = nullptr;
	m_snapDifference = 0;
	m_isRising = false;
	m_bIsHolding = false;
	_touchedRingObject = nullptr;
	_isAccelerating = false;
	_shipRotationPosValid = false;
	_isOnSlope = false;
	_wasOnSlope = false;
	_currentSlope = nullptr;
	_slopeRotation = 0.f;
	_slopeVelocity = 0.f;
	_slopeStartTime = 0.f;
	_totalTime = 0.f;
	_slopeUphillContact = false;
	stopActionByTag(0);
	stopActionByTag(1);
	_hasRingJumped = false;
	_isDashing = false;
	_spiderTeleportQueued = false;
	_queuedHold = false;
	_jumpedTimes = 0;
	_lastP = getPosition();

	dragEffect1->pauseEmissions();
	dragEffect2->pauseEmissions();
	dragEffect3->pauseEmissions();
	shipDragEffect->pauseEmissions();
	landEffect1->pauseEmissions();
	landEffect2->pauseEmissions();

	_particles1Activated = false;
	_particles2Activated = false;
	_particles3Activated = false;

	deactivateStreak();
}

void PlayerObject::playDeathEffect(bool stopMusic)
{
	if (stopMusic)
		AudioEngine::stopAll();
	AudioEngine::play2d("explode_11.ogg", false, 0.8f);

	auto pauseParticle = [](ParticleSystemQuad* ps) {
		if (ps)
			ps->pauseEmissions();
	};
	pauseParticle(dragEffect1);
	pauseParticle(dragEffect2);
	pauseParticle(dragEffect3);
	pauseParticle(shipDragEffect);
	pauseParticle(landEffect1);
	pauseParticle(landEffect2);
	deactivateStreak();

	setVisible(false);

	auto* layer = gameLayer;
	if (!layer)
		return;

	const Vec2 pos = getPosition();
	const Color3B main = getMainColor();
	const Color3B sec = getSecondaryColor();
	auto* cache = Director::getInstance()->getTextureCache();
	auto* square = cache ? cache->addImage("square.png") : nullptr;

	auto spawnBurst = [&](const Color3B& color, float scale, int z) {
		auto* burst = ParticleSystemQuad::create("explodeEffect.plist");
		if (!burst)
			return;
		if (square)
			burst->setTexture(square);
		burst->setPosition(pos);
		burst->setPositionType(ParticleSystem::PositionType::GROUPED);
		burst->setScale(scale);
		burst->setLife(0.4f);
		burst->setLifeVar(0.25f);
		burst->setStartColor(Color4F(color.r / 255.f, color.g / 255.f, color.b / 255.f, 1.f));
		burst->setStartColorVar(Color4F(0.08f, 0.08f, 0.08f, 0.15f));
		burst->setEndColor(Color4F(color.r / 255.f, color.g / 255.f, color.b / 255.f, 0.f));
		burst->setEndColorVar(Color4F(0.f, 0.f, 0.f, 0.f));
		burst->setAutoRemoveOnFinish(true);
		burst->resetSystem();
		layer->addChild(burst, z);
	};

	spawnBurst(main, 1.f, 200);
	spawnBurst(sec, 0.65f, 199);

	if (auto* ring = CircleWave::create(0.45f, Color4B(main.r, main.g, main.b, 255), 6.f, 80.f, true, false, 3.f))
	{
		ring->setPosition(pos);
		layer->addChild(ring, 198);
	}
	if (auto* ring2 = CircleWave::create(0.6f, Color4B(sec.r, sec.g, sec.b, 220), 4.f, 130.f, true, false, 2.f))
	{
		ring2->setPosition(pos);
		layer->addChild(ring2, 197);
	}
}

namespace
{
Sprite* createIconFrame(const std::string& frameName, const char* fallback)
{
	Sprite* spr = Sprite::createWithSpriteFrameName(frameName);
	if (!spr && fallback)
		spr = Sprite::createWithSpriteFrameName(fallback);
	return spr;
}

int clampIconId(IconType mode, int iconId)
{
	const int maxIcon = GameToolbox::getValueForGamemode(mode);
	return GameToolbox::inRange(iconId, 1, maxIcon > 0 ? maxIcon : 1);
}

} // namespace

bool PlayerObject::init(int playerFrame, Layer* gameLayer_, bool menuRandomIcons)
{
	if (!Sprite::init())
		return false;

	auto gm = GameManager::getInstance();
	auto pickIcon = [&](IconType mode, int fallback) -> int {
		if (menuRandomIcons)
		{
			const int maxIcon = GameToolbox::getValueForGamemode(mode);
			return GameToolbox::randomInt(1, maxIcon > 0 ? maxIcon : 1);
		}
		return clampIconId(mode, mode == IconType::kIconTypeCube ? fallback : gm->getSelectedIcon(mode));
	};

	const int frame = pickIcon(IconType::kIconTypeCube, playerFrame);
	const int shipId = pickIcon(IconType::kIconTypeShip, 1);
	const int ballId = pickIcon(IconType::kIconTypeBall, 1);
	const int ufoId = pickIcon(IconType::kIconTypeUfo, 1);
	const int waveId = pickIcon(IconType::kIconTypeWave, 1);
	const int swingId = pickIcon(IconType::kIconTypeSwing, 1);

	auto sprStr1 = StringUtils::format("player_%02d_001.png", frame);
	auto sprStr2 = StringUtils::format("player_%02d_2_001.png", frame);
	GameToolbox::log("1: {}, 2: {}", sprStr1, sprStr2);

	gameLayer = gameLayer_;

	// Check if layer is playlayer
	inPlayLayer = dynamic_cast<PlayLayer*>(gameLayer_) != nullptr;

	// PlayerObject is only a transform/container. Giving it a texture rect makes
	// Axmol render the default white texture behind the actual icon sprites.
	setContentSize({30.f, 30.f});
	setAnchorPoint({0.5f, 0.5f});

	m_pMainSprite = createIconFrame(sprStr1, "player_01_001.png");
	if (!m_pMainSprite)
		return false;
	m_pMainSprite->setStretchEnabled(false);
	m_pMainSprite->setPosition({15.f, 15.f});
	addChild(m_pMainSprite, 1);

	m_pSecondarySprite = createIconFrame(sprStr2, "player_01_2_001.png");
	if (m_pSecondarySprite)
	{
		m_pSecondarySprite->setStretchEnabled(false);
		m_pMainSprite->addChild(m_pSecondarySprite, -1);
		m_pSecondarySprite->setPosition(m_pMainSprite->getContentSize() / 2.f);
	}

	auto attachGlow = [](Sprite* parent, const std::string& frame) -> Sprite* {
		if (!parent)
			return nullptr;
		Sprite* glow = createIconFrame(frame, nullptr);
		if (!glow)
			return nullptr;
		glow->setStretchEnabled(false);
		glow->setBlendFunc(GameToolbox::getBlending());
		glow->setPosition(parent->getContentSize() / 2.f);
		glow->setVisible(false);
		parent->addChild(glow, -2);
		return glow;
	};

	m_pMainGlowSprite = attachGlow(m_pMainSprite, StringUtils::format("player_%02d_glow_001.png", frame));
	if (!m_pMainGlowSprite)
		m_pMainGlowSprite = attachGlow(m_pMainSprite, "player_01_glow_001.png");

	m_pShipSprite = createIconFrame(StringUtils::format("ship_%02d_001.png", shipId), "ship_01_001.png");
	if (!m_pShipSprite)
		return false;
	m_pShipSprite->setStretchEnabled(false);
	m_pShipSprite->setVisible(false);
	m_pShipSprite->setPosition({15.f, 10.f});
	addChild(m_pShipSprite, 2);

	m_pShipSecondarySprite = createIconFrame(StringUtils::format("ship_%02d_2_001.png", shipId), "ship_01_2_001.png");
	if (m_pShipSecondarySprite)
	{
		m_pShipSecondarySprite->setStretchEnabled(false);
		m_pShipSprite->addChild(m_pShipSecondarySprite, -1);
		m_pShipSecondarySprite->setPosition(m_pShipSprite->getContentSize() / 2.f);
	}

	m_pShipGlowSprite = attachGlow(m_pShipSprite, StringUtils::format("ship_%02d_glow_001.png", shipId));
	if (!m_pShipGlowSprite)
		m_pShipGlowSprite = attachGlow(m_pShipSprite, "ship_01_glow_001.png");

	_ballSprite = createIconFrame(StringUtils::format("player_ball_%02d_001.png", ballId), "player_ball_01_001.png");
	if (!_ballSprite)
		return false;
	_ballSprite->setStretchEnabled(false);
	_ballSprite->setVisible(false);
	_ballSprite->setPosition({15.f, 15.f});
	addChild(_ballSprite, 1);

	_ballSecondarySprite = createIconFrame(StringUtils::format("player_ball_%02d_2_001.png", ballId), "player_ball_01_2_001.png");
	if (_ballSecondarySprite)
	{
		_ballSecondarySprite->setStretchEnabled(false);
		_ballSprite->addChild(_ballSecondarySprite, -1);
		_ballSecondarySprite->setPosition(_ballSprite->getContentSize() / 2.f);
	}

	_ballGlowSprite = attachGlow(_ballSprite, StringUtils::format("player_ball_%02d_glow_001.png", ballId));
	if (!_ballGlowSprite)
		_ballGlowSprite = attachGlow(_ballSprite, "player_ball_01_glow_001.png");

	_ufoSprite = createIconFrame(StringUtils::format("bird_%02d_001.png", ufoId), "bird_01_001.png");
	if (!_ufoSprite)
		return false;
	_ufoSprite->setStretchEnabled(false);
	_ufoSprite->setVisible(false);
	_ufoSprite->setPosition({15.f, 8.f});
	addChild(_ufoSprite, 1);

	_ufoSecondarySprite = createIconFrame(StringUtils::format("bird_%02d_2_001.png", ufoId), "bird_01_2_001.png");
	if (_ufoSecondarySprite)
	{
		_ufoSecondarySprite->setStretchEnabled(false);
		_ufoSprite->addChild(_ufoSecondarySprite, -1);
		_ufoSecondarySprite->setPosition(_ufoSprite->getContentSize() / 2.f);
	}

	_ufoTertiarySprite = createIconFrame(StringUtils::format("bird_%02d_3_001.png", ufoId), "bird_01_3_001.png");
	if (_ufoTertiarySprite)
	{
		_ufoTertiarySprite->setStretchEnabled(false);
		_ufoSprite->addChild(_ufoTertiarySprite, -2);
		_ufoTertiarySprite->setPosition(_ufoSprite->getContentSize() / 2.f);
	}

	_ufoGlowSprite = attachGlow(_ufoSprite, StringUtils::format("bird_%02d_glow_001.png", ufoId));
	if (!_ufoGlowSprite)
		_ufoGlowSprite = attachGlow(_ufoSprite, "bird_01_glow_001.png");

	auto attachExtra = [](Sprite* parent, const std::string& frame) {
		if (!parent)
			return;
		Sprite* extra = createIconFrame(frame, nullptr);
		if (!extra)
			return;
		extra->setStretchEnabled(false);
		extra->setPosition(parent->getContentSize() / 2.f);
		parent->addChild(extra, 1);
	};

	_waveSprite = createIconFrame(StringUtils::format("dart_%02d_001.png", waveId), "dart_01_001.png");
	if (_waveSprite)
	{
		_waveSprite->setStretchEnabled(false);
		_waveSprite->setVisible(false);
		_waveSprite->setPosition({15.f, 15.f});
		addChild(_waveSprite, 1);

		_waveSecondarySprite = createIconFrame(StringUtils::format("dart_%02d_2_001.png", waveId), "dart_01_2_001.png");
		if (_waveSecondarySprite)
		{
			_waveSecondarySprite->setStretchEnabled(false);
			_waveSprite->addChild(_waveSecondarySprite, -1);
			_waveSecondarySprite->setPosition(_waveSprite->getContentSize() / 2.f);
		}

		_waveGlowSprite = attachGlow(_waveSprite, StringUtils::format("dart_%02d_glow_001.png", waveId));
		if (!_waveGlowSprite)
			_waveGlowSprite = attachGlow(_waveSprite, "dart_01_glow_001.png");
		attachExtra(_waveSprite, StringUtils::format("dart_%02d_extra_001.png", waveId));
	}

	_swingSprite = createIconFrame(StringUtils::format("swing_%02d_001.png", swingId), "swing_01_001.png");
	if (_swingSprite)
	{
		_swingSprite->setStretchEnabled(false);
		_swingSprite->setVisible(false);
		_swingSprite->setPosition({15.f, 15.f});
		addChild(_swingSprite, 1);

		_swingSecondarySprite = createIconFrame(StringUtils::format("swing_%02d_2_001.png", swingId), "swing_01_2_001.png");
		if (_swingSecondarySprite)
		{
			_swingSecondarySprite->setStretchEnabled(false);
			_swingSprite->addChild(_swingSecondarySprite, -1);
			_swingSecondarySprite->setPosition(_swingSprite->getContentSize() / 2.f);
		}

		_swingGlowSprite = attachGlow(_swingSprite, StringUtils::format("swing_%02d_glow_001.png", swingId));
		if (!_swingGlowSprite)
			_swingGlowSprite = attachGlow(_swingSprite, "swing_01_glow_001.png");
		attachExtra(_swingSprite, StringUtils::format("swing_%02d_extra_001.png", swingId));
	}

	const int robotId = pickIcon(IconType::kIconTypeRobot, 1);
	_robotSprite = AnimatedIconSprite::create(IconType::kIconTypeRobot, robotId);
	if (_robotSprite)
	{
		_robotSprite->setPosition({15.f, 15.f});
		_robotSprite->setVisible(false);
		addChild(_robotSprite, 1);
	}

	const int spiderId = pickIcon(IconType::kIconTypeSpider, 1);
	_spiderSprite = AnimatedIconSprite::create(IconType::kIconTypeSpider, spiderId);
	if (_spiderSprite)
	{
		_spiderSprite->setPosition({15.f, 15.f});
		_spiderSprite->setVisible(false);
		addChild(_spiderSprite, 1);
	}

	setGlowColor(gm->getPlayerGlowColor());
	setGlow(gm->isPlayerGlowEnabled());

	// particles
	auto image = new Image();
	image->initWithImageFile("square.png");
	auto texture = new Texture2D();
	texture->initWithImage(image);

	dragEffect1 = ParticleSystemQuad::create("dragEffect.plist");
	dragEffect1->setTexture(texture);
	dragEffect1->setPositionType(ParticleSystem::PositionType::FREE);
	dragEffect1->pauseEmissions();

	gameLayer->addChild(dragEffect1, 1);

	dragEffect2 = ParticleSystemQuad::create("dragEffect.plist");
	dragEffect2->setTexture(texture);
	dragEffect2->setPositionType(ParticleSystem::PositionType::FREE);
	dragEffect2->pauseEmissions();
	dragEffect2->setPositionY(2);

	gameLayer->addChild(dragEffect2, 1);

	dragEffect3 = ParticleSystemQuad::create("dragEffect.plist");
	dragEffect3->setTexture(texture);
	dragEffect3->setPositionType(ParticleSystem::PositionType::FREE);
	dragEffect3->pauseEmissions();
	dragEffect3->setPositionY(2);

	gameLayer->addChild(dragEffect3, 1);

	// particle properties
	dragEffect2->setSpeed(dragEffect2->getSpeed() * 0.2f);
	dragEffect2->setSpeedVar(dragEffect2->getSpeedVar() * 0.2f);

	dragEffect3->setSpeed(dragEffect3->getSpeed() * 0.2f);
	dragEffect3->setSpeedVar(dragEffect3->getSpeedVar() * 0.2f);
	dragEffect3->setAngleVar(dragEffect3->getAngleVar() * 2.f);
	dragEffect3->setStartSize(dragEffect3->getStartSize() * 1.5f);
	dragEffect3->setStartSizeVar(dragEffect3->getStartSizeVar() * 1.5f);

	// other particles
	shipDragEffect = ParticleSystemQuad::create("shipDragEffect.plist");
	shipDragEffect->setTexture(texture);
	shipDragEffect->setPositionType(ParticleSystem::PositionType::GROUPED);
	shipDragEffect->pauseEmissions();

	gameLayer->addChild(shipDragEffect, 1);

	landEffect1 = ParticleSystemQuad::create("landEffect.plist");
	landEffect1->setTexture(texture);
	landEffect1->setPositionType(ParticleSystem::PositionType::GROUPED);
	landEffect1->pauseEmissions();

	gameLayer->addChild(landEffect1, 1);

	landEffect2 = ParticleSystemQuad::create("landEffect.plist");
	landEffect2->setTexture(texture);
	landEffect2->setPositionType(ParticleSystem::PositionType::GROUPED);
	landEffect2->pauseEmissions();

	gameLayer->addChild(landEffect2, 1);

	_waveTrail = HardStreak::create();
	if (_waveTrail)
	{
		_waveTrail->setVisible(false);
		gameLayer->addChild(_waveTrail, 15);
	}

	deactivateStreak();

	// scheduleUpdate();

	return true;
}

void PlayerObject::setMainColor(Color3B col)
{
	float r = static_cast<float>(col.r);
	float g = static_cast<float>(col.g);
	float b = static_cast<float>(col.b);
	dragEffect1->setStartColor({r, g, b, 100});
	dragEffect1->setEndColor({r, g, b, 0});

	shipDragEffect->setStartColor({r, g, b, 190});
	shipDragEffect->setEndColor({r, g, b, 0});

	if (m_pMainSprite)
		m_pMainSprite->setColor(col);
	if (m_pShipSprite)
		m_pShipSprite->setColor(col);
	if (_ballSprite)
		_ballSprite->setColor(col);
	if (_ufoSprite)
		_ufoSprite->setColor(col);
	if (_waveSprite)
		_waveSprite->setColor(col);
	if (_swingSprite)
		_swingSprite->setColor(col);
	if (_robotSprite)
		_robotSprite->setMainColor(col);
	if (_spiderSprite)
		_spiderSprite->setMainColor(col);
	if (_waveTrail)
		_waveTrail->setTint(col);
}

void PlayerObject::setSecondaryColor(Color3B col)
{
	if (m_pSecondarySprite)
		m_pSecondarySprite->setColor(col);
	if (m_pShipSecondarySprite)
		m_pShipSecondarySprite->setColor(col);
	if (_ballSecondarySprite)
		_ballSecondarySprite->setColor(col);
	if (_ufoSecondarySprite)
		_ufoSecondarySprite->setColor(col);
	if (_waveSecondarySprite)
		_waveSecondarySprite->setColor(col);
	if (_swingSecondarySprite)
		_swingSecondarySprite->setColor(col);
	if (_robotSprite)
		_robotSprite->setSecondaryColor(col);
	if (_spiderSprite)
		_spiderSprite->setSecondaryColor(col);
}

void PlayerObject::setGlow(bool glow)
{
	m_bHasGlow = glow;
	if (m_pMainGlowSprite)
		m_pMainGlowSprite->setVisible(glow);
	if (m_pShipGlowSprite)
		m_pShipGlowSprite->setVisible(glow);
	if (_ballGlowSprite)
		_ballGlowSprite->setVisible(glow);
	if (_ufoGlowSprite)
		_ufoGlowSprite->setVisible(glow);
	if (_waveGlowSprite)
		_waveGlowSprite->setVisible(glow);
	if (_swingGlowSprite)
		_swingGlowSprite->setVisible(glow);
	if (_robotSprite)
		_robotSprite->setGlow(glow);
	if (_spiderSprite)
		_spiderSprite->setGlow(glow);
}

void PlayerObject::setGlowColor(Color3B col)
{
	m_glowColor = col;
	if (m_pMainGlowSprite)
		m_pMainGlowSprite->setColor(col);
	if (m_pShipGlowSprite)
		m_pShipGlowSprite->setColor(col);
	if (_ballGlowSprite)
		_ballGlowSprite->setColor(col);
	if (_ufoGlowSprite)
		_ufoGlowSprite->setColor(col);
	if (_waveGlowSprite)
		_waveGlowSprite->setColor(col);
	if (_swingGlowSprite)
		_swingGlowSprite->setColor(col);
	if (_robotSprite)
		_robotSprite->setGlowColor(col);
	if (_spiderSprite)
		_spiderSprite->setGlowColor(col);
}

Color3B PlayerObject::getMainColor() { return m_pMainSprite ? m_pMainSprite->getColor() : Color3B::WHITE; }

Color3B PlayerObject::getSecondaryColor() { return m_pSecondarySprite ? m_pSecondarySprite->getColor() : Color3B::WHITE; }

void PlayerObject::setIsDead(bool value) { m_bIsDead = value; }

void PlayerObject::setIsOnGround(bool value) { m_bOnGround = value; }

void PlayerObject::update(float dt)
{
	m_prevPos = getPosition();
	if (this->m_bIsDead) return;

	if (isGroundedMode())
	{
		if (isOnGround())
		{
			if (!_particles1Activated)
			{
				dragEffect1->resumeEmissions();
				_particles1Activated = true;
			}
			if (getActionByTag(2)) stopActionByTag(2);
		}
		else
		{
			if (_particles1Activated && !getActionByTag(2))
			{
				Sequence* action = Sequence::create(
					DelayTime::create(1.f / 16.5f), CallFunc::create([&]() {
						if (_particles1Activated) dragEffect1->pauseEmissions();
						_particles1Activated = false;
					}),
					nullptr);
				action->setTag(2);
				runAction(action);
			}
		}
		// shipDragEffect->pauseEmissions();
		if (_particles3Activated)
		{
			dragEffect3->pauseEmissions();
			_particles3Activated = false;
		}
		if (_particles2Activated)
		{
			dragEffect2->pauseEmissions();
			_particles2Activated = false;
		}
	}
	else // is ship
	{
		if (m_bIsHolding)
		{
			if (!_particles3Activated) dragEffect3->resumeEmissions();
			_particles3Activated = true;
		}
		else
		{
			if (_particles3Activated) dragEffect3->pauseEmissions();
			_particles3Activated = false;
		}
		if (!_particles2Activated)
		{
			dragEffect2->resumeEmissions();
			_particles2Activated = true;
		}
		if (_particles1Activated)
		{
			dragEffect1->pauseEmissions();
			_particles1Activated = false;
		}
		// if (isOnGround() && m_dYVel > -1.f)
		//	shipDragEffect->resumeEmissions();
		// else
		//	shipDragEffect->pauseEmissions();
	}

		if (!this->m_bIsLocked)
	{
		direction = clampf(direction, -1.f, 1.f);

		if (!m_bIsPlatformer) direction = 1.f;

		if (_isDashing)
		{
			if (!m_bIsHolding)
			{
				_isDashing = false;
			}
			else
			{
				m_dYVel = 0.0;
				float velX = (float)((double)dt * m_dXVel * (!m_bIsPlatformer ? 1.f : direction) * getPlayerSpeed());
				setPosition(getPosition() + Vec2{velX, 0.f});
			}
		}

		if (!_isDashing)
		{
			float dtSlow = dt * 0.9f;
			this->updateJump(dtSlow);

			float velY = (float)((double)dtSlow * m_dYVel);
			float velX = (float)((double)dt * m_dXVel * (!m_bIsPlatformer ? 1.f : direction) * getPlayerSpeed());

			if (_currentGamemode == PlayerGamemodeWave)
			{
				const float dir = (m_bIsHolding ? 1.f : -1.f) * flipMod();
				velY = velX * dir;
				m_dYVel = velY / (dtSlow > 0.f ? dtSlow : 1.f);
				setRotation(-45.f * dir);
			}

			setPosition(getPosition() + Vec2{velX, velY});
		}
	}

	//setScaleX(direction < -0.05f ? -1.f : direction > 0.05f ? 1.f : getScaleX());

	dragEffect1->setPosition(this->getPosition() + Vec2 {-10.f, flipMod() * -13.f});
	dragEffect2->setPosition(this->getPosition() + m_pShipSprite->getPosition() + Vec2 {-10.f, flipMod() * -3.f});
	dragEffect3->setPosition(dragEffect2->getPosition());
	shipDragEffect->setPosition(this->getPosition() + Vec2 {1.f, flipMod() * -15.f});

	/*if (!_currentGamemode == PlayerGamemodeShip)
		motionStreak->setPosition(this->getPosition() + Vec2{ -5.f, 0.f });
	else
		motionStreak->setPosition(dragEffect2->getPosition());

	motionStreak->setColor(getSecondaryColor());*/
	dragEffect1->setColor(getMainColor());
	shipDragEffect->setColor(getMainColor());

	// auto particle = Sprite::create("square.png");
	// particle->setStretchEnabled(false);
	// particle->setScale(0.05);
	// particle->setPosition(this->getPosition());
	// this->gameLayer->addChild(particle, 999);

	updateIconAnimation();
	updateWaveTrail();

	_touchedRingObject = nullptr;
	_touchedPadObject = nullptr;
}

void PlayerObject::storeShipRotationPos()
{
	_shipRotationPos = getPosition();
	_shipRotationPosValid = true;
}

void PlayerObject::updateShipRotation(float dt)
{
	// dt is in "tick" units (realDelta * 60), matching RobTop's updateShipRotation.
	if (_currentGamemode != PlayerGamemodeShip && _currentGamemode != PlayerGamemodeUFO)
		return;

	const Vec2 pos = getPosition();
	Vec2 diff = pos - (_shipRotationPosValid ? _shipRotationPos : m_prevPos);
	diff.y = -diff.y;

	const float distSq = diff.x * diff.x + diff.y * diff.y;
	if (dt * 1.2f > distSq)
	{
		_shipRotationPos = pos;
		_shipRotationPosValid = true;
		return;
	}

	const float fromAngleDeg = getRotation();
	float toAngleDeg = atan2f(diff.y, diff.x) * 57.29578f;
	float interp = 0.15f;

	if (_currentGamemode == PlayerGamemodeUFO)
	{
		const float clampVal = isGravityFlipped() ? -0.1f : 0.1f;
		float toRad = toAngleDeg * 0.017453292f;
		if (isGravityFlipped())
			toRad = std::max(toRad * -0.4f, clampVal);
		else
			toRad = std::min(toRad * -0.4f, clampVal);
		toAngleDeg = toRad * 57.29578f;
	}

	const float t = std::clamp(std::min(dt * interp, dt), 0.f, 1.f);
	setRotation(GameToolbox::slerp(fromAngleDeg, toAngleDeg, t));

	_shipRotationPos = pos;
	_shipRotationPosValid = true;
}

void PlayerObject::spawnPortalCircle(ax::Color4B color, float radius)
{
	CircleWave* circle = CircleWave::create(0.3f, color, 5.f, radius, true, false);

	circle->setPosition(getPortalP());

	gameLayer->addChild(circle, 0);
}

void PlayerObject::deactivateStreak()
{
	if (!_waveTrail)
		return;
	_waveTrail->stopStroke();
	_waveTrail->reset();
	_waveTrail->setVisible(false);
}

void PlayerObject::activateStreak()
{
	if (!_waveTrail)
		return;
	if (_currentGamemode != PlayerGamemodeWave)
	{
		deactivateStreak();
		return;
	}

	_waveTrail->setStroke(_mini ? 8.f : 14.f);
	_waveTrail->setTint(getMainColor());
	_waveTrail->setVisible(true);
	_waveTrail->resumeStroke();
}

void PlayerObject::updateWaveTrail()
{
	if (!_waveTrail || _currentGamemode != PlayerGamemodeWave || !_waveTrail->isVisible())
		return;

	if (m_bIsDead)
		return;

	_waveTrail->setStroke(_mini ? 8.f : 14.f);
	_waveTrail->addPoint(getPosition());
	_waveTrail->updateStroke();

	const int z = getLocalZOrder();
	_waveTrail->setLocalZOrder(z > 0 ? z - 1 : 0);
}

void PlayerObject::propellPlayer(double force)
{
	m_isRising = true;
	setIsOnGround(false);
	m_dYVel = flipMod() * 16 * force * (_vehicleSize == 1.0 ? 1.0 : 0.8);

	if (_currentGamemode == PlayerGamemodeBall || _currentGamemode == PlayerGamemodeSpider) m_dYVel *= 0.6;
	if (_currentGamemode == PlayerGamemodeShip)
		_isAccelerating = true;

	runRotateAction();
	setLastGroundPos(getPosition());

	activateStreak();
}

void PlayerObject::setTouchedRing(GameObject* obj) { _touchedRingObject = obj; }

void PlayerObject::ringJump(GameObject* obj)
{
	if (_touchedRingObject && _queuedHold && m_bIsHolding)
	{
		GameToolbox::log("h ffsiofsfsdofs");
		_touchedRingObject->triggerActivated(this);
		m_isRising = true;
		_queuedHold = false;
		setIsOnGround(false);

		double newYVel = m_dJumpHeight;

		switch (obj->getGameObjectType())
		{
		case kGameObjectTypeDropRing:
			switch (_currentGamemode)
			{
			case PlayerGamemodeUFO:
				newYVel = -11.2 * flipMod();
				break;
			case PlayerGamemodeShip:
			case PlayerGamemodeWave:
				newYVel = -14 * flipMod();
				break;
			case PlayerGamemodeSpider:
				newYVel = -16.5 * flipMod();
				break;
			default:
				newYVel = -15 * flipMod();
				break;
			}
			m_dYVel = newYVel;
			if (_currentGamemode == PlayerGamemodeShip)
				_isAccelerating = true;
			activateStreak();
			if (_currentGamemode == PlayerGamemodeBall) m_bIsHolding = false;
			_touchedRingObject = nullptr;
			return;
		case kGameObjectTypeRedJumpRing:
			switch (_currentGamemode)
			{
			case PlayerGamemodeShip:
				if (_vehicleSize != 1.0f) newYVel *= 1.4;
				break;
			case PlayerGamemodeUFO:
				if (_vehicleSize == 1.0f)
					newYVel *= 1.02;
				else
					newYVel *= 1.36;
				break;
			case PlayerGamemodeSpider:
			case PlayerGamemodeBall:
				newYVel *= 1.34;
				break;
			case PlayerGamemodeRobot:
				newYVel *= 1.28;
				break;
			default:
				newYVel *= 1.38;
				break;
			}
			break;
		case kGameObjectTypePinkJumpRing:
			switch (_currentGamemode)
			{
			case PlayerGamemodeShip:
				newYVel *= 0.37;
				break;
			case PlayerGamemodeUFO:
				newYVel *= 0.42;
				break;
			case PlayerGamemodeBall:
				newYVel *= 0.77;
				break;
			default:
				newYVel *= 0.72;
				break;
			}
			break;
		case kGameObjectTypeGravityRing:
			newYVel *= 0.8;
			break;
		case kGameObjectTypeGreenRing:
			if (_currentGamemode == PlayerGamemodeShip) {
				newYVel *= 0.7;
			}
			flipGravity(!isGravityFlipped());
			break;
		case kGameObjectTypeDashRing:
		case kGameObjectTypeGravityDashRing:
			_isDashing = true;
			m_dYVel = 0.0;
			if (obj->getGameObjectType() == kGameObjectTypeGravityDashRing)
				flipGravity(!isGravityFlipped());
			_touchedRingObject = nullptr;
			activateStreak();
			return;
		case kGameObjectTypeSpiderRing:
			_spiderTeleportQueued = true;
			_touchedRingObject = nullptr;
			return;
		case kGameObjectTypeCustomRing:
			_touchedRingObject = nullptr;
			return;
		default:
			if (_currentGamemode == PlayerGamemodeRobot) newYVel *= 0.9;
			break;
		}

		newYVel *= flipMod();
		newYVel *= _vehicleSize < 1.f ? 0.8f : 1.f;

		m_dYVel = newYVel;

		if (_currentGamemode == PlayerGamemodeShip)
			_isAccelerating = true;

		if (_currentGamemode == PlayerGamemodeBall)
			runBallRotation();
		else
			runRotateAction();
		_touchedRingObject = nullptr;
		_hasRingJumped = true;
		setLastGroundPos(getPosition());

		activateStreak();

		if (_currentGamemode == PlayerGamemodeBall || _currentGamemode == PlayerGamemodeSpider)
		{
			m_bIsHolding = false;
			m_dYVel *= 0.7;
		}

		if (obj->getGameObjectType() == kGameObjectTypeGravityRing)
		{
			flipGravity(!isGravityFlipped());
		}
	}
}

void PlayerObject::flipGravity(bool gravity)
{
	if (m_bGravityFlipped != gravity)
	{
		m_bGravityFlipped = gravity;
		m_dYVel /= 2.f;

		setScaleY(gravity ? getScale() * -1.f : getScale() * 1.f);

		activateStreak();

		dragEffect1->setAngle(dragEffect1->getAngle() + 180);
		dragEffect1->setGravity(Vec2 {dragEffect1->getGravity().x, -dragEffect1->getGravity().y});

		dragEffect2->setAngle(dragEffect2->getAngle() + 180);
		dragEffect2->setGravity(Vec2 {dragEffect2->getGravity().x, -dragEffect2->getGravity().y});

		dragEffect3->setAngle(dragEffect3->getAngle() + 180);
		dragEffect3->setGravity(Vec2 {dragEffect3->getGravity().x, -dragEffect3->getGravity().y});

		shipDragEffect->setAngle(shipDragEffect->getAngle() + 180);
		shipDragEffect->setGravity(Vec2 {shipDragEffect->getGravity().x, -shipDragEffect->getGravity().y});
	}
}

void PlayerObject::updateJump(float dt)
{
	float localGravity = m_dGravity;

	const int flipGravityMult = flipMod();

	float playerSize = _mini ? 0.8f : 1.0f;

	if (_currentGamemode == PlayerGamemodeWave)
		return;

	if (_currentGamemode == PlayerGamemodeShip || _currentGamemode == PlayerGamemodeUFO ||
		_currentGamemode == PlayerGamemodeSwing)
	{
		if (_mini)
			playerSize = 0.85f;

		float upperVelocityLimit;
		float lowerVelocityLimit;
		if (_vehicleSize == 1.0f)
		{
			upperVelocityLimit = 8.0f / playerSize;
			lowerVelocityLimit = -6.4f / playerSize;
		}
		else
		{
			upperVelocityLimit = 9.4118f;
			lowerVelocityLimit = -7.5294f;
			playerSize = 0.85f;
		}

		// Clear boost-lock once velocity returns inside normal ship limits.
		if (!isGravityFlipped())
		{
			if (m_dYVel >= 0.0 && m_dYVel < upperVelocityLimit)
				_isAccelerating = false;
			if (m_dYVel <= 0.0 && m_dYVel > lowerVelocityLimit)
				_isAccelerating = false;
		}
		else
		{
			if (m_dYVel <= 0.0 && m_dYVel > -upperVelocityLimit)
				_isAccelerating = false;
			if (m_dYVel >= 0.0 && m_dYVel < -lowerVelocityLimit)
				_isAccelerating = false;
		}

		if (this->_currentGamemode == PlayerGamemodeSwing)
		{
			if (_hasJustHeld)
			{
				_hasJustHeld = false;
				flipGravity(!isGravityFlipped());
			}
			float gravMult = playerIsFalling() ? 0.5f : 0.4f;
			m_dYVel -= localGravity * dt * flipGravityMult * gravMult / playerSize;
		}
		else if (this->_currentGamemode == PlayerGamemodeShip)
		{
			// Geometry Dash 2.2 ship thrust (from camila314/gdp decomp).
			float shipAccel = 0.8f;

			if (m_bIsHolding)
			{
				if (_isAccelerating && (isGravityFlipped() ? m_dYVel <= 0.0 : m_dYVel >= 0.0))
					shipAccel = 0.8f;
				else
					shipAccel = -1.0f;
			}
			else if (!_isAccelerating)
			{
				shipAccel = 0.8f;
				if (!playerIsFallingBugged())
					shipAccel = 1.2f;
			}

			float extraBoost = playerIsFallingBugged() ? 0.5f : 0.4f;
			float grav = localGravity;

			if (m_bIsPlatformer || _isAccelerating)
			{
				if (m_bIsPlatformer)
					grav *= 0.8f;
				if (shipAccel < 0.0f)
					grav = localGravity;
			}
			else if (m_bIsHolding)
			{
				grav = localGravity;
			}
			else
			{
				extraBoost = 0.4f;
			}

			m_dYVel -= shipAccel * grav * dt * flipGravityMult * extraBoost / playerSize;
		}
		else if (_currentGamemode == PlayerGamemodeUFO)
		{
			if (m_bIsHolding && _hasJustHeld)
			{
				_hasJustHeld = false;

				const float sizeMult = _mini ? 8.f : 7.f;
				double newVel = flipMod() * sizeMult * playerSize;

				if (!isGravityFlipped() && m_dYVel < newVel || newVel < m_dYVel)
				{
					m_dYVel = newVel;
				}
			}
			float gravityMult = 0.8f;

			if (!playerIsFalling()) gravityMult = 1.2f;

			m_dYVel -= localGravity * dt * flipMod() * gravityMult * 0.5 / playerSize;
		}

		if (!this->isGravityFlipped())
		{
			if (this->m_dYVel <= lowerVelocityLimit) this->m_dYVel = lowerVelocityLimit;
		}
		else
		{
			if (this->m_dYVel <= -upperVelocityLimit) this->m_dYVel = -upperVelocityLimit;

			upperVelocityLimit = (_vehicleSize == 1.0f) ? (6.4f / playerSize) : 7.5294f;
		}
		if (this->m_dYVel >= upperVelocityLimit) this->m_dYVel = upperVelocityLimit;
	}
	else
	{
		float gravityMultiplier = 1.0f;

		if (_currentGamemode == PlayerGamemodeBall) gravityMultiplier = 0.6f;

		bool shouldJump = m_bIsHolding;

		if (_currentGamemode == PlayerGamemodeSpider && _hasJustHeld)
		{
			_hasJustHeld = false;
			_spiderTeleportQueued = true;
			m_bIsHolding = false;
			return;
		}

		if (_currentGamemode == PlayerGamemodeRobot && shouldJump && _hasJustHeld && !isOnGround() && _jumpedTimes < 2)
		{
			_hasJustHeld = false;
			_jumpedTimes = 2;
			m_isRising = true;
			setIsOnGround(false);
			m_dYVel = flipGravityMult * m_dJumpHeight * playerSize * 0.85f;
			if (_robotSprite)
				_robotSprite->playAnimation("jump_start", false, true);
			return;
		}

		if (shouldJump && isOnGround())
		{
			m_isRising = true;
			setIsOnGround(false);

			float jumpAccel = m_dJumpHeight;

			m_dYVel = flipGravityMult * jumpAccel * playerSize;

			if (_currentGamemode == PlayerGamemodeBall)
			{
				this->flipGravity(!this->isGravityFlipped());
				this->m_bIsHolding = false;
				this->m_dYVel *= 0.6;
			}
			else if (_currentGamemode == PlayerGamemodeCube || _currentGamemode == PlayerGamemodeRobot)
			{
				if (!_touchedRingObject) _queuedHold = false;
				runRotateAction();
			}
		}
		else
		{
			if (m_isRising)
			{
				m_dYVel -= localGravity * dt * flipGravityMult * gravityMultiplier;
				if (playerIsFalling())
				{
					m_isRising = false;
					setIsOnGround(false);
				}
			}
			else
			{
				if (!isGravityFlipped())
				{
					if (m_dYVel < -m_dGravity * 2.f) m_bOnGround = false;
				}
				else
				{
					if (m_dYVel > m_dGravity * 2.f) m_bOnGround = false;
				}

				m_dYVel -= localGravity * dt * flipGravityMult * gravityMultiplier;
				m_dYVel = isGravityFlipped() ? std::min(m_dYVel, 15.0) : std::max(m_dYVel, -15.0);
				if (!isGravityFlipped())
				{
					if (m_dYVel >= m_dGravity * 2.0f) return;
				}
				else
				{
					if (m_dYVel <= m_dGravity * 2.0f) return;
				}

				if (_currentGamemode != PlayerGamemodeBall && getActionByTag(0) == nullptr) runRotateAction();

				if (isGravityFlipped())
				{
					if (m_dYVel >= -4.0) return;
				}
				else
				{
					if (m_dYVel <= 4.0) return;
				}
			}
		}
	}
}

bool PlayerObject::playerIsFalling()
{
	// Corrected (post-2.2) falling check: moving with gravity.
	return isGravityFlipped() ? (m_dYVel > 0.0) : (m_dYVel < 0.0);
}

bool PlayerObject::playerIsFallingBugged()
{
	// Legacy threshold used by older ship code paths.
	if (isGravityFlipped())
		return m_dYVel > m_dGravity;
	return m_dYVel < m_dGravity;
}

void PlayerObject::collidedWithObject(float dt, GameObject* obj)
{
	if (_currentGamemode == PlayerGamemodeWave)
	{
		if (auto* pl = PlayLayer::getInstance())
			pl->destroyPlayer(this);
		return;
	}

	const Rect objectBounds = obj->getOuterBounds();
	Rect playerBounds = _mini ? getOuterBounds(0.6f, 0.6f) : getOuterBounds();
	const Rect innerBounds = getInnerBounds();

	const Vec2 movement = getPosition() - m_prevPos;
	Rect previousBounds = playerBounds;
	previousBounds.origin -= movement;

	const bool horizontalOverlap =
		playerBounds.getMaxX() > objectBounds.getMinX() &&
		playerBounds.getMinX() < objectBounds.getMaxX();
	const bool prevHorizontalOverlap =
		previousBounds.getMaxX() > objectBounds.getMinX() &&
		previousBounds.getMinX() < objectBounds.getMaxX();
	if (!horizontalOverlap && !prevHorizontalOverlap)
	{
		if (innerBounds.intersectsRect(objectBounds) && !obj->_isTrigger)
			static_cast<PlayLayer*>(getPlayLayer())->destroyPlayer(this);
		return;
	}

	const bool flying = isFlying();
	const float fallStep = std::abs(static_cast<float>(m_dYVel)) * std::max(dt, 0.f);
	// Cube needs a wide snap so gravity cannot tunnel through a block.
	// Ship/UFO must only land when they actually approach the surface from outside;
	// otherwise they get pushed onto every block and fly through the level.
	const float snapPad = flying ? 6.f : std::max(12.f, fallStep + 6.f);

	if (!isGravityFlipped() && m_dYVel <= 0.f)
	{
		const float blockTop = objectBounds.getMaxY();
		const bool cameFromAbove = previousBounds.getMinY() >= blockTop - snapPad;

		if (flying)
		{
			if (cameFromAbove && playerBounds.getMinY() <= blockTop + 1.f)
			{
				setPositionY(blockTop + playerBounds.size.height * 0.5f);
				hitGround(false);
				return;
			}
		}
		else
		{
			const float embed = blockTop - playerBounds.getMinY();
			const bool slightlyInFromTop =
				embed >= -1.f && embed <= snapPad && getPositionY() >= objectBounds.getMidY();
			const bool tunneledThrough = cameFromAbove && playerBounds.getMaxY() <= blockTop;
			if ((cameFromAbove || slightlyInFromTop || tunneledThrough) &&
				playerBounds.getMinY() <= blockTop + 1.f)
			{
				setPositionY(blockTop + playerBounds.size.height * 0.5f);
				hitGround(false);
				return;
			}
		}
	}
	else if (isGravityFlipped() && m_dYVel >= 0.f)
	{
		const float blockBottom = objectBounds.getMinY();
		const bool cameFromBelow = previousBounds.getMaxY() <= blockBottom + snapPad;

		if (flying)
		{
			if (cameFromBelow && playerBounds.getMaxY() >= blockBottom - 1.f)
			{
				setPositionY(blockBottom - playerBounds.size.height * 0.5f);
				hitGround(true);
				return;
			}
		}
		else
		{
			const float embed = playerBounds.getMaxY() - blockBottom;
			const bool slightlyInFromBottom =
				embed >= -1.f && embed <= snapPad && getPositionY() <= objectBounds.getMidY();
			const bool tunneledThrough = cameFromBelow && playerBounds.getMinY() >= blockBottom;
			if ((cameFromBelow || slightlyInFromBottom || tunneledThrough) &&
				playerBounds.getMaxY() >= blockBottom - 1.f)
			{
				setPositionY(blockBottom - playerBounds.size.height * 0.5f);
				hitGround(true);
				return;
			}
		}
	}

	// Side / underside / embed is fatal. Flying dies on any non-landing contact
	// (the 7.5 inner box is too small and lets the ship pass through walls).
	if (!obj->_isTrigger && (flying || innerBounds.intersectsRect(objectBounds)))
		static_cast<PlayLayer*>(getPlayLayer())->destroyPlayer(this);
}

void PlayerObject::collidedWithSlope(float dt, GameObject* obj)
{
	if (!obj)
		return;

	const Rect objectBounds = obj->getOuterBounds();
	if (objectBounds.size.width <= 0.f || objectBounds.size.height <= 0.f)
		return;

	// Wave cannot land on slopes in GD.
	if (_currentGamemode == PlayerGamemodeWave)
	{
		static_cast<PlayLayer*>(getPlayLayer())->destroyPlayer(this);
		return;
	}

	Rect playerBounds = _mini ? getOuterBounds(0.6f, 0.6f) : getOuterBounds();
	const float playerRadius = playerBounds.size.height * 0.5f;
	const float px = getPositionX();
	const float upsideMod = isGravityFlipped() ? -1.f : 1.f;
	const bool slopeFloorTop = obj->slopeFloorTop();
	const bool slopeUphill = obj->isSlopeUphill();

	// Travelling right into an uphill slope (or left into downhill) = uphill contact.
	const bool playerUphill = slopeUphill;

	const float slopeAngle = obj->getSlopeAngle();
	const float cosA = std::max(std::cos(slopeAngle), 0.15f);
	const float playerRadOnSlope = playerRadius / cosA;
	const float playerRadOnPrevSlope = _wasOnSlope ? (playerRadius / std::max(std::cos(std::abs(_slopeRotation)), 0.15f)) : playerRadius;

	const float clingExtra = playerUphill ? (_wasOnSlope ? 4.f : 1.f) : 0.f;
	const float onSlopeThreshold = getPositionY() - upsideMod * (playerRadOnPrevSlope + clingExtra);

	if (_wasOnSlope)
	{
		if (isGravityFlipped())
		{
			if (onSlopeThreshold < objectBounds.getMinY())
				return;
		}
		else if (onSlopeThreshold > objectBounds.getMaxY())
		{
			return;
		}
	}
	else
	{
		Rect exitRect = objectBounds;
		exitRect.origin.y += 1.f;
		exitRect.size.height = std::max(exitRect.size.height - 2.f, 1.f);
		if (!playerBounds.intersectsRect(exitRect))
			return;
	}

	const float slopeY = static_cast<float>(obj->slopeYPos(px));
	float newPlayerY = slopeY + (playerRadOnSlope) * (slopeFloorTop ? -1.f : 1.f);

	if (slopeFloorTop)
	{
		newPlayerY = std::max(newPlayerY, objectBounds.getMinY() - playerRadius);
		newPlayerY = std::min(newPlayerY, objectBounds.getMaxY());
	}
	else
	{
		newPlayerY = std::min(newPlayerY, objectBounds.getMaxY() + playerRadius);
		newPlayerY = std::max(newPlayerY, objectBounds.getMinY());
	}

	const bool slopeUpsideDown = isGravityFlipped() != slopeFloorTop;
	bool collidedSlope = false;

	if (slopeUpsideDown)
	{
		// Ceiling / inverted contact: kill on hard head hits, otherwise ignore soft contact.
		if (upsideMod * getPositionY() > upsideMod * newPlayerY)
		{
			if (obj->isSlopeHazard() || (!_wasOnSlope && upsideMod * getPositionY() - 2.f > upsideMod * newPlayerY))
			{
				static_cast<PlayLayer*>(getPlayLayer())->destroyPlayer(this);
				return;
			}
			setPositionY(newPlayerY);
			m_dYVel = isGravityFlipped() ? std::max(m_dYVel, 2.0) : std::min(m_dYVel, -2.0);
			setIsOnGround(false);
			return;
		}
		return;
	}

	// Floor slopes: snap when below (or clinging near) the surface.
	if (upsideMod * getPositionY() < upsideMod * newPlayerY)
	{
		collidedSlope = true;
	}
	else if (upsideMod * getPositionY() < upsideMod * (newPlayerY + clingExtra))
	{
		collidedSlope = playerUphill || m_dYVel * upsideMod <= 0.0;
	}

	if (!collidedSlope)
		return;

	if (obj->isSlopeHazard())
	{
		static_cast<PlayLayer*>(getPlayLayer())->destroyPlayer(this);
		return;
	}

	// Side-hit into a downhill slope before the center reaches it → treat like a block top.
	if (!_wasOnSlope && !slopeUphill && m_dYVel <= 0.0 && px < objectBounds.getMinX())
	{
		setPositionY(objectBounds.getMaxY() + playerRadius * upsideMod * (isGravityFlipped() ? -1.f : 1.f));
		if (!isGravityFlipped())
			setPositionY(objectBounds.getMaxY() + playerRadius);
		else
			setPositionY(objectBounds.getMinY() - playerRadius);
		hitGround(isGravityFlipped());
		return;
	}

	_currentSlope = obj;
	_isOnSlope = true;
	_slopeUphillContact = playerUphill;
	_slopeRotation = slopeAngle * (playerUphill ? 1.f : -1.f) * flipMod();

	const float slopeYVelocity = (objectBounds.size.height * m_playerSpeed) / objectBounds.size.width;
	const float angleSafe = std::max(slopeAngle, 0.05f);
	_slopeVelocity = std::min(1.12f / angleSafe, 1.54f) * slopeYVelocity * flipMod() * (playerUphill ? -1.f : 1.f);
	if (isFlying() || _currentGamemode == PlayerGamemodeBall)
		_slopeVelocity *= 0.75f;

	if (!_wasOnSlope)
		_slopeStartTime = _totalTime;

	setPositionY(newPlayerY);
	hitGround(false);

	// Cube rotates to match the slope surface (GD updateSlopeRotation).
	if (_currentGamemode == PlayerGamemodeCube || _currentGamemode == PlayerGamemodeRobot)
	{
		stopRotation();
		setRotation(_slopeRotation * 57.29578f);
	}
	else if (_currentGamemode == PlayerGamemodeBall)
	{
		stopRotation();
		setRotation(_slopeRotation * 57.29578f);
	}

	(void)dt;
}

void PlayerObject::beginSlopePass()
{
	_wasOnSlope = _isOnSlope;
	_isOnSlope = false;
	if (!_wasOnSlope)
	{
		_currentSlope = nullptr;
		_slopeVelocity = 0.f;
	}
}

void PlayerObject::endSlopePass(float dt)
{
	_totalTime += dt / 60.f;

	if (_wasOnSlope && !_isOnSlope)
	{
		// Leave / eject off the slope (RobTop slope exit velocity).
		if (_currentSlope && _slopeUphillContact && !isGravityFlipped())
		{
			const float hold = std::clamp(10.f * (_totalTime - _slopeStartTime), 0.4f, 1.f);
			m_dYVel = _slopeVelocity * hold;
			m_isRising = m_dYVel * flipMod() > 0.0;
			setIsOnGround(false);
			if (_currentGamemode == PlayerGamemodeCube || _currentGamemode == PlayerGamemodeRobot)
				runRotateAction();
		}
		else if (_wasOnSlope && !_slopeUphillContact)
		{
			// Downhill fall-off
			setIsOnGround(false);
			if (_currentGamemode == PlayerGamemodeCube)
				runRotateAction();
		}

		_currentSlope = nullptr;
		_slopeVelocity = 0.f;
		_slopeRotation = 0.f;
	}
	else if (_isOnSlope && (_currentGamemode == PlayerGamemodeCube || _currentGamemode == PlayerGamemodeBall ||
							_currentGamemode == PlayerGamemodeRobot))
	{
		// Keep orientation while riding.
		setRotation(_slopeRotation * 57.29578f);
	}
}

void PlayerObject::setGamemode(PlayerGamemode mode)
{
	if (_currentGamemode != mode)
	{
		if (_ufoSprite)
			_ufoSprite->setVisible(false);
		if (m_pShipSprite)
			m_pShipSprite->setVisible(false);
		if (m_pMainSprite)
			m_pMainSprite->setVisible(false);
		if (_ballSprite)
			_ballSprite->setVisible(false);
		if (_waveSprite)
			_waveSprite->setVisible(false);
		if (_swingSprite)
			_swingSprite->setVisible(false);
		if (_spiderSprite)
			_spiderSprite->setVisible(false);
		if (_robotSprite)
			_robotSprite->setVisible(false);
		stopActionByTag(3);
		stopActionByTag(2);
		stopActionByTag(1);
		stopActionByTag(0);
		if (dragEffect1)
			dragEffect1->pauseEmissions();
		if (dragEffect2)
			dragEffect2->pauseEmissions();
		if (dragEffect3)
			dragEffect3->pauseEmissions();

		_currentGamemode = mode;
		switch (mode)
		{
		case PlayerGamemodeCube:
			setIsOnGround(false);
			if (m_pMainSprite)
			{
				m_pMainSprite->setVisible(true);
				m_pMainSprite->setScale(1.f);
				m_pMainSprite->setPositionY(15.f);
			}
			deactivateStreak();
			break;
		case PlayerGamemodeShip:
			if (m_pShipSprite)
				m_pShipSprite->setVisible(true);
			if (m_pMainSprite)
			{
				m_pMainSprite->setVisible(true);
				m_pMainSprite->setScale(0.55f);
				m_pMainSprite->setPositionY(20.f);
			}
			setRotation(0.f);
			m_dYVel /= 2.f;
			setIsOnGround(false);
			activateStreak();
			runRotateAction();
			break;
		case PlayerGamemodeBall:
			if (_ballSprite)
				_ballSprite->setVisible(true);
			deactivateStreak();
			runBallRotation();
			break;
		case PlayerGamemodeUFO:
			if (m_pMainSprite)
			{
				m_pMainSprite->setVisible(true);
				m_pMainSprite->setScale(0.55f);
				m_pMainSprite->setPositionY(20.f);
			}
			if (_ufoSprite)
				_ufoSprite->setVisible(true);
			deactivateStreak();
			break;
		case PlayerGamemodeWave:
			if (_waveSprite)
				_waveSprite->setVisible(true);
			else if (m_pMainSprite)
			{
				m_pMainSprite->setVisible(true);
				m_pMainSprite->setScale(1.f);
				m_pMainSprite->setPositionY(15.f);
			}
			setIsOnGround(false);
			activateStreak();
			break;
		case PlayerGamemodeSwing:
			if (_swingSprite)
				_swingSprite->setVisible(true);
			else if (m_pMainSprite)
			{
				m_pMainSprite->setVisible(true);
				m_pMainSprite->setScale(1.f);
				m_pMainSprite->setPositionY(15.f);
			}
			setIsOnGround(false);
			activateStreak();
			break;
		case PlayerGamemodeRobot:
			if (_robotSprite)
			{
				_robotSprite->setVisible(true);
				_robotSprite->playAnimation(robotRunAnimation(), true, true);
			}
			else if (m_pMainSprite)
			{
				m_pMainSprite->setVisible(true);
				m_pMainSprite->setScale(1.f);
				m_pMainSprite->setPositionY(15.f);
			}
			setRotation(0.f);
			setIsOnGround(false);
			deactivateStreak();
			break;
		case PlayerGamemodeSpider:
			if (_spiderSprite)
			{
				_spiderSprite->setVisible(true);
				_spiderSprite->playAnimation(spiderRunAnimation(), true, true);
			}
			else if (m_pMainSprite)
			{
				m_pMainSprite->setVisible(true);
				m_pMainSprite->setScale(1.f);
				m_pMainSprite->setPositionY(15.f);
			}
			setRotation(0.f);
			setIsOnGround(false);
			deactivateStreak();
			break;
		default:
			break;
		}

		if (mode == PlayerGamemodeWave)
			activateStreak();
		else
			deactivateStreak();
	}
}

void PlayerObject::checkSnapJumpToObject(GameObject* obj)
{
	if (obj)
	{
		if (m_snappedObject && m_snappedObject->_uniqueID != obj->_uniqueID &&
			m_snappedObject->getGameObjectType() == GameObjectType::kGameObjectTypeSolid)
		{

			auto oldSnapPos = m_snappedObject->getPosition();
			auto newSnapPos = obj->getPosition();

			float unknownUse = 1.0;
			float upTwoGap = 90.0;
			float downOneGap = 150.0;
			float upOneGap = 90.0;
			float xShift = 1.0;

			if (m_playerSpeed == 0.9f)
			{
				/* //if (m_vehicleSize == 1.0) {
					upOneGap = 120.0;
				//} */
			}
			else if (m_playerSpeed == 0.7f)
			{
				upTwoGap = 60.0;
				downOneGap = 120.0;
			}
			else if (m_playerSpeed == 1.1f)
			{
				unknownUse = 0.0;
				xShift = 2.00;
				upTwoGap = 120.0;
				downOneGap = 195.0;
				/* //if (m_vehicleSize == 1.0) {
					upOneGap = 150.0;
				//} */
			}
			else if (m_playerSpeed == 1.3f)
			{
				unknownUse = 0.0;
				xShift = 2.00;
				upTwoGap = 135.0;
				downOneGap = 225.0;
				/* //if (m_vehicleSize == 1.0) {
					upOneGap = 180.0;
				//} */
			}
			else
			{
				upOneGap = 120.0;
			}

			upOneGap += oldSnapPos.x;
			downOneGap += oldSnapPos.x;

			float someMultiplier = (isGravityFlipped() ? 30.0 : -30.0);

			if (unknownUse >= upOneGap) oldSnapPos.y = fabs(newSnapPos.x - upOneGap) + someMultiplier;

			float value1 = fabs(newSnapPos.y - (oldSnapPos.y + someMultiplier));
			float value2 = fabs(newSnapPos.y - (oldSnapPos.y + someMultiplier * 2));
			float value3 = fabs(newSnapPos.x - downOneGap);
			float value4 = fabs(newSnapPos.x - (upTwoGap + oldSnapPos.x));
			float value5 = fabs(newSnapPos.y - oldSnapPos.y);

			if ((unknownUse >= upOneGap && unknownUse >= value5) || (unknownUse >= value3 && unknownUse >= value1) ||
				(unknownUse >= value4 && unknownUse >= value2))
			{
				float newPos = obj->getPositionX() + this->m_snapDifference;
				float oldPos = this->getPositionX();

				if (xShift < fabs(newPos - oldPos))
				{
					if (newPos > oldPos)
					{
						newPos += oldPos;
					}
					else
					{
						newPos = oldPos - xShift;
					}
				}
				this->setPositionX(newPos);
			}
		}

		m_snappedObject = obj;
		m_snapDifference = this->getPositionX() - obj->getPositionX();
	}
}

void PlayerObject::hitGround(bool reverseGravity)
{
	m_dYVel = 0.0f;

	if (!isOnGround() && !reverseGravity)
	{
		landEffect1->setPosition(getPosition() + Vec2 {0.f, flipMod() * -15.f});
		landEffect1->resetSystem();
		landEffect1->start();
	}

	if (_currentGamemode == PlayerGamemodeBall && !isOnGround()) runBallRotation();

	_queuedHold = false;
	_jumpedTimes = 0;
	setIsOnGround(true);

	if (getActionByTag(0)) stopRotation();

	m_obLastGroundPos = getPosition();

	if (_currentGamemode != PlayerGamemodeShip && _currentGamemode != PlayerGamemodeWave)
		deactivateStreak();
}

float PlayerObject::flipMod() { return this->m_bGravityFlipped ? -1.0f : 1.0f; }

bool PlayerObject::isGravityFlipped() { return this->m_bGravityFlipped; }

bool PlayerObject::isDead() { return this->m_bIsDead; }

bool PlayerObject::isOnGround() { return this->m_bOnGround; }

ax::Vec2 PlayerObject::getLastGroundPos() { return this->m_obLastGroundPos; }

void PlayerObject::logValues()
{
	GameToolbox::log("xVel: {} | yVel: {} | gravity: {} | jumpHeight: {} ", m_dXVel, m_dYVel, m_dGravity, m_dJumpHeight);
}

void PlayerObject::runRotateAction()
{
	if (_currentGamemode == PlayerGamemodeRobot || _currentGamemode == PlayerGamemodeSpider)
		return;
	stopRotation();
	auto action = RotateBy::create(0.41f * (_mini ? 0.8f : 1.f), 180.f * flipMod());
	action->setTag(0);
	runAction(action);
}

const char* PlayerObject::robotRunAnimation() const
{
	if (m_playerSpeed >= 1.3f)
		return "run3";
	if (m_playerSpeed >= 1.1f)
		return "run2";
	if (m_playerSpeed <= 0.7f)
		return "skip";
	return "run";
}

const char* PlayerObject::spiderRunAnimation() const
{
	if (m_playerSpeed >= 1.1f)
		return "run2";
	if (m_playerSpeed <= 0.7f)
		return "walk";
	return "run";
}

void PlayerObject::updateIconAnimation()
{
	AnimatedIconSprite* sprite = nullptr;
	if (_currentGamemode == PlayerGamemodeRobot)
		sprite = _robotSprite;
	else if (_currentGamemode == PlayerGamemodeSpider)
		sprite = _spiderSprite;
	if (!sprite || !sprite->isVisible())
		return;

	if (_isDashing)
	{
		if (sprite->hasAnimation("dash_loop"))
			sprite->playAnimation("dash_loop", true);
		else
			sprite->playAnimation(_currentGamemode == PlayerGamemodeRobot ? robotRunAnimation() : spiderRunAnimation(), true);
		return;
	}

	if (isOnGround())
	{
		sprite->playAnimation(_currentGamemode == PlayerGamemodeRobot ? robotRunAnimation() : spiderRunAnimation(), true);
		return;
	}

	if (playerIsFalling())
	{
		const std::string& current = sprite->currentAnimation();
		if (current != "fall_start" && current != "fall_loop")
			sprite->playAnimation("fall_start", false, true);
		return;
	}

	const std::string& current = sprite->currentAnimation();
	if (current != "jump_start" && current != "jump_loop" && current != "jump")
		sprite->playAnimation("jump_start", false, true);
}

void PlayerObject::runBallRotation()
{
	if (getActionByTag(3)) stopActionByTag(3);
	auto action = RotateBy::create(0.5f, 360.f * flipMod());
	auto loop = RepeatForever::create(action);
	loop->setTag(3);
	runAction(loop);
}

void PlayerObject::stopRotation()
{
	stopActionByTag(0);

	if (getActionByTag(1) == nullptr)
	{
		if (getRotation() != 0)
		{
			int degrees = (int)getRotation() % 360;
			auto action = RotateTo::create(0.075f, (90 * roundf(degrees / 90.0f)));
			action->setTag(1);
			runAction(action);
		}
	}
}

void PlayerObject::jump()
{
	this->m_dYVel = this->m_dJumpHeight;
}

void PlayerObject::toggleMini(bool active)
{
	_mini = active;
	_vehicleSize = active ? 0.6f : 1.f;
	auto ac = ScaleTo::create(0.5f, active ? 0.6f : 1.f);
	auto bounce = EaseBounceOut::create(ac);
	this->runAction(bounce);
}

void PlayerObject::pushButton()
{
	m_bIsHolding = true;
	_hasJustHeld = true;
	_queuedHold = true;

	if (this->inPlayLayer && _touchedRingObject && !_touchedRingObject->hasBeenActivatedByPlayer(this))
		ringJump(_touchedRingObject);

	if ((_currentGamemode == PlayerGamemode::PlayerGamemodeCube || _currentGamemode == PlayerGamemode::PlayerGamemodeRobot) &&
		m_bOnGround)
		_jumpedTimes++;
}

void PlayerObject::releaseButton()
{
	_queuedHold = false;
	_hasJustHeld = false;
	m_bIsHolding = false;
}

PlayerObject* PlayerObject::create(int playerFrame, Layer* gameLayer)
{
	auto pRet = new (std::nothrow) PlayerObject();

	if (pRet && pRet->init(playerFrame, gameLayer, false))
	{
		pRet->autorelease();
		return pRet;
	}
	AX_SAFE_DELETE(pRet);
	return pRet;
}

PlayerObject* PlayerObject::createForMenu(Layer* gameLayer)
{
	auto pRet = new (std::nothrow) PlayerObject();

	if (pRet && pRet->init(1, gameLayer, true))
	{
		pRet->autorelease();
		return pRet;
	}
	AX_SAFE_DELETE(pRet);
	return nullptr;
}