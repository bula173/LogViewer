#include "ExportManager.hpp"

#include "Error.hpp"
#include "Logger.hpp"
#include "Result.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QXmlStreamWriter>

namespace ui::qt::ExportManager {

namespace {

// Collect column header strings from the model.
QStringList CollectHeaders(const QAbstractItemModel& model)
{
    QStringList headers;
    const int cols = model.columnCount();
    headers.reserve(cols);
    for (int c = 0; c < cols; ++c)
        headers << model.headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();
    return headers;
}

// Quote a single CSV field per RFC 4180:
// Wrap in double-quotes if the value contains comma, double-quote, or newline.
// Escape embedded double-quotes by doubling them.
QString QuoteCsvField(const QString& value)
{
    if (!value.contains(QLatin1Char(',')) && !value.contains(QLatin1Char('"')) &&
        !value.contains(QLatin1Char('\n')) && !value.contains(QLatin1Char('\r')))
    {
        return value;
    }
    return QLatin1Char('"') + QString(value).replace(QLatin1Char('"'), QStringLiteral("\"\"")) +
           QLatin1Char('"');
}

} // namespace

util::Result<void, error::Error> ToCsv(const QAbstractItemModel& model,
                                        const std::vector<int>& rows,
                                        const QString& path)
{
    using R = util::Result<void, error::Error>;

    util::Logger::Debug("[ExportManager] ToCsv start: {} rows -> {}",
                        rows.size(), path.toStdString());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        const std::string msg = "Failed to open file for CSV export: " + path.toStdString();
        util::Logger::Error("[ExportManager] {}", msg);
        return R::Err(error::Error(error::ErrorCode::IOError, msg, /*showMsgBox=*/false));
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);

    const QStringList headers = CollectHeaders(model);
    const int cols = model.columnCount();

    // Header row
    QStringList headerFields;
    headerFields.reserve(cols);
    for (const auto& h : headers)
        headerFields << QuoteCsvField(h);
    out << headerFields.join(QLatin1Char(',')) << QLatin1Char('\n');

    // Data rows
    for (const int row : rows)
    {
        QStringList fields;
        fields.reserve(cols);
        for (int c = 0; c < cols; ++c)
        {
            const QString value =
                model.data(model.index(row, c), Qt::DisplayRole).toString();
            fields << QuoteCsvField(value);
        }
        out << fields.join(QLatin1Char(',')) << QLatin1Char('\n');
    }

    util::Logger::Info("[ExportManager] CSV export complete: {} rows -> {}",
                       rows.size(), path.toStdString());
    return R::Ok({});
}

util::Result<void, error::Error> ToJson(const QAbstractItemModel& model,
                                         const std::vector<int>& rows,
                                         const QString& path)
{
    using R = util::Result<void, error::Error>;

    util::Logger::Debug("[ExportManager] ToJson start: {} rows -> {}",
                        rows.size(), path.toStdString());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        const std::string msg = "Failed to open file for JSON export: " + path.toStdString();
        util::Logger::Error("[ExportManager] {}", msg);
        return R::Err(error::Error(error::ErrorCode::IOError, msg, /*showMsgBox=*/false));
    }

    const QStringList headers = CollectHeaders(model);
    const int cols = model.columnCount();

    QJsonArray array;

    for (const int row : rows)
    {
        QJsonObject obj;
        for (int c = 0; c < cols; ++c)
        {
            const QString value =
                model.data(model.index(row, c), Qt::DisplayRole).toString();
            obj.insert(headers[c], value);
        }
        array.append(obj);
    }

    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));

    util::Logger::Info("[ExportManager] JSON export complete: {} rows -> {}",
                       rows.size(), path.toStdString());
    return R::Ok({});
}

util::Result<void, error::Error> ToXml(const QAbstractItemModel& model,
                                        const std::vector<int>& rows,
                                        const QString& path)
{
    using R = util::Result<void, error::Error>;

    util::Logger::Debug("[ExportManager] ToXml start: {} rows -> {}",
                        rows.size(), path.toStdString());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        const std::string msg = "Failed to open file for XML export: " + path.toStdString();
        util::Logger::Error("[ExportManager] {}", msg);
        return R::Err(error::Error(error::ErrorCode::IOError, msg, /*showMsgBox=*/false));
    }

    const QStringList headers = CollectHeaders(model);
    const int cols = model.columnCount();

    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("events"));

    for (const int row : rows)
    {
        xml.writeStartElement(QStringLiteral("event"));
        for (int c = 0; c < cols; ++c)
        {
            const QString value =
                model.data(model.index(row, c), Qt::DisplayRole).toString();
            xml.writeAttribute(headers[c], value);
        }
        xml.writeEndElement(); // </event>
    }

    xml.writeEndElement(); // </events>
    xml.writeEndDocument();

    util::Logger::Info("[ExportManager] XML export complete: {} rows -> {}",
                       rows.size(), path.toStdString());
    return R::Ok({});
}

} // namespace ui::qt::ExportManager
