#pragma once

#include "StreamElementsVideoCompositionManager.hpp"
#include <QWidget>
#include <QMouseEvent>
#include <QEnterEvent>
#include <QCursor>

#include "canvas-config.hpp"
#include "canvas-scan.hpp"

class StreamElementsVideoCompositionViewWidget : public QWidget, public StreamElementsVideoCompositionEventListener
{
public:
	bool m_destroyed = false;
	os_event_t *m_destroy_event = nullptr;

	std::shared_ptr<FileTexture> m_overflowTexture =
		std::make_shared<FileTexture>(
			"../../data/obs-studio/images/overflow.png");

public:
	class VisualElementBase {
	public:
		VisualElementBase() {}
		~VisualElementBase() {}

	public:
		virtual void Tick() {}
		virtual void Draw() = 0;

		virtual bool HandleMouseDown(QMouseEvent *event, double worldX, double worldY) { return false; }
		virtual bool HandleMouseUp(QMouseEvent *event, double worldX, double worldY) { return false; }
		virtual bool HandleMouseClick(QMouseEvent *event, double worldX, double worldY) { return false; }
		virtual bool HandleMouseMove(QMouseEvent *event, double worldX, double worldY) { return false; }

		virtual bool SetMouseCursor(QCursor &cursor) { return false; }
			
	};

	class VisualElements {
	private:
		std::vector<
			std::shared_ptr<StreamElementsVideoCompositionViewWidget::
						VisualElementBase>>
			m_topLayer;
		std::vector<
			std::shared_ptr<StreamElementsVideoCompositionViewWidget::
						VisualElementBase>>
			m_bottomLayer;

		obs_scene_t *m_scene;
		obs_sceneitem_t *m_sceneItem;
		obs_sceneitem_t *m_parentSceneItem;

	public:
		VisualElements(StreamElementsVideoCompositionViewWidget *view,
			       obs_scene_t *scene, obs_sceneitem_t *item,
			       obs_sceneitem_t *parentItem);
		~VisualElements() {}

		bool HasParent() { return !!m_parentSceneItem; }

		obs_sceneitem_t *GetSceneItem() { return m_sceneItem; }
		obs_sceneitem_t *GetParentSceneItem() { return m_parentSceneItem; }

		void Tick()
		{
			for (auto element : m_bottomLayer) {
				element->Tick();
			}

			for (auto element : m_topLayer) {
				element->Tick();
			}
		}

		void DrawTopLayer()
		{
			// Draw in reverse order of mouse event processing
			for (size_t i = m_topLayer.size(); i > 0; --i) {
				auto element = m_topLayer[i - 1];

				element->Draw();
			}
		}

		void DrawBottomLayer()
		{
			for (auto element : m_bottomLayer) {
				element->Draw();
			}
		}

		bool HandleMouseDown(QMouseEvent *event, double worldX,
					     double worldY)
		{
			for (auto element : m_topLayer) {
				if (element->HandleMouseDown(event, worldX,
							     worldY))
					return true;
			}

			return false;
		}

		bool HandleMouseUp(QMouseEvent *event, double worldX,
					   double worldY)
		{
			for (auto element : m_topLayer) {
				if (element->HandleMouseUp(event, worldX,
							   worldY))
					return true;
			}

			return false;
		}

		bool HandleMouseClick(QMouseEvent *event, double worldX,
					      double worldY)
		{
			for (auto element : m_topLayer) {
				if (element->HandleMouseClick(event, worldX,
							      worldY))
					return true;
			}

			return false;
		}

		bool HandleMouseMove(QMouseEvent *event, double worldX,
					     double worldY)
		{
			for (auto element : m_topLayer) {
				if (element->HandleMouseMove(event, worldX,
							     worldY))
					return true;
			}

			return false;
		}

		bool SetMouseCursor(QCursor &cursor)
		{
			for (auto element : m_topLayer) {
				if (element->SetMouseCursor(cursor))
					return true;
			}

			return false;
		}
	};

