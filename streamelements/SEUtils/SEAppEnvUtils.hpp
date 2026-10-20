#pragma once

//
// The Qt application's own environment: command line, trace level, theme and
// stylesheet, resource strings, and window stacking.
//

#include <QWidget>

#include <Qt>

#include <string>

bool IsTraceLogLevel();

std::string DockWidgetAreaToString(const Qt::DockWidgetArea area);
std::string LoadResourceString(std::string path);

std::string GetCurrentThemeName();
std::string SerializeAppStyleSheet();
std::string GetAppStyleSheetSelectorContent(std::string selector);

bool IsAlwaysOnTop(QWidget *window);
void SetAlwaysOnTop(QWidget *window, bool enable);
