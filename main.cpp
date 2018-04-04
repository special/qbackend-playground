#include <QGuiApplication>
#include <QAbstractListModel>
#include <QProcess>
#include <QQmlEngine>
#include <QQuickView>

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

class OutOfProcessDataRepo : public QAbstractListModel
{
    Q_OBJECT
public:
    OutOfProcessDataRepo();

    Q_INVOKABLE void write(const QByteArray& data);

protected:
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex&) const override;
    QVariant data(const QModelIndex &index, int role) const override;

private slots:
    void handleModelDataReady();

private:
    QProcess m_process;
    QHash<int, QByteArray> m_roleNames;
    QVector<QByteArray> m_roleNamesInOrder;
    QVector<QVector<QVariant>> m_data;
};

OutOfProcessDataRepo::OutOfProcessDataRepo()
    : QAbstractListModel(0)
{
    m_process.start("go", QStringList() << "run" << "test.go");

    while (true) {
        m_process.waitForReadyRead();
        if (m_process.canReadLine()) {
            QByteArray roleBuf = m_process.readLine();
            roleBuf.truncate(roleBuf.length() - 1);

            Q_ASSERT(roleBuf.startsWith("ROLES"));

            QList<QByteArray> roles = roleBuf.split(' ');

            bool first = true;
            int i = 0;
            for (const QByteArray &ba : roles) {
                if (first == true) {
                    first = false;
                    continue;
                }
                m_roleNames.insert(Qt::UserRole + i++, ba);
                m_roleNamesInOrder.append(ba);
            }
            //qWarning() << m_roleNamesInOrder;
            handleModelDataReady();
            break;
        }

    }

    connect(&m_process, &QIODevice::readyRead, this, &OutOfProcessDataRepo::handleModelDataReady);
}

void OutOfProcessDataRepo::write(const QByteArray& data)
{
    qWarning() << "Sending " << data;
    m_process.write(data);
}

void OutOfProcessDataRepo::handleModelDataReady()
{
    while (m_process.canReadLine()) {
        QByteArray cmdBuf = m_process.readLine();
        qDebug() << "Read " << cmdBuf;
        if (cmdBuf == "\n") {
            // ignore
        } else if (cmdBuf.startsWith("APPEND ")) {
            // Determine length to read.
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);
            QList<QByteArray> parts = cmdBuf.split(' ');
            Q_ASSERT(parts.length() == 2);
            int bytes = parts[1].toInt();

            // Read JSON data
            cmdBuf = m_process.read(bytes);

            QJsonDocument doc = QJsonDocument::fromJson(cmdBuf);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();

                QVector<QVariant> data;

                for (const QByteArray& role : m_roleNamesInOrder) {
                    data.append(obj.value(role).toVariant());
                }

                beginInsertRows(QModelIndex(), m_data.count(), m_data.count());
                m_data.append(data);
                endInsertRows();
            } else {
                Q_UNREACHABLE(); // consider isArray for appending in bulk
            }
        } else if (cmdBuf.startsWith("REMOVE ")) {
            int rowId = QByteArray::fromRawData(cmdBuf.data() + 7, cmdBuf.length()-8).toInt();
            beginRemoveRows(QModelIndex(), rowId, rowId);
            m_data.remove(rowId);
            endRemoveRows();
        } else if (cmdBuf.startsWith("UPDATE ")) {
            // Determine length to read.
            // First, remove the newline.
            cmdBuf.truncate(cmdBuf.length() - 1);
            QList<QByteArray> parts = cmdBuf.split(' ');
            Q_ASSERT(parts.length() == 2);
            int rowId = parts[1].toInt();
            int bytes = parts[2].toInt();

            // Read JSON data
            cmdBuf = m_process.read(bytes);

            qWarning() << "Updating " << rowId;

            QJsonDocument doc = QJsonDocument::fromJson(cmdBuf);
            Q_ASSERT(doc.isObject());
            QJsonObject obj = doc.object();

            QVector<QVariant> data;
            QVector<int> roles;
            int i = 0;

            for (const QByteArray& role : m_roleNamesInOrder) {
                QJsonValue val = obj.value(role);
                if (val == QJsonValue::Undefined) {
                    i++;
                    continue;
                }

                qWarning() << "Changed data for " << rowId << role << " to " << val.toVariant();
                m_data[rowId][i] = val.toVariant();
                roles.append(i + Qt::UserRole);
                i++;
            }


            dataChanged(index(rowId, 0), index(rowId, 0), roles);
        } else if (cmdBuf.startsWith("DEBUG ")) {
            qDebug() << "Debug! " << cmdBuf;
        } else {
            Q_UNREACHABLE();
        }
    }
}

QHash<int, QByteArray> OutOfProcessDataRepo::roleNames() const
{
    return m_roleNames;
}

int OutOfProcessDataRepo::rowCount(const QModelIndex&) const
{
    return m_data.size();
}

QVariant OutOfProcessDataRepo::data(const QModelIndex &index, int role) const
{
    return m_data.at(index.row()).at(role - Qt::UserRole);
}

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    qmlRegisterType<OutOfProcessDataRepo>("com.me", 1, 0, "OutOfProcessDataRepo");
    QQuickView view(QUrl::fromLocalFile("main.qml"));


    view.show();
    return app.exec();
}

#include "main.moc"