	class VisualElementsStateManager {
	private:
		StreamElementsVideoCompositionViewWidget* m_view;
		std::map<obs_sceneitem_t *, std::shared_ptr<VisualElements>>
			m_sceneItemsVisualElementsMap;
		std::vector<obs_sceneitem_t *> m_sceneItemsEventProcessingOrder;

	public:
		VisualElementsStateManager(
			StreamElementsVideoCompositionViewWidget *view)
			: m_view(view)
		{
		}
		~VisualElementsStateManager() {}

		void Clear() {
			m_sceneItemsVisualElementsMap.clear();
			m_sceneItemsEventProcessingOrder.clear();
		}

		void UpdateAndDraw(
			StreamElementsVideoCompositionViewWidget *self,
			obs_scene_t *scene, uint32_t viewportWidth,
			uint32_t viewportHeight,
			std::shared_ptr<StreamElementsVideoCompositionBase::
						CompositionInfo>
				videoCompositionInfo);

		bool ExecProcessingOrder(
			std::function<bool(obs_sceneitem_t *)> callback, bool processAllSelected, bool processAllUnselected)
		{
			bool wasProcessed = false;

			for (auto sceneItem :
			     m_sceneItemsEventProcessingOrder) {
				if (!obs_sceneitem_selected(sceneItem))
					continue;

				wasProcessed |= callback(sceneItem);

				if (!processAllSelected && wasProcessed)
					break;
			}

			if (wasProcessed)
				return true;

			for (auto sceneItem :
			     m_sceneItemsEventProcessingOrder) {
				if (obs_sceneitem_selected(sceneItem))
					continue;

				wasProcessed |= callback(sceneItem);

				if (!processAllUnselected && wasProcessed)
					break;
			}

			return wasProcessed;
		}

		bool ExecProcessingOrderUnselectedFirst(
			std::function<bool(obs_sceneitem_t *)> callback,
			bool processAllSelected, bool processAllUnselected)
		{
			bool wasProcessed = false;

			for (auto sceneItem :
			     m_sceneItemsEventProcessingOrder) {
				if (obs_sceneitem_selected(sceneItem))
					continue;

				wasProcessed |= callback(sceneItem);

				if (!processAllUnselected && wasProcessed)
					break;
			}

			if (wasProcessed)
				return true;

			for (auto sceneItem :
			     m_sceneItemsEventProcessingOrder) {
				if (!obs_sceneitem_selected(sceneItem))
					continue;

				wasProcessed |= callback(sceneItem);

				if (!processAllSelected && wasProcessed)
					break;
			}

			return wasProcessed;
		}

		bool SetMouseCursor(QCursor &cursor)
		{
			return ExecProcessingOrder(
				[&](obs_sceneitem_t *sceneItem) -> bool {
					return m_sceneItemsVisualElementsMap
						[sceneItem]
							->SetMouseCursor(
								cursor);
				},
				false, false);
		}

		bool HandleMouseDown(QMouseEvent *event, double worldX,
					     double worldY)
		{
			bool wasProcessed = ExecProcessingOrder(
				[&](obs_sceneitem_t *sceneItem) -> bool {
					return m_sceneItemsVisualElementsMap
						[sceneItem]
							->HandleMouseDown(
								event, worldX,
								worldY);
				},
				true, true);

			if (!wasProcessed) {
				// Nothing selected
				for (auto sceneItem :
				     m_sceneItemsEventProcessingOrder) {
					if (!obs_sceneitem_selected(sceneItem))
						continue;

					obs_sceneitem_select(sceneItem, false);
				}
			}

			return wasProcessed;
		}

		bool HandleMouseUp(QMouseEvent *event, double worldX,
					   double worldY)
		{
			return ExecProcessingOrder(
				[&](obs_sceneitem_t *sceneItem) -> bool {
					return m_sceneItemsVisualElementsMap
						[sceneItem]
							->HandleMouseUp(
								event, worldX,
								worldY);
				},
				true, true);
		}

		bool HandleMouseClick(QMouseEvent *event, double worldX,
					      double worldY)
		{
			return ExecProcessingOrderUnselectedFirst(
				[&](obs_sceneitem_t *sceneItem) -> bool {
					return m_sceneItemsVisualElementsMap
						[sceneItem]
							->HandleMouseClick(
								event, worldX,
								worldY);
				},
				false, false);
		}

