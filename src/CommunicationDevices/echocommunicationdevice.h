#ifndef ECHOCOMMUNICATIONDEVICE_H
#define ECHOCOMMUNICATIONDEVICE_H

#include "communicationdevice.h"
#include "export.h"
#include <QObject>

class EXPORT EchoCommunicationDevice final : public CommunicationDevice {
	Q_OBJECT
	public:
	EchoCommunicationDevice();
	void send(const QByteArray &data) override;
	bool isConnected() override;
	bool waitReceived(Duration timeout = std::chrono::seconds(1)) override;
	void close() override;
};

#endif // ECHOCOMMUNICATIONDEVICE_H
