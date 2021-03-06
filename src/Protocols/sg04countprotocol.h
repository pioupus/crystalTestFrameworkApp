#ifndef SG04COUNTPROTOCOL_H
#define SG04COUNTPROTOCOL_H

#include "Protocols/protocol.h"
#include "device_protocols_settings.h"

#include <QMetaObject>
#include <chrono>
#include <mutex>
#include <sol_forward.hpp>

class QTreeWidgetItem;
class ScriptEngine;
class CommunicationDevice;

class SG04CountProtocol : public Protocol {
public:
  SG04CountProtocol(CommunicationDevice &device, DeviceProtocolSetting setting);
  ~SG04CountProtocol();
  SG04CountProtocol(const SG04CountProtocol &) = delete;
  SG04CountProtocol(SG04CountProtocol &&other) = delete;
  SG04CountProtocol &operator=(const SG04CountProtocol &&) = delete;
  bool is_correct_protocol();
  void set_ui_description(QTreeWidgetItem *ui_entry);

  void sg04_counts_clear();
  sol::table get_sg04_counts(sol::state &lua, bool clear);
  uint accumulate_counts(ScriptEngine *script_engine, uint time_ms);

  uint16_t get_actual_count_rate();
  unsigned int get_actual_count_rate_cps();

  bool is_currently_receiving_counts() const;

private:
  QMetaObject::Connection connection;
  CommunicationDevice *device;
  DeviceProtocolSetting device_protocol_setting;
  QByteArray incoming_data;
  uint16_t actual_count_rate;
  uint32_t received_counts = 0;
  QList<uint16_t> received_count_packages;
  mutable std::mutex received_counts_mutex;
  std::chrono::system_clock::time_point last_package;
  void sg04_counts_clear_raw();
};

#endif // SG04COUNTPROTOCOL_H
