#include "exceptionalapproval.h"
#include "Windows/exceptiontalapprovaldialog.h"
#include "Windows/mainwindow.h"
#include "util.h"
#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ExceptionalApprovalDB::ExceptionalApprovalDB(const QString &file_name) {
    QFile loadFile(file_name);
    if (loadFile.open(QIODevice::ReadOnly)) {
        QByteArray saveData = loadFile.readAll();
        QJsonDocument loadDoc(QJsonDocument::fromJson(saveData));
        QJsonObject obj = loadDoc.object();

        QJsonArray section_order = obj["exceptional_approvals"].toArray();
        for (auto jitem : section_order) {
            QJsonObject obj = jitem.toObject();
            ExceptionalApproval ea{};
            ea.id = obj["id"].toInt();
            ea.description = obj["description"].toString();
            ea.hidden = obj["hidden"].toBool();
            approvals.append(ea);
        }
    }
}

const QList<FailedField> &ExceptionalApprovalDB::get_failed_fields() const {
    return failed_fields;
}

QString ExceptionalApprovalDB::get_failure_text(const FailedField &ff) {
    return ff.desired_value + " ≠ " + ff.actual_value;
}

QList<ExceptionalApprovalResult> ExceptionalApprovalDB::select_exceptional_approval(QList<FailedField> failed_fields, QWidget *parent) {
    QList<ExceptionalApprovalResult> result;

    if (parent) {
        this->failed_fields = failed_fields;
        Utility::promised_thread_call(MainWindow::mw, [failed_fields, this, parent] {
            ExceptiontalApprovalDialog diag{approvals, failed_fields, parent};
            diag.exec();
        });
    } else {
        auto a = approvals[0];
        for (auto ff : failed_fields) {
            ExceptionalApprovalResult r{};
            r.failed_field = ff;
            r.exceptional_approval = a;
            r.approving_operator_name = "Operator";
            //  r.failure = get_failure_text(ff);
            result.append(r);
        }
    }
    return result;
}
