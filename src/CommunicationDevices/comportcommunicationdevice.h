#ifndef COMPORTCOMMUNICATIONDEVICE_H
#define COMPORTCOMMUNICATIONDEVICE_H

#include "communicationdevice.h"

#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

class ComportCommunicationDevice : public CommunicationDevice {
	public:
	ComportCommunicationDevice(QString target);
	bool isConnected() override;
	bool connect(const QSerialPortInfo &portinfo, QSerialPort::BaudRate baudrate = QSerialPort::Baud115200);
	bool waitReceived(Duration timeout = std::chrono::seconds(1)) override;
	void send(const QByteArray &data) override;
	void close() override;
	QSerialPort port;
};

#endif // COMPORTCOMMUNICATIONDEVICE_H
