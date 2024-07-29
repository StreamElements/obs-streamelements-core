#include <obs-frontend-api.h>
#include <util/platform.h>
#include "graphics/matrix4.h"

#include "StreamElementsVideoCompositionViewWidget.hpp"
#include <QWindow>
#include <QScreen>
#include <QMouseEvent>
#include <QColor>
#include <QGuiApplication>

#include "canvas-config.hpp"
#include "canvas-math.hpp"
#include "canvas-scan.hpp"
#include "canvas-draw.hpp"
#include "canvas-controls.hpp"

StreamElementsVideoCompositionViewWidget::VisualElements::VisualElements(
	StreamElementsVideoCompositionViewWidget *view, obs_scene_t *scene,
	obs_sceneitem_t *sceneItem, obs_sceneitem_t *parentSceneItem)
	: m_scene(scene), m_sceneItem(sceneItem), m_parentSceneItem(parentSceneItem)
{
	// TODO: check for selection

	auto pixelDensity = (double)view->devicePixelRatioF();

	const float thickness = 20.0f * pixelDensity;

	// Overflow box
	m_bottomLayer.push_back(std::make_shared<SceneItemOverflowBox>(
		view, scene, sceneItem, parentSceneItem));

	// Top-left
	m_topLayer.push_back(std::make_shared<SceneItemStretchControlPoint>(
		view, scene, sceneItem, parentSceneItem, 0.0f, 0.0f, thickness, thickness));

	// Top-right
	m_topLayer.push_back(std::make_shared<SceneItemStretchControlPoint>(
		view, scene, sceneItem, parentSceneItem, 1.0f, 0.0f, thickness, thickness));

	// Bottom-Left
	m_topLayer.push_back(std::make_shared<SceneItemStretchControlPoint>(
		view, scene, sceneItem, parentSceneItem, 0.0f, 1.0f, thickness, thickness));

	// Bottom-Right
	m_topLayer.push_back(std::make_shared<SceneItemStretchControlPoint>(
		view, scene, sceneItem, parentSceneItem, 1.0f, 1.0f, thickness, thickness));

	// Top
	auto topPoint = std::make_shared<SceneItemStretchControlPoint>(
		view, scene, sceneItem, parentSceneItem, 0.5f, 0.0f, thickness, thickness);

	m_topLayer.push_back(topPoint);

	// Bottom
	m_topLayer.push_back(std::make_shared<SceneItemStretchControlPoint>(
		view, scene, sceneItem, parentSceneItem, 0.5f, 1.0f, thickness, thickness));

	// Left
	m_topLayer.push_back(std::make_shared<SceneItemStretchControlPoint>(
		view, scene, sceneItem, parentSceneItem, 0.0f, 0.5f, thickness, thickness));

	// Right
	m_topLayer.push_back(std::make_shared<SceneItemStretchControlPoint>(
		view, scene, sceneItem, parentSceneItem, 1.0f, 0.5f, thickness, thickness));

	//
	// Calculate the distance of the rotation handle from the source in _source_ coordinates (0.0 - 1.0)
	// We do this by calculating the scene item box final scale (ratio between world coordinates and source coordinates),
	// and then dividing the rotation control point distance in world coordinates, by that ratio.
	//
	// We have to perform this complex transformation since control point coordinates are relative to the _source_
	// coordinate system, but the visible distance is in _world_ coordinate space:
	// this way we can supply the distance in the same coord space as the rest of the control points.
	//
	const float rotationDistance = 60.0f * pixelDensity;

	auto scale = getSceneItemFinalBoxScale(sceneItem, parentSceneItem);

	double scaledRotationDistance = rotationDistance / abs(scale.y);

	// Rotation
	m_topLayer.push_back(std::make_shared<SceneItemRotationControlPoint>(
		view, scene, sceneItem, parentSceneItem, 0.5f,
		0.0f - scaledRotationDistance,
		thickness, thickness, topPoint));

	// Drag Box
	m_topLayer.push_back(std::make_shared<SceneItemMoveControlBox>(
		view, scene, sceneItem, parentSceneItem));
}

