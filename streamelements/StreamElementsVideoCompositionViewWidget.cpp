#include <util/platform.h>
#include "graphics/matrix4.h"

#include "StreamElementsVideoCompositionViewWidget.hpp"
#include <QWindow>
#include <QScreen>
#include <QMouseEvent>
#include <QColor>

class FileTexture {
private:
	std::string m_path;
	std::mutex m_mutex;
	gs_texture_t *m_texture = nullptr;

public:
	FileTexture(std::string path): m_path(path) {}

	~FileTexture()
	{
		if (m_texture)
			gs_texture_destroy(m_texture);
	}

	gs_texture_t *get()
	{
		if (m_texture)
			return m_texture;

		std::lock_guard<decltype(m_mutex)> lock(m_mutex);

		if (m_texture)
			return m_texture;

		auto textureFilePath = os_get_abs_path_ptr(m_path.c_str());
		m_texture = gs_texture_create_from_file(textureFilePath);
		bfree(textureFilePath);

		return m_texture;
	}
};

static FileTexture g_overflowTexture("../../data/obs-studio/images/overflow.png");

#define PI (3.1415926535f)

static inline double radiansToDegrees(double radians) {
	return radians * 180.0f / PI;
}

static inline double degreesToRadians(double degrees) {
	return degrees * PI / 180.0f;
}

static inline double normalizeDegrees(double degrees) {
	while (degrees >= 360.0f)
		degrees -= 360.0f;

	while (degrees < 0.0f)
		degrees += 360.0f;

	return degrees;
}

static inline double getCircleDegrees(vec2 center, vec2 periphery) {
	double adjacent = periphery.x - center.x;
	double opposite = periphery.y - center.y;

	if (adjacent == 0.0f) {
		return opposite > 0.0f ? 0.0f : 180.0f;
	} else {
		auto result = radiansToDegrees(atan(abs(opposite) / abs(adjacent)));

		if (opposite > 0.0f) {
			// bottom
			if (adjacent > 0.0f) {
				// right
				result += 270.0f;
			} else {
				// left
				result = 90.0f - result;
			}
		} else {
			// top
			if (adjacent > 0.0f) {
				// right
				result = 90.0f - result + 180.0f;
			} else {
				// left
				result += 90.0f;
			}
		}

		return normalizeDegrees(result + 180.0f);
	}
}

static inline void
scanGroupSceneItems(obs_sceneitem_t *group,
		    std::function<void(obs_sceneitem_t * /*item*/)> callback,
		    bool recursive)
{
	struct data_t {
		std::function<void(obs_sceneitem_t * /*item*/)> callback;
		bool recursive;
	};

	data_t data = {};
	data.callback = callback;
	data.recursive = recursive;

	obs_sceneitem_group_enum_items(
		group,
		[](obs_scene_t *scene, obs_sceneitem_t *item, void *data_p) {
			auto data = (data_t *)data_p;

			data->callback(item);

			if (data->recursive && obs_sceneitem_is_group(item)) {
				scanGroupSceneItems(item, data->callback, data->recursive);
			}

			return true;
		},
		&data);
}

static inline void scanSceneItems(
	obs_scene_t* scene,
	std::function<void(obs_sceneitem_t * /*item*/)> callback,
	bool recursive)
{
	struct data_t {
		std::function<void(obs_sceneitem_t * /*item*/)> callback;
		bool recursive;
	};

	data_t data = {};
	data.callback = callback;
	data.recursive = recursive;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t * /* scene */, obs_sceneitem_t *item,
		   void *data_p) {
			auto data = (data_t *)data_p;

			data->callback(item);

			if (data->recursive && obs_sceneitem_is_group(item)) {
				scanGroupSceneItems(item, data->callback,
						    data->recursive);
			}

			return true;
		},
		&data);
}

