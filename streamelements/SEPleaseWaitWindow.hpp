#ifndef STREAMELEMENTSPLEASEWAITWINDOW_H
#define STREAMELEMENTSPLEASEWAITWINDOW_H

#include <QPointer>
#include <QWidget>
#include <QDialog>

#include <memory>
#include <mutex>

namespace Ui {
class SEPleaseWaitWindow;
}

class SEPleaseWaitWindow : public QDialog {
    Q_OBJECT

public:
	//
	// Owned by SEGlobalStateManager and published in SEApiServices. It
	// was a singleton reached by the transaction map in SEUtils, which
	// is now a service that takes it (CORE-757).
	//
	// Modality does not depend on the Qt parent -- the constructor sets
	// ApplicationModal explicitly -- but the parent is kept for stacking
	// and centring, which means Qt can destroy it with the main window.
	// Create() hands back a shared_ptr whose deleter copes with that.
	//
	static std::shared_ptr<SEPleaseWaitWindow> Create(QWidget *parent);

	void Show();
	void Hide();

	explicit SEPleaseWaitWindow(QWidget *parent = 0);
	~SEPleaseWaitWindow();

private:
	Ui::SEPleaseWaitWindow *ui;

	long m_showCount = 0;
};

#endif // STREAMELEMENTSPLEASEWAITWINDOW_H