//
// Remove state for scene items which are no longer present on the scene, and
// add scene items which are present on the scene but haven't been tracked so far.
//
// After that, draw bottom layer visuals, render video, and draw top layer visuals.
//
void StreamElementsVideoCompositionViewWidget::VisualElementsStateManager::
	UpdateAndDraw(obs_scene_t *scene, double worldWidth, double worldHeight)
{
	std::map<obs_sceneitem_t *, obs_sceneitem_t *> existingSceneItems;

	m_sceneItemsEventProcessingOrder.clear();

	scanSceneItems(
		scene,
		[&](obs_sceneitem_t *item, obs_sceneitem_t *parent) {
			if (obs_sceneitem_locked(item))
				return;

			if (!obs_sceneitem_visible(item))
				return;

			existingSceneItems[item] = parent;

			m_sceneItemsEventProcessingOrder.push_back(item);
		},
		true);

	// Remove scene items which have been removed from the scene, hidden or locked
	for (auto it = m_sceneItemsVisualElementsMap.cbegin(); it != m_sceneItemsVisualElementsMap.cend(); ++it) {
		if (!existingSceneItems.count(it->first)) {
			m_sceneItemsVisualElementsMap.erase(it);
		}
	}

	// Add scene items which do not exist in the state yet
	for (auto kv : existingSceneItems) {
		auto item = kv.first;
		auto parentItem = kv.second;

		if (m_sceneItemsVisualElementsMap.count(item))
			continue;

		m_sceneItemsVisualElementsMap[item] = std::make_shared<
			StreamElementsVideoCompositionViewWidget::VisualElements>(
			m_view, scene, item, parentItem);
	}

	//
	// Draw visual elements and video render
	//

	for (auto kv : m_sceneItemsVisualElementsMap) {
		kv.second->Tick();
	}

	for (auto kv : m_sceneItemsVisualElementsMap) {
		kv.second->DrawBottomLayer();
	}

	// Fill video viewport background with black color
	fillRect(0, 0, worldWidth, worldHeight, QColor(0, 0, 0, 255));

	// Render the view into the region set above
	m_view->m_videoCompositionInfo->Render();

	// Draw groups first
	for (auto kv : m_sceneItemsVisualElementsMap) {
		if (!kv.second->HasParent())
			kv.second->DrawTopLayer();
	}

	// Draw children second
	for (auto kv : m_sceneItemsVisualElementsMap) {
		if (kv.second->HasParent())
			kv.second->DrawTopLayer();
	}
}

static inline QSize GetPixelSize(QWidget *widget)
{
	return widget->size() * widget->devicePixelRatioF();
}

static bool QTToGSWindow(QWindow *window, gs_window &gswindow)
{
	bool success = true;

#ifdef _WIN32
	gswindow.hwnd = (HWND)window->winId();
#elif __APPLE__
	gswindow.view = (id)window->winId();
#else
	switch (obs_get_nix_platform()) {
	case OBS_NIX_PLATFORM_X11_EGL:
		gswindow.id = window->winId();
		gswindow.display = obs_get_nix_platform_display();
		break;
#ifdef ENABLE_WAYLAND
	case OBS_NIX_PLATFORM_WAYLAND: {
		QPlatformNativeInterface *native =
			QGuiApplication::platformNativeInterface();
		gswindow.display =
			native->nativeResourceForWindow("surface", window);
		success = gswindow.display != nullptr;
		break;
	}
#endif
	default:
		success = false;
		break;
	}
#endif
	return success;
}

StreamElementsVideoCompositionViewWidget::StreamElementsVideoCompositionViewWidget(
	QWidget* parent,
	std::shared_ptr<StreamElementsVideoCompositionBase> videoComposition)
	: m_videoComposition(videoComposition),
	  QWidget(parent),
	  m_visualElementsState(this)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);

	setMouseTracking(true);

	m_videoCompositionInfo = m_videoComposition->GetCompositionInfo(this);

	auto windowVisible = [this](bool visible) {
		if (!visible) {
#if !defined(_WIN32) && !defined(__APPLE__)
			//m_display = nullptr;
#endif
			return;
		}

		if (!m_display) {
			//CreateDisplay();
		} else {
			QSize size = GetPixelSize(this);
			obs_enter_graphics();
			obs_display_resize(m_display, size.width(),
					   size.height());
			obs_leave_graphics();
		}
	};

	auto screenChanged = [this](QScreen *) {
		//CreateDisplay();

		if (!m_display)
			return;

		QSize size = GetPixelSize(this);
		obs_enter_graphics();
		obs_display_resize(m_display, size.width(), size.height());
		obs_leave_graphics();
	};

	connect(windowHandle(), &QWindow::visibleChanged, windowVisible);
	connect(windowHandle(), &QWindow::screenChanged, screenChanged);

