#include "StreamElementsBrowserWidgetManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include <QUuid>
#include <QMainWindow>
#include <QDockWidget>
#include <QSpacerItem>

#include <algorithm> // std::sort
#include <functional>
#include <regex>

#include <obs-module.h>

StreamElementsBrowserWidgetManager::StreamElementsBrowserWidgetManager(
	QMainWindow *parent)
	: StreamElementsWidgetManager(parent), m_notificationBarToolBar(nullptr)
{
}

StreamElementsBrowserWidgetManager::~StreamElementsBrowserWidgetManager() {}

static QDockWidget *GetSystemWidgetById(const char *widgetId)
{
	QMainWindow *main = (QMainWindow *)obs_frontend_get_main_window();

	if (!main)
		return nullptr;

	auto list = main->findChildren<QDockWidget *>();

	for (auto item : list) {
		auto name = QString(":") + item->objectName();

		if (name == widgetId)
			return item;
	}

	return nullptr;
}

void StreamElementsBrowserWidgetManager::PushCentralBrowserWidget(
	const char *const url, const char *const executeJavaScriptCodeOnLoad)
{
	if (!url) {
		return;
	}

	blog(LOG_INFO, "obs-streamelements-core: central widget: loading url: %s", url);

	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	StreamElementsBrowserWidget *widget = new StreamElementsBrowserWidget(
		nullptr, StreamElementsMessageBus::DEST_UI, url,
		executeJavaScriptCodeOnLoad, "reload", "center",
		CreateGloballyUniqueIdString().c_str(),
		std::make_shared<StreamElementsApiMessageHandler>("centralWidget"));

	PushCentralWidget(widget);
}

bool StreamElementsBrowserWidgetManager::DestroyCurrentCentralBrowserWidget()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return DestroyCurrentCentralWidget();
}

