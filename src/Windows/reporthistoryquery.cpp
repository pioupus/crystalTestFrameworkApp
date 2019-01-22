#include "reporthistoryquery.h"
#include "Windows/mainwindow.h"
#include "data_engine/data_engine.h"
#include "ui_reporthistoryquery.h"
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QDirIterator>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageBox>
#include <QTreeWidgetItem>
#include <fstream>

ReportHistoryQuery::~ReportHistoryQuery() {
    delete ui;
}

ReportHistoryQuery::ReportHistoryQuery(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ReportHistoryQuery) {
    ui->setupUi(this);
    clear_query_pages();
    clear_where_pages();
    add_new_query_page();
    old_stk_report_history_index_m = ui->stk_report_history->currentIndex();
    ui->btn_export_query->setVisible(false);
}

void ReportHistoryQuery::load_query_from_file(QString file_name) {
    clear_query_pages();
    clear_where_pages();
    report_query_config_file_m.load_from_file(file_name);

    for (auto &report_query : report_query_config_file_m.get_queries_not_const()) {
        QWidget *tool_widget = new QWidget(ui->tb_queries);
        QGridLayout *grid_layout = new QGridLayout(tool_widget);
        report_query_config_file_m.create_new_query_ui(tool_widget, report_query);
        add_new_query_page(report_query, grid_layout, tool_widget);
    }
    for (auto &where : report_query_config_file_m.get_where_fields_not_const()) {
        QWidget *tool_widget = new QWidget(ui->tb_where);
        QGridLayout *grid_layout = new QGridLayout(tool_widget);
        report_query_config_file_m.create_new_where_ui(tool_widget, where);
        add_new_where_page(where, grid_layout, tool_widget);
    }
}

void ReportHistoryQuery::add_new_where_page(const QString &field_name, EntryType field_type) {
    if (field_name == "") {
        return;
    }
    QWidget *tool_widget = new QWidget(ui->tb_where);
    QGridLayout *grid_layout = new QGridLayout(tool_widget);
    auto &report_where = report_query_config_file_m.add_new_where(tool_widget, field_name, field_type);
    ui->tb_where->addItem(tool_widget, field_name + "(" + field_type.to_string() + ")");
    tool_widget->setProperty("field_name", field_name);
    ui->tb_where->setCurrentIndex(ui->tb_where->count() - 1);

    (void)report_where;
    (void)grid_layout;
}

void ReportHistoryQuery::add_new_where_page(ReportQueryWhereField &report_where, QGridLayout *grid_layout, QWidget *tool_widget) {
    ui->tb_where->addItem(tool_widget, report_where.field_name_m + "(" + report_where.field_type_m.to_string() + ")");
    tool_widget->setProperty("field_name", report_where.field_name_m);
    ui->tb_where->setCurrentIndex(ui->tb_where->count() - 1);

    (void)grid_layout;
}

void ReportHistoryQuery::add_new_query_page() {
    QWidget *tool_widget = new QWidget(ui->tb_queries);
    QGridLayout *grid_layout = new QGridLayout(tool_widget);
    ReportQuery &report_query = report_query_config_file_m.add_new_query(tool_widget);
    add_new_query_page(report_query, grid_layout, tool_widget);
}

