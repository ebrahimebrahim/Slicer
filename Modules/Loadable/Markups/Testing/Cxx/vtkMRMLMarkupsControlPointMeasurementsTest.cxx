/*==============================================================================

  Program: 3D Slicer

  Portions (c) Copyright Brigham and Women's Hospital (BWH) All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

==============================================================================*/

// MRML includes
#include "vtkCurveMeasurementsCalculator.h"
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLMarkupsCurveNode.h"
#include "vtkMRMLMarkupsFiducialNode.h"
#include "vtkMRMLMeasurementLength.h"
#include "vtkMRMLStaticMeasurement.h"

// VTK includes
#include <vtkCallbackCommand.h>
#include <vtkDataObject.h>
#include <vtkDoubleArray.h>
#include <vtkInformation.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{

class vtkMRMLMarkupsFiducialNodeTestHelper : public vtkMRMLMarkupsFiducialNode
{
public:
  static vtkMRMLMarkupsFiducialNodeTestHelper* New();
  vtkTypeMacro(vtkMRMLMarkupsFiducialNodeTestHelper, vtkMRMLMarkupsFiducialNode);

  vtkMTimeType GetStorableModifiedTimeForTesting() { return this->StorableModifiedTime.GetMTime(); }

protected:
  vtkMRMLMarkupsFiducialNodeTestHelper() = default;
  ~vtkMRMLMarkupsFiducialNodeTestHelper() override = default;
};

vtkStandardNewMacro(vtkMRMLMarkupsFiducialNodeTestHelper);

class vtkMRMLMarkupsCurveNodeTestHelper : public vtkMRMLMarkupsCurveNode
{
public:
  static vtkMRMLMarkupsCurveNodeTestHelper* New();
  vtkTypeMacro(vtkMRMLMarkupsCurveNodeTestHelper, vtkMRMLMarkupsCurveNode);

  vtkCurveMeasurementsCalculator* GetCurveMeasurementsCalculatorForTesting() { return this->CurveMeasurementsCalculator; }

protected:
  vtkMRMLMarkupsCurveNodeTestHelper() = default;
  ~vtkMRMLMarkupsCurveNodeTestHelper() override = default;
};

vtkStandardNewMacro(vtkMRMLMarkupsCurveNodeTestHelper);

class vtkMRMLMeasurementWithControlPointOutput : public vtkMRMLMeasurement
{
public:
  static vtkMRMLMeasurementWithControlPointOutput* New();
  vtkTypeMacro(vtkMRMLMeasurementWithControlPointOutput, vtkMRMLMeasurement);

  vtkMRMLMeasurement* CreateInstance() const override { return vtkMRMLMeasurementWithControlPointOutput::New(); }

  void Compute() override
  {
    ++this->ComputeCount;
    vtkNew<vtkDoubleArray> values;
    values->InsertNextValue(static_cast<double>(this->ComputeCount));
    this->SetControlPointValues(values);
  }

  int GetComputeCount() const { return this->ComputeCount; }

protected:
  vtkMRMLMeasurementWithControlPointOutput() = default;
  ~vtkMRMLMeasurementWithControlPointOutput() override = default;

private:
  int ComputeCount{ 0 };
};

vtkStandardNewMacro(vtkMRMLMeasurementWithControlPointOutput);

//----------------------------------------------------------------------------
vtkSmartPointer<vtkDoubleArray> CreateArray(int numberOfComponents, const std::vector<double>& values)
{
  vtkSmartPointer<vtkDoubleArray> array = vtkSmartPointer<vtkDoubleArray>::New();
  array->SetNumberOfComponents(numberOfComponents);
  array->SetNumberOfTuples(static_cast<vtkIdType>(values.size()) / numberOfComponents);
  for (vtkIdType valueIndex = 0; valueIndex < static_cast<vtkIdType>(values.size()); ++valueIndex)
  {
    array->SetValue(valueIndex, values[static_cast<size_t>(valueIndex)]);
  }
  return array;
}

