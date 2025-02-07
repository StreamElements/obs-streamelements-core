#pragma once

#include <QDialog>

#include <curl/curl.h>

#include <util/config-file.h>

#include "../StreamElementsAsyncTaskQueue.hpp"

namespace Ui {
	class NetworkDialog;
}

///
// Async file download with UI
//
class NetworkDialog : public QDialog
{
	Q_OBJECT

public:
	explicit NetworkDialog(QWidget *parent = 0);
	~NetworkDialog();

private:
	Ui::NetworkDialog *ui;

private:
	StreamElementsAsyncTaskQueue m_taskQueue; // asynchronous tasks queue
	bool m_cancel_pending; // set to true to cancel current download

	///
	// curl transfer progress callback
	//
	static int DownloadFileAsyncXferProgressCallback(
		void *clientp,
		curl_off_t dltotal,
		curl_off_t dlnow,
		curl_off_t ultotal,
		curl_off_t ulnow);

public:
	///
	// Asynchronously download a file
	//
	// @param dest		download destination file path
	// @param url		source file URL
	// @param callback	function to call when done: callback(download_success, param)
	// @param param		second parameter to @callback
	// @param message	message to display while downloading
	//
	void DownloadFileAsync(
		const char* dest,
		const char* url,
		bool large_file,
		void(*callback)(bool, void*),
		void* param,
		const char* message);

	///
	// We need this instead of QDialog::hide() to make certain that
	// the dialog is set to non-modal when hidden.
	// This is necessary due to an issue in OBS Studio main code which
	// prevents OBS minimizing to system tray icon if a modal dialog
	// (even hidden) exists in the widget tree.
	//
	void HideDialog();

private slots:
	///
	// Called by DownloadFileAsyncXferProgressCallback on Qt UI thread to
	// update the user interface
	//
	void DownloadFileAsyncUpdateUserInterface(long dltotal, long dlnow);

	///
	// Handle user's click on "cancel" button
	//
	// Sets m_cancel_pending = true
	//
	void on_ctl_cancelButton_clicked();
};