void ReportHistoryQuery::add_new_query_page(ReportQuery &report_query, QGridLayout *grid_layout, QWidget *tool_widget) {
    ui->tb_queries->addItem(tool_widget, "");
    ui->tb_queries->setCurrentIndex(ui->tb_queries->count() - 1);

    connect(report_query.btn_query_report_file_browse_m, &QToolButton::clicked, [this, report_query](bool checked) {
        (void)checked; //
        const auto selected_dir = QFileDialog::getExistingDirectory(this, QObject::tr("Open report directory"), report_query.edt_query_report_folder_m->text(),
                                                                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
        if (!selected_dir.isEmpty()) {
            report_query.edt_query_report_folder_m->setText(selected_dir);
        }
    });

    connect(report_query.btn_query_data_engine_source_file_browse_m, &QToolButton::clicked, [this, report_query](bool checked) {
        (void)checked; //
        QFileDialog dialog(this);
        dialog.setDirectory(report_query.edt_query_data_engine_source_file_m->text());
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setNameFilter(tr("Data Engine Input files (*.json)"));
        if (dialog.exec()) {
            report_query.edt_query_data_engine_source_file_m->setText(dialog.selectedFiles()[0]);
        }
    });

    connect(report_query.btn_query_add_m, &QToolButton::clicked, [this](bool checked) {
        (void)checked; //
        add_new_query_page();
    });

    connect(report_query.btn_query_del_m, &QToolButton::clicked, [this, tool_widget](bool checked) {
        (void)checked; //
        remove_query_page(tool_widget);
    });

    connect(report_query.edt_query_data_engine_source_file_m, &QLineEdit::textChanged, [this, tool_widget](const QString &arg) {
        int index = ui->tb_queries->indexOf(tool_widget);
        ui->tb_queries->setItemText(index, arg);
    });

    (void)grid_layout;
}

void ReportHistoryQuery::on_tree_query_fields_itemDoubleClicked(QTreeWidgetItem *item, int column) {
    (void)column;
    QString field_name = item->data(0, Qt::UserRole).toString();
    if (field_name.count()) {
        QString field_type_str = item->text(1);
        EntryType field_type(field_type_str);
        add_new_where_page(field_name, field_type);
        item->setCheckState(0, Qt::Checked);
    }
}

void ReportHistoryQuery::remove_query_page(QWidget *tool_widget) {
    int current_index = ui->tb_where->indexOf(tool_widget);
    report_query_config_file_m.remove_query(current_index);
    ui->tb_queries->removeItem(current_index);
    delete tool_widget;
}

void ReportHistoryQuery::clear_query_pages() {
    while (ui->tb_queries->count()) {
        QWidget *widget = ui->tb_queries->widget(0);
        delete widget;
        ui->tb_queries->removeItem(0);
    }
}

void ReportHistoryQuery::remove_where_page(int index) {
    QWidget *widget = ui->tb_where->widget(index);
    delete widget;
    ui->tb_queries->removeItem(index);
}

void ReportHistoryQuery::clear_where_pages() {
    while (ui->tb_where->count()) {
        remove_where_page(0);
    }
}

void ReportHistoryQuery::on_btn_next_clicked() {
    int i = ui->stk_report_history->currentIndex() + 1;
    if (i < ui->stk_report_history->count()) {
        ui->stk_report_history->setCurrentIndex(i);
    }
}

void ReportHistoryQuery::on_btn_back_clicked() {
    int i = ui->stk_report_history->currentIndex() - 1;
    if (i > -1) {
        ui->stk_report_history->setCurrentIndex(i);
    }
}

void ReportHistoryQuery::load_select_ui_to_query() {
    for (int i = 0; i < ui->tree_query_fields->topLevelItemCount(); i++) {
        QTreeWidgetItem *top_level_item = ui->tree_query_fields->topLevelItem(i);
        auto &query = report_query_config_file_m.find_query(top_level_item->text(0));
        query.select_field_names_m.clear();

        for (int j = 0; j < top_level_item->childCount(); j++) {
            if (top_level_item->child(j)->text(0) == "report") {
                QTreeWidgetItem *report_item = top_level_item->child(j);

                for (int k = 0; k < report_item->childCount(); k++) {
                    QTreeWidgetItem *section_item = report_item->child(k);
                    QString section_name = section_item->text(0);
                    for (int m = 0; m < section_item->childCount(); m++) {
                        QTreeWidgetItem *field_item = section_item->child(m);
                        if (field_item->checkState(0) == Qt::Checked) {
                            QString field_name = field_item->text(0);
                            query.select_field_names_m.append("report/" + section_name + "/" + field_name);
                            qDebug() << "report/" + section_name + "/" + field_name;
                        }
                    }
                }
            } else if (top_level_item->child(j)->text(0) == "general") {
                QTreeWidgetItem *general_item = top_level_item->child(j);
                for (int k = 0; k < general_item->childCount(); k++) {
                    QTreeWidgetItem *field_item = general_item->child(k);
                    if (field_item->checkState(0) == Qt::Checked) {
                        QString field_name = field_item->text(0);
                        query.select_field_names_m.append("general/" + field_name);
                        qDebug() << "general/" + field_name;
                    }
                }
            } else {
                assert(0);
            }
        }
    }
    QList<ReportQueryWhereField> &where_fields = report_query_config_file_m.get_where_fields_not_const();
    for (auto &where_field : where_fields) {
        where_field.load_values_from_plain_text();
    }
}

void ReportHistoryQuery::on_stk_report_history_currentChanged(int arg1) {
    if ((old_stk_report_history_index_m == 0) && (arg1 == 1)) {
        ui->tree_query_fields->clear();
        //T:/qt/crystalTestFramework/tests/scripts/report_query/data_engine_source_1.json
        const Qt::ItemFlags item_flags_checkable = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
        for (auto &query : report_query_config_file_m.get_queries_not_const()) {
            query.update_from_gui();
            QTreeWidgetItem *root_item = new QTreeWidgetItem(QStringList{query.data_engine_source_file_m});
            ui->tree_query_fields->addTopLevelItem(root_item);

            const DataEngineSourceFields &fields = query.get_data_engine_fields();
            QTreeWidgetItem *root_general_widget = new QTreeWidgetItem(root_item, QStringList{"general"});
            QTreeWidgetItem *root_report_widget = new QTreeWidgetItem(root_item, QStringList{"report"});

            for (const auto &general_field : fields.general_fields_m) {
                QString typ_name = "";
                QTreeWidgetItem *general_entry =
                    new QTreeWidgetItem(root_general_widget, QStringList{general_field.field_name_m, general_field.field_type_m.to_string()});

                general_entry->setCheckState(0, Qt::Unchecked);
                general_entry->setFlags(item_flags_checkable);
                general_entry->setData(0, Qt::UserRole, "general/" + general_field.field_name_m);
                //  qDebug() << "general/" + general_field.field_name;
            }

            for (const auto &section_name : fields.report_fields_m.keys()) {
                QTreeWidgetItem *report_section_entry = new QTreeWidgetItem(root_report_widget, QStringList{section_name});
                for (const auto &data_engine_field : fields.report_fields_m.value(section_name)) {
                    QTreeWidgetItem *report_entry =
                        new QTreeWidgetItem(report_section_entry, QStringList{data_engine_field.field_name_m, data_engine_field.field_type_m.to_string()});
                    report_entry->setCheckState(0, Qt::Unchecked);
                    report_entry->setFlags(item_flags_checkable);
                    report_entry->setData(0, Qt::UserRole, "report/" + section_name + "/" + data_engine_field.field_name_m);
                    //    qDebug() << "report/" + section_name + "/" + data_engine_field.field_name;
                }
            }
        }
        ui->tree_query_fields->expandAll();
    } else if ((old_stk_report_history_index_m == 1) && (arg1 == 2)) {
        load_select_ui_to_query();
        QMap<QString, QList<QVariant>> query_result = report_query_config_file_m.execute_query();
        ui->tableWidget->clear();
        ui->tableWidget->setColumnCount(query_result.keys().count());
        ui->tableWidget->setHorizontalHeaderLabels(query_result.keys());
        int row_count = 0;
        for (auto key : query_result.keys()) {
            if (query_result.value(key).count() > row_count) {
                row_count = query_result.value(key).count();
            }
        }
        ui->tableWidget->setRowCount(row_count);
        int col_index = 0;
        for (auto key : query_result.keys()) {
            const auto &row_values = query_result.value(key);
            int row_index = 0;
            for (const auto &val : row_values) {
                QString s;
                if (val.canConvert<DataEngineDateTime>()) {
                    s = val.value<DataEngineDateTime>().str();
                } else {
                    s = val.toString();
                }
                QTableWidgetItem *item = new QTableWidgetItem(s, 0);
                ui->tableWidget->setItem(row_index, col_index, item);
                row_index++;
            }
            col_index++;
        }
    }
    old_stk_report_history_index_m = arg1;
    ui->btn_export_query->setVisible((arg1 == 1) || (arg1 == 2));
}

void ReportHistoryQuery::on_btn_close_clicked() {
    close();
}

void ReportHistoryQuery::on_tree_query_fields_itemClicked(QTreeWidgetItem *item, int column) {
    if (column == 0) {
        QString field_name = item->data(0, Qt::UserRole).toString();
        if ((item->checkState(0) == Qt::Unchecked) && (field_name.count())) {
            if (report_query_config_file_m.remove_where(field_name)) {
                for (int i = 0; i < ui->tb_where->count(); i++) {
                    if (ui->tb_where->widget(i)->property("field_name").toString() == field_name) {
                        remove_where_page(i);
                        break;
                    }
                }
            }
        }
    }
}

void ReportHistoryQuery::on_btn_where_del_clicked() {
    QWidget *tool_widget = ui->tb_where->currentWidget();
    if (tool_widget) {
        QString field_name = tool_widget->property("field_name").toString();
        report_query_config_file_m.remove_where(field_name);
        remove_where_page(ui->tb_where->currentIndex());
    }
}

void ReportQueryConfigFile::create_new_where_ui(QWidget *parent, ReportQueryWhereField &report_where) {
    if (parent) {
        QGridLayout *gl = dynamic_cast<QGridLayout *>(parent->layout());
        report_where.plainTextEdit_m = new QPlainTextEdit();
        QStringList text;
        for (auto &where_seg : report_where.field_values_m) {
            for (auto &where_segment_value : where_seg.values_m) {
                switch (report_where.field_type_m.t) {
                    case EntryType::Bool: {
                        bool val = where_segment_value.toBool();
                        if (val) {
                            text.append("true");
                        } else {
                            text.append("false");
                        }
                    } break;
                    case EntryType::Number: {
                        double val = where_segment_value.toDouble();
                        text.append(QString::number(val));
                    } break;
                    case EntryType::Text:
                        text.append(where_segment_value.toString());
                        break;
                    case EntryType::DateTime: {
                        DataEngineDateTime dt = where_segment_value.value<DataEngineDateTime>();
                        text.append(dt.str());
                    } break;
                    case EntryType::Unspecified: {
                        assert(0);
                    } break;
                    case EntryType::Reference: {
                        assert(0);
                    } break;
                }
            }
            if (where_seg.include_greater_values_till_next_entry_m) {
                text.append("*");
            }
        }
        for (auto &t : text) {
            report_where.plainTextEdit_m->appendPlainText(t);
        }
        gl->addWidget(report_where.plainTextEdit_m, 0, 1);
    }
};

ReportQueryWhereField &ReportQueryConfigFile::add_new_where(QWidget *parent, QString field_name, EntryType field_type) {
    ReportQueryWhereField report_where{};
    report_where.field_name_m = field_name;
    report_where.field_type_m = field_type;
    create_new_where_ui(parent, report_where);
    query_where_fields_m.append(report_where);
    return query_where_fields_m.last();
}

bool ReportQueryConfigFile::remove_where(QString field_name) {
    for (int i = 0; i < query_where_fields_m.count(); i++) {
        if (query_where_fields_m[i].field_name_m == field_name) {
            query_where_fields_m.removeAt(i);
            return true;
        }
    }
    return false;
}

void ReportQueryConfigFile::create_new_query_ui(QWidget *parent, ReportQuery &report_query) {
    if (parent) {
        QGridLayout *gl = dynamic_cast<QGridLayout *>(parent->layout());

        QLabel *lbl_data_engine_source = new QLabel();
        lbl_data_engine_source->setText(QObject::tr("Data engine source file:"));
        gl->addWidget(lbl_data_engine_source, 0, 0);

        report_query.edt_query_data_engine_source_file_m = new QLineEdit();
        report_query.edt_query_data_engine_source_file_m->setText(report_query.data_engine_source_file_m);
        gl->addWidget(report_query.edt_query_data_engine_source_file_m, 0, 1);

        report_query.btn_query_data_engine_source_file_browse_m = new QToolButton();
        report_query.btn_query_data_engine_source_file_browse_m->setText("..");
        gl->addWidget(report_query.btn_query_data_engine_source_file_browse_m, 0, 2);

        QLabel *lbl_report_path = new QLabel();
        lbl_report_path->setText(QObject::tr("Test report seach folder:"));
        gl->addWidget(lbl_report_path, 1, 0);

        report_query.edt_query_report_folder_m = new QLineEdit();
        report_query.edt_query_report_folder_m->setText(report_query.report_path_m);

        gl->addWidget(report_query.edt_query_report_folder_m, 1, 1);

        report_query.btn_query_report_file_browse_m = new QToolButton();
        report_query.btn_query_report_file_browse_m->setText("..");
        gl->addWidget(report_query.btn_query_report_file_browse_m, 1, 2);

        QHBoxLayout *layout_add_del = new QHBoxLayout();
        layout_add_del->addStretch(1);
        report_query.btn_query_add_m = new QToolButton();
        report_query.btn_query_add_m->setText("+");
        layout_add_del->addWidget(report_query.btn_query_add_m);

        report_query.btn_query_del_m = new QToolButton();
        report_query.btn_query_del_m->setText("-");
        layout_add_del->addWidget(report_query.btn_query_del_m);
        gl->addLayout(layout_add_del, 2, 1, 2, 1);
        gl->setRowStretch(4, 1);
    }
}

ReportQuery &ReportQueryConfigFile::add_new_query(QWidget *parent) {
    ReportQuery report_query{};
    create_new_query_ui(parent, report_query);
    report_queries_m.append(report_query);
    return report_queries_m.last();
}

ReportQuery &ReportQueryConfigFile::find_query(QString data_engine_source_file) {
    for (auto &query : report_queries_m) {
        if (query.data_engine_source_file_m.endsWith(data_engine_source_file)) {
            return query;
        }
    }
    throw std::runtime_error("ReportQueryConfigFile: can not find query with " + data_engine_source_file.toStdString() + " as filename.");
}

void ReportQueryConfigFile::remove_query(int index) {
    report_queries_m.removeAt(index);
}

QMap<QString, QList<QVariant>> ReportQueryConfigFile::execute_query() const {
    QList<ReportLink> rl = scan_folder_for_reports("");
    return filter_and_select_reports(rl);
}

ReportQueryConfigFile::ReportQueryConfigFile() {}

ReportQueryConfigFile::~ReportQueryConfigFile() {}

void ReportQueryConfigFile::load_from_file(QString file_name) {
    if ((file_name == "") || !QFile::exists(file_name)) {
        QString msg = QString{"report query config file %1 does not exist."}.arg(file_name);
        throw sol::error(msg.toStdString());
    }
    QFile loadFile(file_name);
    if (!loadFile.open(QIODevice::ReadOnly)) {
        QString msg = QString{"cant open report query config file %1"}.arg(file_name);
        throw sol::error(msg.toStdString());
    }
    QByteArray config_file_content = loadFile.readAll();
    QJsonDocument loadDoc(QJsonDocument::fromJson(config_file_content));

    QJsonObject doc_obj = loadDoc.object();
    QJsonArray js_where_fields = doc_obj["where_fields"].toArray();

    query_where_fields_m.clear();
    for (auto js_where_field : js_where_fields) {
        ReportQueryWhereField where_field;
        QJsonObject js_where_field_obj = js_where_field.toObject();
        where_field.field_name_m = js_where_field_obj["field_name"].toString();
        QString s = js_where_field_obj["field_type"].toString();
        where_field.field_type_m = EntryType(s);
        where_field.incremention_selector_expression_ = js_where_field_obj["incremention_selector_expression"].toString();
        QJsonArray js_conditions = js_where_field_obj["conditions"].toArray();

        for (auto js_condition : js_conditions) {
            ReportQueryWhereFieldValues field_values;
            QJsonObject obj = js_condition.toObject();
            field_values.include_greater_values_till_next_entry_m = obj["include_greater_values_till_next_entry"].toBool();
            QJsonArray js_values = obj["values"].toArray();
            for (auto value : js_values) {
                field_values.values_m.append(value.toInt());
            }
            where_field.field_values_m.append(field_values);
        }
        query_where_fields_m.append(where_field);
    }

    QJsonArray js_queries = doc_obj["queries"].toArray();
    report_queries_m.clear();
    for (const auto js_query : js_queries) {
        ReportQuery rq;
        const auto js_query_obj = js_query.toObject();
        rq.report_path_m = js_query_obj["report_path"].toString();
        rq.data_engine_source_file_m = js_query_obj["data_engine_source_file"].toString();
        QJsonArray js_select_field_name = js_query_obj["select_field_names"].toArray();
        for (const auto v : js_select_field_name) {
            rq.select_field_names_m.append(v.toString());
        }
        report_queries_m.append(rq);
    }
}

void ReportQueryConfigFile::save_to_file(QString file_name) {
    QJsonArray js_where_array;
    QJsonArray js_query_array;
    for (auto &where : query_where_fields_m) {
        QJsonArray js_where_value_segment_array;
        for (auto &value_segment : where.field_values_m) {
            QJsonArray js_where_value_array;
            for (auto &value : value_segment.values_m) {
                switch (where.field_type_m.t) {
                    case EntryType::Number:
                        js_where_value_array.append(value.toDouble());
                        break;
                    case EntryType::DateTime: {
                        DataEngineDateTime dt = value.value<DataEngineDateTime>();
                        js_where_value_array.append(dt.str());
                    } break;
                    case EntryType::Text:
                        js_where_value_array.append(value.toString());
                        break;
                    case EntryType::Bool:
                        js_where_value_array.append(value.toBool());
                        break;
                    case EntryType::Unspecified:
                        assert(0);
                        break;
                    case EntryType::Reference:
                        assert(0);
                        break;
                }
            }
            QJsonObject js_where_value_segment_obj;
            js_where_value_segment_obj["include_greater_values_till_next_entry"] = value_segment.include_greater_values_till_next_entry_m;
            js_where_value_segment_obj["values"] = js_where_value_array;
            js_where_value_segment_array.append(js_where_value_segment_obj);
        }
        QJsonObject js_where_entry;
        js_where_entry["conditions"] = js_where_value_segment_array;
        js_where_entry["field_name"] = where.field_name_m;
        js_where_entry["field_type"] = where.field_type_m.to_string();
        js_where_array.append(js_where_entry);
    }
    for (auto &query : report_queries_m) {
        QJsonObject js_query_entry;
        js_query_entry["report_path"] = query.report_path_m;
        js_query_entry["data_engine_source_file"] = query.data_engine_source_file_m;
        QJsonArray js_query_select_array;
        for (auto &select_file_name : query.select_field_names_m) {
            js_query_select_array.append(select_file_name);
        }
        js_query_entry["select_field_names"] = js_query_select_array;
        js_query_array.append(js_query_entry);
    }
    QJsonObject obj;
    obj["queries"] = js_query_array;
    obj["where_fields"] = js_where_array;
    QFile saveFile(file_name);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        return;
    }
    QJsonDocument saveDoc(obj);
    saveFile.write(saveDoc.toJson());
}