std::string StreamElementsBrowserWidgetManager::AddDockBrowserWidget(
	CefRefPtr<CefValue> input, std::string requestId)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	std::string id = CreateGloballyUniqueIdString();

	CefRefPtr<CefDictionaryValue> widgetDictionary = input->GetDictionary();

	if (!requestId.size()) {
		if (widgetDictionary->HasKey("id") &&
		    widgetDictionary->GetType("id") == VTYPE_STRING) {
			requestId =
				widgetDictionary->GetString("id").ToString();
		}
	}

	if (requestId.size() && !m_browserWidgets.count(requestId)) {
		id = requestId;
	}

	if (widgetDictionary.get()) {
		if (widgetDictionary->HasKey("title") &&
		    widgetDictionary->HasKey("url")) {
			std::string title;
			std::string url;
			std::string dockingAreaString = "floating";
			std::string executeJavaScriptOnLoad;
			std::string reloadPolicy = "reload";
			bool visible = true;
			QSizePolicy sizePolicy(QSizePolicy::Preferred,
					       QSizePolicy::Preferred);
			int requestWidth = 100;
			int requestHeight = 100;
			int minWidth = 0;
			int minHeight = 0;
			int left = -1;
			int top = -1;

#if QT_VERSION_MAJOR >= 6
			QRect rec = QApplication::primaryScreen()
					    ->availableGeometry();
#else
			QRect rec = QApplication::desktop()->screenGeometry();
#endif

			title = widgetDictionary->GetString("title");
			url = widgetDictionary->GetString("url");

			if (widgetDictionary->HasKey("reloadPolicy") &&
			    widgetDictionary->GetType("reloadPolicy") ==
				    VTYPE_STRING) {
				reloadPolicy = widgetDictionary->GetString(
					"reloadPolicy");
			}

			if (widgetDictionary->HasKey("dockingArea")) {
				dockingAreaString = widgetDictionary->GetString(
					"dockingArea");
			}

			if (widgetDictionary->HasKey(
				    "executeJavaScriptOnLoad")) {
				executeJavaScriptOnLoad =
					widgetDictionary->GetString(
						"executeJavaScriptOnLoad");
			}

			if (widgetDictionary->HasKey("visible")) {
				visible = widgetDictionary->GetBool("visible");
			}

			Qt::DockWidgetArea dockingArea = Qt::NoDockWidgetArea;

			if (dockingAreaString == "left") {
				dockingArea = Qt::LeftDockWidgetArea;
				sizePolicy.setVerticalPolicy(
					QSizePolicy::Expanding);
			} else if (dockingAreaString == "right") {
				dockingArea = Qt::RightDockWidgetArea;
				sizePolicy.setVerticalPolicy(
					QSizePolicy::Expanding);
			} else if (dockingAreaString == "top") {
				dockingArea = Qt::TopDockWidgetArea;
				sizePolicy.setHorizontalPolicy(
					QSizePolicy::Expanding);
			} else if (dockingAreaString == "bottom") {
				dockingArea = Qt::BottomDockWidgetArea;
				sizePolicy.setHorizontalPolicy(
					QSizePolicy::Expanding);
			} else {
				sizePolicy.setHorizontalPolicy(
					QSizePolicy::Expanding);
				sizePolicy.setVerticalPolicy(
					QSizePolicy::Expanding);
			}

			if (widgetDictionary->HasKey("minWidth")) {
				minWidth = widgetDictionary->GetInt("minWidth");
			}

			if (widgetDictionary->HasKey("width")) {
				requestWidth =
					widgetDictionary->GetInt("width");
			}

			if (widgetDictionary->HasKey("minHeight")) {
				minHeight =
					widgetDictionary->GetInt("minHeight");
			}

			if (widgetDictionary->HasKey("height")) {
				requestHeight =
					widgetDictionary->GetInt("height");
			}

			if (widgetDictionary->HasKey("left")) {
				left = widgetDictionary->GetInt("left");
			}

			if (widgetDictionary->HasKey("top")) {
				top = widgetDictionary->GetInt("top");
			}

			if (AddDockBrowserWidget(
				    id.c_str(), title.c_str(), url.c_str(),
				    executeJavaScriptOnLoad.c_str(),
				    reloadPolicy.c_str(), dockingArea)) {
				QDockWidget *widget = GetDockWidget(id.c_str());

				widget->setVisible(!visible);
				QApplication::sendPostedEvents();
				widget->setVisible(visible);
				QApplication::sendPostedEvents();

				//widget->setSizePolicy(sizePolicy);
				widget->widget()->setSizePolicy(sizePolicy);

				//widget->setMinimumSize(requestWidth, requestHeight);
				widget->widget()->setMinimumSize(requestWidth,
								 requestHeight);

				QApplication::sendPostedEvents();

				//widget->setMinimumSize(minWidth, minHeight);
				widget->widget()->setMinimumSize(minWidth,
								 minHeight);

				if (left >= 0 || top >= 0) {
					widget->move(left, top);
				}

				QApplication::sendPostedEvents();
			}

			return id;
		}
	}

	return "";
}

bool StreamElementsBrowserWidgetManager::AddDockBrowserWidget(
	const char *const id, const char *const title, const char *const url,
	const char *const executeJavaScriptCodeOnLoad,
	const char *const reloadPolicy, const Qt::DockWidgetArea area,
	const Qt::DockWidgetAreas allowedAreas,
	const QDockWidget::DockWidgetFeatures features)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	QMainWindow *main = new QMainWindow(nullptr);

	QAction *reloadAction =
		new QAction(QIcon(QPixmap(":/images/toolbar/reload.png")), "");
	QAction *floatAction =
		new QAction(QIcon(QPixmap(":/images/toolbar/dockToggle.png")), "");
	QAction *closeAction =
		new QAction(QIcon(QPixmap(":/images/toolbar/close.png")), "");

	QFont font;
	font.setStyleStrategy(QFont::PreferAntialias);

	reloadAction->setFont(font);
	floatAction->setFont(font);
	closeAction->setFont(font);

	StreamElementsBrowserWidget *widget = new StreamElementsBrowserWidget(
		nullptr, StreamElementsMessageBus::DEST_UI, url, executeJavaScriptCodeOnLoad, reloadPolicy,
		DockWidgetAreaToString(area).c_str(), id,
		std::make_shared<StreamElementsApiMessageHandler>("dockingWidget"));

	main->setCentralWidget(widget);

	std::string reloadPolicyCopy = reloadPolicy;

	reloadAction->connect(reloadAction, &QAction::triggered,
			      [widget, reloadPolicyCopy] {
				      if (reloadPolicyCopy == "navigate") {
					      widget->BrowserLoadInitialPage();
				      } else {
					      widget->BrowserReload(true);
				      }
			      });

	if (AddDockWidget(id, title, main, area, allowedAreas, features)) {
		m_browserWidgets[id] = widget;

		QDockWidget *dock = GetDockWidget(id);

		return true;
	} else {
		return false;
	}
}

