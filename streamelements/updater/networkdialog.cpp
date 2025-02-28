#include "networkdialog.hpp"
#include "ui_networkdialog.h"

#include <obs.h>
#include <util/platform.h>

#include "../StreamElementsUtils.hpp"

NetworkDialog::NetworkDialog(QWidget *parent)
	: QDialog(parent),
	  ui(new Ui::NetworkDialog),
	  m_cancel_pending(false),
	  m_taskQueue("Updater NetworkDialog")
{
	ui->setupUi(this);

	// Disable close button and context help button
	this->setWindowFlags(((windowFlags() | Qt::CustomizeWindowHint) &
			      ~Qt::WindowCloseButtonHint &
			      ~Qt::WindowContextHelpButtonHint));

	setModal(false);
}

NetworkDialog::~NetworkDialog()
{
	delete ui;
}

struct local_context {
	NetworkDialog* self;
	CURL* curl;
	void* user_param;
	void(*user_callback)(bool, void*);
	char* user_dest;
	char* user_url;
	char* user_message;
	FILE* output_file_handle;
	time_t last_progress_report_time;
	size_t last_progress_report_percent;
	bool large_file;
};

void NetworkDialog::DownloadFileAsyncUpdateUserInterface(long dltotal, long dlnow)
{
	// Show if hidden
	if (!isVisible()) {
		setModal(true);

		show();
	}

	ui->ctl_progressBar->setMaximum((int)dltotal);
	ui->ctl_progressBar->setValue((int)dlnow);
}

int NetworkDialog::DownloadFileAsyncXferProgressCallback(
	void *clientp,
	curl_off_t dltotal,
	curl_off_t dlnow,
	curl_off_t ultotal,
	curl_off_t ulnow)
{
	// Async progress tracking
	local_context* task_context =
		(local_context*)clientp;

	time_t now = time(&now);

	time_t time_delta = now - task_context->last_progress_report_time;

	long percent = 0;
	if (dltotal > 0L) {
		percent = (long)(dlnow * 100L / dltotal);
	}
	size_t percent_delta = percent - task_context->last_progress_report_percent;

	// Show progress once per second
	bool show = time_delta >= 1 && percent_delta != 0L && dltotal > 32768L;
	// Or initially when downloading large file
	show |= task_context->large_file && !task_context->self->isVisible();

	if (show) {
			QMetaObject::invokeMethod(
			task_context->self,
			"DownloadFileAsyncUpdateUserInterface",
			Qt::QueuedConnection,
			Q_ARG(long, (long)100),
			Q_ARG(long, (long)percent));

		task_context->last_progress_report_time = now;
		task_context->last_progress_report_percent = percent;
	}

	if (!task_context->self->m_cancel_pending) {
		return (int)CURLE_OK;
	}
	else {
		task_context->self->m_cancel_pending = false;

		return (int)CURLE_ABORTED_BY_CALLBACK;
	}
}

void NetworkDialog::DownloadFileAsync(const char* dest, const char* url, bool large_file, void(*callback)(bool, void*), void* param, const char* message)
{
	local_context* task_context = new local_context();

	task_context->self = this;
	task_context->user_param = param;
	task_context->user_callback = callback;
	task_context->last_progress_report_time = 0;
	task_context->last_progress_report_percent = 0;
	task_context->output_file_handle = os_fopen(dest, "wb");
	task_context->user_dest = strdup(dest);
	task_context->user_url = strdup(url);
	task_context->large_file = large_file;

	if (NULL != message)
		task_context->user_message = strdup(message);
	else
		task_context->user_message = strdup(url);

	m_taskQueue.Enqueue(
		[](void* task_arg) {
			local_context* task_context =
				(local_context*)task_arg;

			QMetaObject::invokeMethod(
				task_context->self->ui->ctl_message,
				"setText",
				Qt::QueuedConnection,
				Q_ARG(QString, QString(task_context->user_message)));

			task_context->self->m_cancel_pending = false;

			QMetaObject::invokeMethod(
				task_context->self->ui->ctl_cancelButton,
				"setEnabled",
				Qt::QueuedConnection,
				Q_ARG(bool, true));

			bool result = false;

			if (NULL != task_context->output_file_handle) {
				task_context->curl = curl_easy_init();
				if (task_context->curl) {
					curl_easy_setopt(task_context->curl, CURLOPT_URL, task_context->user_url);

					curl_easy_setopt(task_context->curl, CURLOPT_BUFFERSIZE, 512L * 1024L);

					curl_easy_setopt(task_context->curl, CURLOPT_FOLLOWLOCATION, 1L);
					curl_easy_setopt(task_context->curl, CURLOPT_FAILONERROR, 1L);
					curl_easy_setopt(task_context->curl, CURLOPT_NOSIGNAL, 1L);

					curl_easy_setopt(task_context->curl, CURLOPT_XFERINFOFUNCTION,
						DownloadFileAsyncXferProgressCallback);
					curl_easy_setopt(task_context->curl, CURLOPT_XFERINFODATA,
						task_context);

					// curl_easy_setopt(task_context->curl, CURLOPT_WRITEFUNCTION, NULL);
					curl_easy_setopt(task_context->curl, CURLOPT_WRITEDATA, task_context->output_file_handle);

					curl_easy_setopt(task_context->curl, CURLOPT_NOPROGRESS, 0L);

					SetGlobalCURLOptions(task_context->curl, task_context->user_url);

					CURLcode res = curl_easy_perform(task_context->curl);

					if (CURLE_OK == res) {
                        long response_code;
                        curl_easy_getinfo(task_context->curl, CURLINFO_HTTP_CODE, &response_code);
                        
                        blog(LOG_INFO, "obs-streamelements: DownloadFileAsync: %lu ('%s')", response_code, task_context->user_url);

                        if (response_code >= 200 && response_code <= 299) {
                            result = true;
                        }
					}

					curl_easy_cleanup(task_context->curl);
				}

				task_context->self->HideDialog();

				// Close open write file handle
				fclose(task_context->output_file_handle);

				// Call the user-defined callback with user-defined parameter
				task_context->user_callback(result, task_context->user_param);
			}

			// Free memory
			free(task_context->user_message);
			free(task_context->user_dest);
			free(task_context->user_url);

			delete task_context;
		},
		task_context);
}

void NetworkDialog::on_ctl_cancelButton_clicked()
{
	ui->ctl_cancelButton->setEnabled(false);

	m_cancel_pending = true;
}

void NetworkDialog::HideDialog()
{
	QMetaObject::invokeMethod(this,
				  "hide", Qt::QueuedConnection);

	QMetaObject::invokeMethod(this, "setModal", Qt::QueuedConnection,
				  Q_ARG(bool, false));
}
