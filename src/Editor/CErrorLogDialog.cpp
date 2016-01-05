#include "CErrorLogDialog.h"
#include "ui_CErrorLogDialog.h"
#include "UICommon.h"
#include <Core/Log.h>

CErrorLogDialog::CErrorLogDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CErrorLogDialog)
{
    ui->setupUi(this);
    connect(ui->CloseButton, SIGNAL(clicked()), this, SLOT(close()));
}

CErrorLogDialog::~CErrorLogDialog()
{
    delete ui;
}

bool CErrorLogDialog::GatherErrors()
{
    const TStringList& rkErrors = Log::GetErrorLog();
    if (rkErrors.empty()) return false;

    QString DialogString;

    for (auto it = rkErrors.begin(); it != rkErrors.end(); it++)
    {
        QString Error = TO_QSTRING(*it);
        QString LineColor;

        if (Error.startsWith("ERROR: "))
            LineColor = "#ff0000";
        else if (Error.startsWith("Warning: "))
            LineColor = "#ff8000";

        QString FullLine = Error;

        if (!LineColor.isEmpty())
        {
            FullLine.prepend(QString("<font color=\"%1\">").arg(LineColor));
            FullLine.append("</font>");
        }
        FullLine.append("<br />");

        DialogString += FullLine;
    }

    ui->ErrorLogTextEdit->setText(DialogString);
    Log::ClearErrorLog();
    return true;
}