bool StreamElementsBrowserWidgetManager::ToggleWidgetFloatingStateById(
	const char *const id)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return StreamElementsWidgetManager::ToggleWidgetFloatingStateById(id);
}

bool StreamElementsBrowserWidgetManager::SetWidgetDimensionsById(
	const char *const id, const int width, const int height)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return StreamElementsWidgetManager::SetWidgetDimensionsById(id, width,
								    height);
}

bool StreamElementsBrowserWidgetManager::SetWidgetPositionById(
	const char *const id, const int left, const int top)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return StreamElementsWidgetManager::SetWidgetPositionById(id, left,
								  top);
}

bool StreamElementsBrowserWidgetManager::SetWidgetUrlById(const char *const id,
							  const char *const url)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!m_browserWidgets.count(id))
		return false;

	m_browserWidgets[id]->BrowserLoadInitialPage(url);

	return true;
}

void StreamElementsBrowserWidgetManager::RemoveAllDockWidgets()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	while (m_browserWidgets.size()) {
		RemoveDockWidget(m_browserWidgets.begin()->first.c_str());
	}
}

bool StreamElementsBrowserWidgetManager::RemoveDockWidget(const char *const id)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (StreamElementsWidgetManager::RemoveDockWidget(id)) {
		if (m_browserWidgets.count(id)) {
			m_browserWidgets.erase(id);

			return true;
		}
	}

	return false;
}

void StreamElementsBrowserWidgetManager::GetDockBrowserWidgetIdentifiers(
	std::vector<std::string> &result)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return GetDockWidgetIdentifiers(result);
}

StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo *
StreamElementsBrowserWidgetManager::GetDockBrowserWidgetInfo(
	const char *const id)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	StreamElementsBrowserWidgetManager::DockWidgetInfo *baseInfo =
		GetDockWidgetInfo(id);

	if (!baseInfo) {
		return nullptr;
	}

	StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo *result =
		new StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo(
			*baseInfo);

	delete baseInfo;

	result->m_url = m_browserWidgets[id]->GetStartUrl();

	result->m_executeJavaScriptOnLoad =
		m_browserWidgets[id]->GetExecuteJavaScriptCodeOnLoad();

	result->m_reloadPolicy = m_browserWidgets[id]->GetReloadPolicy();

	return result;
}

void StreamElementsBrowserWidgetManager::SerializeDockingWidgets(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	CefRefPtr<CefDictionaryValue> rootDictionary =
		CefDictionaryValue::Create();
	output->SetDictionary(rootDictionary);

	std::vector<std::string> widgetIdentifiers;

	GetDockBrowserWidgetIdentifiers(widgetIdentifiers);

	for (auto id : widgetIdentifiers) {
		CefRefPtr<CefValue> widgetValue = CefValue::Create();
		CefRefPtr<CefDictionaryValue> widgetDictionary =
			CefDictionaryValue::Create();
		widgetValue->SetDictionary(widgetDictionary);

		StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo *info =
			GetDockBrowserWidgetInfo(id.c_str());

		if (info) {
			widgetDictionary->SetString("id", id);
			widgetDictionary->SetString("url", info->m_url);
			widgetDictionary->SetString("dockingArea",
						    info->m_dockingArea);
			widgetDictionary->SetString("title", info->m_title);
			widgetDictionary->SetString(
				"executeJavaScriptOnLoad",
				info->m_executeJavaScriptOnLoad);
			widgetDictionary->SetString("reloadPolicy",
						    info->m_reloadPolicy);
			widgetDictionary->SetBool("visible", info->m_visible);

			QDockWidget *widget = GetDockWidget(id.c_str());

			QSize minSize = widget->widget()->minimumSize();
			QSize size = widget->size();

			widgetDictionary->SetInt("minWidth", minSize.width());
			widgetDictionary->SetInt("minHeight", minSize.height());

			widgetDictionary->SetInt("width", size.width());
			widgetDictionary->SetInt("height", size.height());

			widgetDictionary->SetInt("left", widget->pos().x());
			widgetDictionary->SetInt("top", widget->pos().y());
		}

		rootDictionary->SetValue(id, widgetValue);
	}
}

