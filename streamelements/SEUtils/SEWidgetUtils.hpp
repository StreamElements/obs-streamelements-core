#pragma once

//
// Widgets, menus and actions built from a CefValue description, and the
// action invocation they trigger.
//
// These still reach SEGlobalStateManager::GetInstance(): the docks menu needs
// the widget, menu and analytics managers, and an invoked action needs the
// host API surface. Injecting them is phase 2 of this work; this file is a
// move.
//

#include <cef-headers.hpp>

#include <QMenu>
#include <QPixmap>
#include <QWidget>

#include <functional>

bool DeserializeAndInvokeAction(CefRefPtr<CefValue> input,
				std::function<void()> defaultAction,
				std::function<void()> defaultContextMenu);

bool DeserializeDocksMenu(QMenu &menu);

bool DeserializeMenu(
	CefRefPtr<CefValue> input, QMenu &menu,
	std::function<void()> defaultAction = []() {},
	std::function<void()> defaultContextMenu = []() {});

QWidget *DeserializeAuxiliaryControlWidget(
	CefRefPtr<CefValue> input,
	std::function<void()> defaultAction = []() {},
	std::function<void()> defaultContextMenu = []() {});

QWidget *DeserializeRemoteIconWidget(CefRefPtr<CefValue> input,
				     QPixmap *defaultPixmap = nullptr);

void DeserializeRevealFileInGraphicalShell(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);
