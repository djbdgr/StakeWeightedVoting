#include "BitsharesWalletBridge.hpp"
#include <Utilities.hpp>

#include <kj/debug.h>

#include <memory>

namespace swv { namespace bts {

BitsharesWalletBridge::BitsharesWalletBridge(const QString& serverName)
    : QWebSocketServer (serverName, QWebSocketServer::SslMode::NonSecureMode) {}

BitsharesWalletBridge::~BitsharesWalletBridge() {}

////////////////////////////// BEGIN BlockchainWalletServer implementation
using BWB = BitsharesWalletBridge;
class BWB::BlockchainWalletServer : public BlockchainWallet::Server {
    std::unique_ptr<QWebSocket> connection;
    int64_t nextQueryId = 0;
    std::map<int64_t, kj::Own<kj::PromiseFulfiller<QJsonValue>>> pendingRequests;

public:
    BlockchainWalletServer(std::unique_ptr<QWebSocket> connection);
    virtual ~BlockchainWalletServer() {}

protected:
    void checkConnection();
    kj::Promise<QJsonValue> beginCall(QString method, QJsonArray params);
    void finishCall(QString message);

    // BlockchainWallet::Server interface
    virtual ::kj::Promise<void> getCoinById(GetCoinByIdContext context) override;
    virtual ::kj::Promise<void> getCoinBySymbol(GetCoinBySymbolContext context) override;
    virtual ::kj::Promise<void> getAllCoins(GetAllCoinsContext context) override;
    virtual ::kj::Promise<void> listMyAccounts(ListMyAccountsContext context) override;
    virtual ::kj::Promise<void> getBalance(GetBalanceContext context) override;
    virtual ::kj::Promise<void> getBalancesBelongingTo(GetBalancesBelongingToContext context) override;
    virtual ::kj::Promise<void> getContestById(GetContestByIdContext context) override;
    virtual ::kj::Promise<void> getDatagramByBalance(GetDatagramByBalanceContext context) override;
    virtual ::kj::Promise<void> publishDatagram(PublishDatagramContext context) override;
    virtual ::kj::Promise<void> transfer(TransferContext context) override;
};

BWB::BlockchainWalletServer::BlockchainWalletServer(std::unique_ptr<QWebSocket> connection)
    : connection(kj::mv(connection)) {
    KJ_REQUIRE(this->connection && this->connection->state() == QAbstractSocket::SocketState::ConnectedState,
               "Internal Error: Attempted to create Bitshares blockchain wallet server with no connection to a "
               "Bitshares wallet");
    connect(this->connection.get(), &QWebSocket::textMessageReceived, [this](QString message) {
        finishCall(message);
    });
    connect(this->connection.get(), &QWebSocket::disconnected, [this] {
        if (!pendingRequests.empty()) {
            KJ_LOG(WARNING, "Connection to Bitshares wallet lost while requests pending", pendingRequests.size());
            while (!pendingRequests.empty()) {
                pendingRequests.begin()->second->reject(KJ_EXCEPTION(DISCONNECTED,
                                                                     "Connection to Bitshares wallet lost"));
                pendingRequests.erase(pendingRequests.begin());
            }
        }
    });

    // Someday we can implment encrypted communication with the BTS wallet, which would be negotiated here.
}

void BWB::BlockchainWalletServer::checkConnection() {
    if (!connection || connection->state() != QAbstractSocket::SocketState::ConnectedState)
        throw KJ_EXCEPTION(DISCONNECTED, "Connection to Bitshares wallet has failed.");
}

void BWB::BlockchainWalletServer::finishCall(QString message) {
    QJsonParseError error;
    auto response = QJsonDocument::fromJson(message.toLocal8Bit(), &error).object();

    if (error.error != QJsonParseError::ParseError::NoError || !response.contains("id") ||
            (!response.contains("error") && !response.contains("result"))) {
        KJ_LOG(ERROR, "Got unrecognizeable message back from Bitshares wallet", message.toStdString());
        return;
    }

    auto itr = pendingRequests.find(response["id"].toVariant().toLongLong());
    if (itr == pendingRequests.end()) {
        KJ_LOG(ERROR, "Got response from Bitshares wallet, but the ID doesn't match any outstanding call",
               message.toStdString());
        return;
    }

    if (response.contains("result"))
        itr->second->fulfill(response["result"]);
    else
        itr->second->reject(KJ_EXCEPTION(FAILED,
                                         QString(QJsonDocument(response["error"].toObject()).toJson()).toStdString()));
    pendingRequests.erase(itr);
}

kj::Promise<QJsonValue> BWB::BlockchainWalletServer::beginCall(QString method, QJsonArray params) {
    checkConnection();


    QJsonObject call {
        {"jsonrpc", "2.0"},
        {"method",  method},
        {"params",  params},
        {"id",      qint64(nextQueryId)}
    };
    connection->sendTextMessage(QJsonDocument(call).toJson(QJsonDocument::JsonFormat::Compact));

    // Create a new PendingRequest, consisting of
    auto paf = kj::newPromiseAndFulfiller<QJsonValue>();
    pendingRequests.emplace(std::make_pair(nextQueryId++, kj::mv(paf.fulfiller)));
    return kj::mv(paf.promise);
}

void populateCoin(::Coin::Builder builder, QJsonObject coin) {
    builder.setId(coin["id"].toString().replace("1.3.", QString::null).toULongLong());
    builder.setCreator(coin["issuer"].toString().toStdString());
    builder.setName(coin["symbol"].toString().toStdString());
    builder.setPrecision(coin["precision"].toInt());
}

kj::Promise<void> BWB::BlockchainWalletServer::getCoinById(GetCoinByIdContext context) {
    KJ_LOG(DBG, __FUNCTION__);
    auto id = QStringLiteral("1.3.%1").arg(context.getParams().getId());
    return beginCall("blockchain.getObjectById", QJsonArray() << id).then([context](QJsonValue response) mutable {
        populateCoin(context.initResults().initCoin(), response.toObject());
    });
}

kj::Promise<void> BWB::BlockchainWalletServer::getCoinBySymbol(GetCoinBySymbolContext context) {
    KJ_LOG(DBG, __FUNCTION__);
    auto symbol = QString::fromStdString(context.getParams().getSymbol());
    return beginCall("blockchain.getAssetBySymbol",
                     QJsonArray() << symbol).then([context](QJsonValue response) mutable {
        populateCoin(context.initResults().initCoin(), response.toObject());
    });
}

kj::Promise<void> BWB::BlockchainWalletServer::getAllCoins(GetAllCoinsContext context) {
    KJ_LOG(DBG, __FUNCTION__);
    return beginCall("blockchain.getAllAssets", {}).then([context](QJsonValue response) mutable {
        auto assets = response.toArray();
        auto coins = context.initResults().initCoins(assets.size());
        auto index = 0u;
        for (const auto& asset : assets)
            populateCoin(coins[index++], asset.toObject());
    });
}

kj::Promise<void> BWB::BlockchainWalletServer::listMyAccounts(ListMyAccountsContext context) {
    KJ_LOG(DBG, __FUNCTION__);
    return beginCall("wallet.getMyAccounts", {}).then([this, context](QJsonValue response) mutable {
        auto accounts = response.toArray();
        auto results = context.initResults().initAccountNames(accounts.size());
        auto index = 0u;
        for (const auto& account : accounts) {
            results.set(index++, account.toString().toStdString());
        }
    });
}

kj::Promise<void> BWB::BlockchainWalletServer::getBalance(GetBalanceContext context) {
    KJ_LOG(DBG, __FUNCTION__);
    auto balanceId = context.getParams().getId();
    auto accountId = QStringLiteral("1.2.%1").arg(balanceId.getAccountInstance());
    auto coinId = QStringLiteral("1.3.%1").arg(balanceId.getCoinInstance());
    return beginCall("blockchain.getObjectById",
                     QJsonArray() << accountId).then([this](QJsonValue response) {
        return beginCall("blockchain.getAccountBalances", QJsonArray() << response.toObject()["name"].toString());
    }).then([context, coinId, balanceId](QJsonValue response) mutable {
        auto balances = response.toArray();
        auto balanceItr = std::find_if(balances.begin(), balances.end(), [coinId](const QJsonValue& balance ) {
            return balance.toObject()["type"].toString() == coinId;
        });
        KJ_REQUIRE(balanceItr != balances.end(), "No such balance");
        auto result = context.initResults().initBalance();
        result.setId(context.getParams().getId());
        result.setAmount(balanceItr->toObject()["amount"].toVariant().toLongLong());
        result.setType(balanceItr->toObject()["type"].toString().replace("1.3.", "").toULongLong());
        result.setCreationOrder(balanceId.getAccountInstance());
    });
}

kj::Promise<void> BWB::BlockchainWalletServer::getBalancesBelongingTo(GetBalancesBelongingToContext context) {
    KJ_LOG(DBG, __FUNCTION__);
    auto owner = QString::fromStdString(context.getParams().getOwner());
    auto accountPromise = beginCall("blockchain.getAccountByName", QJsonArray() << owner);
    return beginCall("blockchain.getAccountBalances", QJsonArray() << owner)
            .then([context, accountPromise = kj::mv(accountPromise)](QJsonValue response) mutable {
        return accountPromise.then([context, response](QJsonValue accountValue) mutable {
            auto accountId = accountValue.toObject()["id"].toString();
            auto balances = response.toArray();
            auto results = context.initResults().initBalances(balances.size());
            auto index = 0u;
            for (const auto& balance : balances) {
                auto balanceObject = balance.toObject();
                auto result = results[index++];
                result.getId().setAccountInstance(accountId.replace("1.2.", "").toULongLong());
                result.getId().setCoinInstance(balanceObject["type"].toString().replace("1.3.", "").toULongLong());
                result.setAmount(balanceObject["amount"].toVariant().toULongLong());
                result.setType(result.getId().getCoinInstance());
                result.setCreationOrder(result.getId().getAccountInstance());
            }
        });
    });
}

kj::Promise<void> BWB::BlockchainWalletServer::getContestById(GetContestByIdContext context) {
    KJ_LOG(DBG, __FUNCTION__);
    return beginCall({}, {}).then([](auto){});
}

kj::Promise<void> BWB::BlockchainWalletServer::getDatagramByBalance(GetDatagramByBalanceContext context) {
    KJ_LOG(DBG, __FUNCTION__);
    return beginCall({}, {}).then([](auto){});
}

kj::Promise<void> BWB::BlockchainWalletServer::publishDatagram(PublishDatagramContext context) {
    KJ_LOG(DBG, __FUNCTION__);
    return beginCall({}, {}).then([](auto){});
}

kj::Promise<void> BWB::BlockchainWalletServer::transfer(TransferContext context) {
    KJ_LOG(DBG, __FUNCTION__);
    return beginCall({}, {}).then([](auto){});
}
////////////////////////////// END BlockchainWalletServer implementation

kj::Promise<BlockchainWallet::Client> BitsharesWalletBridge::nextWalletClient() {
    if (hasPendingConnections()) {
        auto server = kj::heap<BlockchainWalletServer>(std::unique_ptr<QWebSocket>(nextPendingConnection()));
        return BlockchainWallet::Client(kj::mv(server));
    }

    auto paf = kj::newPromiseAndFulfiller<BlockchainWallet::Client>();
    // Use shared_ptr's to avoid making a noncopyable lambda
    auto connection = std::make_shared<QMetaObject::Connection>();
    auto fulfiller = std::make_shared<decltype(paf.fulfiller)>(kj::mv(paf.fulfiller));
    *connection = connect(this, &QWebSocketServer::newConnection, [this, connection, fulfiller]() mutable {
        disconnect(*connection);
        connection.reset();
        KJ_LOG(DBG, "Fulfilling promise for a BlockchainWallet client");
        std::unique_ptr<QWebSocket> peer(nextPendingConnection());
        qDebug() << "Connection from" << peer->peerName() << "at" << peer->peerAddress() << ":" << peer->peerPort();
        (*fulfiller)->fulfill(kj::heap<BlockchainWalletServer>(kj::mv(peer)));
        fulfiller.reset();
    });
    KJ_LOG(DBG, "Promising a BlockchainWallet client");
    return kj::mv(paf.promise);
}

} } // namespace swv::bts