//----------------------------------------------------------------------------
bool CheckArray(vtkDoubleArray* array, int expectedNumberOfComponents, const std::vector<double>& expectedValues, const std::string& description)
{
  if (!array)
  {
    std::cerr << description << ": array is null" << std::endl;
    return false;
  }
  if (array->GetNumberOfComponents() != expectedNumberOfComponents)
  {
    std::cerr << description << ": expected " << expectedNumberOfComponents << " components, got " << array->GetNumberOfComponents() << std::endl;
    return false;
  }
  if (array->GetNumberOfValues() != static_cast<vtkIdType>(expectedValues.size()))
  {
    std::cerr << description << ": expected " << expectedValues.size() << " values, got " << array->GetNumberOfValues() << std::endl;
    return false;
  }
  for (vtkIdType valueIndex = 0; valueIndex < static_cast<vtkIdType>(expectedValues.size()); ++valueIndex)
  {
    const double actualValue = array->GetValue(valueIndex);
    const double expectedValue = expectedValues[static_cast<size_t>(valueIndex)];
    if ((std::isnan(expectedValue) && !std::isnan(actualValue)) || (!std::isnan(expectedValue) && actualValue != expectedValue))
    {
      std::cerr << description << ": expected value " << expectedValue << " at flat index " << valueIndex << ", got " << actualValue << std::endl;
      return false;
    }
  }
  return true;
}

