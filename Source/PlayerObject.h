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
#include "GameObject.h"
#include "Types.h"
#include "math/Vec2.h"

class GameObject;
class MotionTrail;
class AnimatedIconSprite;
class HardStreak;

namespace ax 
{ 
	class Layer; 
	class Sprite;
	class ParticleSystemQuad;
	class Texture2D;
}


enum PlayerGamemode
{
	PlayerGamemodeCube = 0,
	PlayerGamemodeShip = 1,
	PlayerGamemodeBall = 2,
	PlayerGamemodeUFO = 3,
	PlayerGamemodeWave = 4,
	PlayerGamemodeRobot = 5,
	PlayerGamemodeSpider = 6,
	PlayerGamemodeSwing = 7,
};

class PlayerObject : public GameObject
{
  private:
	void updateJump(float dt);
	bool init(int, ax::Layer*, bool menuRandomIcons = false);
	void runRotateAction();
	void runBallRotation();
	void updateIconAnimation();
	const char* robotRunAnimation() const;
	const char* spiderRunAnimation() const;

	void logValues();

	ax::Layer* gameLayer;
	bool inPlayLayer;

	ax::Sprite* m_pMainSprite;
	ax::Sprite* m_pSecondarySprite;
	ax::Sprite* m_pMainGlowSprite = nullptr;
	ax::Sprite* m_pShipSprite;
	ax::Sprite* m_pShipSecondarySprite;
	ax::Sprite* m_pShipGlowSprite = nullptr;
	ax::Sprite* _ballSprite;
	ax::Sprite* _ballSecondarySprite;
	ax::Sprite* _ballGlowSprite = nullptr;
	ax::Sprite* _ufoSprite;
	ax::Sprite* _ufoSecondarySprite;
	ax::Sprite* _ufoTertiarySprite;
	ax::Sprite* _ufoGlowSprite = nullptr;
	ax::Sprite* _waveSprite = nullptr;
	ax::Sprite* _waveSecondarySprite = nullptr;
	ax::Sprite* _waveGlowSprite = nullptr;
	ax::Sprite* _swingSprite = nullptr;
	ax::Sprite* _swingSecondarySprite = nullptr;
	ax::Sprite* _swingGlowSprite = nullptr;
	AnimatedIconSprite* _robotSprite = nullptr;
	AnimatedIconSprite* _spiderSprite = nullptr;
	bool m_bHasGlow = false;
	ax::Color3B m_glowColor{255, 255, 255};

	ax::ParticleSystemQuad* dragEffect1;
	ax::ParticleSystemQuad* dragEffect2;
	ax::ParticleSystemQuad* dragEffect3;
	ax::ParticleSystemQuad* shipDragEffect;
	ax::ParticleSystemQuad* landEffect1;
	ax::ParticleSystemQuad* landEffect2;

	
	double m_dYVel = 0;
	double m_dGravity = 0.958199;
	double m_dJumpHeight = 11.180032;

	bool m_bOnGround;

	bool m_bIsDead;
	bool m_bIsLocked;

	bool m_bGravityFlipped;

	bool m_isRising;

	bool _particles1Activated;
	bool _particles2Activated;
	bool _particles3Activated;

	float m_playerSpeed = 0.9f;
	float m_snapDifference;

	ax::Vec2 m_obLastGroundPos;
	ax::Vec2 m_prevPos;
	ax::Vec2 _shipRotationPos{0.f, 0.f};
	bool _shipRotationPosValid = false;
	bool _isAccelerating = false;

	GameObject* m_snappedObject;

	bool _isOnSlope = false;
	bool _wasOnSlope = false;
	GameObject* _currentSlope = nullptr;
	float _slopeRotation = 0.f;
	float _slopeVelocity = 0.f;
	float _slopeStartTime = 0.f;
	float _totalTime = 0.f;
	bool _slopeUphillContact = false;

  public:

  double m_dXVel = 5.770002;

	GameObject* _touchedPadObject;

	bool _mini = false;
	bool m_bIsHolding;
	bool _hasJustHeld;