const QList<ReportQuery> &ReportQueryConfigFile::get_queries() {
    return report_queries_m;
}

QList<ReportQuery> &ReportQueryConfigFile::get_queries_not_const() {
    return report_queries_m;
}

const QList<ReportQueryWhereField> &ReportQueryConfigFile::get_where_fields() {
    return query_where_fields_m;
}

QList<ReportQueryWhereField> &ReportQueryConfigFile::get_where_fields_not_const() {
    return query_where_fields_m;
}

QMap<QString, QList<QVariant>> ReportQueryConfigFile::filter_and_select_reports(const QList<ReportLink> &report_file_list) const {
    QMap<QString, QList<QVariant>> result;
    for (const auto &report_link : report_file_list) {
        ReportFile report_file;
        report_file.load_from_file(report_link.report_path_m);
        if (only_successful_reports_m) {
            if ((report_file.get_field_value("general/everything_complete").toBool() == false) ||
                (report_file.get_field_value("general/everything_in_range").toBool()) == false) {
                continue;
            }
        }
        //    qDebug() << report_link.query.data_engine_source_file;
        QString data_engine_source_file_name = QFileInfo(report_link.query_m.data_engine_source_file_m).fileName().split('.')[0];

        //  qDebug() << data_engine_source_file_name;
        for (const auto &report_query_where_field : query_where_fields_m) {
            auto value = report_file.get_field_value(report_query_where_field.field_name_m);
            if (report_query_where_field.matches_value(value)) {
                for (auto select_field_name : report_link.query_m.select_field_names_m) {
                    QString key = data_engine_source_file_name + "/" + select_field_name;
                    if (result.contains(key)) {
                        //QList<QVariant> &value = result[key];
                        result[key].append(report_file.get_field_value(select_field_name));
                        //result.insert(key, value);
                    } else {
                        QList<QVariant> value{report_file.get_field_value(select_field_name)};
                        result.insert(key, value);
                    }

                    //result.insertMulti(, report_file.get_field_value(select_field_name));
                }
            }
        }
    }
    return result;
}

