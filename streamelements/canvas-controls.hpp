#pragma once

#include <obs.h>
#include "canvas-config.hpp"
#include "canvas-scan.hpp"
#include "canvas-math.hpp"
#include "canvas-draw.hpp"

static FileTexture g_overflowTexture("../../data/obs-studio/images/overflow.png");

static ConfigAccessibilityColor g_colorSelection("SelectRed",
						 QColor(255, 0, 0));
static ConfigAccessibilityColor g_colorCrop("SelectGreen", QColor(255, 0, 0));
static ConfigAccessibilityColor g_colorHover("SelectBlue", QColor(0, 0, 255));

static inline bool isCropped(const obs_sceneitem_crop *crop)
{
	return crop->left > 0 || crop->top > 0 || crop->right > 0 ||
	       crop->bottom > 0;
}

static vec3 getSnapOffset(const vec3 &tl, const vec3 &br, double worldWidth, double worldHeight)
{
	vec2 screenSize;
	vec2_set(&screenSize, worldWidth, worldHeight);

	vec3 clampOffset;

	vec3_zero(&clampOffset);

	const bool snap = config_get_bool(obs_frontend_get_global_config(), "BasicWindow",
					  "SnappingEnabled");
	if (snap == false)
		return clampOffset;

	const bool screenSnap = config_get_bool(obs_frontend_get_global_config(), "BasicWindow",
				"ScreenSnapping");
	const bool centerSnap = config_get_bool(obs_frontend_get_global_config(), "BasicWindow",
				"CenterSnapping");

	const float clampDist =
		config_get_double(obs_frontend_get_global_config(),
				  "BasicWindow", "SnapDistance");
	const float centerX = br.x - (br.x - tl.x) / 2.0f;
	const float centerY = br.y - (br.y - tl.y) / 2.0f;

	// Left screen edge.
	if (screenSnap && fabsf(tl.x) < clampDist)
		clampOffset.x = -tl.x;
	// Right screen edge.
	if (screenSnap && fabsf(clampOffset.x) < EPSILON &&
	    fabsf(screenSize.x - br.x) < clampDist)
		clampOffset.x = screenSize.x - br.x;
	// Horizontal center.
	if (centerSnap && fabsf(screenSize.x - (br.x - tl.x)) > clampDist &&
	    fabsf(screenSize.x / 2.0f - centerX) < clampDist)
		clampOffset.x = screenSize.x / 2.0f - centerX;

	// Top screen edge.
	if (screenSnap && fabsf(tl.y) < clampDist)
		clampOffset.y = -tl.y;
	// Bottom screen edge.
	if (screenSnap && fabsf(clampOffset.y) < EPSILON &&
	    fabsf(screenSize.y - br.y) < clampDist)
		clampOffset.y = screenSize.y - br.y;
	// Vertical center.
	if (centerSnap && fabsf(screenSize.y - (br.y - tl.y)) > clampDist &&
	    fabsf(screenSize.y / 2.0f - centerY) < clampDist)
		clampOffset.y = screenSize.y / 2.0f - centerY;

	return clampOffset;
}

class SceneItemControlBase
	: public StreamElementsVideoCompositionViewWidget::VisualElementBase {
protected:
	obs_scene_t *m_scene;
	obs_sceneitem_t *m_sceneItem;
	obs_sceneitem_t *m_parentSceneItem;
	StreamElementsVideoCompositionViewWidget *m_view;

public:
	SceneItemControlBase(StreamElementsVideoCompositionViewWidget *view,
			     obs_scene_t *scene, obs_sceneitem_t *sceneItem,
			     obs_sceneitem_t *parentSceneItem)
		: m_view(view),
		  m_scene(scene),
		  m_sceneItem(sceneItem),
		  m_parentSceneItem(parentSceneItem)
	{
		obs_scene_addref(m_scene);
		obs_sceneitem_addref(m_sceneItem);
	}
	SceneItemControlBase(SceneItemControlBase &) = delete;
	void operator=(SceneItemControlBase &) = delete;

	~SceneItemControlBase()
	{
		obs_sceneitem_release(m_sceneItem);
		obs_scene_release(m_scene);
	}

	obs_sceneitem_t *GetSceneItem() { return m_sceneItem; }
	obs_sceneitem_t *GetParentSceneItem() { return m_parentSceneItem; }
};

class SceneItemMoveControlBox : public SceneItemControlBase {
private:
	bool m_isMouseOver = false;
	bool m_isDragging = false;

	vec3 m_dragStartMouseParentPosition = {0, 0, 0};
	vec2 m_dragStartSceneItemTranslatePos = {0, 0};
	vec2 m_mouseWorldPosition = {0, 0};

private:
	void checkMouseOver(double worldX, double worldY)
	{
		vec2_set(&m_mouseWorldPosition, worldX, worldY);

		matrix4 transform, inv_transform;
		getSceneItemBoxTransformMatrices(m_sceneItem, m_parentSceneItem,
						 &transform, &inv_transform);

		auto mousePosition =
			getTransformedPosition(worldX, worldY, inv_transform);

		m_isMouseOver =
			(mousePosition.x >= 0.0f && mousePosition.x <= 1.0f &&
			 mousePosition.y >= 0.0f && mousePosition.y <= 1.0f);
	}

	bool hasSelectedChildrenAtPosition(double worldX, double worldY)
	{
		bool result = false;

		scanGroupSceneItems(
			m_sceneItem,
			[&](obs_sceneitem_t *sceneItem,
			    obs_sceneitem_t *parentSceneItem) {
				matrix4 transform, inv_transform;
				getSceneItemBoxTransformMatrices(
					sceneItem, parentSceneItem, &transform,
					&inv_transform);

				auto mousePosition = getTransformedPosition(
					worldX, worldY, inv_transform);

				bool isMouseOver = (mousePosition.x >= 0.0f &&
						    mousePosition.x <= 1.0f &&
						    mousePosition.y >= 0.0f &&
						    mousePosition.y <= 1.0f);

				if (isMouseOver) {
					result |= obs_sceneitem_selected(
						sceneItem);
				}

				return !result; // Continue iteration until no result found
			},
			true);

		return result;
	}

