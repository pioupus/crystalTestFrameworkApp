#ifndef TESTREPORTHISTORY_H
#define TESTREPORTHISTORY_H

#include "autotest.h"
#include <QList>
#include <QObject>

class ReportLink;
class DataEngineField;

class TestReportHistory : public QObject {
    Q_OBJECT

    public:
    private slots:

    void load_query_onfig_file();
    void load_report_file1();
    void scan_report_files();
    void test_match_value();
    void scan_filter_report_files();
    void read_report_fields();

    void test_reduce_path();

    private:
    bool report_link_list_contains_path(const QList<ReportLink> &report_link_list, QString path);
    bool data_engine_field_list_contains_path(const QList<DataEngineField> &data_engine_field_list, QString field_name);
    const DataEngineField get_data_engine_field(const QList<DataEngineField> &data_engine_field_list, QString field_name);
};

DECLARE_TEST(TestReportHistory)

#endif // TESTREPORTHISTORY_H
