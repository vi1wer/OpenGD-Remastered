/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "BaseGameLayer.h"
#include "EventKeyboard.h"
#include "math/Vec2.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

class GameObject;
class GroundLayer;
class GJGameLevel;
class MenuItemSpriteExtra;

namespace ax
{
	class DrawNode;
	class Event;
	class Label;
	class LayerColor;
	class Menu;
	class Node;
	class Scene;
	class Sprite;
	class Touch;
}

enum class EditorToolMode
{
	Build = 0,
	Edit = 1,
	Delete = 2,
};

enum class EditorObjectTab
{
	Blocks = 0,
	Outlines,
	Slopes,
	Spikes,
	ThreeD,
	Gameplay,
	Animated,
	Pixel,
	Items,
	Symbols,
	Decor,
	Spin,
	Triggers,
	Custom,
	Count
};

enum class EditorUndoType
{
	Place,
	Delete,
	Move,
	Rotate,
};

struct EditorUndoAction
{
	EditorUndoType type = EditorUndoType::Place;
	std::string objectData;
	float oldX = 0.f;
	float oldY = 0.f;
	float oldRot = 0.f;
};

class LevelEditorLayer : public BaseGameLayer
{
public:
	static ax::Scene* scene(GJGameLevel* level);
	static LevelEditorLayer* create(GJGameLevel* level);

	void onEnter() override;
	void onExit() override;
	void update(float delta) override;

	void onKeyPressed(ax::EventKeyboard::KeyCode keyCode, ax::Event* event);
	void onKeyReleased(ax::EventKeyboard::KeyCode keyCode, ax::Event* event);

	void addObject(GameObject* obj) override;
	GameObject* findObject(float x, float y);
	void removeEditorObject(GameObject* obj, bool recordUndo = true);

	void saveLevelString();
	void goBack();
	void exitWithoutSaving();
	void openPlaytest();
	void startEditorPlaytest();
	void stopEditorPlaytest(bool died);
	void destroyPlayer(PlayerObject* player) override;
	void showEditorMenu();
	void hideEditorMenu();

	bool onTouchBegan(ax::Touch* touch, ax::Event* event);
	void onTouchEnded(ax::Touch* touch, ax::Event* event);
	void onTouchMoved(ax::Touch* touch, ax::Event* event);

	void showEditObjectPopup(GameObject* obj);
	void showEditGroupPopup(GameObject* obj);
	void showEditSpecialPopup(GameObject* obj);
	void showEditColorPopup(GameObject* obj);
	void showLevelSettings();
	void applyLevelSettingsVisuals();

	LevelSettings& levelSettings() { return _levelSettings; }
	std::map<int, SpriteColor>& colorChannels() { return _colorChannels; }
	ax::Node* hudLayer() const { return _hudLayer; }
	ax::Vec2 editorCamPos() const { return m_obCamPos; }

private:
	bool init(GJGameLevel* level) override;
	void setupEditorWorld();
	void setupToolbar();
	void setupChromeHud();
	void setupLayerHud();
	void setupPlaytestTrail();
	void setToolMode(EditorToolMode mode);
	void refreshModeButtons();
	void rebuildToolContent();
	void rebuildOptionsButtons();
	void setObjectTab(EditorObjectTab tab);
	void setPalettePage(int page);
	const std::vector<int>& objectsForTab(EditorObjectTab tab) const;
	void updateCamera(float dt);
	void rebuildObjectCache();
	void ensureSectionCapacity(int section);
	void placeObjectAt(const ax::Vec2& snappedWorldPos);
	void selectObjectAt(const ax::Vec2& snappedWorldPos);
	void deleteObjectAt(const ax::Vec2& snappedWorldPos);
	void deleteSelectedObject();
	void deselectObject();
	void clearSelection();
	void setSelectedObjects(const std::vector<GameObject*>& objs);
	void selectObjectsInMarquee(const ax::Vec2& a, const ax::Vec2& b);
	void refreshMarqueeVisual();
	void refreshSelectionHighlight();
	void moveSelected(float dx, float dy);
	void rotateSelected(float degrees);
	void setSelectedObject(GameObject* obj);
	ax::Vec2 touchToWorld(const ax::Vec2& screenPos, bool snap) const;
	ax::Vec2 snapTouchToGrid(const ax::Vec2& screenPos) const;
	GameObject* findObjectNear(const ax::Vec2& worldPos, float radius) const;
	void refreshPlaytestButton();
	void refreshMusicButton();
	void refreshLayerLabel();
	void setEditorHudVisible(bool visible);
	void updatePlaytest(float dt);
	void updatePlaytestCamera(float dt);
	void checkPlaytestCollisions(PlayerObject* player, float dt);
	void setPlaytestDualMode(bool dual);
	void applyPlaytestSpeed(int speed);
	void resetPlaytestObjectState();
	void rebuildPlaytestSections();
	void recordPlaytestTrail(const ax::Vec2& pos);
	void spawnPlaytestDeathCross(const ax::Vec2& pos);
	void focusCameraOnPlaytestDeath();
	void applyEditorPan(const ax::Vec2& screenPos);
	void commitEditorTap(const ax::Vec2& screenPos);
	void showComingSoon(const char* feature);
	void applyEditorZoom();
	void applyEditorLayerVisibility();
	void setEditorLayer(int layer);
	void changeEditorLayer(int delta);
	void editorUndo();
	void editorRedo();
	void pushUndo(EditorUndoAction action);
	void copySelection();
	void pasteClipboard();
	void duplicateSelection();
	std::string serializeObject(GameObject* obj) const;
	GameObject* deserializeObject(const std::string& data);
	GameObject* findObjectFromUndoData(const std::string& data);
	void startEditorMusic();
	void stopEditorMusic();
	void setScrubCameraX(float normalized);
	void refreshScrubSlider();
	ax::Sprite* makeFrameSprite(const char* frame, float scale = 1.f) const;
	ax::Node* makeBackedIconButton(const char* frame, float btnSize, float iconMax, bool selected,
		const char* backingTexture) const;
	ax::Node* makeRightGridButton(const char* fullFrame, const char* label, float labelScale, float cell) const;
	MenuItemSpriteExtra* makeHudIconBtn(const char* frame, std::function<void(ax::Node*)> cb, float targetSize = 38.f);