void StreamElementsBrowserWidgetManager::DeserializeDockingWidgets(
	CefRefPtr<CefValue> &input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (!input.get()) {
		return;
	}

	CefRefPtr<CefDictionaryValue> rootDictionary = input->GetDictionary();

	CefDictionaryValue::KeyList rootDictionaryKeys;

	if (!rootDictionary->GetKeys(rootDictionaryKeys)) {
		return;
	}

	// 1. Build maps:
	//	area -> start coord -> vector of ids
	//	id -> secondary start coord
	//	id -> minimum size
	//
	typedef std::vector<std::string> id_arr_t;
	typedef std::map<int, id_arr_t> start_to_ids_map_t;
	typedef std::map<std::string, start_to_ids_map_t> docks_map_t;

	docks_map_t docksMap;
	std::map<std::string, int> secondaryStartMap;

	for (auto id : rootDictionaryKeys) {
		CefRefPtr<CefValue> widgetValue = rootDictionary->GetValue(id);

		if (widgetValue->GetDictionary()->HasKey("dockingArea")) {
			std::string area = widgetValue->GetDictionary()
						   ->GetString("dockingArea")
						   .ToString();

			int start = -1;
			int secondary = -1;

			if (widgetValue->GetDictionary()->HasKey("left") &&
			    widgetValue->GetDictionary()->HasKey("top")) {
				if ((area == "left" || area == "right")) {
					start = widgetValue->GetDictionary()
							->GetInt("top");
					secondary = widgetValue->GetDictionary()
							    ->GetInt("left");
				} else if ((area == "top" ||
					    area == "bottom")) {
					start = widgetValue->GetDictionary()
							->GetInt("left");
					secondary = widgetValue->GetDictionary()
							    ->GetInt("top");
				}
			}

			docksMap[area][start].push_back(id.ToString());
			secondaryStartMap[id.ToString()] = secondary;
		}
	}

	// 2. For each area
	//
	for (docks_map_t::iterator areaPair = docksMap.begin();
	     areaPair != docksMap.end(); ++areaPair) {
		std::string area = areaPair->first;

		for (start_to_ids_map_t::iterator startPair =
			     areaPair->second.begin();
		     startPair != areaPair->second.end(); ++startPair) {
			//int start = startPair->first;
			id_arr_t dockIds = startPair->second;

			// 3. Sort dock IDs by secondary start coord
			std::sort(dockIds.begin(), dockIds.end(),
				  [&](std::string i, std::string j) -> bool {
					  return secondaryStartMap[i] <
						 secondaryStartMap[j];
				  });

			std::map<std::string, QSize> idToMinSizeMap;

			// 4. For each dock Id
			//
			for (int i = 0; i < dockIds.size(); ++i) {
				CefRefPtr<CefValue> widgetValue =
					rootDictionary->GetValue(dockIds[i]);

				// 5. Create docking widget
				//
				dockIds[i] = AddDockBrowserWidget(widgetValue,
								  dockIds[i]);

				/*
				QDockWidget* prev = i > 0 ? GetDockWidget(dockIds[i - 1].c_str()) : nullptr;
				QDockWidget* curr = GetDockWidget(dockIds[i].c_str());

				idToMinSizeMap[dockIds[i]] = curr->widget()->minimumSize();

				if (prev && curr) {
					// 6. If it's not the first: split dock widgets which should occupy the
					//    same space, and set previous minimum size
					//
					if (area == "left" || area == "right") {
						mainWindow()->splitDockWidget(prev, curr, Qt::Horizontal);

					}
					else if (area == "top" || area == "bottom") {
						mainWindow()->splitDockWidget(prev, curr, Qt::Vertical);
					}

					QApplication::sendPostedEvents();
					prev->setMinimumSize(idToMinSizeMap[dockIds[i - 1]]);
					prev->widget()->setMinimumSize(idToMinSizeMap[dockIds[i - 1]]);
					QApplication::sendPostedEvents();
				}
				*/
			}
		}
	}
}

