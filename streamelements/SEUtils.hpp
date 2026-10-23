#pragma once

#include "SETrace.hpp"

#include "SEUtils/SEStringUtils.hpp"
#include "SEUtils/SECryptoUtils.hpp"
#include "SEUtils/SEDiagnosticsUtils.hpp"
#include "SEUtils/SEEnvConfigUtils.hpp"
#include "SEUtils/SEPathUtils.hpp"
#include "SEUtils/SECancelableTask.hpp"
#include "SEUtils/SEHttpUtils.hpp"
#include "SEUtils/SEObsSceneUtils.hpp"
#include "SEUtils/SEObsDataUtils.hpp"
#include "SEUtils/SEObsSourceCatalogUtils.hpp"
#include "SEUtils/SEObsEncoderUtils.hpp"
#include "SEUtils/SELazyOBSVideoEncoderProvider.hpp"
#include "SEUtils/SEReentrancyUtils.hpp"
#include "SEUtils/SEAsyncTaskUtils.hpp"
#include "SEUtils/SELifecycleUtils.hpp"
#include "SEUtils/SESystemInfoUtils.hpp"
#include "SEUtils/SEAppEnvUtils.hpp"
#include "SEUtils/SEWidgetUtils.hpp"
#include "SEUtils/SEOverlayUrlUtils.hpp"

#include <cef-headers.hpp>
#include <obs.h>
#include <obs.hpp>
#include <obs-module.h>

#include <memory>
#include <iostream>
#include <mutex>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <map>

#include <functional>
#include <future>

#include <shared_mutex>

#include <util/threading.h>
#include <util/platform.h>
#include <curl/curl.h>

#include <QMenu>
#include <QDockWidget>
#include <QPointer>
#include <QWidget>

#include <QTimer>
#include <QApplication>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QWidget>
