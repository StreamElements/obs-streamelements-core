#pragma once

#include "StreamElementsVideoCompositionManager.hpp"
#include <QWidget>
#include <QMouseEvent>
#include <QEnterEvent>

class StreamElementsVideoCompositionViewWidget : public QWidget, public StreamElementsCompositionEventListener
{
public:
	class VisualElementBase {
	public:
		VisualElementBase() {}
		~VisualElementBase() {}

	public:
		virtual void Draw() = 0;

		virtual bool HandleMouseDown(QMouseEvent *event, double worldX, double worldY) { return false; }
		virtual bool HandleMouseUp(QMouseEvent *event, double worldX, double worldY) { return false; }
		virtual bool HandleMouseClick(QMouseEvent *event, double worldX, double worldY) { return false; }
		virtual bool HandleMouseMove(QMouseEvent *event, double worldX, double worldY) { return false; }
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

		virtual bool HandleMouseDown(QMouseEvent *event, double worldX,
					     double worldY)
		{
			for (auto element : m_topLayer) {
				if (element->HandleMouseDown(event, worldX,
							     worldY))
					return true;
			}

			return false;
		}

		virtual bool HandleMouseUp(QMouseEvent *event, double worldX,
					   double worldY)
		{
			for (auto element : m_topLayer) {
				if (element->HandleMouseUp(event, worldX,
							   worldY))
					return true;
			}

			return false;
		}

		virtual bool HandleMouseClick(QMouseEvent *event, double worldX,
					      double worldY)
		{
			for (auto element : m_topLayer) {
				if (element->HandleMouseClick(event, worldX,
							      worldY))
					return true;
			}

			return false;
		}

		virtual bool HandleMouseMove(QMouseEvent *event, double worldX,
					     double worldY)
		{
			for (auto element : m_topLayer) {
				if (element->HandleMouseMove(event, worldX,
							     worldY))
					return true;
			}

			return false;
		}
	};

	static class VisualElementsStateManager {
	private:
		StreamElementsVideoCompositionViewWidget* m_view;
		obs_scene_t *m_scene;
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

		void UpdateAndDraw(obs_scene_t *scene, double worldWidth,
				   double worldHeight);

		virtual bool HandleMouseDown(QMouseEvent *event, double worldX,
					     double worldY)
		{
			for (auto sceneItem :
			     m_sceneItemsEventProcessingOrder) {
				if (m_sceneItemsVisualElementsMap[sceneItem]
					    ->HandleMouseDown(event, worldX,
							       worldY))
					return true;
			}

			return false;
		}

		virtual bool HandleMouseUp(QMouseEvent *event, double worldX,
					   double worldY)
		{
			for (auto sceneItem :
			     m_sceneItemsEventProcessingOrder) {
				if (m_sceneItemsVisualElementsMap[sceneItem]
					    ->HandleMouseUp(event, worldX,
							    worldY))
					return true;
			}

			return false;
		}

		virtual bool HandleMouseClick(QMouseEvent *event, double worldX,
					      double worldY)
		{
			for (auto sceneItem :
			     m_sceneItemsEventProcessingOrder) {
				if (m_sceneItemsVisualElementsMap[sceneItem]
					    ->HandleMouseClick(event, worldX,
								worldY))
					return true;
			}

			return false;
		}

		virtual bool HandleMouseMove(QMouseEvent *event, double worldX,
					     double worldY)
		{
			for (auto sceneItem :
			     m_sceneItemsEventProcessingOrder) {
				if (m_sceneItemsVisualElementsMap[sceneItem]
					    ->HandleMouseMove(event, worldX,
							       worldY))
					return true;
			}

			return false;
		}
	};


private:
	std::shared_ptr<StreamElementsVideoCompositionBase> m_videoComposition;
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo> m_videoCompositionInfo;

	obs_display_t *m_display = nullptr;

	VisualElementsStateManager m_visualElementsState;

public:
	StreamElementsVideoCompositionViewWidget(
		QWidget *parent,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition);
	~StreamElementsVideoCompositionViewWidget();

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
