#pragma once

#include <Yandex/logger_useful.h>

#include <Poco/Net/StreamSocket.h>

#include <DB/Core/Defines.h>
#include <DB/Core/Block.h>
#include <DB/Core/Progress.h>
#include <DB/Core/Protocol.h>
#include <DB/Core/QueryProcessingStage.h>

#include <DB/IO/ReadBufferFromPocoSocket.h>
#include <DB/IO/WriteBufferFromPocoSocket.h>

#include <DB/DataTypes/DataTypeFactory.h>

#include <DB/DataStreams/IBlockInputStream.h>
#include <DB/DataStreams/IBlockOutputStream.h>


namespace DB
{

using Poco::SharedPtr;


/** Соединение с сервером БД для использования в клиенте.
  * Как использовать - см. Core/Protocol.h
  * (Реализацию на стороне сервера - см. Server/TCPHandler.h)
  *
  * В качестве default_database может быть указана пустая строка
  *  - в этом случае сервер использует свою БД по-умолчанию.
  */
class Connection
{
public:
	Connection(const String & host_, UInt16 port_, const String & default_database_,
		const DataTypeFactory & data_type_factory_,
		const String & client_name_ = "client",
		Protocol::Compression::Enum compression_ = Protocol::Compression::Enable,
		Poco::Timespan connect_timeout_ = Poco::Timespan(DBMS_DEFAULT_CONNECT_TIMEOUT_SEC, 0),
		Poco::Timespan receive_timeout_ = Poco::Timespan(DBMS_DEFAULT_RECEIVE_TIMEOUT_SEC, 0),
		Poco::Timespan send_timeout_ = Poco::Timespan(DBMS_DEFAULT_SEND_TIMEOUT_SEC, 0))
		: host(host_), port(port_), default_database(default_database_), client_name(client_name_), connected(false),
		server_version_major(0), server_version_minor(0), server_revision(0),
		socket(), in(new ReadBufferFromPocoSocket(socket)), out(new WriteBufferFromPocoSocket(socket)),
		query_id(0), compression(compression_), data_type_factory(data_type_factory_),
		connect_timeout(connect_timeout_), receive_timeout(receive_timeout_), send_timeout(send_timeout_),
		log(&Logger::get("Connection (" + Poco::Net::SocketAddress(host, port).toString() + ")"))
	{
		/// Соединеняемся не сразу, а при первой необходимости.
	}

	virtual ~Connection() {};


	/// Пакет, который может быть получен от сервера.
	struct Packet
	{
		UInt64 type;

		Block block;
		SharedPtr<Exception> exception;
		Progress progress;

		Packet() : type(Protocol::Server::Hello) {}
	};

	void getServerVersion(String & name, UInt64 & version_major, UInt64 & version_minor, UInt64 & revision);

	/// Адрес сервера - для сообщений в логе и в эксепшенах.
	String getServerAddress() const;

	/// query_id не должен быть равен 0.
	void sendQuery(const String & query, UInt64 query_id_ = 1, UInt64 stage = QueryProcessingStage::Complete);
	void sendCancel();
	void sendData(const Block & block);

	/// Проверить, если ли данные, которые можно прочитать.
	bool poll(size_t timeout_microseconds = 0);

	/// Получить пакет от сервера.
	Packet receivePacket();

private:
	String host;
	UInt16 port;
	String default_database;

	String client_name;

	bool connected;

	String server_name;
	UInt64 server_version_major;
	UInt64 server_version_minor;
	UInt64 server_revision;
	
	Poco::Net::StreamSocket socket;
	SharedPtr<ReadBufferFromPocoSocket> in;
	SharedPtr<WriteBufferFromPocoSocket> out;

	UInt64 query_id;
	UInt64 compression;		/// Сжимать ли данные при взаимодействии с сервером.

	const DataTypeFactory & data_type_factory;

	Poco::Timespan connect_timeout;
	Poco::Timespan receive_timeout;
	Poco::Timespan send_timeout;

	/// Откуда читать результат выполнения запроса.
	SharedPtr<ReadBuffer> maybe_compressed_in;
	BlockInputStreamPtr block_in;

	/// Куда писать данные INSERT-а.
	SharedPtr<WriteBuffer> maybe_compressed_out;
	BlockOutputStreamPtr block_out;

	Logger * log;

	void connect();
	void disconnect();
	void sendHello();
	void receiveHello();
	void forceConnected();
	bool ping();

	Block receiveData();
	SharedPtr<Exception> receiveException();
	Progress receiveProgress();
};


typedef SharedPtr<Connection> ConnectionPtr;
typedef std::vector<ConnectionPtr> Connections;

}