#ifdef ENABLE_WAYLAND
	if (obs_get_nix_platform() == OBS_NIX_PLATFORM_WAYLAND)
		windowHandle()->installEventFilter(
			new SurfaceEventFilter(this));
#endif

	//CreateDisplay();
}

StreamElementsVideoCompositionViewWidget::
~StreamElementsVideoCompositionViewWidget()
{
	if (m_display) {
		obs_enter_graphics();
		obs_display_remove_draw_callback(
			m_display, obs_display_draw_callback, this);

		obs_display_destroy(m_display);
		obs_leave_graphics();

		m_display = nullptr;
	}

	m_videoCompositionInfo = nullptr;
}

void StreamElementsVideoCompositionViewWidget::CreateDisplay()
{
	if (m_display)
		return;

	if (!isVisible())
		return;

	if (!windowHandle()->isExposed())
		return;

	QSize size = GetPixelSize(this);

	gs_init_data info = {};
	info.cx = size.width();
	info.cy = size.height();
	info.format = GS_BGRA;
	info.zsformat = GS_ZS_NONE;

	if (!QTToGSWindow(windowHandle(), info.window))
		return;

	obs_enter_graphics();
	m_display = obs_display_create(&info, 0x303030L);

	obs_display_add_draw_callback(m_display, obs_display_draw_callback, this);
	obs_leave_graphics();
}

void StreamElementsVideoCompositionViewWidget::paintEvent(QPaintEvent *event)
{
	CreateDisplay();

	QWidget::paintEvent(event);
}