	void addSceneItemWorldPointsOfInterest(obs_sceneitem_t *sceneItem,
					       obs_sceneitem_t *parentSceneItem,
					       std::vector<vec2> &result)
	{
		matrix4 transform, inv_transform;
		getSceneItemBoxTransformMatrices(sceneItem, parentSceneItem,
						 &transform, &inv_transform);

		auto add = [&](double x, double y) {
			auto pos = getTransformedPosition(
				x, y, transform); // To world coords

			vec2 v;
			vec2_set(&v, pos.x, pos.y);
			result.push_back(v);
		};

		add(0.0f, 0.0f);
		add(1.0f, 0.0f);
		add(0.0f, 1.0f);
		add(1.0f, 1.0f);

		add(0.5f, 0.5f);
	}

	bool getClosestSnapPoint(const std::vector<vec2> &what,
				 const std::vector<vec2> &where,
				 double snapDistance,
				 vec2 &targetPoint, vec2 &sourcePoint, bool &hasX, bool &hasY)
	{
		if (!what.size())
			return false;

		if (!where.size())
			return false;

		double xDist = 0.0f;
		double yDist = 0.0f;

		bool isFirst = true;

		hasX = false;
		hasY = false;

		for (auto target : where) {
			for (auto source : what) {
				double currXDist = abs(target.x - source.x);
				double currYDist = abs(target.y - source.y);

				if (isFirst) {
					//vec2_copy(&targetPoint, &target);
					//vec2_copy(&sourcePoint, &source);

					xDist = currXDist;
					yDist = currYDist;

					isFirst = false;
				}

				if (currXDist <= xDist && currXDist < snapDistance) {
					targetPoint.x = target.x;
					sourcePoint.x = source.x;

					xDist = currXDist;

					hasX = true;
				}

				if (currYDist <= yDist && currYDist < snapDistance) {
					targetPoint.y = target.y;
					sourcePoint.y = source.y;

					yDist = currYDist;

					hasY = true;
				}
			}
		}

		return hasX || hasY;
	}

	void addWorldSnapPoints(std::vector<vec2> &result)
	{
		const bool isSnapEnabled =
			config_get_bool(obs_frontend_get_global_config(),
					"BasicWindow", "SnappingEnabled");

		if (!isSnapEnabled)
			return;

		const bool snapToSources =
			config_get_bool(obs_frontend_get_global_config(),
					"BasicWindow", "SourceSnapping");

		const bool snapToScreen =
			config_get_bool(obs_frontend_get_global_config(),
					"BasicWindow", "ScreenSnapping");
		const bool snapToScreenCenter =
			config_get_bool(obs_frontend_get_global_config(),
					"BasicWindow", "CenterSnapping");

		uint32_t worldWidth, worldHeight;
		m_view->GetVideoBaseDimensions(&worldWidth, &worldHeight);

		auto add = [&](double x, double y) {
			vec2 v;
			vec2_set(&v, x, y);
			result.push_back(v);
		};

		if (snapToScreen) {
			add(0, 0);
			add(0, worldHeight);
			add(worldWidth, 0);
			add(worldWidth, worldHeight);
		}

		if (snapToScreenCenter) {
			add(worldWidth / 2.0f, worldHeight / 2.0f);
		}

		// Snap to sources is the largest workload
		if (!snapToSources)
			return;

		bool isFirst = true;

		vec2 tl;
		vec2 br;

		std::map<obs_sceneitem_t *, bool> selectedSceneItemsParentsMap;

		// Get a list of parent groups of all the selected scene items
		scanSceneItems(
			m_scene,
			[&](obs_sceneitem_t *sceneItem,
			    obs_sceneitem_t *parentSceneItem) -> bool {
				if (!obs_sceneitem_selected(sceneItem))
					return true;

				if (!parentSceneItem)
					return true;

				selectedSceneItemsParentsMap[parentSceneItem] =
					true;

				return true;
			},
			true);

		scanSceneItems(m_scene, [&](obs_sceneitem_t *sceneItem,
					    obs_sceneitem_t *parentSceneItem) -> bool {
				if (obs_sceneitem_selected(sceneItem))
					return true;

				if (obs_sceneitem_locked(sceneItem))
					return true;

				// Check if we're a parent of a selected scene item
				if (selectedSceneItemsParentsMap.count(
					    sceneItem))
					return true;

				if (parentSceneItem) {
					if (obs_sceneitem_selected(
						    parentSceneItem))
						return true;

					if (obs_sceneitem_locked(
						    parentSceneItem))
						return true;
				}

				std::vector<vec2> points;
				addSceneItemWorldPointsOfInterest(
					sceneItem, parentSceneItem, points);

				for (auto p : points) {
					if (isFirst) {
						vec2_copy(&tl, &p);
						vec2_copy(&br, &p);

						isFirst = false;

						continue;
					}

					if (p.x < tl.x)
						tl.x = p.x;
					if (p.y < tl.y)
						tl.y = p.y;

					if (p.x > br.x)
						br.x = p.x;
					if (p.y > br.y)
						br.y = p.y;
				}

				return true;
		}, true);

		result.push_back(tl);
		result.push_back(br);

		double width = (br.x - tl.x);
		double height = (br.y - tl.y);
		vec2 center;
		vec2_set(&center, tl.x + width / 2.0f, tl.y + height / 2.0f);

		result.push_back(center);
	}