	ax::Vec2 m_camDelta;
	ax::Vec2 m_obCamPos;
	float _lastGroundCamX = 0.f;
	ax::Node* _cameraFollow = nullptr;
	ax::Node* _gameLayer = nullptr;
	ax::Node* _hudLayer = nullptr;
	ax::Sprite* m_pBG = nullptr;
	ax::Sprite* _middleGround = nullptr;
	GroundLayer* _bottomGround = nullptr;
	GroundLayer* _ceiling = nullptr;
	ax::DrawNode* _gridNode = nullptr;
	ax::Sprite* _toolbarBg = nullptr;
	ax::Menu* _toolbarMenu = nullptr;
	ax::Menu* _optionsMenu = nullptr;
	ax::Node* _toolContent = nullptr;
	ax::LayerColor* _editorMenu = nullptr;

	ax::Menu* _topHudMenu = nullptr;
	ax::Menu* _leftHudMenu = nullptr;
	ax::Menu* _rightHudMenu = nullptr;
	ax::Node* _layerHud = nullptr;
	ax::Node* _scrubSlider = nullptr;
	std::function<void(float)> _scrubSliderSet;

	MenuItemSpriteExtra* _buildModeBtn = nullptr;
	MenuItemSpriteExtra* _editModeBtn = nullptr;
	MenuItemSpriteExtra* _deleteModeBtn = nullptr;
	MenuItemSpriteExtra* _musicBtn = nullptr;
	MenuItemSpriteExtra* _playtestBtn = nullptr;
	MenuItemSpriteExtra* _zoomInBtn = nullptr;
	MenuItemSpriteExtra* _zoomOutBtn = nullptr;
	ax::Sprite* _buildModeSpr = nullptr;
	ax::Sprite* _editModeSpr = nullptr;
	ax::Sprite* _deleteModeSpr = nullptr;
	ax::Label* _layerLabel = nullptr;

	float _toolbarHeight = 120.f;
	float _tabStripHeight = 0.f;
	float _contentLeft = 170.f;
	float _contentRight = 500.f;
	float _rowYTop = 90.f;
	float _rowYMid = 55.f;
	float _rowYBot = 20.f;

	EditorToolMode _toolMode = EditorToolMode::Build;
	EditorObjectTab _objectTab = EditorObjectTab::Blocks;
	int _palettePage = 0;
	int _currentEditorLayer = 0;
	float _editorZoom = 1.f;
	bool _inSwapMode = false;
	bool _swipeEnabled = false;
	bool _freeRotateEnabled = false;
	bool _freeMoveEnabled = false;
	bool _snapEnabled = true;
	bool _shiftPressed = false;
	bool _ctrlPressed = false;
	bool _spacePressed = false;
	bool _cameraPanning = false;
	bool _leftPanning = false;
	bool _pendingTap = false;
	bool _draggingSelection = false;
	bool _menuOpen = false;
	bool _musicPlaying = false;
	int _musicAudioId = -1;
	float _lastRotateAngle = 0.f;
	ax::Vec2 _panTouchStart;
	ax::Vec2 _camAtPanStart;
	ax::Vec2 _dragTouchStart;
	ax::Vec2 _dragObjStart;

	std::map<std::string, GameObject*> _objectPositionCache;
	int _selectedObject = 1;
	std::vector<GameObject*> _selectedObjects;
	GameObject* _selectedObjectReal = nullptr;

	bool _marqueeSelecting = false;
	ax::Vec2 _marqueeStartWorld;
	ax::Vec2 _marqueeEndWorld;
	ax::DrawNode* _marqueeNode = nullptr;

	std::vector<EditorUndoAction> _undoStack;
	std::vector<EditorUndoAction> _redoStack;
	std::vector<std::string> _clipboard;

	ax::Menu* _playtestMenu = nullptr;
	ax::DrawNode* _playtestTrail = nullptr;
	ax::Node* _playtestDeathMarks = nullptr;

	bool _playtesting = false;
	bool _playtestDual = false;
	bool _playtestDead = false;
	bool _playtestJumpHeld = false;
	float _playtestCamYCenter = 0.f;
	float _editorZoomBeforePlaytest = 1.f;
	ax::Vec2 _editorCamBeforePlaytest;
	ax::Vec2 _playtestDeathPos;
	bool _hasPlaytestDeathPos = false;
	std::vector<ax::Vec2> _playtestTrailPts;
};
