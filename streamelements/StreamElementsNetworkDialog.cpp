#include "StreamElementsNetworkDialog.hpp"
#include "StreamElementsUtils.hpp"
#include "ui_StreamElementsNetworkDialog.h"

#ifdef WIN32
#include <io.h>
#else
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#endif
#include <fcntl.h>

#include <obs.h>
#include <util/platform.h>

#include <QCloseEvent>

#ifndef S_IWRITE
#define S_IWRITE 0
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

inline std::string GetCommandLineOptionValue(const std::string key)
{
	QStringList args = QCoreApplication::instance()->arguments();

	std::string search = "--" + key + "=";

	for (int i = 0; i < args.size(); ++i) {
		std::string arg = args.at(i).toStdString();

		if (arg.substr(0, search.size()) == search) {
			return arg.substr(search.size());
		}
	}

	return std::string();
}

StreamElementsNetworkDialog::StreamElementsNetworkDialog(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::StreamElementsNetworkDialog),
	  m_cancel_pending(false),
	  m_running(false),
	  m_taskQueue("StreamElementsNetworkDialog")
{
	ui->setupUi(this);

	// Disable close button and context help button
	this->setWindowFlags(
		((windowFlags() | Qt::CustomizeWindowHint) &
			~Qt::WindowCloseButtonHint &
			~Qt::WindowContextHelpButtonHint));
}

StreamElementsNetworkDialog::~StreamElementsNetworkDialog()
{
	delete ui;
}

struct local_context {
	StreamElementsNetworkDialog *self = nullptr;
	CURL *curl = nullptr;
	void *user_param = nullptr;
	void (*user_callback)(bool, void *) = nullptr;
	char *user_localFilePath = nullptr;
	char *user_url = nullptr;
	char *user_message = nullptr;
	int local_file_handle = -1;
	time_t last_progress_report_time = 0;
	size_t last_progress_report_percent = 0;
	bool large_file = false;
	curl_httppost *curl_httppost_data = nullptr;
};

void StreamElementsNetworkDialog::DownloadFileAsyncUpdateUserInterface(
	long dltotal, long dlnow)
{
	// Show if hidden
	if (!isVisible())
		show();

	ui->ctl_progressBar->setMaximum((int)dltotal);
	ui->ctl_progressBar->setValue((int)dlnow);
}

void StreamElementsNetworkDialog::UploadFileAsyncUpdateUserInterface(
	long dltotal, long dlnow)
{
	// Show if hidden
	if (!isVisible())
		show();

	ui->ctl_progressBar->setMaximum((int)dltotal);
	ui->ctl_progressBar->setValue((int)dlnow);
}

int StreamElementsNetworkDialog::DownloadFileAsyncXferProgressCallback(
	void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
	curl_off_t ulnow)
{
	UNUSED_PARAMETER(ultotal);
	UNUSED_PARAMETER(ulnow);

	// Async progress tracking
	local_context *task_context = (local_context *)clientp;

	time_t now = time(&now);

	time_t time_delta = now - task_context->last_progress_report_time;

	long percent = 0;
	if (dltotal > 0L) {
		percent = (long)(dlnow * 100L / dltotal);
	}
	size_t percent_delta =
		percent - task_context->last_progress_report_percent;

	// Show progress once per second
	bool show = time_delta >= 1 && percent_delta != 0L && dltotal > 32768L;
	// Or initially when downloading large file
	show |= task_context->large_file && !task_context->self->isVisible();

	if (show) {
		QMetaObject::invokeMethod(
			task_context->self,
			"DownloadFileAsyncUpdateUserInterface",
			Qt::QueuedConnection, Q_ARG(long, (long)100),
			Q_ARG(long, (long)percent));

		task_context->last_progress_report_time = now;
		task_context->last_progress_report_percent = percent;
	}

	if (!task_context->self->m_cancel_pending) {
		return (int)CURLE_OK;
	} else {
		task_context->self->m_cancel_pending = false;

		return (int)CURLE_ABORTED_BY_CALLBACK;
	}
}