	void addSelectedSceneItemsWorldCoordsSnapPoints(std::vector<vec2>& result)
	{
		scanSceneItems(
			m_scene,
			[&](obs_sceneitem_t *sceneItem,
			    obs_sceneitem_t *parentSceneItem) -> bool {
				if (!obs_sceneitem_selected(sceneItem))
					return true;

				if (obs_sceneitem_locked(sceneItem))
					return true;

				addSceneItemWorldPointsOfInterest(
					sceneItem, parentSceneItem,
					result);

				return true;
			},
			true);
	}

	void getParentCoordsSnapOffset(vec2 &result, vec2 &snapWorldCoords) {
		vec2_zero(&result);
		vec2_set(&snapWorldCoords, -1.0f, -1.0f);

		std::vector<vec2> selectedSceneItemsPoints, worldSnapPoints;

		addSelectedSceneItemsWorldCoordsSnapPoints(
			selectedSceneItemsPoints);
		addWorldSnapPoints(worldSnapPoints);

		vec2 targetPoint, sourcePoint;

		const float snapDistance =
			config_get_double(obs_frontend_get_global_config(),
					  "BasicWindow", "SnapDistance");

		bool hasXSnapPoint, hasYSnapPoint;
		if (!getClosestSnapPoint(selectedSceneItemsPoints, worldSnapPoints, snapDistance,
					 targetPoint, sourcePoint,
					 hasXSnapPoint, hasYSnapPoint))
			return;

		matrix4 parentTransform, parentInvTransform;

		getSceneItemParentDrawTransformMatrices(m_sceneItem,
							m_parentSceneItem,
							&parentTransform,
							&parentInvTransform);

		auto parentSourcePoint = getTransformedPosition(
			sourcePoint.x, sourcePoint.y, parentInvTransform);
		auto parentTargetPoint = getTransformedPosition(
			targetPoint.x, targetPoint.y, parentInvTransform);

		if (hasXSnapPoint) {
			//result.x = parentTargetPoint.x - parentSourcePoint.x;
			result.x = targetPoint.x - sourcePoint.x;

			snapWorldCoords.x = targetPoint.x;
		}

		if (hasYSnapPoint) {
			//result.y = parentTargetPoint.y - parentSourcePoint.y;
			result.y = targetPoint.y - sourcePoint.y;

			snapWorldCoords.y = targetPoint.y;
		}

		/*
		for (auto point : worldSnapPoints) {
			m_rulersX.push_back(point.x);
			m_rulersY.push_back(point.y);
		}
		*/
	}

public:
	SceneItemMoveControlBox(StreamElementsVideoCompositionViewWidget *view,
			    obs_scene_t *scene, obs_sceneitem_t *sceneItem,
			    obs_sceneitem_t *parentSceneItem)
		: SceneItemControlBase(view, scene, sceneItem, parentSceneItem)
	{
	}

	~SceneItemMoveControlBox() {}

	std::vector<double> m_rulersX;
	std::vector<double> m_rulersY;

	virtual void Tick() override {
		if (!m_isDragging)
			return;

		for (auto coord : m_rulersX)
			m_view->AddVerticalRulerX(coord);

		for (auto coord : m_rulersY)
			m_view->AddHorizontalRulerY(coord);

	}

	virtual bool HandleMouseMove(QMouseEvent *event, double worldX,
				     double worldY) override
	{
		checkMouseOver(worldX, worldY);

		m_rulersX.clear();
		m_rulersY.clear();

		if (!obs_sceneitem_selected(m_sceneItem))
			m_isDragging = false;

		if (!m_isDragging)
			return false;

		// Actual dragging here
		matrix4 parentTransform, parentInvTransform;

		getSceneItemParentDrawTransformMatrices(m_sceneItem,
							m_parentSceneItem,
							&parentTransform,
							&parentInvTransform);

		auto currMouseParentPosition = getTransformedPosition(
			m_mouseWorldPosition.x, m_mouseWorldPosition.y,
			parentInvTransform);

		vec2 moveOffset;
		vec2_set(&moveOffset,
			 currMouseParentPosition.x -
				 m_dragStartMouseParentPosition.x,
			 currMouseParentPosition.y -
				 m_dragStartMouseParentPosition.y);

		vec2 newPos;
		vec2_set(&newPos,
			 m_dragStartSceneItemTranslatePos.x + moveOffset.x,
			 m_dragStartSceneItemTranslatePos.y + moveOffset.y);

		obs_sceneitem_set_pos(m_sceneItem, &newPos);

		if (!(QGuiApplication::keyboardModifiers() &
		      Qt::ControlModifier)) {
			// TODO: Somewhere in the future, we want to make sure that child items coordinates
			//       are treated properly. The current state is jumpy, so we'll disable snapping
			//       for group child items until we can properly figure out why is it not behaving
			//       100% properly.
			if (!m_parentSceneItem) {
				// SNAP
				vec2 snapParentMoveOffset, snapWorldCoords;
				getParentCoordsSnapOffset(snapParentMoveOffset,
							  snapWorldCoords);

				newPos.x += snapParentMoveOffset.x;
				newPos.y += snapParentMoveOffset.y;

				obs_sceneitem_set_pos(m_sceneItem, &newPos);

				if (snapWorldCoords.x >= 0) {
					//m_rulersX.push_back(snapWorldCoords.x);
				}

				if (snapWorldCoords.y >= 0) {
					//m_rulersY.push_back(snapWorldCoords.x);
				}
			}
		}

		return false;
	}

	virtual bool HandleMouseUp(QMouseEvent *event, double worldX,
				   double worldY) override
	{
		m_isDragging = false;

		return false;
	}