//----------------------------------------------------------------------------
bool CheckArrayMetadata(vtkDoubleArray* array, const std::string& description)
{
  if (!array || !array->GetName() || std::string(array->GetName()) != "VectorValues")
  {
    std::cerr << description << ": array name was not preserved" << std::endl;
    return false;
  }

  const std::vector<std::string> expectedComponentNames{ "X value", "Y value", "Z value" };
  if (array->GetNumberOfComponents() != static_cast<int>(expectedComponentNames.size()))
  {
    std::cerr << description << ": component count changed" << std::endl;
    return false;
  }
  for (int componentIndex = 0; componentIndex < array->GetNumberOfComponents(); ++componentIndex)
  {
    const char* componentName = array->GetComponentName(componentIndex);
    if (!componentName || componentName != expectedComponentNames[static_cast<size_t>(componentIndex)])
    {
      std::cerr << description << ": component name " << componentIndex << " was not preserved" << std::endl;
      return false;
    }
  }

  vtkInformation* information = array->GetInformation();
  if (!information->Has(vtkDataObject::FIELD_NAME()) || std::string(information->Get(vtkDataObject::FIELD_NAME())) != "measurement-metadata")
  {
    std::cerr << description << ": vtkInformation metadata was not preserved" << std::endl;
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
bool CheckStructuralEditNotifications(vtkMRMLCoreTestingUtilities::vtkMRMLNodeCallback* callback, const std::string& description)
{
  const int modifiedEventCount = callback->GetNumberOfEvents(vtkCommand::ModifiedEvent);
  const int measurementsModifiedEventCount = callback->GetNumberOfEvents(vtkMRMLMarkupsNode::MeasurementsModifiedEvent);
  if (modifiedEventCount != 1 || measurementsModifiedEventCount != 1)
  {
    std::cerr << description << ": expected one ModifiedEvent and one MeasurementsModifiedEvent, got " << modifiedEventCount << " and "
              << measurementsModifiedEventCount << std::endl;
    return false;
  }
  callback->ResetNumberOfEvents();
  return true;
}

//----------------------------------------------------------------------------
void RecordEvent(vtkObject*, unsigned long event, void* clientData, void*)
{
  std::vector<unsigned long>* events = reinterpret_cast<std::vector<unsigned long>*>(clientData);
  if (events)
  {
    events->push_back(event);
  }
}

//----------------------------------------------------------------------------
bool CheckMeasurementNotificationFollowsPointEvent(
  const std::vector<unsigned long>& events, unsigned long pointEvent, const std::string& description)
{
  const auto pointEventIt = std::find(events.begin(), events.end(), pointEvent);
  const auto measurementEventIt = std::find(events.begin(), events.end(), vtkMRMLMarkupsNode::MeasurementsModifiedEvent);
  if (pointEventIt == events.end() || measurementEventIt == events.end() || pointEventIt > measurementEventIt)
  {
    std::cerr << description << ": measurement notification did not follow the structural point event" << std::endl;
    return false;
  }
  return true;
}

//----------------------------------------------------------------------------
int TestControlPointValueAlignment()
{
  const double undefined = std::numeric_limits<double>::quiet_NaN();

  vtkNew<vtkMRMLMarkupsFiducialNodeTestHelper> node;
  node->AddNControlPoints(3);

  vtkNew<vtkMRMLStaticMeasurement> scalarMeasurement;
  scalarMeasurement->SetName("Scalar");
  scalarMeasurement->SetControlPointValues(CreateArray(1, { 10.0, 20.0, 30.0 }));
  node->AddMeasurement(scalarMeasurement);

  vtkNew<vtkMRMLStaticMeasurement> vectorMeasurement;
  vectorMeasurement->SetName("Vector");
  vtkSmartPointer<vtkDoubleArray> vectorValues = CreateArray(3, { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 });
  vectorValues->SetName("VectorValues");
  vectorValues->SetComponentName(0, "X value");
  vectorValues->SetComponentName(1, "Y value");
  vectorValues->SetComponentName(2, "Z value");
  vectorValues->GetInformation()->Set(vtkDataObject::FIELD_NAME(), "measurement-metadata");
  vectorMeasurement->SetControlPointValues(vectorValues);
  node->AddMeasurement(vectorMeasurement);

  vtkNew<vtkMRMLStaticMeasurement> malformedMeasurement;
  malformedMeasurement->SetName("Malformed");
  malformedMeasurement->SetControlPointValues(CreateArray(1, { 100.0, 200.0 }));
  node->AddMeasurement(malformedMeasurement);

  vtkNew<vtkMRMLMeasurementLength> dynamicMeasurement;
  dynamicMeasurement->SetName("Dynamic");
  dynamicMeasurement->SetEnabled(false);
  dynamicMeasurement->SetControlPointValues(CreateArray(1, { 1000.0, 2000.0, 3000.0 }));
  node->AddMeasurement(dynamicMeasurement);

  CHECK_BOOL(node->InsertControlPoint(1, vtkVector3d(1.0, 2.0, 3.0)), true);
  CHECK_INT(node->GetNumberOfControlPoints(), 4);
  if (!CheckArray(scalarMeasurement->GetControlPointValues(), 1, { 10.0, undefined, 20.0, 30.0 }, "scalar values after insert") ||
      !CheckArray(vectorMeasurement->GetControlPointValues(),
        3,
        { 1.0, 2.0, 3.0, undefined, undefined, undefined, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0 },
        "vector values after insert") ||
      !CheckArrayMetadata(vectorMeasurement->GetControlPointValues(), "vector metadata after insert"))
  {
    return EXIT_FAILURE;
  }

  node->SwapControlPoints(0, 2);
  if (!CheckArray(scalarMeasurement->GetControlPointValues(), 1, { 20.0, undefined, 10.0, 30.0 }, "scalar values after swap") ||
      !CheckArray(vectorMeasurement->GetControlPointValues(),
        3,
        { 4.0, 5.0, 6.0, undefined, undefined, undefined, 1.0, 2.0, 3.0, 7.0, 8.0, 9.0 },
        "vector values after swap") ||
      !CheckArrayMetadata(vectorMeasurement->GetControlPointValues(), "vector metadata after swap"))
  {
    return EXIT_FAILURE;
  }

  node->RemoveNthControlPoint(1);
  CHECK_INT(node->GetNumberOfControlPoints(), 3);
  if (!CheckArray(scalarMeasurement->GetControlPointValues(), 1, { 20.0, 10.0, 30.0 }, "scalar values after remove") ||
      !CheckArray(vectorMeasurement->GetControlPointValues(), 3, { 4.0, 5.0, 6.0, 1.0, 2.0, 3.0, 7.0, 8.0, 9.0 }, "vector values after remove") ||
      !CheckArrayMetadata(vectorMeasurement->GetControlPointValues(), "vector metadata after remove"))
  {
    return EXIT_FAILURE;
  }

  node->AddNControlPoints(1);
  CHECK_INT(node->GetNumberOfControlPoints(), 4);
  if (!CheckArray(scalarMeasurement->GetControlPointValues(), 1, { 20.0, 10.0, 30.0, undefined }, "scalar values after append") ||
      !CheckArray(vectorMeasurement->GetControlPointValues(),
        3,
        { 4.0, 5.0, 6.0, 1.0, 2.0, 3.0, 7.0, 8.0, 9.0, undefined, undefined, undefined },
        "vector values after append") ||
      !CheckArrayMetadata(vectorMeasurement->GetControlPointValues(), "vector metadata after append"))
  {
    return EXIT_FAILURE;
  }

  node->RemoveAllControlPoints();
  CHECK_INT(node->GetNumberOfControlPoints(), 0);
  if (!CheckArray(scalarMeasurement->GetControlPointValues(), 1, {}, "scalar values after clear") ||
      !CheckArray(vectorMeasurement->GetControlPointValues(), 3, {}, "vector values after clear") ||
      !CheckArrayMetadata(vectorMeasurement->GetControlPointValues(), "vector metadata after clear"))
  {
    return EXIT_FAILURE;
  }

  node->AddNControlPoints(1);
  CHECK_INT(node->GetNumberOfControlPoints(), 1);
  if (!CheckArray(scalarMeasurement->GetControlPointValues(), 1, { undefined }, "scalar values after re-add") ||
      !CheckArray(vectorMeasurement->GetControlPointValues(), 3, { undefined, undefined, undefined }, "vector values after re-add") ||
      !CheckArrayMetadata(vectorMeasurement->GetControlPointValues(), "vector metadata after re-add") ||
      !CheckArray(malformedMeasurement->GetControlPointValues(), 1, { 100.0, 200.0 }, "malformed static values") ||
      !CheckArray(dynamicMeasurement->GetControlPointValues(), 1, { 1000.0, 2000.0, 3000.0 }, "non-static values"))
  {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestStructuralEditNotifications()
{
  vtkNew<vtkMRMLMarkupsFiducialNode> node;
  node->AddNControlPoints(2);

  vtkNew<vtkMRMLStaticMeasurement> scalarMeasurement;
  scalarMeasurement->SetControlPointValues(CreateArray(1, { 1.0, 2.0 }));
  node->AddMeasurement(scalarMeasurement);

  vtkNew<vtkMRMLStaticMeasurement> vectorMeasurement;
  vectorMeasurement->SetControlPointValues(CreateArray(3, { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 }));
  node->AddMeasurement(vectorMeasurement);

  vtkNew<vtkMRMLStaticMeasurement> mismatchedMeasurement;
  mismatchedMeasurement->SetControlPointValues(CreateArray(1, { 100.0 }));
  node->AddMeasurement(mismatchedMeasurement);

  vtkNew<vtkMRMLCoreTestingUtilities::vtkMRMLNodeCallback> callback;
  node->AddObserver(vtkCommand::ModifiedEvent, callback);
  node->AddObserver(vtkMRMLMarkupsNode::MeasurementsModifiedEvent, callback);

  std::vector<unsigned long> orderedEvents;
  vtkNew<vtkCallbackCommand> eventOrderCallback;
  eventOrderCallback->SetClientData(&orderedEvents);
  eventOrderCallback->SetCallback(RecordEvent);
  node->AddObserver(vtkMRMLMarkupsNode::PointAddedEvent, eventOrderCallback);
  node->AddObserver(vtkMRMLMarkupsNode::PointModifiedEvent, eventOrderCallback);
  node->AddObserver(vtkMRMLMarkupsNode::PointRemovedEvent, eventOrderCallback);
  node->AddObserver(vtkMRMLMarkupsNode::MeasurementsModifiedEvent, eventOrderCallback);

  node->AddNControlPoints(1);
  if (!CheckStructuralEditNotifications(callback, "append notification")
      || !CheckMeasurementNotificationFollowsPointEvent(orderedEvents, vtkMRMLMarkupsNode::PointAddedEvent, "append notification order"))
  {
    return EXIT_FAILURE;
  }
  orderedEvents.clear();

  CHECK_BOOL(node->InsertControlPoint(1, vtkVector3d(1.0, 2.0, 3.0)), true);
  if (!CheckStructuralEditNotifications(callback, "insert notification")
      || !CheckMeasurementNotificationFollowsPointEvent(orderedEvents, vtkMRMLMarkupsNode::PointAddedEvent, "insert notification order"))
  {
    return EXIT_FAILURE;
  }
  orderedEvents.clear();

  node->SwapControlPoints(0, 2);
  if (!CheckStructuralEditNotifications(callback, "swap notification")
      || !CheckMeasurementNotificationFollowsPointEvent(orderedEvents, vtkMRMLMarkupsNode::PointModifiedEvent, "swap notification order"))
  {
    return EXIT_FAILURE;
  }
  orderedEvents.clear();

  node->RemoveNthControlPoint(1);
  if (!CheckStructuralEditNotifications(callback, "remove notification")
      || !CheckMeasurementNotificationFollowsPointEvent(orderedEvents, vtkMRMLMarkupsNode::PointRemovedEvent, "remove notification order"))
  {
    return EXIT_FAILURE;
  }
  orderedEvents.clear();

  node->RemoveAllControlPoints();
  if (!CheckStructuralEditNotifications(callback, "clear notification")
      || !CheckMeasurementNotificationFollowsPointEvent(orderedEvents, vtkMRMLMarkupsNode::PointRemovedEvent, "clear notification order"))
  {
    return EXIT_FAILURE;
  }

  vtkNew<vtkMRMLMarkupsFiducialNode> nodeWithoutAlignedMeasurement;
  nodeWithoutAlignedMeasurement->AddNControlPoints(2);
  vtkNew<vtkMRMLStaticMeasurement> onlyMismatchedMeasurement;
  onlyMismatchedMeasurement->SetControlPointValues(CreateArray(1, { 1.0 }));
  nodeWithoutAlignedMeasurement->AddMeasurement(onlyMismatchedMeasurement);
  vtkNew<vtkMRMLCoreTestingUtilities::vtkMRMLNodeCallback> unmatchedCallback;
  nodeWithoutAlignedMeasurement->AddObserver(vtkMRMLMarkupsNode::MeasurementsModifiedEvent, unmatchedCallback);
  CHECK_BOOL(nodeWithoutAlignedMeasurement->InsertControlPoint(1, vtkVector3d(1.0, 2.0, 3.0)), true);
  CHECK_INT(unmatchedCallback->GetNumberOfEvents(vtkMRMLMarkupsNode::MeasurementsModifiedEvent), 0);

  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestControlPointValueEvents()
{
  vtkNew<vtkMRMLMarkupsFiducialNodeTestHelper> node;
  vtkNew<vtkMRMLStaticMeasurement> measurement;
  measurement->SetControlPointValues(CreateArray(1, { 1.0 }));
  node->AddMeasurement(measurement);

  vtkNew<vtkMRMLCoreTestingUtilities::vtkMRMLNodeCallback> measurementCallback;
  measurement->AddObserver(vtkMRMLMeasurement::ControlPointValuesModifiedEvent, measurementCallback);
  vtkNew<vtkMRMLCoreTestingUtilities::vtkMRMLNodeCallback> nodeCallback;
  node->AddObserver(vtkMRMLMarkupsNode::MeasurementsModifiedEvent, nodeCallback);

  const vtkMTimeType storableModifiedTimeBeforeSet = node->GetStorableModifiedTimeForTesting();
  measurement->SetControlPointValues(CreateArray(1, { 2.0 }));
  CHECK_BOOL(measurementCallback->GetNumberOfEvents(vtkMRMLMeasurement::ControlPointValuesModifiedEvent) > 0, true);
  CHECK_BOOL(nodeCallback->GetNumberOfEvents(vtkMRMLMarkupsNode::MeasurementsModifiedEvent) > 0, true);
  CHECK_BOOL(node->GetStorableModifiedTimeForTesting() > storableModifiedTimeBeforeSet, true);
  if (!CheckArray(measurement->GetControlPointValues(), 1, { 2.0 }, "values after setter update"))
  {
    return EXIT_FAILURE;
  }

  measurementCallback->ResetNumberOfEvents();
  nodeCallback->ResetNumberOfEvents();
  const vtkMTimeType storableModifiedTimeBeforeDirectEdit = node->GetStorableModifiedTimeForTesting();
  measurement->GetControlPointValues()->GetPointer(0)[0] = 3.0;
  measurement->GetControlPointValues()->Modified();
  CHECK_BOOL(measurementCallback->GetNumberOfEvents(vtkMRMLMeasurement::ControlPointValuesModifiedEvent) > 0, true);
  CHECK_BOOL(nodeCallback->GetNumberOfEvents(vtkMRMLMarkupsNode::MeasurementsModifiedEvent) > 0, true);
  CHECK_BOOL(node->GetStorableModifiedTimeForTesting() > storableModifiedTimeBeforeDirectEdit, true);

  node->RemoveNthMeasurement(0);
  measurementCallback->ResetNumberOfEvents();
  nodeCallback->ResetNumberOfEvents();
  const vtkMTimeType storableModifiedTimeAfterRemoval = node->GetStorableModifiedTimeForTesting();
  measurement->GetControlPointValues()->GetPointer(0)[0] = 4.0;
  measurement->GetControlPointValues()->Modified();
  CHECK_BOOL(measurementCallback->GetNumberOfEvents(vtkMRMLMeasurement::ControlPointValuesModifiedEvent) > 0, true);
  CHECK_INT(nodeCallback->GetNumberOfEvents(vtkMRMLMarkupsNode::MeasurementsModifiedEvent), 0);
  CHECK_BOOL(node->GetStorableModifiedTimeForTesting() == storableModifiedTimeAfterRemoval, true);

  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestMeasurementCopy()
{
  vtkNew<vtkMRMLStaticMeasurement> sourceMeasurement;
  sourceMeasurement->SetControlPointValues(CreateArray(3, { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 }));

  vtkNew<vtkMRMLStaticMeasurement> copiedMeasurement;
  vtkNew<vtkMRMLCoreTestingUtilities::vtkMRMLNodeCallback> copiedMeasurementCallback;
  copiedMeasurement->AddObserver(vtkMRMLMeasurement::ControlPointValuesModifiedEvent, copiedMeasurementCallback);
  copiedMeasurement->Copy(sourceMeasurement);

  CHECK_BOOL(copiedMeasurementCallback->GetNumberOfEvents(vtkMRMLMeasurement::ControlPointValuesModifiedEvent) > 0, true);
  CHECK_POINTER_DIFFERENT(copiedMeasurement->GetControlPointValues(), sourceMeasurement->GetControlPointValues());
  if (!CheckArray(copiedMeasurement->GetControlPointValues(), 3, { 1.0, 2.0, 3.0, 4.0, 5.0, 6.0 }, "copied values"))
  {
    return EXIT_FAILURE;
  }

  copiedMeasurementCallback->ResetNumberOfEvents();
  copiedMeasurement->GetControlPointValues()->GetPointer(0)[0] = 99.0;
  copiedMeasurement->GetControlPointValues()->Modified();
  CHECK_BOOL(copiedMeasurementCallback->GetNumberOfEvents(vtkMRMLMeasurement::ControlPointValuesModifiedEvent) > 0, true);
  CHECK_DOUBLE(sourceMeasurement->GetControlPointValues()->GetValue(0), 1.0);

  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestMeasurementDestructionDoesNotPublishValueChanges()
{
  vtkNew<vtkMRMLCoreTestingUtilities::vtkMRMLNodeCallback> callback;
  {
    vtkNew<vtkMRMLStaticMeasurement> measurement;
    measurement->SetControlPointValues(CreateArray(1, { 1.0 }));
    measurement->AddObserver(vtkMRMLMeasurement::ControlPointValuesModifiedEvent, callback);
    callback->ResetNumberOfEvents();
  }
  CHECK_INT(callback->GetNumberOfEvents(vtkMRMLMeasurement::ControlPointValuesModifiedEvent), 0);
  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestMeasurementUpdateDoesNotRecurse()
{
  vtkNew<vtkMRMLMarkupsFiducialNode> node;
  vtkNew<vtkMRMLMeasurementWithControlPointOutput> measurement;
  measurement->SetInputMRMLNode(node);
  node->AddMeasurement(measurement);

  vtkNew<vtkMRMLCoreTestingUtilities::vtkMRMLNodeCallback> nodeCallback;
  node->AddObserver(vtkMRMLMarkupsNode::MeasurementsModifiedEvent, nodeCallback);

  node->UpdateAllMeasurements();
  CHECK_INT(measurement->GetComputeCount(), 1);
  CHECK_BOOL(nodeCallback->GetNumberOfEvents(vtkMRMLMarkupsNode::MeasurementsModifiedEvent) > 0, true);

  nodeCallback->ResetNumberOfEvents();
  node->UpdateAllMeasurements();
  CHECK_INT(measurement->GetComputeCount(), 2);
  CHECK_BOOL(nodeCallback->GetNumberOfEvents(vtkMRMLMarkupsNode::MeasurementsModifiedEvent) > 0, true);

  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestMultiComponentMeasurementInterpolation()
{
  vtkSmartPointer<vtkDoubleArray> controlPointValues = CreateArray(3, { 0.0, 0.25, 0.5, 1.0, 0.75, 0.0 });
  vtkSmartPointer<vtkDoubleArray> pedigreeIds = CreateArray(1, { 0.0, 0.5, 1.0 });
  vtkNew<vtkDoubleArray> interpolatedValues;
  CHECK_BOOL(vtkCurveMeasurementsCalculator::InterpolateArray(controlPointValues, false, interpolatedValues, pedigreeIds), true);
  if (!CheckArray(interpolatedValues, 3, { 0.0, 0.25, 0.5, 0.5, 0.5, 0.25, 1.0, 0.75, 0.0 }, "interpolated RGB values"))
  {
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestMultiComponentMeasurementCurvePipeline()
{
  vtkNew<vtkMRMLMarkupsCurveNode> curveNode;
  curveNode->AddControlPoint(vtkVector3d(0.0, 0.0, 0.0));
  curveNode->AddControlPoint(vtkVector3d(10.0, 0.0, 0.0));
  curveNode->AddControlPoint(vtkVector3d(20.0, 0.0, 0.0));

  vtkNew<vtkMRMLStaticMeasurement> rgbMeasurement;
  rgbMeasurement->SetName("RGB");
  rgbMeasurement->SetControlPointValues(CreateArray(3, { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0 }));
  curveNode->AddMeasurement(rgbMeasurement);

  vtkNew<vtkMRMLStaticMeasurement> scalarMeasurement;
  scalarMeasurement->SetName("ScalarAfterRGB");
  scalarMeasurement->SetControlPointValues(CreateArray(1, { 10.0, 20.0, 30.0 }));
  curveNode->AddMeasurement(scalarMeasurement);

  vtkPolyData* curveWorld = curveNode->GetCurveWorld();
  vtkDoubleArray* interpolatedRgb = curveWorld ? vtkDoubleArray::SafeDownCast(curveWorld->GetPointData()->GetArray("RGB")) : nullptr;
  vtkDoubleArray* interpolatedScalar = curveWorld ? vtkDoubleArray::SafeDownCast(curveWorld->GetPointData()->GetArray("ScalarAfterRGB")) : nullptr;
  CHECK_NOT_NULL(interpolatedRgb);
  CHECK_INT(interpolatedRgb->GetNumberOfComponents(), 3);
  CHECK_NOT_NULL(interpolatedScalar);
  CHECK_INT(interpolatedScalar->GetNumberOfComponents(), 1);
  CHECK_INT(interpolatedRgb->GetNumberOfTuples(), curveWorld->GetNumberOfPoints());
  CHECK_INT(interpolatedScalar->GetNumberOfTuples(), curveWorld->GetNumberOfPoints());
  return EXIT_SUCCESS;
}

//----------------------------------------------------------------------------
int TestCurveMeasurementObserversDoNotAccumulate()
{
  vtkNew<vtkMRMLMarkupsCurveNodeTestHelper> curveNode;
  curveNode->AddControlPoint(vtkVector3d(0.0, 0.0, 0.0));
  curveNode->AddControlPoint(vtkVector3d(10.0, 0.0, 0.0));
  curveNode->AddControlPoint(vtkVector3d(20.0, 0.0, 0.0));

  vtkNew<vtkMRMLStaticMeasurement> measurement;
  measurement->SetName("RepeatedEdits");
  measurement->SetControlPointValues(CreateArray(1, { 1.0, 2.0, 3.0 }));
  curveNode->AddMeasurement(measurement);

  vtkCurveMeasurementsCalculator* calculator = curveNode->GetCurveMeasurementsCalculatorForTesting();
  CHECK_NOT_NULL(calculator);
  for (int updateIndex = 0; updateIndex < 4; ++updateIndex)
  {
    calculator->Modified();
    CHECK_NOT_NULL(curveNode->GetCurveWorld());
  }

  vtkNew<vtkMRMLCoreTestingUtilities::vtkMRMLNodeCallback> calculatorCallback;
  calculator->AddObserver(vtkCommand::ModifiedEvent, calculatorCallback);
  measurement->GetControlPointValues()->Modified();
  CHECK_INT(calculatorCallback->GetNumberOfEvents(vtkCommand::ModifiedEvent), 1);
  return EXIT_SUCCESS;
}

} // namespace

//----------------------------------------------------------------------------
int vtkMRMLMarkupsControlPointMeasurementsTest(int, char*[])
{
  CHECK_EXIT_SUCCESS(TestControlPointValueAlignment());
  CHECK_EXIT_SUCCESS(TestStructuralEditNotifications());
  CHECK_EXIT_SUCCESS(TestControlPointValueEvents());
  CHECK_EXIT_SUCCESS(TestMeasurementCopy());
  CHECK_EXIT_SUCCESS(TestMeasurementDestructionDoesNotPublishValueChanges());
  CHECK_EXIT_SUCCESS(TestMeasurementUpdateDoesNotRecurse());
  CHECK_EXIT_SUCCESS(TestMultiComponentMeasurementInterpolation());
  CHECK_EXIT_SUCCESS(TestMultiComponentMeasurementCurvePipeline());
  CHECK_EXIT_SUCCESS(TestCurveMeasurementObserversDoNotAccumulate());
  return EXIT_SUCCESS;
}
