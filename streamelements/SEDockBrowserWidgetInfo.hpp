#pragma once

#include <string>

//
// The last of part 2's five vocabulary types, and the only one that had to
// become a value.
//
// SEBrowserWidgetManager::GetDockBrowserWidgetInfo returns an owned
// DockBrowserWidgetInfo *, a class nested in the concrete manager and
// derived from SEWidgetManager::DockWidgetInfo. An interface cannot name
// it: a nested class cannot be forward-declared without its enclosing
// class, and including the enclosing header is what the interface exists
// to avoid. Unlike ObsOutputType, which only needed a home of its own,
// this one needed a different shape as well.
//
// So the contract carries this instead: the five fields the two container
// consumers actually read -- m_dockingArea and m_url by the handler's
// getCurrentContainerProperties, m_id, m_title and m_visible by
// SEAuxiliaryControlsBuilder's docks menu. The field names are the nested
// class's own, so the call sites change type and nothing else.
//
// What it deliberately leaves out: m_executeJavaScriptOnLoad and
// m_reloadPolicy, which nothing outside the manager reads, and the
// QWidget * that DockWidgetInfo keeps privately behind GetWidget() -- the
// one dirty member, and the reason the nested class could never be named
// on a clean header.
//
// Returning it by value also retires two `delete`s on a pointer handed
// out by a getter, which is a tidy-up rather than a fix: both callers do
// free it today.
//
class SEDockBrowserWidgetInfo {
public:
	std::string m_id;
	std::string m_title;
	bool m_visible = true;
	std::string m_dockingArea;
	std::string m_url;
};