	virtual bool HandleMouseDown(QMouseEvent *event, double worldX,
				     double worldY) override
	{
		if (obs_sceneitem_locked(m_sceneItem))
			return false;

		if (m_isMouseOver &&
		    !hasSelectedChildrenAtPosition(worldX, worldY)) {
			if (QGuiApplication::keyboardModifiers() &
			    Qt::ControlModifier) {
				// Control modifier: toggle selection of multiple items
				if (obs_sceneitem_selected(m_sceneItem)) {
					// Toggle selection off if _this_ item is already selected
					obs_sceneitem_select(m_sceneItem,
							     false);
				} else if (!m_parentSceneItem) {
					// Otherwise, toggle selection on only if we don't have a parent scene item (always yield mouse selection to the parent)
					obs_sceneitem_select(m_sceneItem, true);
				}
			} else {
				if (!obs_sceneitem_selected(m_sceneItem)) {
					if (!m_parentSceneItem) {
						// No modifiers: select me, but only if we don't have a parent scene item (always yield mouse selection to the parent)
						scanSceneItems(
							m_scene,
							[&](obs_sceneitem_t
								    *item,
							    obs_sceneitem_t
								    * /*parent*/) -> bool {
								if (obs_sceneitem_selected(
									    item) !=
								    (item ==
								     m_sceneItem)) {
									obs_sceneitem_select(
										item,
										item == m_sceneItem);
								}

								return true;
							},
							true);
					}
				}
			}
		}

		if (obs_sceneitem_selected(m_sceneItem)) {
			// Unselect all children if we're a group
			scanGroupSceneItems(
				m_sceneItem,
				[&](obs_sceneitem_t *sceneItem,
				    obs_sceneitem_t *
				    /*parentSceneItem*/) -> bool {
					if (obs_sceneitem_selected(sceneItem)) {
						obs_sceneitem_select(sceneItem,
								     false);
					}

					return true;
				},
				true);

			// Begin drag and select the scene item if it hasn't been selected yet
			matrix4 parentTransform, parentInvTransform;

			getSceneItemParentDrawTransformMatrices(
				m_sceneItem, m_parentSceneItem,
				&parentTransform, &parentInvTransform);

			m_dragStartMouseParentPosition = getTransformedPosition(
				worldX, worldY, parentInvTransform);

			obs_sceneitem_get_pos(
				m_sceneItem, &m_dragStartSceneItemTranslatePos);

			m_isDragging = true;

			return m_isMouseOver;
		}

		return m_isMouseOver;
	}

	virtual void Draw() override
	{
		QColor color;

		bool isSelected = obs_sceneitem_selected(m_sceneItem);

		obs_sceneitem_crop crop = {0, 0, 0, 0};

		if (isSelected) {
			color = g_colorSelection.get();
		} else if (m_isMouseOver) {
			color = g_colorHover.get();
		} else {
			obs_sceneitem_get_crop(m_sceneItem, &crop);

			if (!isCropped(&crop))
				return;

			obs_transform_info info;
			obs_sceneitem_get_info(m_sceneItem, &info);

			if (info.bounds_type != OBS_BOUNDS_NONE)
				return;

			return;
		}

		const double thickness = 4.0f * m_view->devicePixelRatioF() *
					 m_view->m_worldScale.x;

		matrix4 transform, inv_transform;
		getSceneItemBoxTransformMatrices(m_sceneItem, m_parentSceneItem,
						 &transform, &inv_transform);

		auto boxScale = getSceneItemFinalBoxScale(m_sceneItem,
							  m_parentSceneItem);

		gs_matrix_push();
		gs_matrix_mul(&transform);

		if (isSelected || m_isMouseOver) {
			drawLine(0.0f, 0.0f, 1.0f, 0.0f, thickness, boxScale,
				 color);
			drawLine(0.0f, 1.0f, 1.0f, 1.0f, thickness, boxScale,
				 color);
			drawLine(0.0f, 0.0f, 0.0f, 1.0f, thickness, boxScale,
				 color);
			drawLine(1.0f, 0.0f, 1.0f, 1.0f, thickness, boxScale,
				 color);
		} else {
			QColor cropMarkerColor = g_colorCrop.get();

			if (crop.top > 0)
				drawStripedLine(0.0f, 0.0f, 1.0f, 0.0f,
						thickness, boxScale,
						cropMarkerColor);

			if (crop.bottom > 0)
				drawStripedLine(0.0f, 1.0f, 1.0f, 1.0f,
						thickness, boxScale,
						cropMarkerColor);

			if (crop.left > 0)
				drawStripedLine(0.0f, 0.0f, 0.0f, 1.0f,
						thickness, boxScale,
						cropMarkerColor);

			if (crop.right > 0)
				drawStripedLine(1.0f, 0.0f, 1.0f, 1.0f,
						thickness, boxScale,
						cropMarkerColor);
		}

		gs_matrix_pop();
	}
};

class SceneItemOverflowBox : public SceneItemControlBase {
public:
	SceneItemOverflowBox(StreamElementsVideoCompositionViewWidget *view,
			     obs_scene_t *scene, obs_sceneitem_t *sceneItem,
			     obs_sceneitem_t *parentSceneItem)
		: SceneItemControlBase(view, scene, sceneItem, parentSceneItem)
	{
	}

	~SceneItemOverflowBox() {}