int StreamElementsNetworkDialog::UploadFileAsyncXferProgressCallback(
	void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
	curl_off_t ulnow)
{
	UNUSED_PARAMETER(ultotal);
	UNUSED_PARAMETER(ulnow);

	// Async progress tracking
	local_context *task_context = (local_context *)clientp;

	if (!task_context->self->m_cancel_pending) {
		if (dltotal > 0 && dlnow >= dltotal) {
			// Defeat keep-alive

			// Upload complete and data was received
			return (int)CURLE_ABORTED_BY_CALLBACK;
		} else {
			return (int)CURLE_OK;
		}
	} else {
		task_context->self->m_cancel_pending = false;

		return (int)CURLE_ABORTED_BY_CALLBACK;
	}

	/*
	time_t now = time(&now);

	time_t time_delta = now - task_context->last_progress_report_time;

	long percent = 0;
	if (ultotal > 0L) {
		percent = (long)(ulnow * 100L / ultotal);
	}
	//size_t percent_delta = percent - task_context->last_progress_report_percent;

	// Show progress once per second
	bool show = time_delta >= 1 && ultotal > 32768L;
	// Or initially when downloading large file
	show |= task_context->large_file && !task_context->self->isVisible();


	if (show) {
		QMetaObject::invokeMethod(
			task_context->self,
			"UploadFileAsyncUpdateUserInterface",
			Qt::BlockingQueuedConnection,
			Q_ARG(long, (long)100),
			Q_ARG(long, (long)percent));

		task_context->last_progress_report_time = now;
		task_context->last_progress_report_percent = percent;
	}*/
}

static size_t write_callback(char *ptr, size_t size, size_t nmemb,
			     void *userdata)
{
	local_context *context = (local_context *)userdata;

	if (size * nmemb > 0)
		return write(context->local_file_handle, ptr, size * nmemb);
	else
		return 0;
}