QList<ReportLink> ReportQueryConfigFile::scan_folder_for_reports(QString base_dir_str) const {
    QList<ReportLink> result;
    for (const auto &query : report_queries_m) {
        QDir base_dir{base_dir_str};
        QString report_search_path = query.report_path_m;
        QDir report_path{report_search_path};
        if (report_path.isRelative()) {
            report_search_path = base_dir.absoluteFilePath(report_search_path);
        }
        QDirIterator dit{report_search_path, QStringList{} << "*.json", QDir::Files, QDirIterator::Subdirectories};
        while (dit.hasNext()) {
            const auto &file_path = dit.next();
            result.append(ReportLink{file_path, //
                                     query});
        }
    }
    return result;
}

ReportFile::ReportFile() {}

ReportFile::~ReportFile() {}

void ReportFile::load_from_file(QString file_name) {
    if ((file_name == "") || !QFile::exists(file_name)) {
        QString msg = QString{"report query config file %1 does not exist."}.arg(file_name);
        throw sol::error(msg.toStdString());
    }
    QFile loadFile(file_name);
    if (!loadFile.open(QIODevice::ReadOnly)) {
        QString msg = QString{"cant open report query config file %1"}.arg(file_name);
        throw sol::error(msg.toStdString());
    }
    QByteArray report_file_content = loadFile.readAll();
    QJsonDocument loadDoc(QJsonDocument::fromJson(report_file_content));
    js_report_object_m = loadDoc.object();
}

