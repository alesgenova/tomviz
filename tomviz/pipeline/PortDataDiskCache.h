/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#ifndef tomvizPipelinePortDataDiskCache_h
#define tomvizPipelinePortDataDiskCache_h

#include "PortData.h"

#include <QString>

namespace tomviz {
namespace pipeline {

/// Serialize a PortData payload to a .tvh5 file on disk.
///
/// Internally builds a transient single-source-single-port Pipeline
/// carrying the payload and writes it through Tvh5Format. That reuses
/// every payload-type round-trip path Tvh5Format already supports
/// (Volume, Table, Molecule, ...) instead of growing a parallel
/// per-type serializer. Returns false on I/O failure or unsupported
/// payload type.
bool writePortDataToFile(const PortData& data, const QString& path);

/// Inverse of writePortDataToFile. Returns an invalid PortData on
/// failure (missing file, malformed contents, unsupported type).
PortData readPortDataFromFile(const QString& path);

} // namespace pipeline
} // namespace tomviz

#endif
