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
#include <string_view>
#include <unordered_map>
#include <vector>

#include "EventKeyboard.h"
#include "BaseGameLayer.h"
#include "PlayerObject.h"
#include "math/Vec2.h"

class GJGameLevel;
class GameObject;
class SimpleProgressBar;
class UILayer;
class GroundLayer;
class MenuItemSpriteExtra;
class PauseLayer;

namespace ax
{
	class Event;
	class Sprite;
	class Label;
	class Node;
}

struct PracticeCheckpoint
{
	ax::Vec2 pos1;
	ax::Vec2 pos2;
	ax::Vec2 camPos;
	float rot1 = 0.f;
	float rot2 = 0.f;
	double yVel1 = 0.0;
	double yVel2 = 0.0;
	double xVel1 = 5.77;
	double xVel2 = 5.77;
	float speed1 = 0.9f;
	float speed2 = 0.9f;
	bool gravity1 = false;
	bool gravity2 = false;
	bool mini1 = false;
	bool mini2 = false;
	bool dual = false;
	PlayerGamemode gamemode1 = PlayerGamemodeCube;
	PlayerGamemode gamemode2 = PlayerGamemodeCube;
	float songTime = 0.f;
	ax::Sprite* sprite = nullptr;
};

class PlayLayer : public BaseGameLayer
{
protected:
	bool init(GJGameLevel* level) override;
	void onEnter() override;
	void onExit() override;
	void onDrawImGui();
	virtual void onKeyPressed(ax::EventKeyboard::KeyCode keyCode, ax::Event* event);
	virtual void onKeyReleased(ax::EventKeyboard::KeyCode keyCode, ax::Event* event);
	void createLevelEnd();

	ax::Node* cameraFollow;
	ax::Node* _gameLayer = nullptr;

	ax::Sprite* m_pBG;
	GroundLayer *_bottomGround, *_ceiling;
	
	ax::Vec2 m_obCamPos;

	MenuItemSpriteExtra* backbtn;

	ax::DrawNode* dn;

	std::vector<GameObject*> _pObjects;

	float m_fCameraYCenter;
	float m_lastObjXPos = 570.0f;
	float _lastTriggerScanX = -30.f;
	bool m_bFirstAttempt = true;
	bool m_bMoveCameraX;
	bool m_bMoveCameraY;
	bool _editorVisibilityAllSections = false;
	bool m_bShakingCamera;
	float m_fEndOfLevel = FLT_MAX;
	float m_fShakeIntensity = 1;

	bool m_bIsJumpPressed;

	SimpleProgressBar* m_pBar;
	ax::Label* m_pPercentage;

	//----IMGUI DEBUG MEMBERS----
	bool m_freezePlayer;
	bool m_platformerMode;

	bool m_bEndAnimation;

	void setInstance();
public:
	int _enterEffectID = 0;

	int _groundID = 1;
	int _bgID = 1;

	UILayer* m_pHudLayer;

	int _secondsSinceStart;
	int _attempts;
	int _jumps;
	bool _everyplay_recorded;
	bool _testMode;

	std::vector<bool> _coinsCollected;
	std::vector<ax::Sprite*> _coinHUD;
	ax::Node* _newBestBanner = nullptr;

	bool _isDualMode;
	bool _isPaused = false;
	bool _isPracticeMode = false;
	PauseLayer* _pauseLayer = nullptr;
	int _musicAudioId = -1;
	std::vector<PracticeCheckpoint> _checkpoints;
	float _lastAutoCheckpointX = -9999.f;
	bool _freezeHitboxesOnDeath = false;
	bool _isMirror = false;
	float _mirrorVisual = 0.f; // 0 = normal, 1 = mirrored (animated)
	static constexpr float kMirrorAnimDuration = 0.5f;
	void applyMirrorVisual(float screenWidth);
	float _shakeTime = 0.f;
	float _shakeStrength = 0.f;
	std::unordered_map<int, int> _itemCounts;

	virtual void destroyPlayer(PlayerObject* player) override;
	void pickupCoin(GameObject* obj, PlayerObject* player);
	void setupCoinHUD();
	void refreshCoinHUD();
	void showNewBest(int percent);
	int currentPercent() const;
	void recordAttemptProgress(bool completed);

	void loadLevel(std::string_view levelStr);

	void spawnCircle();
	void spawnBounceEffect(ax::Vec2 pos, ax::Color4B color, bool isOrb);
	void showEndLayer();
	virtual void showCompleteText();

	void update(float delta) override;
	virtual void updateCamera(float dt);
	virtual void updateVisibility();
	void moveCameraToPos(ax::Vec2);
	void changeGameMode(GameObject* obj, PlayerObject* player, PlayerGamemode gameMode);
	void setDualMode(bool dual);
	void applyLevelStartGamemode(PlayerObject* player, PlayerGamemode mode);
	virtual void resetLevel();
	void exit();
	void pauseGame();
	void resumeGame();
	void togglePracticeMode();
	void markCheckpoint();
	void removeCheckpoint();
	void clearCheckpoints();
	void applyCheckpoint(const PracticeCheckpoint& checkpoint);
	void applyMusicVolume();
	void applyHudVisibility();
	bool isPaused() const { return _isPaused; }
	bool isPracticeMode() const { return _isPracticeMode; }

	void tweenBottomGround(float y);
	void tweenCeiling(float y);
	float flyingFloorY(const PlayerObject* player) const;
	float flyingCeilY(const PlayerObject* player) const;

	// dt?
	void checkCollisions(PlayerObject* player, float delta);
	void renderRect(ax::Rect rect, ax::Color4B col);

	void applyEnterEffect(GameObject* obj);
	float getRelativeMod(ax::Vec2 objPos, float v1, float v2, float v3);

	int sectionForPos(float x);

	void changePlayerSpeed(int speed);
	void changeGravity(bool gravityFlipped);
	void setMirror(bool mirror);
	void spiderTeleport(PlayerObject* player);
	void teleportPlayer(PlayerObject* player, GameObject* from);
	GameObject* findTeleportDestination(GameObject* src);
	void fireOnDeathTriggers();
	bool tryActivateCountTrigger(class EffectGameObject* trigger);

	void incrementTime();

	ax::Color3B getLightBG();

	static ax::Scene* scene(GJGameLevel* level);
	static PlayLayer* create(GJGameLevel* level);

	static PlayLayer* getInstance();
};