QVariant ReportFile::get_field_value(QString field_name) {
    auto field_path = field_name.split("/");
    const bool is_report_field = field_path[0] == "report";
    auto last_field_name_component = field_path.last();
    field_path.removeLast();
    if (is_report_field) {
        field_path[0] = "sections";
    }
    QJsonObject js_value_obj = js_report_object_m;
    for (const auto &field_path_component : field_path) {
        js_value_obj = js_value_obj[field_path_component].toObject();
    }
    if (is_report_field) {
        QJsonArray js_instance = js_value_obj["instances"].toArray();
        QJsonArray js_fields_in_section = js_instance[0].toObject()["fields"].toArray();
        for (auto field : js_fields_in_section) {
            auto js_field_obj = field.toObject();
            auto field_name_to_probe = js_field_obj["name"].toString();
            if (field_name_to_probe == last_field_name_component) {
                auto js_numeric_obj = js_field_obj["numeric"].toObject();
                auto js_text_obj = js_field_obj["text"].toObject();
                auto js_bool_obj = js_field_obj["bool"].toObject();
                auto js_datetime_obj = js_field_obj["datetime"].toObject();

                if (!js_numeric_obj.isEmpty()) {
                    return QVariant(js_numeric_obj["actual"].toObject()["value"].toDouble());
                } else if (!js_text_obj.isEmpty()) {
                    return QVariant(js_text_obj["actual"].toObject()["value"].toString());
                } else if (!js_bool_obj.isEmpty()) {
                    return QVariant(js_bool_obj["actual"].toObject()["value"].toBool());
                } else if (!js_datetime_obj.isEmpty()) {
                    auto ms_since_epoch = js_datetime_obj["actual"].toObject()["ms_since_epoch"].toDouble();
                    DataEngineDateTime dt(QDateTime::fromMSecsSinceEpoch(round(ms_since_epoch)));
                    QVariant res;
                    res.setValue<DataEngineDateTime>(dt);
                    return res;
                }
            }
        }
    } else {
        QVariant result = js_value_obj[last_field_name_component].toVariant();
        if (result.type() == QVariant::String) {
            DataEngineDateTime dt(result.toString());

            if (dt.isValid()) {
                result.setValue<DataEngineDateTime>(dt);
            }
        }
        return result;
    }
    return QVariant();
}

