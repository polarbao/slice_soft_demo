#include "slicer_core/reports/ReportSchema.h"

namespace slicer_core
{

ReportSchemaRef PreviewReportSchemaRef()
{
    return ReportSchemaRef{"p0.preview_report.1", "preview_report.json"};
}

}  // namespace slicer_core
