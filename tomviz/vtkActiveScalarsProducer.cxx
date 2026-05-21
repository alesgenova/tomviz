/* This source file is part of the Tomviz project, https://tomviz.org/.
   It is released under the 3-Clause BSD License, see "LICENSE". */

#include "vtkActiveScalarsProducer.h"

#include <vtkDataArray.h>
#include <vtkExecutive.h>
#include <vtkGarbageCollector.h>
#include <vtkImageData.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>

vtkStandardNewMacro(vtkActiveScalarsProducer)

vtkActiveScalarsProducer::vtkActiveScalarsProducer()
{
}

vtkActiveScalarsProducer::~vtkActiveScalarsProducer()
{
  this->SetOutput(nullptr);
}

//----------------------------------------------------------------------------
void vtkActiveScalarsProducer::SetOutput(vtkDataObject* newOutput)
{
  auto oldOutput = this->OriginalOutput;
  if (newOutput != oldOutput) {
    // Release the previous internal Output before overwriting. Without
    // this, every re-execution leaks the previous wrapper vtkImageData,
    // which still holds (via its ShallowCopy'd PointData) refs to the
    // previous image's scalar arrays — pinning the entire previous voxel
    // payload in memory.
    if (this->Output) {
      this->Output->Delete();
      this->Output = nullptr;
    }

    if (newOutput) {
      newOutput->Register(this);
      this->Output = vtkImageData::New();
      this->Output->ShallowCopy(newOutput);
    }

    this->OriginalOutput = newOutput;

    this->GetExecutive()->SetOutputData(0, this->Output);
    if (oldOutput) {
      oldOutput->UnRegister(this);
    }
    this->Modified();
  }
}

void vtkActiveScalarsProducer::SetActiveScalars(const char* name)
{
  auto data = vtkImageData::SafeDownCast(this->Output);
  if (data) {
    data->GetPointData()->SetActiveScalars(name);
    data->Modified();
  }
}

vtkMTimeType vtkActiveScalarsProducer::GetMTime()
{
  auto mtime = this->Superclass::GetMTime();
  if (this->OriginalOutput) {
    auto omtime = this->OriginalOutput->GetMTime();
    if (omtime > mtime) {
      mtime = omtime;
    }
  }
  if (this->Output) {
    auto omtime = this->Output->GetMTime();
    if (omtime >= mtime) {
      mtime = omtime;
    } else {
      this->ReSync();
    }
  }
  return mtime;
}

//----------------------------------------------------------------------------
void vtkActiveScalarsProducer::ReSync()
{
  auto data = vtkImageData::SafeDownCast(this->Output);
  auto originalData = vtkImageData::SafeDownCast(this->OriginalOutput);
  if (data && originalData) {
    auto originalActiveScalars =
      std::string(originalData->GetPointData()->GetScalars()->GetName());
    auto currenActiveScalars =
      std::string(data->GetPointData()->GetScalars()->GetName());
    data->ShallowCopy(originalData);
    data->GetPointData()->SetActiveScalars(currenActiveScalars.c_str());
  }
}

//----------------------------------------------------------------------------
void vtkActiveScalarsProducer::ReportReferences(vtkGarbageCollector* collector)
{
  this->Superclass::ReportReferences(collector);
  vtkGarbageCollectorReport(collector, this->OriginalOutput, "OriginalOutput");
}