	virtual void Draw() override
	{
		if (obs_sceneitem_get_group(m_scene, m_sceneItem))
			return;

		matrix4 transform, inv_tranform;
		getSceneItemBoxTransformMatrices(m_sceneItem, m_parentSceneItem,
						 &transform, &inv_tranform);

		auto itemScale =
			getSceneItemFinalScale(m_sceneItem, m_parentSceneItem);

		gs_matrix_push();
		gs_matrix_mul(&transform);

		const bool previous = gs_framebuffer_srgb_enabled();
		gs_enable_framebuffer_srgb(true);

		gs_blend_state_push();

		gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_REPEAT);
		gs_eparam_t *image =
			gs_effect_get_param_by_name(solid, "image");
		gs_eparam_t *scale =
			gs_effect_get_param_by_name(solid, "scale");

		gs_eparam_t *const param =
			gs_effect_get_param_by_name(solid, "image");
		gs_effect_set_texture_srgb(param, g_overflowTexture.get());

		vec2 s;
		// Black magic copied from OBS code. This keeps the output scale of the texture
		// rectangle constant disregarding the scaling of the scene item so it always
		// has stripes of the same visual size.
		vec2_set(&s, transform.x.x / 96, transform.y.y / 96);
		gs_effect_set_vec2(scale, &s);
		gs_effect_set_texture(image, g_overflowTexture.get());

		while (gs_effect_loop(solid, "Draw")) {
			gs_draw_sprite(g_overflowTexture.get(), 0, 1,
				       1); // Output width & height
		}

		gs_blend_state_pop();

		gs_enable_framebuffer_srgb(previous);

		gs_matrix_pop();
	}
};

class SceneItemControlPoint : public SceneItemControlBase {
public:
	double m_x;
	double m_y;
	double m_width;
	double m_height;

	std::shared_ptr<SceneItemControlPoint> m_lineTo;

protected:
	bool m_isDragging = false;
	bool m_isMouseOver = false;
	vec3 m_mousePosition = {0, 0};
	vec3 m_worldMousePosition = {0, 0};

	void checkMouseOver(double worldX, double worldY)
	{
		vec3_set(&m_worldMousePosition, worldX, worldY, 0.0f);

		matrix4 transform, inv_transform;
		getSceneItemBoxTransformMatrices(m_sceneItem, m_parentSceneItem,
						 &transform, &inv_transform);

		m_mousePosition =
			getTransformedPosition(worldX, worldY, inv_transform);

		auto boxScale = getSceneItemFinalBoxScale(m_sceneItem,
							  m_parentSceneItem);

		boxScale.x = fabsf(boxScale.x);
		boxScale.y = fabsf(boxScale.y);

		auto x1 = m_x - (m_width / boxScale.x * m_view->m_worldScale.x /
				 2.0f);
		auto x2 = m_x + (m_width / boxScale.x * m_view->m_worldScale.x /
				 2.0f);
		auto y1 = m_y - (m_height / boxScale.y *
				 m_view->m_worldScale.x / 2.0f);
		auto y2 = m_y + (m_height / boxScale.y *
				 m_view->m_worldScale.x / 2.0f);

		m_isMouseOver =
			(m_mousePosition.x >= x1 && m_mousePosition.x <= x2 &&
			 m_mousePosition.y >= y1 && m_mousePosition.y <= y2);
	}

public:
	SceneItemControlPoint(
		StreamElementsVideoCompositionViewWidget *view,
		obs_scene_t *scene, obs_sceneitem_t *sceneItem,
		obs_sceneitem_t *parentSceneItem, double x,
		double y, double width, double height,
		std::shared_ptr<SceneItemControlPoint> lineTo = nullptr)
		: SceneItemControlBase(view, scene, sceneItem, parentSceneItem),
		  m_x(x),
		  m_y(y),
		  m_width(width),
		  m_height(height),
		  m_lineTo(lineTo)
	{
	}

	~SceneItemControlPoint() {}

	virtual void Draw() override
	{
		if (!obs_sceneitem_selected(m_sceneItem) ||
		    obs_sceneitem_locked(m_sceneItem))
			return;

		QColor color(m_isMouseOver ? g_colorHover.get()
					   : g_colorSelection.get());

		matrix4 transform, inv_tranform;
		getSceneItemBoxTransformMatrices(m_sceneItem, m_parentSceneItem, &transform,
					      &inv_tranform);

		auto boxScale = getSceneItemFinalBoxScale(m_sceneItem, m_parentSceneItem);

		boxScale.x = fabsf(boxScale.x);
		boxScale.y = fabsf(boxScale.y);

		auto x1 = m_x - (m_width / boxScale.x * m_view->m_worldScale.x /
				 2.0f);
		auto x2 = m_x + (m_width / boxScale.x * m_view->m_worldScale.x /
				 2.0f);
		auto y1 = m_y - (m_height / boxScale.y *
				 m_view->m_worldScale.x / 2.0f);
		auto y2 = m_y + (m_height / boxScale.y *
				 m_view->m_worldScale.x / 2.0f);

		gs_matrix_push();
		gs_matrix_mul(&transform);

		fillRect(x1, y1, x2, y2, color);

		if (m_lineTo.get()) {
			double thickness = 2.0f * m_view->devicePixelRatioF() *
					   m_view->m_worldScale.x;

			drawLine(m_x, m_y, m_lineTo.get()->m_x,
				 m_lineTo.get()->m_y, thickness, boxScale, color);
		}

		gs_matrix_pop();
	}
};

class SceneItemRotationControlPoint : public SceneItemControlPoint {
public:
	SceneItemRotationControlPoint(
		StreamElementsVideoCompositionViewWidget *view,
		obs_scene_t *scene, obs_sceneitem_t *sceneItem,
		obs_sceneitem_t *parentSceneItem, double x, double y,
		double width, double height,
		std::shared_ptr<SceneItemControlPoint> lineTo)
		: SceneItemControlPoint(view, scene, sceneItem,
					parentSceneItem, x, y, width, height, lineTo)
	{
	}