//
// Calculate video viewport position and size based on world width & height, and
// viewport width & height constraints.
//
// Adds mandatory padding, so video will not occupy 100% of either X or Y axis
// of the viewport.
//
// The output of this function can be used as input to startProjectionRegion()
// output position and dimensions.
//
static inline void calculateVideoViewportPositionAndSize(
	StreamElementsVideoCompositionViewWidget *self, double worldWidth,
	double worldHeight, double unpaddedViewportWidth, double unpaddedViewportHeight,
	double *viewX, double *viewY, double *viewWidth, double *viewHeight)
{
	double pixelDensity = self->devicePixelRatioF();

	double paddingX = 20.0f * pixelDensity;
	double paddingY = 20.0f * pixelDensity;

	double viewportWidth = unpaddedViewportWidth - (paddingX * 2.0f);
	double viewportHeight = unpaddedViewportHeight - (paddingY * 2.0f);

	*viewWidth = worldWidth;
	*viewHeight = worldHeight;

	if (*viewWidth > viewportWidth) {
		double ratio = viewportWidth / (*viewWidth);

		*viewWidth *= ratio;
		*viewHeight *= ratio;
	}

	if (*viewHeight > viewportHeight) {
		double ratio = viewportHeight / (*viewHeight);

		*viewWidth *= ratio;
		*viewHeight *= ratio;
	}

	*viewWidth = std::floor(*viewWidth);
	*viewHeight = std::floor(*viewHeight);

	*viewX = std::floor((unpaddedViewportWidth / 2.0f) - (*viewWidth / 2.0f));
	*viewY = std::floor((unpaddedViewportHeight / 2.0f) - (*viewHeight / 2.0f));
}

//
// Define a projection region for drawing a view texture.
//
// The projection region maps a rectangle of the view source dimensions onto
// a rectangle on the destination display viewport.
//
// That way when obs_render_main_texture() or obs_view_render() are called,
// they can just draw the source scene/transition and not bother themselves
// with scaling and projection onto the target surface at the correct
// coordinates: everything is predetermined.
//
static inline void startProjectionRegion(int vX, int vY, int vCX, int vCY,
					 float oL, float oT, float oR, float oB)
{
	gs_projection_push();
	gs_viewport_push();
	gs_set_viewport(vX, vY, vCX, vCY);

	// We're projecting source-view-sized rectangle to the viewport rectangle set above
	gs_ortho(oL, oR, oT, oB, -100.0f, 100.0f);
}

//
// Restore projection to whatever state it was before call to startProjectionRegion()
// above.
//
static inline void endProjectionRegion()
{
	gs_viewport_pop();
	gs_projection_pop();
}

//
// Transform QColor to format suitable for use with gs_XXX functions
//
static inline void colorToVec4(QColor color, vec4 *vec) {
	vec4_set(vec, color.redF(), color.greenF(),
		 color.blueF(), color.alphaF());
}

