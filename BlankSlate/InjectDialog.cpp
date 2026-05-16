#include "InjectDialog.h"
#include <QFileDialog>
#include <QMessageBox>
#include "InjectHelper.h"

InjectDialog::InjectDialog(DWORD ProcessId, QWidget *parent)
	: QDialog(parent), m_ProcessId(ProcessId)
{
	ui.setupUi(this);
	ui.label_pid->setText(QString::number(ProcessId));
	setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

	connect(ui.btn_browse, &QPushButton::clicked, this, &InjectDialog::OnBrowseDll);
	connect(ui.btn_inject, &QPushButton::clicked, this, &InjectDialog::OnInject);
	connect(ui.btn_cancel, &QPushButton::clicked, this, &QDialog::reject);
}

InjectDialog::~InjectDialog()
{
}

QString InjectDialog::GetDllPath() const
{
	return ui.lineEdit_dllpath->text();
}

int InjectDialog::GetInjectMethod() const
{
	return ui.comboBox_method->currentIndex();
}

void InjectDialog::OnBrowseDll()
{
	QString dllPath = QFileDialog::getOpenFileName(
		this,
		QStringLiteral("选择DLL文件"),
		QString(),
		QStringLiteral("DLL文件 (*.dll);;所有文件 (*.*)")
	);
	if (!dllPath.isEmpty()) {
		dllPath.replace('/', '\\');
		ui.lineEdit_dllpath->setText(dllPath);
	}
}

void InjectDialog::OnInject()
{
	QString dllPath = ui.lineEdit_dllpath->text();
	if (dllPath.isEmpty()) {
		QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择要注入的DLL文件！"));
		return;
	}

	int method = ui.comboBox_method->currentIndex();

	// 注册表注入警告
	if (method == INJECT_REGISTER) {
		QMessageBox::StandardButton reply = QMessageBox::warning(
			this,
			QStringLiteral("警告"),
			QStringLiteral("注册表注入是系统范围的注入方式，会影响所有加载User32.dll的新进程！\n确定要继续吗？"),
			QMessageBox::Yes | QMessageBox::No
		);
		if (reply != QMessageBox::Yes) return;
	}
	BOOL result = _INJECT_HELPER_::DoInject(m_ProcessId, method, (const wchar_t*)dllPath.utf16());

	if (result) {
		//QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("注入成功！"));
		accept();
	}
	else {
		QMessageBox::critical(this, QStringLiteral("错误"), QStringLiteral("注入失败！请检查权限和参数。"));
	}
}