	~SceneItemRotationControlPoint() {}

private:
	double getMouseAngleDegreesDelta()
	{
		matrix4 transform, inv_transform;
		getSceneItemBoxTransformMatrices(m_sceneItem, m_parentSceneItem,
						 &transform, &inv_transform);

		vec2 center;
		vec2_set(&center, 0.5f, 0.5f);

		auto localMouse = getTransformedPosition(m_worldMousePosition.x,
							 m_worldMousePosition.y,
							 inv_transform);

		vec2 mouse;
		vec2_set(&mouse, localMouse.x, localMouse.y);

		return std::round(getCircleDegrees(center, mouse));
	}

	void rotatePos(vec2 *pos, float rot)
	{
		float cosR = cos(rot);
		float sinR = sin(rot);

		vec2 newPos;

		newPos.x = cosR * pos->x - sinR * pos->y;
		newPos.y = sinR * pos->x + cosR * pos->y;

		vec2_copy(pos, &newPos);
	}

public:
	virtual void Tick() override
	{
		if (!m_isDragging)
			return;

		double newAngle =
			normalizeDegrees(obs_sceneitem_get_rot(m_sceneItem) +
					 getMouseAngleDegreesDelta());

		Qt::KeyboardModifiers modifiers =
		QGuiApplication::keyboardModifiers();
		bool shiftDown = (modifiers & Qt::ShiftModifier);
		bool ctrlDown = (modifiers & Qt::ControlModifier);

		auto snapRotationAngleDegrees = [&](double angle, double thres) {
			if (fabsf(normalizeDegrees(angle) -
				  normalizeDegrees(newAngle)) < thres)
				newAngle = normalizeDegrees(angle);
		};

		if (shiftDown) {
			for (int i = 0; i < 360 / 15; i++) {
				snapRotationAngleDegrees(i * 15, 7.5f);
			}
		} else if (!ctrlDown) {
			snapRotationAngleDegrees(obs_sceneitem_get_rot(m_sceneItem), 5);

			snapRotationAngleDegrees(0, 5);
			snapRotationAngleDegrees(45, 5);
			snapRotationAngleDegrees(90, 5);
			snapRotationAngleDegrees(135, 5);
			snapRotationAngleDegrees(180, 5);
			snapRotationAngleDegrees(225, 5);
			snapRotationAngleDegrees(270, 5);
			snapRotationAngleDegrees(315, 5);
		}

		if (newAngle == obs_sceneitem_get_rot(m_sceneItem))
			return;

		//
		// This piece of code was copied directly from OBS code.
		//
		// I don't know exactly how it works, but apparently it does in both
		// cases where there is a parent group, and where there is none.
		//
		// TODO: Figure this out later
		matrix4 transform;
		obs_sceneitem_get_box_transform(m_sceneItem, &transform);
		vec2 rotatePoint;
		vec2_set(&rotatePoint,
			 transform.t.x + transform.x.x / 2 + transform.y.x / 2,
			 transform.t.y + transform.x.y / 2 + transform.y.y / 2);
		vec2 offsetPoint;
		obs_sceneitem_get_pos(m_sceneItem, &offsetPoint);

		offsetPoint.x -= rotatePoint.x;
		offsetPoint.y -= rotatePoint.y;

		rotatePos(&offsetPoint, -degreesToRadians(obs_sceneitem_get_rot(
						m_sceneItem)));

		vec2 newPosition;
		vec2_copy(&newPosition, &offsetPoint);
		rotatePos(&newPosition, degreesToRadians(newAngle));
		newPosition.x += rotatePoint.x;
		newPosition.y += rotatePoint.y;

		obs_sceneitem_set_rot(m_sceneItem, newAngle);
		obs_sceneitem_set_pos(m_sceneItem, &newPosition);
	}

	virtual bool HandleMouseUp(QMouseEvent *event, double worldX,
				   double worldY) override
	{
		m_isDragging = false;

		return false;
	}

	virtual bool HandleMouseMove(QMouseEvent *event, double worldX,
				     double worldY) override
	{
		checkMouseOver(worldX, worldY);

		if (!m_isDragging)
			return false;

		return true;
	}

	virtual bool HandleMouseDown(QMouseEvent *event, double worldX,
				     double worldY) override
	{
		if (obs_sceneitem_locked(m_sceneItem))
			return false;

		if (!obs_sceneitem_selected(m_sceneItem))
			return false;

		if (!m_isMouseOver)
			return false;

		m_isDragging = true;

		scanSceneItems(
			m_scene,
			[&](obs_sceneitem_t *sceneItem,
			    obs_sceneitem_t * /* parentSceneItem */) -> bool {
				if (sceneItem == m_sceneItem)
					return true;

				// Unselect everything else
				obs_sceneitem_select(sceneItem, false);

				return true;
			},
			true);

		return true;
	}
};

class SceneItemStretchControlPoint : public SceneItemControlPoint {
public:
	SceneItemStretchControlPoint(
		StreamElementsVideoCompositionViewWidget *view,
		obs_scene_t *scene, obs_sceneitem_t *sceneItem,
		obs_sceneitem_t *parentSceneItem, double x, double y,
		double width, double height)
		: SceneItemControlPoint(view, scene, sceneItem, parentSceneItem,
					x, y, width, height, nullptr)
	{
	}

	~SceneItemStretchControlPoint() {}

	virtual bool HandleMouseUp(QMouseEvent* event, double worldX,
		double worldY) override
	{
		m_isDragging = false;

		return false;
	}

