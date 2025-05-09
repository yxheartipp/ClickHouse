#pragma once

#include <Common/PoolBase.h>
#include <Common/Priority.h>
#include <Client/Connection.h>
#include <IO/ConnectionTimeouts.h>
#include <base/defines.h>

namespace DB
{

struct Settings;

/** Interface for connection pools.
  *
  * Usage (using the usual `ConnectionPool` example)
  * ConnectionPool pool(...);
  *
  *    void thread()
  *    {
  *        auto connection = pool.get();
  *        connection->sendQuery(...);
  *    }
  */

class IConnectionPool : private boost::noncopyable
{
public:
    using Entry = PoolBase<Connection>::Entry;

    IConnectionPool() = default;
    IConnectionPool(String host_, UInt16 port_, Priority config_priority_)
        : host(host_), port(port_), address(host + ":" + toString(port_)), config_priority(config_priority_)
    {
    }

    virtual ~IConnectionPool() = default;

    /// Selects the connection to work.
    virtual Entry get(const ConnectionTimeouts & timeouts) = 0;
    /// If force_connected is false, the client must manually ensure that returned connection is good.
    virtual Entry get(const ConnectionTimeouts & timeouts, /// NOLINT
                      const Settings & settings,
                      bool force_connected = true) = 0;

    const std::string & getHost() const { return host; }
    UInt16 getPort() const { return port; }
    const String & getAddress() const { return address; }
    Priority getConfigPriority() const { return config_priority; }

protected:
    const String host;
    const UInt16 port = 0;
    const String address;
    const Priority config_priority;
};

using ConnectionPoolPtr = std::shared_ptr<IConnectionPool>;
using ConnectionPoolPtrs = std::vector<ConnectionPoolPtr>;

/** A common connection pool, without fault tolerance.
  */
class ConnectionPool : public IConnectionPool, private PoolBase<Connection>
{
public:
    using Entry = IConnectionPool::Entry;
    using Base = PoolBase<Connection>;

    ConnectionPool(
        unsigned max_connections_,
        const String & host_,
        UInt16 port_,
        const String & default_database_,
        const String & user_,
        const String & password_,
        const String & proto_send_chunked_,
        const String & proto_recv_chunked_,
        const String & quota_key_,
        const String & cluster_,
        const String & cluster_secret_,
        const String & client_name_,
        Protocol::Compression compression_,
        Protocol::Secure secure_,
        const String & bind_host_,
        Priority config_priority_ = Priority{1})
        : IConnectionPool(host_, port_, config_priority_)
        , Base(max_connections_, getLogger("ConnectionPool (" + host_ + ":" + toString(port_) + ")"))
        , default_database(default_database_)
        , user(user_)
        , password(password_)
        , proto_send_chunked(proto_send_chunked_)
        , proto_recv_chunked(proto_recv_chunked_)
        , quota_key(quota_key_)
        , cluster(cluster_)
        , cluster_secret(cluster_secret_)
        , client_name(client_name_)
        , compression(compression_)
        , secure(secure_)
        , bind_host(bind_host_)
    {
    }

    Entry get(const ConnectionTimeouts & timeouts) override
    {
        Entry entry = Base::get(-1);
        entry->forceConnected(timeouts);
        return entry;
    }

    Entry get(const ConnectionTimeouts & timeouts, /// NOLINT
              const Settings & settings,
              bool force_connected) override;

    std::string getDescription() const
    {
        return host + ":" + toString(port);
    }

protected:
    /** Creates a new object to put in the pool. */
    ConnectionPtr allocObject() override
    {
        return std::make_shared<Connection>(
            host, port,
            default_database, user, password,
            proto_send_chunked, proto_recv_chunked,
            SSHKey(), /*jwt*/ "", quota_key,
            cluster, cluster_secret,
            client_name, compression, secure, bind_host);
    }

private:
    String default_database;
    String user;
    String password;
    String proto_send_chunked;
    String proto_recv_chunked;
    String quota_key;

    /// For inter-server authorization
    String cluster;
    String cluster_secret;

    String client_name;
    Protocol::Compression compression; /// Whether to compress data when interacting with the server.
    Protocol::Secure secure;           /// Whether to encrypt data when interacting with the server.
    String bind_host;
};

/**
 * Connection pool factory. Responsible for creating new connection pools and reuse existing ones.
 */
class ConnectionPoolFactory final : private boost::noncopyable
{
public:
    struct Key
    {
        unsigned max_connections;
        String host;
        UInt16 port;
        String default_database;
        String user;
        String password;
        String proto_send_chunked;
        String proto_recv_chunked;
        String quota_key;
        String cluster;
        String cluster_secret;
        String client_name;
        Protocol::Compression compression;
        Protocol::Secure secure;
        String bind_host;
        Priority priority;
    };

    struct KeyHash
    {
        size_t operator()(const ConnectionPoolFactory::Key & k) const;
    };

    static ConnectionPoolFactory & instance();

    ConnectionPoolPtr
    get(unsigned max_connections,
        String host,
        UInt16 port,
        String default_database,
        String user,
        String password,
        String proto_send_chunked,
        String proto_recv_chunked,
        String quota_key,
        String cluster,
        String cluster_secret,
        String client_name,
        Protocol::Compression compression,
        Protocol::Secure secure,
        String bind_host,
        Priority priority);
private:
    mutable std::mutex mutex;
    using ConnectionPoolWeakPtr = std::weak_ptr<IConnectionPool>;
    std::unordered_map<Key, ConnectionPoolWeakPtr, KeyHash> pools TSA_GUARDED_BY(mutex);
};

inline bool operator==(const ConnectionPoolFactory::Key & lhs, const ConnectionPoolFactory::Key & rhs)
{
    return lhs.max_connections == rhs.max_connections && lhs.host == rhs.host && lhs.port == rhs.port
        && lhs.default_database == rhs.default_database && lhs.user == rhs.user && lhs.password == rhs.password
        && lhs.proto_send_chunked == rhs.proto_send_chunked && lhs.proto_recv_chunked == rhs.proto_recv_chunked
        && lhs.quota_key == rhs.quota_key
        && lhs.cluster == rhs.cluster && lhs.cluster_secret == rhs.cluster_secret && lhs.client_name == rhs.client_name
        && lhs.compression == rhs.compression && lhs.secure == rhs.secure && lhs.bind_host == rhs.bind_host && lhs.priority == rhs.priority;
}

}
