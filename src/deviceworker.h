#ifndef DEVICEWORKER_H
#define DEVICEWORKER_H

#include "Protocols/protocol.h"
#include "qt_util.h"
#include "scriptengine.h"

#include "CommunicationDevices/libusbscan.h"
#include "CommunicationDevices/usbtmc.h"
#include "scpimetadata.h"
#include <future>
#include <list>
#include <vector>
#include <QSemaphore>

class CommunicationDevice;
class MainWindow;
class QPlainTextEdit;
class QTreeWidgetItem;
struct PortDescription;

class DeviceWorker : public QObject {
    Q_OBJECT
    public:
    DeviceWorker();
    ~DeviceWorker();

    void refresh_devices(QTreeWidgetItem *device_items, bool dut_only);

    void connect_to_device_console(QPlainTextEdit *console, CommunicationDevice *comport);
    std::vector<PortDescription *> get_devices_with_protocol(const QString &protocol, const QStringList device_names);
    void set_currently_running_test(CommunicationDevice *com_device, const QString &test_name);
    QStringList get_string_list(ScriptEngine &script, const QString &name);

    bool is_dut_device(QTreeWidgetItem *item);
    bool is_device_in_use(QTreeWidgetItem *item);

    private:
    void forget_device(QTreeWidgetItem *item);
    void detect_device(QTreeWidgetItem *item);
    void update_devices();
    void detect_devices();
    std::list<PortDescription> communication_devices;
    bool contains_port(QMap<QString, QVariant> port_info);
    void detect_devices(std::vector<PortDescription *> device_list);
    DeviceMetaData device_meta_data;
    LIBUSBScan usbtmc_scan;
    QSemaphore  refresh_semaphore{1};
    //QWaitCondition refresh_wait_condition{};
    bool refreshing_devices = false;
    bool is_dut_device_(QTreeWidgetItem *item);
    bool is_device_in_use_(QTreeWidgetItem *item);
    void forget_device_(QTreeWidgetItem *item);

    signals:
    void device_discrovery_done();
};

#endif // DEVICEWORKER_H