	virtual bool HandleMouseDown(QMouseEvent* event, double worldX,
		double worldY) override
	{
		checkMouseOver(worldX, worldY);

		if (!m_isMouseOver)
			return false;

		if (obs_sceneitem_locked(m_sceneItem))
			return false;

		if (!obs_sceneitem_selected(m_sceneItem))
			return false;

		m_isDragging = true;

		scanSceneItems(
			m_scene,
			[&](obs_sceneitem_t *sceneItem,
			    obs_sceneitem_t * /* parentSceneItem */) -> bool {
				if (sceneItem == m_sceneItem)
					return true;

				// Unselect everything else
				obs_sceneitem_select(sceneItem, false);

				return true;
			}, true);

		return true;
	}

	virtual bool HandleMouseMove(QMouseEvent* event, double worldX,
		double worldY) override
	{
		checkMouseOver(worldX, worldY);

		if (!m_isDragging)
			return false;

		process();

		return false;
	}

private:
	vec2 getSceneItemSize()
	{
		obs_bounds_type boundsType =
			obs_sceneitem_get_bounds_type(m_sceneItem);
		vec2 size;

		if (boundsType != OBS_BOUNDS_NONE) {
			obs_sceneitem_get_bounds(m_sceneItem, &size);
		} else {
			obs_source_t *source =
				obs_sceneitem_get_source(m_sceneItem);
			obs_sceneitem_crop crop;
			vec2 scale;

			obs_sceneitem_get_scale(m_sceneItem, &scale);
			obs_sceneitem_get_crop(m_sceneItem, &crop);
			size.x = float(obs_source_get_width(source) -
				       crop.left - crop.right) *
				 scale.x;
			size.y = float(obs_source_get_height(source) -
				       crop.top - crop.bottom) *
				 scale.y;
		}

		return size;
	}

	vec3 calculateStretchPos(const vec3 &tl, const vec3 &br)
	{
		uint32_t alignment = obs_sceneitem_get_alignment(m_sceneItem);
		vec3 pos;

		vec3_zero(&pos);

		if (alignment & OBS_ALIGN_LEFT)
			pos.x = tl.x;
		else if (alignment & OBS_ALIGN_RIGHT)
			pos.x = br.x;
		else
			pos.x = (br.x - tl.x) * 0.5f + tl.x;

		if (alignment & OBS_ALIGN_TOP)
			pos.y = tl.y;
		else if (alignment & OBS_ALIGN_BOTTOM)
			pos.y = br.y;
		else
			pos.y = (br.y - tl.y) * 0.5f + tl.y;

		return pos;
	}

	void clampAspect(vec3 &tl, vec3 &br, vec2 &size, const vec2 &baseSize)
	{
		float baseAspect = baseSize.x / baseSize.y;
		float aspect = size.x / size.y;

		bool isCorner = (m_x == 0.0f || m_x == 1.0f) &&
				(m_y == 0.0f || m_y == 1.0f);

		bool isTopOrBottomCenter = (m_x == 0.5f) &&
					   (m_y == 0.0f || m_y == 1.0f);

		bool isLeftOrRightCenter = (m_y == 0.5f) &&
					   (m_x == 0.0f || m_x == 1.0f);

		if (isCorner) {
			if (aspect < baseAspect) {
				if ((size.y >= 0.0f && size.x >= 0.0f) ||
				    (size.y <= 0.0f && size.x <= 0.0f))
					size.x = size.y * baseAspect;
				else
					size.x = size.y * baseAspect * -1.0f;
			} else {
				if ((size.y >= 0.0f && size.x >= 0.0f) ||
				    (size.y <= 0.0f && size.x <= 0.0f))
					size.y = size.x / baseAspect;
				else
					size.y = size.x / baseAspect * -1.0f;
			}

		} else if (isTopOrBottomCenter) {
			if ((size.y >= 0.0f && size.x >= 0.0f) ||
			    (size.y <= 0.0f && size.x <= 0.0f))
				size.x = size.y * baseAspect;
			else
				size.x = size.y * baseAspect * -1.0f;

		} else if (isLeftOrRightCenter) {
			if ((size.y >= 0.0f && size.x >= 0.0f) ||
			    (size.y <= 0.0f && size.x <= 0.0f))
				size.y = size.x / baseAspect;
			else
				size.y = size.x / baseAspect * -1.0f;
		}

		size.x = std::round(size.x);
		size.y = std::round(size.y);

		if (m_x == 0.0f)
			tl.x = br.x - size.x;
		else if (m_x == 1.0f)
			br.x = tl.x + size.x;

		if (m_y == 0.0f)
			tl.y = br.y - size.y;
		else if (m_y == 1.0f)
			br.y = tl.y + size.y;
	}

	void snapStretchingToScreen(vec3 &tl, vec3 &br, matrix4 &itemToScreen, matrix4 &screenToItem)
	{
		uint32_t worldWidth;
		uint32_t worldHeight;
		m_view->GetVideoBaseDimensions(&worldWidth, &worldHeight);

		vec3 newTL = getTransformedPosition(tl.x, tl.y, itemToScreen);
		vec3 newTR = getTransformedPosition(br.x, tl.y, itemToScreen);
		vec3 newBL = getTransformedPosition(tl.x, br.y, itemToScreen);
		vec3 newBR = getTransformedPosition(br.x, br.y, itemToScreen);
		vec3 boundingTL;
		vec3 boundingBR;

		vec3_copy(&boundingTL, &newTL);
		vec3_min(&boundingTL, &boundingTL, &newTR);
		vec3_min(&boundingTL, &boundingTL, &newBL);
		vec3_min(&boundingTL, &boundingTL, &newBR);

		vec3_copy(&boundingBR, &newTL);
		vec3_max(&boundingBR, &boundingBR, &newTR);
		vec3_max(&boundingBR, &boundingBR, &newBL);
		vec3_max(&boundingBR, &boundingBR, &newBR);

		vec3 offset = getSnapOffset(boundingTL, boundingBR, worldWidth, worldHeight);
		vec3_add(&offset, &offset, &newTL);
		vec3_transform(&offset, &offset, &screenToItem);
		vec3_sub(&offset, &offset, &tl);

		if (m_x == 0.0f)
			tl.x += offset.x;
		else if (m_x == 1.0f)
			br.x += offset.x;

		if (m_y == 0.0f)
			tl.y += offset.y;
		else if (m_y == 1.0f)
			br.y += offset.y;
	}