bool StreamElementsBrowserWidgetManager::GroupDockingWidgetPairByIds(
	const char *firstId, const char *secondId)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	QDockWidget *first = GetDockWidget(firstId);
	QDockWidget *second = GetDockWidget(secondId);

	if (!first)
		first = GetSystemWidgetById(firstId);

	if (!second)
		second = GetSystemWidgetById(secondId);

	if (!first || !second)
		return false;

	QMainWindow *main = (QMainWindow *)obs_frontend_get_main_window();

	if (!main)
		return false;

	main->tabifyDockWidget(first, second);

	return true;
}

bool StreamElementsBrowserWidgetManager::InsertDockingWidgetRelativeToId(
		const char* firstId, const char* secondId, const bool isBefore)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	QDockWidget *first = GetDockWidget(firstId);
	QDockWidget *second = GetDockWidget(secondId);

	if (!first)
		first = GetSystemWidgetById(firstId);

	if (!second)
		second = GetSystemWidgetById(secondId);

	if (!first || !second)
		return false;

	if (!first->parent())
		return false;

	if (!second->parent())
		return false;

	QMainWindow *main = (QMainWindow *)obs_frontend_get_main_window();

	if (!main)
		return false;

	auto area = main->dockWidgetArea(first);

	if (area == Qt::NoDockWidgetArea)
		return false;

	auto list = main->findChildren<QDockWidget *>();

	QList<std::shared_ptr<QList<QDockWidget *>>> result;
	bool isFound = false;
	int prevListSize = 0;

	std::map<std::string, bool> visited;

	auto getTabified = [&](QDockWidget *dock) {
		auto result = std::make_shared<QList<QDockWidget *>>();

		auto tabified = main->tabifiedDockWidgets(dock);

		// If dock is not part of a group, and is our dock to insert, count it as a dock which is rearranged, and not inserted
		if (dock == second && tabified.size() == 0 &&
		    !dock->isFloating() && dock->isVisible())
			++prevListSize;

		if (dock != second && visited.count(dock->objectName().toStdString()) == 0 && !dock->isFloating() && dock->isVisible()) {
			result->push_back(dock);

			visited[dock->objectName().toStdString()] = true;
		}

		for (auto item : tabified) {
			if (item != second &&
			    visited.count(item->objectName().toStdString()) ==
				    0 &&
			    !item->isFloating() && item->isVisible()) {
				result->push_back(item);

				visited[item->objectName().toStdString()] =
					true;
			}
		}

		return result;
	};

	auto hasItem = [&](std::shared_ptr<QList<QDockWidget *>> where,
			   QDockWidget *dock) {
		return where->count(dock) > 0;
	};

	auto visibleDockedCount =
		[&](std::shared_ptr<QList<QDockWidget *>> where) {
			int count = 0;
			for (auto item : *where) {
				if (item->isFloating())
					continue;

				if (!item->isVisible())
					continue;

				++count;
			}
			return count;
		};

	auto secondItemList = std::make_shared<QList<QDockWidget *>>();
	secondItemList->push_back(second);

	for (auto item : list) {
		if (main->dockWidgetArea(item) != area)
			continue;

		auto tabified = getTabified(item);

		if (visibleDockedCount(tabified) == 0)
			continue;

		++prevListSize;

		if (item == second)
			continue;

		if (hasItem(tabified, first)) {
			isFound = true;

			if (!isBefore) {
				result.push_back(tabified);
			}

			result.push_back(secondItemList);

			if (isBefore) {
				result.push_back(tabified);
			}
		} else {
			result.push_back(tabified);
		}
	}

	if (!isFound) {
		if (isBefore) {
			result.push_front(secondItemList);
		} else {
			result.push_back(secondItemList);
		}
	}

	struct attr {
		QDockWidget *dock;
		QSize minSize;
		QSize maxSize;
		QSize sizeHint;
		int width;
		int height;
		bool visible;
	};

	std::vector<std::shared_ptr<attr>> props;

	// Remove all items first
	for (auto group : result) {
		auto item = group->at(0);
		auto key = item->objectName();

		auto p = std::make_shared<attr>();
		p->dock = item;
		p->minSize = item->minimumSize();
		p->maxSize = item->maximumSize();
		p->sizeHint = item->sizeHint();
		p->width = item->width();
		p->height = item->height();
		p->visible = item->isVisible();

		props.push_back(p);

		for (auto dock : *group) {
			main->removeDockWidget(dock);
		}
	}

	if (result.size() != prevListSize) {
		// Recalculate sizes
		// When untabifying, take into account the space that we have to free

		int targetTotalDim = 0;
		int totalDim = 0;
		int totalDimDelta = 0;
		int canChangeCount = 0;

		auto get = [&](size_t index) {
			auto item = props.at(index);

			if (area == Qt::TopDockWidgetArea ||
			    area == Qt::BottomDockWidgetArea) {
				return item->width;
			}

			if (area == Qt::LeftDockWidgetArea ||
			    area == Qt::RightDockWidgetArea) {
				return item->height;
			}

			return 0;
		};

		auto canChange = [&](size_t index, int direction) {
			auto item = props.at(index);

			if (area == Qt::TopDockWidgetArea ||
			    area == Qt::BottomDockWidgetArea) {
				if (direction < 0) {
					return item->width >
					       item->minSize.width();
				} else if (direction > 0) {
					return item->width <
					       item->maxSize.width();
				}
			}

			if (area == Qt::LeftDockWidgetArea ||
			    area == Qt::RightDockWidgetArea) {
				if (direction < 0) {
					return item->height >
					       item->minSize.height();
				} else if (direction > 0) {
					return item->height <
					       item->maxSize.height();
				}
			}

			return false;
		};

		auto set = [&](size_t index, int newVal) {
			auto item = props.at(index);

			if (area == Qt::TopDockWidgetArea ||
			    area == Qt::BottomDockWidgetArea) {
				int prev = item->width;

				//item->width = std::min(
				//	std::max(item->minSize.width(), newVal),
				//	item->maxSize.width());

				item->width = newVal;

				return item->width != prev;
			}

			if (area == Qt::LeftDockWidgetArea ||
			    area == Qt::RightDockWidgetArea) {
				int prev = item->height;

				//item->height = std::min(
				//	std::max(item->minSize.height(),
				//		 newVal),
				//	item->maxSize.height());

				item->height = newVal;

				return item->height != prev;
			}

			return false;
		};

		auto recalc = [&]() {
			for (size_t i = 0; i < props.size(); ++i) {
				int val = get(i);

				totalDim += val;

				if (props.at(i)->dock != second)
					targetTotalDim += val;
			}

			totalDimDelta = targetTotalDim - totalDim;

			canChangeCount = 0;

			for (size_t i = 0; i < props.size(); ++i) {
				if (!canChange(i, totalDimDelta))
					continue;

				++canChangeCount;	
			}
		};

		recalc();

		for (size_t i = 0; i < props.size(); ++i) {
			set(i, targetTotalDim / props.size());
		}
	}

	// Add items back in their order, tabifying as necessary
	for (auto group : result) {
		main->addDockWidget(area, group->at(0));
		group->at(0)->setFloating(false);

		for (size_t i = 1; i < group->size(); ++i) {
			main->addDockWidget(area, group->at(i));
			group->at(i)->setFloating(false);
			main->tabifyDockWidget(group->at(0), group->at(i));
		}
	}

	// Set size
	for (size_t key = 0; key < result.size(); ++key) {
		auto group = result.at(key);

		for (auto item : *group) {
			int width = props[key]->width;
			int height = props[key]->height;

			if (area == Qt::TopDockWidgetArea ||
			    area == Qt::BottomDockWidgetArea) {
				item->setMinimumWidth(width);
				item->setMaximumWidth(width);
			}

			if (area == Qt::LeftDockWidgetArea ||
			    area == Qt::RightDockWidgetArea) {
				item->setMinimumHeight(height);
				item->setMaximumHeight(height);
			}

			item->setVisible(props[key]->visible);
		}
	}

	//QApplication::sendPostedEvents();

	// Restore original size constraints
	for (size_t key = 0; key < result.size(); ++key) {
		auto group = result.at(key);

		for (auto item : *group) {
			if (area == Qt::TopDockWidgetArea ||
			    area == Qt::BottomDockWidgetArea) {
				item->setMinimumWidth(props[key]->minSize.width());
				item->setMaximumWidth(props[key]->maxSize.width());
			}

			if (area == Qt::LeftDockWidgetArea ||
			    area == Qt::RightDockWidgetArea) {
				item->setMinimumHeight(
					props[key]->minSize.height());
				item->setMaximumHeight(
					props[key]->maxSize.height());
			}
		}
	}

	QApplication::sendPostedEvents();

	return true;
}