static inline void fillRect(float x1, float y1, float x2, float y2, QColor color)
{
	x1 = std::round(x1);
	x2 = std::round(x2);
	y1 = std::round(y1);
	y2 = std::round(y2);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_eparam_t *colParam = gs_effect_get_param_by_name(gs_get_effect(), "color");

	vec4 colorVec4;
	colorToVec4(color, &colorVec4);

	gs_matrix_push();
	//gs_matrix_identity();
	
	gs_matrix_translate3f(x1, y1, 0.0f); // Start top/left position
	gs_matrix_scale3f(x2 - x1, y2 - y1, 1.0f); // Width/height

	gs_effect_set_vec4(colParam, &colorVec4);

	gs_render_start(true);
	gs_vertex2f(0.0f, 0.0f);
	gs_vertex2f(1.0f, 0.0f);
	gs_vertex2f(0.0f, 1.0f);
	gs_vertex2f(1.0f, 1.0f);
	auto vertexBuffer = gs_render_save();

	gs_load_vertexbuffer(vertexBuffer); // tl, tr, br, bl
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_vertexbuffer_destroy(vertexBuffer);

	gs_matrix_pop();

	gs_load_vertexbuffer(nullptr);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static inline void drawRect(float x1, float y1, float x2, float y2,
			    float thickness, QColor color)
{
	x1 = std::round(x1);
	x2 = std::round(x2);
	y1 = std::round(y1);
	y2 = std::round(y2);

	fillRect(x1, y1, x2, y1 + thickness, color);
	fillRect(x1, y2 - thickness, x2, y2, color);
	fillRect(x1, y1 + thickness, x1 + thickness, y2 - thickness, color);
	fillRect(x2 - thickness, y1 + thickness, x2, y2 - thickness, color);
}

static inline void drawLine(float x1, float y1, float x2, float y2,
			    float thickness,
			    QColor color)
{
	x1 = std::round(x1);
	x2 = std::round(x2);
	y1 = std::round(y1);
	y2 = std::round(y2);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");

	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);

	gs_eparam_t *colParam =
		gs_effect_get_param_by_name(gs_get_effect(), "color");

	vec4 colorVec4;
	colorToVec4(color, &colorVec4);

	gs_matrix_push();
	//gs_matrix_identity();

	gs_effect_set_vec4(colParam, &colorVec4);

	gs_render_start(true);
	gs_vertex2f(x1 - (thickness / 2.0f), y1 - (thickness / 2.0f));
	gs_vertex2f(x1 + thickness, y1 + thickness);
	gs_vertex2f(x2 + thickness, y2 + thickness);
	gs_vertex2f(x2, y2);
	gs_vertex2f(x1, y1);
	auto vertexBuffer = gs_render_save();

	gs_load_vertexbuffer(vertexBuffer);
	gs_draw(GS_TRISTRIP, 0, 0);
	gs_vertexbuffer_destroy(vertexBuffer);

	gs_matrix_pop();

	gs_load_vertexbuffer(nullptr);

	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static void getSceneItemDrawTransformMatrices(obs_sceneitem_t *sceneItem,
					      matrix4 *transform,
					      matrix4 *inv_transform)
{
	obs_sceneitem_get_draw_transform(sceneItem, transform);

	auto scene = obs_sceneitem_get_scene(sceneItem);
	auto parentSceneItem = obs_sceneitem_get_group(scene, sceneItem);
	if (parentSceneItem) {
		matrix4 parentTransform;
		obs_sceneitem_get_draw_transform(parentSceneItem,
						 &parentTransform);

		matrix4_mul(transform, transform, &parentTransform);
	}

	matrix4_inv(inv_transform, transform);
}

static void getSceneItemBoxTransformMatrices(obs_sceneitem_t *sceneItem,
				   matrix4 *transform, matrix4 *inv_transform)
{
	obs_sceneitem_get_box_transform(sceneItem, transform);

	auto scene = obs_sceneitem_get_scene(sceneItem);
	auto parentSceneItem = obs_sceneitem_get_group(scene, sceneItem);
	if (parentSceneItem) {
		matrix4 parentTransform;
		obs_sceneitem_get_draw_transform(parentSceneItem,
						 &parentTransform);

		matrix4_mul(transform, transform, &parentTransform);
	}

	matrix4_inv(inv_transform, transform);
}

static vec2 getSceneItemFinalScale(obs_sceneitem_t *sceneItem)
{
	obs_transform_info info;
	obs_sceneitem_get_info(sceneItem, &info);

	auto scene = obs_sceneitem_get_scene(sceneItem);
	auto parentSceneItem = obs_sceneitem_get_group(scene, sceneItem);
	if (parentSceneItem) {
		obs_transform_info parentInfo;
		obs_sceneitem_get_info(parentSceneItem, &parentInfo);

		info.scale.x *= parentInfo.scale.x;
		info.scale.y *= parentInfo.scale.y;
	}

	return info.scale;
}

static vec3 getTransformedPosition(float x, float y, const matrix4 &mat)
{
	vec3 result;
	vec3_set(&result, x, y, 0.0f);
	vec3_transform(&result, &result, &mat);
	return result;
}

class SceneItemControlBase
	: public StreamElementsVideoCompositionViewWidget::VisualElementBase {
protected:
	obs_scene_t *m_scene;
	obs_sceneitem_t *m_sceneItem;
	StreamElementsVideoCompositionViewWidget *m_view;

public:
	SceneItemControlBase(StreamElementsVideoCompositionViewWidget *view,
			     obs_scene_t *scene, obs_sceneitem_t *sceneItem)
		: m_view(view), m_scene(scene), m_sceneItem(sceneItem)
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

class SceneItemControlPoint : public SceneItemControlBase {
public:
	double m_x;
	double m_y;
	double m_width;
	double m_height;

	bool m_modifyX;
	bool m_modifyY;
	bool m_modifyRotation;

	std::shared_ptr<SceneItemControlPoint> m_lineTo;

public:
	SceneItemControlPoint(
		StreamElementsVideoCompositionViewWidget *view,
		obs_scene_t *scene, obs_sceneitem_t *sceneItem, double x,
		double y, double width, double height, bool modifyX,
		bool modifyY, bool modifyRotation,
		std::shared_ptr<SceneItemControlPoint> lineTo = nullptr)
		: SceneItemControlBase(view, scene, sceneItem),
		  m_x(x),
		  m_y(y),
		  m_width(width),
		  m_height(height),
		  m_modifyX(modifyX),
		  m_modifyY(modifyY),
		  m_modifyRotation(modifyRotation),
		  m_lineTo(lineTo)
	{
	}

	~SceneItemControlPoint() {}

	virtual void Draw() override
	{
		QColor color(255, 0, 0, 255);

		matrix4 transform, inv_tranform;
		getSceneItemBoxTransformMatrices(m_sceneItem, &transform,
					      &inv_tranform);

		auto anchorPosition =
			getTransformedPosition(m_x, m_y, transform);

		fillRect(anchorPosition.x - m_width / 2.0f,
			 anchorPosition.y - m_height / 2.0f,
			 anchorPosition.x + m_width / 2.0f,
			 anchorPosition.y + m_width / 2.0f, color);

		if (m_lineTo.get()) {
			double thickness = 5.0f * m_view->devicePixelRatioF();

			auto otherPosition = getTransformedPosition(
				m_lineTo->m_x, m_lineTo->m_y, transform);

			drawLine(anchorPosition.x, anchorPosition.y,
				 otherPosition.x, otherPosition.y, thickness,
				 color);
		}

		/*
		vec2 center;
		vec2_set(&center, 0.5f, 0.5f);
		vec3 mouse3 = getTransformedPosition(view->m_currMouseWorldX,
						     view->m_currMouseWorldY,
						     inv_tranform);
		vec2 mouse;
		vec2_set(&mouse, mouse3.x, mouse3.y);

		auto degrees = getCircleDegrees(center, mouse);

		char buf[512];
		sprintf(buf, "degrees: %0.2fn", degrees);
		OutputDebugStringA(buf);
		*/
	}
};

class SceneItemControlBox : public SceneItemControlBase {
public:
	SceneItemControlBox(StreamElementsVideoCompositionViewWidget *view,
			    obs_scene_t *scene, obs_sceneitem_t *sceneItem)
		: SceneItemControlBase(view, scene, sceneItem)
	{
	}

	~SceneItemControlBox() {}

	virtual void Draw() override
	{
		const double thickness = 5.0f * m_view->devicePixelRatioF();

		matrix4 transform, inv_tranform;
		getSceneItemBoxTransformMatrices(m_sceneItem, &transform,
					      &inv_tranform);

		auto tl = getTransformedPosition(0.0f, 0.0f, transform);
		auto tr = getTransformedPosition(1.0f, 0.0f, transform);
		auto bl = getTransformedPosition(0.0f, 1.0f, transform);
		auto br = getTransformedPosition(1.0f, 1.0f, transform);

		QColor color(255, 0, 0, 255);
		drawLine(tl.x, tl.y, tr.x, tr.y, thickness, color);
		drawLine(tr.x, tr.y, br.x, br.y, thickness, color);
		drawLine(br.x, br.y, bl.x, bl.y, thickness, color);
		drawLine(bl.x, bl.y, tl.x, tl.y, thickness, color);
	}
};

class SceneItemOverflowBox : public SceneItemControlBase {
public:
	SceneItemOverflowBox(StreamElementsVideoCompositionViewWidget *view,
			    obs_scene_t *scene, obs_sceneitem_t *sceneItem)
		: SceneItemControlBase(view, scene, sceneItem)
	{
	}

	~SceneItemOverflowBox() {}

	virtual void Draw() override
	{
		matrix4 transform, inv_tranform;
		getSceneItemBoxTransformMatrices(m_sceneItem, &transform,
						  &inv_tranform);

		auto itemScale = getSceneItemFinalScale(m_sceneItem);

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
			gs_draw_sprite(g_overflowTexture.get(), 0, 1, 1); // Output width & height
		}

		gs_blend_state_pop();

		gs_enable_framebuffer_srgb(previous);

		gs_matrix_pop();
	}
};

StreamElementsVideoCompositionViewWidget::VisualElements::VisualElements(
	StreamElementsVideoCompositionViewWidget *view,
		obs_scene_t *scene, obs_sceneitem_t *item)
{
	// TODO: check for selection

	auto pixelDensity = (double)view->devicePixelRatioF();

	const float thickness = 20.0f * pixelDensity;
	const float rotationDistance = 200.0f * pixelDensity;

	// Overflow box
	m_bottomLayer.push_back(std::make_shared<SceneItemOverflowBox>(
		view, scene, item));

	// Box
	m_topLayer.push_back(std::make_shared<SceneItemControlBox>(
		view, scene, item));

	// Top-left
	m_topLayer.push_back(std::make_shared<SceneItemControlPoint>(
		view, scene, item, 0.0f, 0.0f, thickness, thickness,
		true, true, false));

	// Top-right
	m_topLayer.push_back(std::make_shared<SceneItemControlPoint>(
		view, scene, item, 1.0f, 0.0f, thickness, thickness,
		true, true, false));

	// Bottom-Left
	m_topLayer.push_back(std::make_shared<SceneItemControlPoint>(
		view, scene, item, 0.0f, 1.0f, thickness, thickness,
		true, true, false));

	// Bottom-Right
	m_topLayer.push_back(std::make_shared<SceneItemControlPoint>(
		view, scene, item, 1.0f, 1.0f, thickness, thickness,
		true, true, false));

	// Top
	auto topPoint = std::make_shared<SceneItemControlPoint>(
		view, scene, item, 0.5f, 0.0f, thickness, thickness,
		false, true, false);

	m_topLayer.push_back(topPoint);

	// Bottom
	m_topLayer.push_back(std::make_shared<SceneItemControlPoint>(
		view, scene, item, 0.5f, 1.0f, thickness, thickness,
		false, true, false));

	// Left
	m_topLayer.push_back(std::make_shared<SceneItemControlPoint>(
		view, scene, item, 0.0f, 0.5f, thickness, thickness,
		true, false, false));

	// Right
	m_topLayer.push_back(std::make_shared<SceneItemControlPoint>(
		view, scene, item, 1.0f, 0.5f, thickness, thickness,
		true, false, false));

	//
	// Calculate the distance of the rotation handle from the source in _source_ coordinates (0.0 - 1.0)
	// We do this by calculating the source _final_ scale, based on it's own scale and it's parent scale,
	// dividing it by the source height, and then multiplying it by the rotation distance in _world_ coordinate
	// space.
	//
	// We have to perform this complex transformation since control point coordinates are relative to the _source_
	// coordinate system, but the visible distance is in _world_ coordinate space:
	// this way we can supply the distance in the same coord space as the rest of the control points.
	//
	auto scale = getSceneItemFinalScale(item);
	auto scaledRotationDistance =
		rotationDistance * scale.y /
		(double)obs_source_get_height(
			obs_sceneitem_get_source(item));

	// Rotation
	m_topLayer.push_back(std::make_shared<SceneItemControlPoint>(
		view, scene, item, 0.5f, 0.0f - scaledRotationDistance,
		thickness, thickness, false, false, true, topPoint));
}

//
// Remove state for scene items which are no longer present on the scene, and
// add scene items which are present on the scene but haven't been tracked so far.
//
// After that, draw bottom layer visuals, render video, and draw top layer visuals.
//
void StreamElementsVideoCompositionViewWidget::VisualElementsStateManager::UpdateAndDraw(
	obs_scene_t *scene)
{
	std::map<obs_sceneitem_t *, bool> existingSceneItems;

	scanSceneItems(
		scene,
		[&](obs_sceneitem_t *item) { existingSceneItems[item] = true; },
		true);

	// Remove scene items which have been removed from the scene
	for (auto it = m_sceneItems.cbegin(); it != m_sceneItems.cend(); ++it) {
		if (!existingSceneItems.count(it->first)) {
			m_sceneItems.erase(it);
		}
	}

	// Add scene items which do not exist in the state yet
	for (auto kv : existingSceneItems) {
		auto item = kv.first;

		if (m_sceneItems.count(item))
			continue;

		m_sceneItems[item] = std::make_shared<
			StreamElementsVideoCompositionViewWidget::VisualElements>(
			m_view, scene, item);
	}

	//
	// Draw visual elements and video render
	//

	for (auto kv : m_sceneItems) {
		kv.second->DrawBottomLayer();
	}

	// Render the view into the region set above
	m_view->m_videoCompositionInfo->Render();

	for (auto kv : m_sceneItems) {
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

	QWidget::mouseMoveEvent(event);
}

void StreamElementsVideoCompositionViewWidget::mousePressEvent(
	QMouseEvent *event)
{
	m_currMouseWidgetX = event->localPos().x();
	m_currMouseWidgetY = event->localPos().y();

	QWidget::mousePressEvent(event);
}

void StreamElementsVideoCompositionViewWidget::mouseReleaseEvent(
	QMouseEvent *event)
{
	m_currMouseWidgetX = event->localPos().x();
	m_currMouseWidgetY = event->localPos().y();

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
	//int viewX, viewY;
	//int viewWidth, viewHeight;
	//float scale;

	auto video = self->m_videoCompositionInfo->GetVideo();
	auto ovi = video_output_get_info(video);

	// TODO: Figure out what to do with scaled output size: i.e. we probably want to get the video dimension info from the view, or maybe from another place
	worldWidth = ovi->width;
	worldHeight = ovi->height;

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

	// Fill video viewport background with black color
	fillRect(0, 0, worldWidth, worldHeight, QColor(0, 0, 0, 255));

	// Render the view into the region set above
	// self->m_videoCompositionInfo->Render();

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

		drawLine(0, worldHeight, self->m_currMouseWorldX, self->m_currMouseWorldY,
			 5.0f, QColor(0, 255, 0, 175));

		drawLine(worldWidth, worldHeight, self->m_currMouseWorldX,
			 self->m_currMouseWorldY, 5.0f, QColor(0, 255, 0, 175));

		drawLine(worldWidth, 0, self->m_currMouseWorldX,
			 self->m_currMouseWorldY, 5.0f, QColor(0, 255, 0, 175));
	}

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
		self->m_videoComposition->GetCurrentScene());

	gs_matrix_pop();
	gs_viewport_pop();

	endProjectionRegion();
}
