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
};

class SceneItemControlBox : public SceneItemControlBase {
private:
	bool m_isMouseOver = false;
	bool m_isDragging = false;

	vec3 m_dragStartMouseParentPosition = {0, 0, 0};
	vec2 m_dragStartSceneItemTranslatePos = {0, 0};

private:
	void checkMouseOver(double worldX, double worldY)
	{
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
			},
			true);

		return result;
	}

public:
	SceneItemControlBox(StreamElementsVideoCompositionViewWidget *view,
			    obs_scene_t *scene, obs_sceneitem_t *sceneItem,
			    obs_sceneitem_t *parentSceneItem)
		: SceneItemControlBase(view, scene, sceneItem, parentSceneItem)
	{
	}

	~SceneItemControlBox() {}

	virtual bool HandleMouseMove(QMouseEvent *event, double worldX,
				     double worldY) override
	{
		checkMouseOver(worldX, worldY);

		if (!m_isDragging)
			return false;

		// Actual dragging here
		matrix4 parentTransform, parentInvTransform;

		getSceneItemParentDrawTransformMatrices(m_sceneItem,
							m_parentSceneItem,
							&parentTransform,
							&parentInvTransform);

		auto currMouseParentPosition = getTransformedPosition(
			worldX, worldY, parentInvTransform);

		double deltaX = currMouseParentPosition.x -
				m_dragStartMouseParentPosition.x;

		double deltaY = currMouseParentPosition.y -
				m_dragStartMouseParentPosition.y;

		vec2 newPos;
		vec2_set(&newPos, m_dragStartSceneItemTranslatePos.x + deltaX,
			 m_dragStartSceneItemTranslatePos.y + deltaY);

		obs_sceneitem_set_pos(m_sceneItem, &newPos);

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
								    * /*parent*/) {
								if (obs_sceneitem_selected(
									    item) !=
								    (item ==
								     m_sceneItem)) {
									obs_sceneitem_select(
										item,
										item == m_sceneItem);
								}
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
				    /*parentSceneItem*/) {
					if (obs_sceneitem_selected(sceneItem))
						obs_sceneitem_select(sceneItem,
								     false);
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

			return false;
		}

		return false;
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

		const double thickness =
			4.0f * m_view->devicePixelRatioF() / 2.0f;

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

		auto x1 = m_x - m_width / boxScale.x / 2;
		auto x2 = m_x + m_width / boxScale.x / 2;
		auto y1 = m_y - m_height / boxScale.y / 2;
		auto y2 = m_y + m_height / boxScale.y / 2;

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

		auto x1 = m_x - m_width / boxScale.x / 2;
		auto x2 = m_x + m_width / boxScale.x / 2;
		auto y1 = m_y - m_height / boxScale.y / 2;
		auto y2 = m_y + m_height / boxScale.y / 2;

		gs_matrix_push();
		gs_matrix_mul(&transform);

		fillRect(x1, y1, x2, y2, color);

		if (m_lineTo.get()) {
			double thickness = 3.0f * m_view->devicePixelRatioF();

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

		if (!m_isMouseOver)
			return false;

		m_isDragging = true;

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
};
