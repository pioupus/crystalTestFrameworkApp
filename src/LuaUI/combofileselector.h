#ifndef COMBOFILESELECTOR_H
#define COMBOFILESELECTOR_H
#include <QComboBox>
#include <QDateTime>
#include <QList>
#include <QMetaObject>
#include <QPushButton>
#include <functional>
#include <sol.hpp>
#include <string>

class UI_container;

class FileEntry {
    public:
    QString filename;
    QString filenpath;
    QDateTime date;
};

class ComboBoxFileSelector {
    public:
	ComboBoxFileSelector(UI_container *parent, const std::string &directory, const sol::table &filter);
    ~ComboBoxFileSelector();

    std::string get_selected_file();
    void set_order_by(const std::string &field, const bool ascending);

    private:
	void scan_directory();
	void fill_combobox();

	UI_container *parent = nullptr;
    QWidget *base_widget = nullptr;
    QComboBox *combobox = nullptr;
    QPushButton *button = nullptr;
    QList<FileEntry> file_entries;
    QStringList filters;
    QMetaObject::Connection button_clicked_connection;
    QString current_directory;
};

#endif // COMBOFILESELECTOR_H