void StreamElementsBrowserWidgetManager::ShowNotificationBar(
	const char *const url, const int height,
	const char *const executeJavaScriptCodeOnLoad)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	HideNotificationBar();

	m_notificationBarBrowserWidget = new StreamElementsBrowserWidget(
		nullptr, StreamElementsMessageBus::DEST_UI, url, executeJavaScriptCodeOnLoad, "reload", "notification",
		CreateGloballyUniqueIdString().c_str(),
		std::make_shared<StreamElementsApiMessageHandler>(
			"notificationBar"));

	const Qt::ToolBarArea NOTIFICATION_BAR_AREA = Qt::TopToolBarArea;

	m_notificationBarToolBar = new QToolBar(mainWindow());
	m_notificationBarToolBar->setAutoFillBackground(true);
	m_notificationBarToolBar->setAllowedAreas(NOTIFICATION_BAR_AREA);
	m_notificationBarToolBar->setFloatable(false);
	m_notificationBarToolBar->setMovable(false);
	m_notificationBarToolBar->setMinimumHeight(height);
	m_notificationBarToolBar->setMaximumHeight(height);
	m_notificationBarToolBar->setLayout(new QVBoxLayout());
	m_notificationBarToolBar->addWidget(m_notificationBarBrowserWidget);
	mainWindow()->addToolBar(NOTIFICATION_BAR_AREA,
				 m_notificationBarToolBar);
}