void StreamElementsNetworkDialog::DownloadFileAsyncTask(void *task_arg)
{
	local_context *task_context = (local_context *)task_arg;

	QMetaObject::invokeMethod(task_context->self->ui->ctl_message,
				  "setText", Qt::QueuedConnection,
				  Q_ARG(QString,
					QString(task_context->user_message)));

	task_context->self->m_cancel_pending = false;

	QMetaObject::invokeMethod(task_context->self->ui->ctl_cancelButton,
				  "setEnabled", Qt::QueuedConnection,
				  Q_ARG(bool, true));

	bool result = false;

	task_context->curl = curl_easy_init();
	if (task_context->curl) {
		curl_easy_setopt(task_context->curl, CURLOPT_URL,
				 task_context->user_url);

		curl_easy_setopt(task_context->curl, CURLOPT_BUFFERSIZE,
				 512L * 1024L);

		curl_easy_setopt(task_context->curl, CURLOPT_FOLLOWLOCATION,
				 1L);
		curl_easy_setopt(task_context->curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(task_context->curl, CURLOPT_NOSIGNAL, 1L);

		curl_easy_setopt(task_context->curl, CURLOPT_XFERINFOFUNCTION,
				 DownloadFileAsyncXferProgressCallback);
		curl_easy_setopt(task_context->curl, CURLOPT_XFERINFODATA,
				 task_context);

		curl_easy_setopt(task_context->curl, CURLOPT_WRITEFUNCTION,
				 write_callback);
		curl_easy_setopt(task_context->curl, CURLOPT_WRITEDATA,
				 task_context);

		curl_easy_setopt(task_context->curl, CURLOPT_NOPROGRESS, 0L);

		SetGlobalCURLOptions(task_context->curl,
				     task_context->user_url);

		CURLcode res = curl_easy_perform(task_context->curl);

		if (CURLE_OK == res) {
			result = true;
		}

		curl_easy_cleanup(task_context->curl);
	}

	/*
	QMetaObject::invokeMethod(task_context->self, "hide",
				  Qt::QueuedConnection);
	*/

	if (result) {
		QMetaObject::invokeMethod(task_context->self, "accept",
					  Qt::QueuedConnection);
	} else {
		QMetaObject::invokeMethod(task_context->self, "hide",
					  Qt::QueuedConnection);
	}

	// Close open write file handle
	::close(task_context->local_file_handle);

	// Call the user-defined callback with user-defined parameter
	task_context->user_callback(result, task_context->user_param);

	// Free memory
	free(task_context->user_message);
	free(task_context->user_localFilePath);
	free(task_context->user_url);

	task_context->self->m_running = false;

	delete task_context;
}

void StreamElementsNetworkDialog::DownloadFileAsync(
	const char *localFilePath, const char *url, bool large_file,
	void (*callback)(bool, void *), void *param, const char *message)
{
	std::string localPath(localFilePath);
#ifdef WIN32
	int fd = _wopen(utf8_to_wstring(localPath).c_str(),
			O_WRONLY | O_BINARY,
			S_IWRITE /*_S_IREAD | _S_IWRITE*/);
#else
	int fd = ::open(localPath.c_str(),
			O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
#endif
	if (fd < 0) {
		callback(false, param);
		return;
	}

	ui->ctl_cancelButton->setEnabled(true);

	local_context *task_context = new local_context();

	task_context->self = this;
	task_context->user_param = param;
	task_context->user_callback = callback;
	task_context->last_progress_report_time = 0;
	task_context->last_progress_report_percent = 0;
	task_context->local_file_handle = fd;
	task_context->user_localFilePath = strdup(localFilePath);
	task_context->user_url = strdup(url);
	task_context->large_file = large_file;

	if (NULL != message)
		task_context->user_message = strdup(message);
	else
		task_context->user_message = strdup(url);

	task_context->self->m_cancel_pending = false;
	task_context->self->m_running = true;

	m_taskQueue.Enqueue(
		DownloadFileAsyncTask,
		task_context);
}

bool StreamElementsNetworkDialog::DownloadFile(const char *localFilePath,
					       const char *url, bool large_file,
					       const char *message)
{
	DownloadFileAsync(
		localFilePath, url, large_file, [](bool result, void *data) {},
		nullptr, message);

	return exec() == QDialog::Accepted;
}

void StreamElementsNetworkDialog::on_ctl_cancelButton_clicked()
{
	ui->ctl_cancelButton->setEnabled(false);

	m_cancel_pending = true;
}

bool StreamElementsNetworkDialog::UploadFile(const char *localFilePath,
					     const char *url,
					     const char *fieldName,
					     const char *message)
{
	local_context *task_context = new local_context();

	ui->ctl_progressBar->setMinimum(0);
	ui->ctl_progressBar->setMaximum(100);
	ui->ctl_progressBar->setValue(0);

	task_context->self = this;
	//task_context->user_param = param;
	//task_context->user_callback = callback;
	task_context->last_progress_report_time = 0;
	task_context->last_progress_report_percent = 0;
	//task_context->local_file_handle = os_fopen(localFilePath, "rb");
	task_context->user_localFilePath = strdup(localFilePath);
	task_context->user_url = strdup(url);
	task_context->large_file = true;

	// Add POST form fields
	curl_httppost *last_httppost_ptr = nullptr;
	curl_formadd(&task_context->curl_httppost_data, &last_httppost_ptr,
		     CURLFORM_COPYNAME, fieldName, CURLFORM_FILE, localFilePath,
		     CURLFORM_END);

	if (NULL != message)
		task_context->user_message = strdup(message);
	else
		task_context->user_message = strdup(url);

	task_context->self->m_cancel_pending = false;
	task_context->self->m_running = true;
	ui->ctl_cancelButton->setEnabled(true);

	m_taskQueue.Enqueue(
		[](void *task_arg) {
			local_context *task_context = (local_context *)task_arg;

			QMetaObject::invokeMethod(
				task_context->self->ui->ctl_message, "setText",
				Qt::QueuedConnection,
				Q_ARG(QString,
				      QString(task_context->user_message)));

			task_context->self->m_cancel_pending = false;

			QMetaObject::invokeMethod(
				task_context->self->ui->ctl_cancelButton,
				"setEnabled", Qt::QueuedConnection,
				Q_ARG(bool, true));

			/*
		if (task_context->large_file) {
			QMetaObject::invokeMethod(
				task_context->self->ui->ctl_cancelButton,
				"show",
				Qt::BlockingQueuedConnection,
				Q_ARG(bool, true));
		}*/

			bool result = false;

			task_context->curl = curl_easy_init();
			if (task_context->curl) {
				curl_easy_setopt(task_context->curl,
						 CURLOPT_URL,
						 task_context->user_url);
				curl_easy_setopt(
					task_context->curl, CURLOPT_HTTPPOST,
					task_context->curl_httppost_data);

				//curl_easy_setopt(task_context->curl, CURLOPT_BUFFERSIZE, 512L * 1024L);
				//curl_easy_setopt(task_context->curl, CURLOPT_BUFFERSIZE, 1);

				curl_easy_setopt(task_context->curl,
						 CURLOPT_FOLLOWLOCATION, 1L);
				curl_easy_setopt(task_context->curl,
						 CURLOPT_FAILONERROR, 1L);
				curl_easy_setopt(task_context->curl,
						 CURLOPT_NOSIGNAL, 1L);

				curl_easy_setopt(
					task_context->curl,
					CURLOPT_XFERINFOFUNCTION,
					UploadFileAsyncXferProgressCallback);
				curl_easy_setopt(task_context->curl,
						 CURLOPT_XFERINFODATA,
						 task_context);

				// curl_easy_setopt(task_context->curl, CURLOPT_READDATA, task_context->local_file_handle);

				curl_easy_setopt(task_context->curl,
						 CURLOPT_NOPROGRESS, 0L);

				SetGlobalCURLOptions(task_context->curl,
						     task_context->user_url);

				char *errorBuffer =
					new char[CURL_ERROR_SIZE + 1];
				curl_easy_setopt(task_context->curl,
						 CURLOPT_ERRORBUFFER,
						 errorBuffer);

				CURLcode res =
					curl_easy_perform(task_context->curl);

				if (CURLE_OK == res ||
				    CURLE_ABORTED_BY_CALLBACK == res) {
					result = true;
				} else {
					blog(LOG_WARNING,
					     "obs-browser: StreamElementsNetworkDialog: failed uploading file '%s' to '%s': %s",
					     task_context->user_localFilePath,
					     task_context->user_url,
					     errorBuffer);
				}

				delete[] errorBuffer;

				curl_easy_cleanup(task_context->curl);
			}

			//if (task_context->local_file_handle) {
			//	// Close open file handle
			//	fclose(task_context->local_file_handle);
			//}

			// Call the user-defined callback with user-defined parameter
			//task_context->user_callback(result, task_context->user_param);

			// Free memory
			if (task_context->curl_httppost_data != nullptr) {
				curl_formfree(task_context->curl_httppost_data);
			}

			free(task_context->user_message);
			free(task_context->user_localFilePath);
			free(task_context->user_url);

			task_context->self->m_cancel_pending = false;
			task_context->self->m_running = false;

			if (result) {
				QMetaObject::invokeMethod(task_context->self,
							  "accept",
							  Qt::QueuedConnection);
			} else {
				QMetaObject::invokeMethod(task_context->self,
							  "hide",
							  Qt::QueuedConnection);
			}

			delete task_context;
		},
		task_context);

	return exec() == QDialog::Accepted;
}

void StreamElementsNetworkDialog::reject()
{
	if (!m_running) {
		QDialog::reject();
	} else {
		m_cancel_pending = true;
	}
}
