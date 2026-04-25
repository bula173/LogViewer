#pragma once

#include <QAbstractItemModel>
#include <QString>
#include <vector>

namespace ui::qt::ExportManager {

/**
 * @brief Export the given view rows to a CSV file.
 *
 * The first row of the output is a header derived from the model's horizontal
 * header data. Each subsequent row corresponds to one entry in @p rows.
 * Fields containing commas, double-quotes, or newlines are quoted per RFC 4180.
 *
 * @param model  The item model (e.g. EventsTableModel) to read data from.
 * @param rows   View row indices to export (in order).
 * @param path   Destination file path.
 * @return true on success, false if the file could not be opened for writing.
 */
bool ToCsv(const QAbstractItemModel& model, const std::vector<int>& rows,
           const QString& path);

/**
 * @brief Export the given view rows to a JSON file.
 *
 * Output is a JSON array of objects. Each object maps column header names to
 * cell values, e.g. [{"timestamp": "...", "level": "ERROR"}, ...].
 *
 * @param model  The item model to read data from.
 * @param rows   View row indices to export (in order).
 * @param path   Destination file path.
 * @return true on success, false if the file could not be opened for writing.
 */
bool ToJson(const QAbstractItemModel& model, const std::vector<int>& rows,
            const QString& path);

/**
 * @brief Export the given view rows to an XML file.
 *
 * Output is a well-formed XML document with a root <events> element.
 * Each row becomes an <event> child; column values are stored as attributes.
 * Attribute names are derived from the model's horizontal header data.
 *
 * @param model  The item model to read data from.
 * @param rows   View row indices to export (in order).
 * @param path   Destination file path.
 * @return true on success, false if the file could not be opened for writing.
 */
bool ToXml(const QAbstractItemModel& model, const std::vector<int>& rows,
           const QString& path);

} // namespace ui::qt::ExportManager