void StreamElementsBrowserWidgetManager::HideNotificationBar()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_notificationBarToolBar) {
		m_notificationBarToolBar->setVisible(false);

		QApplication::sendPostedEvents();

		mainWindow()->removeToolBar(m_notificationBarToolBar);

		m_notificationBarToolBar->deleteLater();

		m_notificationBarToolBar = nullptr;
		m_notificationBarBrowserWidget = nullptr;
	}
}

bool StreamElementsBrowserWidgetManager::HasNotificationBar()
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	return !!m_notificationBarToolBar;
}

void StreamElementsBrowserWidgetManager::SerializeNotificationBar(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (m_notificationBarToolBar) {
		CefRefPtr<CefDictionaryValue> rootDictionary =
			CefDictionaryValue::Create();
		output->SetDictionary(rootDictionary);

		rootDictionary->SetString(
			"url", m_notificationBarBrowserWidget->GetStartUrl());
		rootDictionary->SetString(
			"executeJavaScriptOnLoad",
			m_notificationBarBrowserWidget
				->GetExecuteJavaScriptCodeOnLoad());
		rootDictionary->SetInt(
			"height", m_notificationBarToolBar->size().height());
	} else {
		output->SetNull();
	}
}

void StreamElementsBrowserWidgetManager::DeserializeNotificationBar(
	CefRefPtr<CefValue> &input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	if (input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> rootDictionary =
			input->GetDictionary();

		int height = 100;
		std::string url = "about:blank";
		std::string executeJavaScriptOnLoad = "";

		if (rootDictionary->HasKey("height")) {
			height = rootDictionary->GetInt("height");
		}

		if (rootDictionary->HasKey("url")) {
			url = rootDictionary->GetString("url").ToString();
		}

		if (rootDictionary->HasKey("executeJavaScriptOnLoad")) {
			executeJavaScriptOnLoad =
				rootDictionary
					->GetString("executeJavaScriptOnLoad")
					.ToString();
		}

		ShowNotificationBar(url.c_str(), height,
				    executeJavaScriptOnLoad.c_str());
	}
}

void StreamElementsBrowserWidgetManager::SerializeNotificationBar(
	std::string &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	CefRefPtr<CefValue> root = CefValue::Create();

	SerializeNotificationBar(root);

	// Convert data to JSON
	CefString jsonString = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	output = jsonString.ToString();
}

void StreamElementsBrowserWidgetManager::DeserializeNotificationBar(
	std::string &input)
{
	std::lock_guard<std::recursive_mutex> guard(m_mutex);

	// Convert JSON string to CefValue
	CefRefPtr<CefValue> root = CefParseJSON(
		CefString(input), JSON_PARSER_ALLOW_TRAILING_COMMAS);

	DeserializeNotificationBar(root);
}