void StreamElementsVideoCompositionViewWidget::moveEvent(QMoveEvent *event)
{
	QWidget::moveEvent(event);

	OnMove();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool StreamElementsVideoCompositionViewWidget::nativeEvent(const QByteArray &,
							   void *message,
							   qintptr *)
#else
bool StreamElementsVideoCompositionViewWidget::nativeEvent(const QByteArray &,
							   void *message,
							   long *)
#endif
{
#ifdef _WIN32
	const MSG &msg = *static_cast<MSG *>(message);
	switch (msg.message) {
	case WM_DISPLAYCHANGE:
		OnDisplayChange();
	}
#else
	UNUSED_PARAMETER(message);
#endif

	return false;
}

void StreamElementsVideoCompositionViewWidget::resizeEvent(QResizeEvent *event)
{
	QWidget::resizeEvent(event);

	CreateDisplay();

	if (isVisible() && m_display) {
		QSize size = GetPixelSize(this);
		obs_display_resize(m_display, size.width(), size.height());
	}
}

QPaintEngine *StreamElementsVideoCompositionViewWidget::paintEngine() const
{
	return nullptr;
}

void StreamElementsVideoCompositionViewWidget::OnMove()
{
	if (m_display)
		obs_display_update_color_space(m_display);
}

void StreamElementsVideoCompositionViewWidget::OnDisplayChange()
{
	if (m_display)
		obs_display_update_color_space(m_display);
}

void StreamElementsVideoCompositionViewWidget::viewportToWorldCoords(
	uint32_t viewportX, uint32_t viewportY,
	uint32_t* worldX, uint32_t* worldY)
{
	auto displaySize = GetPixelSize(this);

	auto ovi = video_output_get_info(m_videoCompositionInfo->GetVideo());

	auto displayWidth = (uint32_t)displaySize.width();
	auto displayHeight = (uint32_t)displaySize.height();

	auto worldWidth = ovi->width;
	auto worldHeight = ovi->height;

	auto widthRatio = (double)displayWidth / (double)worldWidth;
	auto heightRatio = (double)displayHeight / (double)worldHeight;

	*worldX = (uint32_t)((double)viewportX / widthRatio);
	*worldY = (uint32_t)((double)viewportY / heightRatio);
}

void StreamElementsVideoCompositionViewWidget::mouseMoveEvent(QMouseEvent *event)
{
	m_currMouseWidgetX = event->localPos().x();
	m_currMouseWidgetY = event->localPos().y();

	m_mouseArmedForClickEvent = false;

	m_visualElementsState.HandleMouseMove(event, m_currMouseWorldX,
					      m_currMouseWorldY);

	QWidget::mouseMoveEvent(event);
}

void StreamElementsVideoCompositionViewWidget::mousePressEvent(
	QMouseEvent *event)
{
	m_currMouseWidgetX = event->localPos().x();
	m_currMouseWidgetY = event->localPos().y();

	m_mouseArmedForClickEvent = true;

	m_visualElementsState.HandleMouseDown(event, m_currMouseWorldX,
					      m_currMouseWorldY);

	QWidget::mousePressEvent(event);
}

void StreamElementsVideoCompositionViewWidget::mouseReleaseEvent(
	QMouseEvent *event)
{
	m_currMouseWidgetX = event->localPos().x();
	m_currMouseWidgetY = event->localPos().y();

	m_visualElementsState.HandleMouseUp(event, m_currMouseWorldX,
					    m_currMouseWorldY);

	if (m_mouseArmedForClickEvent) {
		m_mouseArmedForClickEvent = false;

		m_visualElementsState.HandleMouseClick(event, m_currMouseWorldX,
						       m_currMouseWorldY);
	}

	QWidget::mouseReleaseEvent(event);
}

void StreamElementsVideoCompositionViewWidget::obs_display_draw_callback(void* data, uint32_t viewportWidth,
	uint32_t viewportHeight)
{
	//OutputDebugStringA("obs_display_draw_callback\n");
	StreamElementsVideoCompositionViewWidget *self =
		reinterpret_cast<StreamElementsVideoCompositionViewWidget *>(
			data);

	uint32_t worldWidth;
	uint32_t worldHeight;

	self->m_videoCompositionInfo->GetVideoBaseDimensions(&worldWidth,
							     &worldHeight);

	double viewX, viewY, viewWidth, viewHeight;
	calculateVideoViewportPositionAndSize(self, worldWidth, worldHeight,
					 viewportWidth, viewportHeight, &viewX,
					 &viewY, &viewWidth, &viewHeight);

	startProjectionRegion(viewX, viewY, viewWidth, viewHeight, 0.0f, 0.0f,
			      double(worldWidth), double(worldHeight));

	// Calculate World mouse coordinates based on viewport position and size (scale)
	//
	// This can also be done with getTransformedPosition() using an inverted projection matrix,
	// but since the calculation is so simple, we won't be using matrix algebra here.
	//
	self->m_currMouseWorldX =
		(double)(self->m_currMouseWidgetX - viewX) / viewWidth * worldWidth;
	self->m_currMouseWorldY =
		(double)(self->m_currMouseWidgetY - viewY) / viewWidth * worldWidth;

	//
	// Set the viewport to the entire area of the display, and adjust translation & scaling,
	// so we can draw outside of the video boundaries while still using the same
	// coordinate system.
	//
	gs_viewport_push();
	gs_set_viewport(0, 0, viewportWidth, viewportHeight); // Viewport was originally set to the video projection area, now we extend it so elements we draw can be seen
	gs_matrix_push();
	gs_matrix_scale3f(viewWidth / double(viewportWidth),
			  viewHeight / double(viewportHeight), 1.0f);

	// TODO: Figure out where did this 2.0f factor come from
	gs_matrix_translate3f(viewX * 2.0f, viewY * 2.0f, 0.0f);

	// Update visual elements state and draw them on screen
	self->m_visualElementsState.UpdateAndDraw(
		self->m_videoComposition->GetCurrentScene(), worldWidth,
		worldHeight);

	gs_matrix_pop();
	gs_viewport_pop();

	/*
	if (self->m_currUnderMouse) {
		// Temporary mouse tracking debugger
		fillRect(self->m_currMouseWorldX, self->m_currMouseWorldY,
			 self->m_currMouseWorldX + 50,
			 self->m_currMouseWorldY + 50,
			 QColor(255, 255, 0, 175));

		drawRect(self->m_currMouseWorldX, self->m_currMouseWorldY,
			 self->m_currMouseWorldX + 50,
			 self->m_currMouseWorldY + 50, 5.0f,
			 QColor(255, 0, 0, 175));

		drawLine(0, 0, self->m_currMouseWorldX, self->m_currMouseWorldY,
			 5.0f, QColor(0, 255, 0, 175));

		drawLine(0, worldHeight, self->m_currMouseWorldX,
			 self->m_currMouseWorldY, 5.0f, QColor(0, 255, 0, 175));

		drawLine(worldWidth, worldHeight, self->m_currMouseWorldX,
			 self->m_currMouseWorldY, 5.0f, QColor(0, 255, 0, 175));

		drawLine(worldWidth, 0, self->m_currMouseWorldX,
			 self->m_currMouseWorldY, 5.0f, QColor(0, 255, 0, 175));
	}
	*/

	endProjectionRegion();
}
