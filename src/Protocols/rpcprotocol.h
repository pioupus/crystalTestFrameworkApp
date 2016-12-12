#ifndef RPCPROTOCOL_H
#define RPCPROTOCOL_H

#include "CommunicationDevices/communicationdevice.h"
#include "channel_codec_wrapper.h"
#include "protocol.h"
#include "rpcruntime_decoder.h"
#include "rpcruntime_encoder.h"
#include "rpcruntime_protocol_description.h"
#include "rpcruntime_transfer.h"

#include <memory>
#include <sol.hpp>

class RPCRuntimeEncodedFunctionCall;
class QTreeWidgetItem;

struct Device_data {
	QString githash;
	QString gitDate_unix;

	QString serialnumber;
	QString deviceID;
	QString guid;
	QString boardRevision;

	QString name;
	QString version;

	QString get_summary() const;
	void get_lua_data(sol::table &t) const;

	private:
	struct Description_source {
		QString description;
		const QString &source;
	};
	std::vector<Description_source> get_description_source() const;
};

class RPCProtocol : public Protocol {
	public:
	RPCProtocol(CommunicationDevice &device);
	~RPCProtocol();
	RPCProtocol(const RPCProtocol &) = delete;
	RPCProtocol(RPCProtocol &&other);
	bool is_correct_protocol();
	std::unique_ptr<RPCRuntimeDecodedFunctionCall> call_and_wait(const RPCRuntimeEncodedFunctionCall &call,
																 CommunicationDevice::Duration duration = std::chrono::seconds{1});
	const RPCRunTimeProtocolDescription &get_description();
	void set_ui_description(QTreeWidgetItem *ui_entry);
	RPCProtocol &operator=(const RPCProtocol &&) = delete;
	void get_lua_device_descriptor(sol::table &t) const;

	private:
	RPCRunTimeProtocolDescription description;
	RPCRuntimeDecoder decoder;
	RPCRuntimeEncoder encoder;
	Channel_codec_wrapper channel_codec;
	QMetaObject::Connection connection;
	std::unique_ptr<RPCRuntimeDecodedFunctionCall> descriptor_answer;
	CommunicationDevice *device;
	Device_data device_data;
};

#endif // RPCPROTOCOL_H