		bool HandleMouseMove(QMouseEvent *event, double worldX,
					     double worldY)
		{
			return ExecProcessingOrder(
				[&](obs_sceneitem_t *sceneItem) -> bool {
					return m_sceneItemsVisualElementsMap
						[sceneItem]
							->HandleMouseMove(
								event, worldX,
								worldY);
				},
				false, false);
		}
	};

private:
	std::shared_ptr<StreamElementsVideoCompositionBase> m_videoComposition;
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo> m_videoCompositionInfo;

	obs_display_t *m_display = nullptr;

	VisualElementsStateManager m_visualElementsState;

	std::vector<double> m_worldVerticalRulersX;
	std::vector<double> m_worldHorizontalRulersY;

public:
	StreamElementsVideoCompositionViewWidget(
		QWidget *parent,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition);
	~StreamElementsVideoCompositionViewWidget();

	void Destroy();

	void GetVideoBaseDimensions(uint32_t *videoWidth, uint32_t *videoHeight)
	{
		m_videoCompositionInfo->GetVideoBaseDimensions(videoWidth,
							       videoHeight);
	}

	void AddVerticalRulerX(double x)
	{
		m_worldVerticalRulersX.push_back(x);
	}

	void AddHorizontalRulerY(double y)
	{
		m_worldHorizontalRulersY.push_back(y);
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoComposition()
	{
		return m_videoComposition;
	}

	obs_scene_t *GetCurrentSceneRef()
	{
		if (m_videoComposition.get())
			return m_videoComposition->GetCurrentSceneRef();
		else
			return nullptr;
	}

private:
	void CreateDisplay();

	void OnMove();
	void OnDisplayChange();

protected:
	virtual void paintEvent(QPaintEvent *event) override;
	virtual void moveEvent(QMoveEvent *event) override;
	virtual void resizeEvent(QResizeEvent *event) override;
	virtual QPaintEngine *paintEngine() const override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	virtual bool nativeEvent(const QByteArray &, void *message,
				 qintptr *) override;
#else
	virtual bool nativeEvent(const QByteArray &, void *message,
				 long *) override;
#endif

public: // Used by ControlPoint
	double m_currMouseWidgetX = 0;
	double m_currMouseWidgetY = 0;
	double m_currMouseWorldX = 0;
	double m_currMouseWorldY = 0;
	bool m_currUnderMouse = false;

	bool m_mouseArmedForClickEvent = false;

	vec2 m_worldScale = {1.0f, 1.0f};
	vec2 m_worldPixelDensity = {1.0f, 1.0f};
	vec2 m_worldDimensions = {0.0f, 0.0f};

protected:

	void viewportToWorldCoords(uint32_t viewportX, uint32_t viewportY,
				   uint32_t *worldX, uint32_t *worldY);

	inline void viewportToWorldCoords(QPoint pos, uint32_t *worldX,
					  uint32_t *worldY)
	{
		viewportToWorldCoords(pos.x(), pos.y(), worldX, worldY);
	}

	inline void viewportToWorldCoords(QPointF pos, uint32_t *worldX,
					  uint32_t *worldY)
	{
		viewportToWorldCoords(pos.x(), pos.y(), worldX, worldY);
	}

	inline void viewportToWorldCoords(QMouseEvent *event, uint32_t *worldX,
					  uint32_t *worldY)
	{
		viewportToWorldCoords(event->localPos(), worldX, worldY);
	}

	virtual void mouseMoveEvent(QMouseEvent *event) override;
	virtual void mousePressEvent(QMouseEvent *event) override;
	virtual void mouseReleaseEvent(QMouseEvent *event) override;

	virtual void enterEvent(QEnterEvent *event) override {
		m_currMouseWidgetX = event->localPos().x();
		m_currMouseWidgetY = event->localPos().y();

		m_currUnderMouse = true;
	}
	virtual void leaveEvent(QEvent *event) override { m_currUnderMouse = false; }

private:
	static void obs_display_draw_callback(void *data, uint32_t cx,
					      uint32_t cy);
};