bool ReportQueryWhereField::matches_value(QVariant value) const {
    for (int i = 0; i < field_values_m.count(); i++) {
        auto field_value = field_values_m[i];
        if (field_value.values_m.contains(value)) {
            return true;
        }

        if (field_value.include_greater_values_till_next_entry_m) {
            const bool is_num = (field_type_m.t == EntryType::Number);
            const bool is_date = (field_type_m.t == EntryType::DateTime);
            if (is_num || is_date) {
                if (i == field_values_m.count() - 1) { //is last condition?
                    if (value > field_value.values_m.last()) {
                        return true;
                    }
                } else {
                    if ((value > field_value.values_m.last()) && (value < field_values_m[i + 1].values_m.first())) {
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void ReportQueryWhereField::load_values_from_plain_text() {
    QString plainTextEditContents = plainTextEdit_m->toPlainText();
    QStringList plain_text = plainTextEditContents.split("\n");
    field_values_m.clear();
    try {
        switch (field_type_m.t) {
            case EntryType::Number: {
                ReportQueryWhereFieldValues value;
                for (auto s : plain_text) {
                    s = s.trimmed();
                    if (s == "") {
                        continue;
                    }
                    if (s == "*") {
                        value.include_greater_values_till_next_entry_m = true;
                        field_values_m.append(value);
                        value = ReportQueryWhereFieldValues();
                        continue;
                    }
                    bool parse_ok = false;
                    double num_value = s.toDouble(&parse_ok);
                    if (!parse_ok) {
                        throw WhereFieldInterpretationError("cannot convert '" + s + "' to number");
                    }
                    value.values_m.append(QVariant(num_value));
                }
                if (value.values_m.count()) {
                    field_values_m.append(value);
                }
            } //
            break;
            case EntryType::DateTime: //
            {
                ReportQueryWhereFieldValues value;
                for (auto s : plain_text) {
                    s = s.trimmed();
                    if (s == "") {
                        continue;
                    }
                    if (s == "*") {
                        value.include_greater_values_till_next_entry_m = true;
                        field_values_m.append(value);
                        value = ReportQueryWhereFieldValues();
                        continue;
                    }
                    DataEngineDateTime datetime_value(s);
                    if (!datetime_value.isValid()) {
                        throw WhereFieldInterpretationError("cannot convert '" + s + "' to date time. Allowed formats: " +
                                                            DataEngineDateTime::allowed_formats().join(", "));
                    }
                    QVariant val;
                    val.setValue<DataEngineDateTime>(datetime_value);
                    value.values_m.append(val);
                }
                if (value.values_m.count()) {
                    field_values_m.append(value);
                }
                if (field_values_m.count() == 0) {
                    throw WhereFieldInterpretationError("Match values for " + field_name_m + " are not yet defined.");
                }
            } //
            break;
            case EntryType::Text: {
                ReportQueryWhereFieldValues value;
                for (auto s : plain_text) {
                    s = s.trimmed();
                    if (s == "") {
                        continue;
                    }
                    if (s == "*") {
                        throw WhereFieldInterpretationError("Wild cards for text fields are not allowed.");
                    }

                    value.values_m.append(s);
                }
                if (value.values_m.count()) {
                    field_values_m.append(value);
                }
                if (field_values_m.count() == 0) {
                    throw WhereFieldInterpretationError("Match values for " + field_name_m + " are not yet defined.");
                }
            } //
            break;
            case EntryType::Bool: //
            {
                ReportQueryWhereFieldValues value;
                int counter = 0;
                for (auto s : plain_text) {
                    s = s.trimmed();
                    if (s == "") {
                        continue;
                    }
                    QStringList true_words{"true", "ja", "yes", "wahr"};
                    QStringList false_words{"false", "nein", "no", "falsch"};
                    if (true_words.contains(s.toLower())) {
                        value.values_m.append(QVariant(true));
                    } else if (false_words.contains(s.toLower())) {
                        value.values_m.append(QVariant(false));
                    } else {
                        throw WhereFieldInterpretationError("cannot convert '" + s + "' to bool. Allowed formats: " + true_words.join(", ") + " / " +
                                                            false_words.join(", "));
                    }

                    value.values_m.append(s);
                    counter++;
                }
                if (value.values_m.count()) {
                    field_values_m.append(value);
                }
                if (field_values_m.count() == 0) {
                    throw WhereFieldInterpretationError("Match values for " + field_name_m + " are not yet defined.");
                }
                if (counter > 1) {
                    throw WhereFieldInterpretationError("Bool fields only allow one match value");
                }
            } //
            break;
            case EntryType::Unspecified: //
                assert(0);
                break;
            case EntryType::Reference: //
                assert(0);
                break;
        }
    } catch (WhereFieldInterpretationError &e) {
        qDebug() << e.what();
    }
}

void ReportQuery::update_from_gui() {
    if (edt_query_report_folder_m) {
        report_path_m = edt_query_report_folder_m->text();
    }
    if (edt_query_data_engine_source_file_m) {
        data_engine_source_file_m = edt_query_data_engine_source_file_m->text();
    }
}

DataEngineSourceFields ReportQuery::get_data_engine_fields_raw() const {
    std::ifstream f(data_engine_source_file_m.toStdString());
    DataEngineSourceFields result{};

    result.general_fields_m = QList<DataEngineField>{
        {"data_source_path", EntryType::Text},        {"datetime_str", EntryType::DateTime},      {"datetime_unix", EntryType::Number},
        {"everything_complete", EntryType::Bool},     {"everything_in_range", EntryType::Bool},   {"exceptional_approval_exists", EntryType::Bool},
        {"framework_git_hash", EntryType::Text},      {"os_username", EntryType::Text},           {"script_path", EntryType::Text},
        {"test_duration_seconds", EntryType::Number}, {"test_git_date_str", EntryType::DateTime}, {"test_git_hash", EntryType::Text},
        {"test_git_modified", EntryType::Text}};
    Data_engine data_engine;
    data_engine.set_source(f);
    auto section_names = data_engine.get_section_names();
    for (auto section_name : section_names) {
        QStringList field_names = data_engine.get_ids_of_section(section_name);
        QList<DataEngineField> field_names_result;
        for (auto field_name : field_names) {
            DataEngineField data_engine_field;
            data_engine_field.field_type_m = data_engine.get_entry_type_dummy_mode(field_name);
            data_engine_field.field_name_m = field_name.split('/')[1];
            field_names_result.append(data_engine_field);
        }
        result.report_fields_m.insert(section_name, field_names_result);
    }
    return result;
}

DataEngineSourceFields ReportQuery::get_data_engine_fields() {
    try {
        DataEngineSourceFields result = get_data_engine_fields_raw();
        is_valid_m = true;
        return result;
    } catch (DataEngineError &e) {
        QMessageBox::warning(MainWindow::mw, QString("Dataengine error"), QString("Dataengine error:\n\n %1").arg(e.what()));
    }
    return DataEngineSourceFields{};
}

void ReportHistoryQuery::on_btn_export_query_clicked() {
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilter(tr("Report Query file (*.rqu)"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    if (dialog.exec()) {
        load_select_ui_to_query();
        report_query_config_file_m.save_to_file(dialog.selectedFiles()[0]);
    }
}

void ReportHistoryQuery::on_btn_import_clicked() {
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilter(tr("Report Query file (*.rqu)"));
    dialog.setAcceptMode(QFileDialog::AcceptOpen);
    if (dialog.exec()) {
        load_query_from_file(dialog.selectedFiles()[0]);
    }
}