	int _jumpedTimes;

	static ax::Texture2D* motionStreakTex;
	MotionTrail* motionStreak;
	HardStreak* _waveTrail = nullptr;

	PlayerGamemode _currentGamemode = PlayerGamemodeCube;

	GameObject* _touchedRingObject;
	
	bool _hasRingJumped;
	bool _queuedHold;
	bool _isDashing = false;
	bool _spiderTeleportQueued = false;

	void reset();

	bool m_bIsPlatformer;
	float direction, _vehicleSize = 1.f;

	static PlayerObject* create(int, ax::Layer*);
	static PlayerObject* createForMenu(ax::Layer* gameLayer);

	void setMainColor(ax::Color3B col);
	void setSecondaryColor(ax::Color3B col);
	void setGlow(bool glow);
	void setGlowColor(ax::Color3B col);

	ax::Color3B getMainColor();
	ax::Color3B getSecondaryColor();

	void jump();
	void collidedWithObject(float dt, GameObject* obj);
	void collidedWithSlope(float dt, GameObject* obj);
	void beginSlopePass();
	void endSlopePass(float dt);
	void checkSnapJumpToObject(GameObject* obj);

	void updateShipRotation(float dt);
	void storeShipRotationPos();
	bool isDead();
	bool isOnGround();
	bool isGravityFlipped();
	bool isRestricted()
	{
		return _currentGamemode == PlayerGamemodeShip || _currentGamemode == PlayerGamemodeSpider ||
			   _currentGamemode == PlayerGamemodeBall;
	}
	bool isFlying()
	{
		return _currentGamemode == PlayerGamemodeShip || _currentGamemode == PlayerGamemodeUFO ||
			   _currentGamemode == PlayerGamemodeWave || _currentGamemode == PlayerGamemodeSwing;
	}
	bool isGroundedMode()
	{
		return _currentGamemode == PlayerGamemodeCube || _currentGamemode == PlayerGamemodeRobot ||
			   _currentGamemode == PlayerGamemodeSpider;
	}
	void stopRotation();
	void toggleMini(bool active);
	float flipMod();

	bool playerIsFalling();
	bool playerIsFallingBugged();

	double getYVel()
	{
		return m_dYVel;
	}
	void setYVel(double yVel)
	{
		m_dYVel = yVel;
	}

	void setIsDead(bool);
	void setIsOnGround(bool);
	void setGamemode(PlayerGamemode mode);

	ax::Layer* getPlayLayer()
	{
		return gameLayer;
	}

	void playDeathEffect(bool stopMusic = true);

	void hitGround(bool reverseGravity);

	void flipGravity(bool gravity);

	bool noclip;

	ax::Vec2 getLastGroundPos();
	void setLastGroundPos(ax::Vec2 pos)
	{
		m_obLastGroundPos = pos;
	}
	void update(float dt);

	float getPlayerSpeed()
	{
		return m_playerSpeed;
	}
	void setPlayerSpeed(float v)
	{
		m_playerSpeed = v;
	}

	void propellPlayer(double force, ax::Color4B effectColor = ax::Color4B(255, 255, 0, 255));

	void setTouchedRing(GameObject* obj);
	void ringJump(GameObject* obj);

	void activateStreak();
	void deactivateStreak();
	void updateWaveTrail();

	void pushButton();
	void releaseButton();

	ax::Vec2 _portalP;
	ax::Vec2 _lastP;

	ax::Vec2 getPortalP()
	{
		return _portalP;
	}
	ax::Vec2 getLastP()
	{
		return _lastP;
	}
	void setPortalP(ax::Vec2 portalP)
	{
		_portalP = portalP;
	}
	void setLastP(ax::Vec2 lastP)
	{
		_lastP = lastP;
	}

	GameObject* _portalObject;

	GameObject* getPortalObject()
	{
		return _portalObject;
	}
	void setPortalObject(GameObject* portal)
	{
		_portalObject = portal;
	}

	void spawnPortalCircle(ax::Color4B color, float radius);
};