	void process() {
		auto pos = m_worldMousePosition;
		if (m_parentSceneItem) {
			matrix4 invGroupTransform;
			obs_sceneitem_get_draw_transform(m_parentSceneItem,
							 &invGroupTransform);
			matrix4_inv(&invGroupTransform, &invGroupTransform);

			vec3 group_pos;
			vec3_set(&group_pos, pos.x, pos.y, 0.0f);
			vec3_transform(&group_pos, &group_pos,
				       &invGroupTransform);
			pos.x = group_pos.x;
			pos.y = group_pos.y;
		}

		matrix4 boxTransform;
		vec3 itemUL;
		float itemRot;

		auto stretchItemSize = getSceneItemSize();

		obs_sceneitem_get_box_transform(m_sceneItem, &boxTransform);
		itemRot = obs_sceneitem_get_rot(m_sceneItem);
		vec3_from_vec4(&itemUL, &boxTransform.t);

		/* build the item space conversion matrices */
		matrix4 itemToScreen;
		matrix4_identity(&itemToScreen);
		matrix4_rotate_aa4f(&itemToScreen, &itemToScreen, 0.0f, 0.0f,
				    1.0f, degreesToRadians(itemRot));
		matrix4_translate3f(&itemToScreen, &itemToScreen, itemUL.x,
				    itemUL.y, 0.0f);

		matrix4 screenToItem;
		matrix4_identity(&screenToItem);
		matrix4_translate3f(&screenToItem, &screenToItem, -itemUL.x,
				    -itemUL.y, 0.0f);
		matrix4_rotate_aa4f(&screenToItem, &screenToItem, 0.0f, 0.0f,
				    1.0f, degreesToRadians(-itemRot));

		obs_sceneitem_crop startCrop;
		obs_sceneitem_get_crop(m_sceneItem, &startCrop);
		vec2 startItemPos;
		obs_sceneitem_get_pos(m_sceneItem, &startItemPos);

		obs_source_t *source = obs_sceneitem_get_source(m_sceneItem);
		vec2 cropSize;
		vec2_zero(&cropSize);
		cropSize.x = float(obs_source_get_width(source) -
				   startCrop.left - startCrop.right);
		cropSize.y = float(obs_source_get_height(source) -
				   startCrop.top - startCrop.bottom);

		if (m_parentSceneItem) {
			matrix4 invGroupTransform;
			obs_sceneitem_get_draw_transform(m_parentSceneItem,
							 &invGroupTransform);
			matrix4_inv(&invGroupTransform, &invGroupTransform);
			obs_sceneitem_defer_group_resize_begin(m_parentSceneItem);
		}

		///////////////////////////////////////////////////////////////////////////////

		Qt::KeyboardModifiers modifiers =
			QGuiApplication::keyboardModifiers();
		obs_bounds_type boundsType =
			obs_sceneitem_get_bounds_type(m_sceneItem);
		bool shiftDown = (modifiers & Qt::ShiftModifier);
		vec3 tl, br, pos3;

		vec3_zero(&tl);
		vec3_set(&br, stretchItemSize.x, stretchItemSize.y, 0.0f);

		vec3_set(&pos3, pos.x, pos.y, 0.0f);
		vec3_transform(&pos3, &pos3, &screenToItem);

		if (m_x == 0.0f)
			tl.x = pos3.x;
		else if (m_x == 1.0f)
			br.x = pos3.x;

		if (m_y == 0.0f)
			tl.y = pos3.y;
		else if (m_y == 1.0f)
			br.y = pos3.y;

		if (!(modifiers & Qt::ControlModifier))
			snapStretchingToScreen(tl, br, itemToScreen, screenToItem);

		uint32_t source_cx = obs_source_get_width(source);
		uint32_t source_cy = obs_source_get_height(source);

		// if the source's internal size has been set to 0 for whatever reason
		// while resizing, do not update transform, otherwise source will be
		// stuck invisible until a complete transform reset
		if (!source_cx || !source_cy)
			return;

		vec2 baseSize;
		vec2_set(&baseSize, float(source_cx), float(source_cy));

		vec2 size;
		vec2_set(&size, br.x - tl.x, br.y - tl.y);

		if (boundsType != OBS_BOUNDS_NONE) {
			if (shiftDown)
				clampAspect(tl, br, size, baseSize);

			if (tl.x > br.x)
				std::swap(tl.x, br.x);
			if (tl.y > br.y)
				std::swap(tl.y, br.y);

			vec2_abs(&size, &size);

			obs_sceneitem_set_bounds(m_sceneItem, &size);
		} else {
			obs_sceneitem_crop crop;
			obs_sceneitem_get_crop(m_sceneItem, &crop);

			baseSize.x -= float(crop.left + crop.right);
			baseSize.y -= float(crop.top + crop.bottom);

			if (!shiftDown)
				clampAspect(tl, br, size, baseSize);

			vec2_div(&size, &size, &baseSize);
			obs_sceneitem_set_scale(m_sceneItem, &size);
		}

		pos3 = calculateStretchPos(tl, br);
		vec3_transform(&pos3, &pos3, &itemToScreen);

		vec2 newPos;
		vec2_set(&newPos, std::round(pos3.x), std::round(pos3.y));
		obs_sceneitem_set_pos(m_sceneItem, &newPos);
	}
};
