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
#include "vtkMRMLCoreTestingMacros.h"
#include "vtkMRMLMarkupsDisplayNode.h"
#include "vtkMRMLMarkupsFiducialNode.h"
#include "vtkMRMLScene.h"
#include "vtkMRMLStaticMeasurement.h"

// VTK includes
#include <vtkAssignAttribute.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>

// STD includes
#include <iostream>
#include <limits>

namespace
{
int TestControlPointScalarDisplay()
{
  vtkNew<vtkMRMLMarkupsDisplayNode> defaultDisplayNode;
  if (defaultDisplayNode->GetControlPointScalarVisibility())
  {
    std::cerr << "Control point scalar visibility must be disabled by default" << std::endl;
    return EXIT_FAILURE;
  }

  vtkNew<vtkMRMLScene> persistenceScene;
  vtkNew<vtkMRMLMarkupsDisplayNode> persistedDisplayNode;
  persistedDisplayNode->SetName("ControlPointScalarPersistence");
  persistedDisplayNode->ControlPointScalarVisibilityOn();
  persistenceScene->AddNode(persistedDisplayNode);
  persistenceScene->SetSaveToXMLString(1);
  CHECK_BOOL(persistenceScene->Commit() != 0, true);

  vtkNew<vtkMRMLScene> loadedPersistenceScene;
  loadedPersistenceScene->SetLoadFromXMLString(1);
  loadedPersistenceScene->SetSceneXMLString(persistenceScene->GetSceneXMLString());
  CHECK_BOOL(loadedPersistenceScene->Import() != 0, true);
  vtkMRMLMarkupsDisplayNode* loadedPersistedDisplayNode =
    vtkMRMLMarkupsDisplayNode::SafeDownCast(loadedPersistenceScene->GetFirstNodeByName("ControlPointScalarPersistence"));
  CHECK_NOT_NULL(loadedPersistedDisplayNode);
  CHECK_BOOL(loadedPersistedDisplayNode->GetControlPointScalarVisibility(), true);

  vtkNew<vtkMRMLMarkupsDisplayNode> copiedDisplayNode;
  copiedDisplayNode->CopyContent(loadedPersistedDisplayNode);
  if (!copiedDisplayNode->GetControlPointScalarVisibility())
  {
    std::cerr << "Failed to copy control point scalar visibility" << std::endl;
    return EXIT_FAILURE;
  }

  vtkNew<vtkMRMLScene> scene;
  vtkNew<vtkMRMLMarkupsFiducialNode> markupsNode;
  vtkNew<vtkMRMLMarkupsDisplayNode> displayNode;
  scene->AddNode(markupsNode);
  scene->AddNode(displayNode);
  markupsNode->SetAndObserveDisplayNodeID(displayNode->GetID());
  markupsNode->AddNControlPoints(3);

  vtkNew<vtkDoubleArray> controlPointValues;
  controlPointValues->SetName("ArrayNameIsNotUsedForLookup");
  controlPointValues->InsertNextValue(std::numeric_limits<double>::quiet_NaN());
  controlPointValues->InsertNextValue(-2.5);
  controlPointValues->InsertNextValue(7.0);

  vtkNew<vtkMRMLStaticMeasurement> measurement;
  measurement->SetName("temperature");
  measurement->SetControlPointValues(controlPointValues);
  markupsNode->AddMeasurement(measurement);

  displayNode->SetActiveScalar("missing", vtkAssignAttribute::POINT_DATA);
  if (displayNode->GetActiveControlPointMeasurement() != nullptr || displayNode->GetActiveControlPointScalarArray() != nullptr)
  {
    std::cerr << "A missing active measurement unexpectedly resolved" << std::endl;
    return EXIT_FAILURE;
  }

  displayNode->SetActiveScalar("temperature", vtkAssignAttribute::POINT_DATA);
  if (displayNode->GetActiveControlPointMeasurement() != measurement.GetPointer())
  {
    std::cerr << "Active measurement was not resolved by measurement name" << std::endl;
    return EXIT_FAILURE;
  }
  if (displayNode->GetActiveControlPointScalarArray() != measurement->GetControlPointValues())
  {
    std::cerr << "Active raw control point scalar array was not resolved" << std::endl;
    return EXIT_FAILURE;
  }

  vtkPolyData* curveWorld = markupsNode->GetCurveWorld();
  if (!curveWorld)
  {
    std::cerr << "Failed to obtain markups curve data" << std::endl;
    return EXIT_FAILURE;
  }
  vtkNew<vtkDoubleArray> curveValues;
  curveValues->SetName("temperature");
  curveValues->SetNumberOfTuples(curveWorld->GetNumberOfPoints());
  curveValues->FillComponent(0, 100.0);
  curveWorld->GetPointData()->AddArray(curveValues);

  vtkNew<vtkDoubleArray> curveOnlyValues;
  curveOnlyValues->SetName("curveOnly");
  curveOnlyValues->SetNumberOfTuples(curveWorld->GetNumberOfPoints());
  for (vtkIdType pointIndex = 0; pointIndex < curveOnlyValues->GetNumberOfTuples(); ++pointIndex)
  {
    curveOnlyValues->SetValue(pointIndex, 4.0 + pointIndex);
  }
  curveWorld->GetPointData()->AddArray(curveOnlyValues);

  displayNode->SetActiveScalar("curveOnly", vtkAssignAttribute::POINT_DATA);
  displayNode->ControlPointScalarVisibilityOn();
  displayNode->SetScalarRangeFlag(vtkMRMLDisplayNode::UseDataScalarRange);
  if (displayNode->GetActiveScalarArray() != curveOnlyValues.GetPointer())
  {
    std::cerr << "A curve-only source did not remain available while control point coloring was enabled" << std::endl;
    return EXIT_FAILURE;
  }
  double curveOnlyRange[2] = { 0.0, 0.0 };
  curveOnlyValues->GetRange(curveOnlyRange);
  if (displayNode->GetScalarRange()[0] != curveOnlyRange[0] || displayNode->GetScalarRange()[1] != curveOnlyRange[1])
  {
    std::cerr << "A curve-only source did not determine the data scalar range" << std::endl;
    return EXIT_FAILURE;
  }

  displayNode->SetActiveScalar("temperature", vtkAssignAttribute::POINT_DATA);

  displayNode->ControlPointScalarVisibilityOff();
  if (displayNode->GetActiveScalarArray() != curveValues.GetPointer())
  {
    std::cerr << "Disabling control point scalar visibility did not retain curve scalar lookup" << std::endl;
    return EXIT_FAILURE;
  }

  displayNode->SetScalarRangeFlag(vtkMRMLDisplayNode::UseManualScalarRange);
  displayNode->SetScalarRange(10.0, 20.0);
  displayNode->SetScalarRangeFlag(vtkMRMLDisplayNode::UseDataScalarRange);
  displayNode->ControlPointScalarVisibilityOn();
  if (displayNode->GetActiveControlPointScalarArray() != measurement->GetControlPointValues())
  {
    std::cerr << "Enabling control point scalar visibility did not retain the raw measurement source" << std::endl;
    return EXIT_FAILURE;
  }
  if (displayNode->GetActiveScalarArray() != curveValues.GetPointer())
  {
    std::cerr << "Control point scalar visibility changed the curve-backed active scalar API" << std::endl;
    return EXIT_FAILURE;
  }

  double* scalarRange = displayNode->GetScalarRange();
  if (scalarRange[0] != -2.5 || scalarRange[1] != 7.0)
  {
    std::cerr << "Finite raw values did not determine scalar range: " << scalarRange[0] << ", " << scalarRange[1] << std::endl;
    return EXIT_FAILURE;
  }

  vtkNew<vtkMRMLMarkupsDisplayNode> secondDisplayNode;
  scene->AddNode(secondDisplayNode);
  markupsNode->AddAndObserveDisplayNodeID(secondDisplayNode->GetID());
  secondDisplayNode->SetActiveScalar("temperature", vtkAssignAttribute::POINT_DATA);
  secondDisplayNode->ControlPointScalarVisibilityOn();
  secondDisplayNode->SetScalarRangeFlag(vtkMRMLDisplayNode::UseDataScalarRange);

  vtkDoubleArray* storedValues = measurement->GetControlPointValues();
  storedValues->SetValue(1, -4.0);
  storedValues->SetValue(2, 9.0);
  storedValues->Modified();
  scalarRange = displayNode->GetScalarRange();
  if (scalarRange[0] != -4.0 || scalarRange[1] != 9.0)
  {
    std::cerr << "Changing measurement values did not update the data scalar range: " << scalarRange[0] << ", " << scalarRange[1] << std::endl;
    return EXIT_FAILURE;
  }
  if (secondDisplayNode->GetScalarRange()[0] != -4.0 || secondDisplayNode->GetScalarRange()[1] != 9.0)
  {
    std::cerr << "Changing measurement values did not update every display node's data range" << std::endl;
    return EXIT_FAILURE;
  }

  storedValues->FillComponent(0, std::numeric_limits<double>::quiet_NaN());
  storedValues->Modified();
  displayNode->SetScalarRange(30.0, 40.0);
  displayNode->UpdateScalarRange();
  scalarRange = displayNode->GetScalarRange();
  if (scalarRange[0] != 30.0 || scalarRange[1] != 40.0)
  {
    std::cerr << "An all-NaN raw array corrupted the last valid scalar range" << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
} // namespace

int vtkMRMLMarkupsDisplayNodeTest1(int, char*[])
{
  vtkNew<vtkMRMLMarkupsDisplayNode> node1;
  TESTING_OUTPUT_ASSERT_WARNINGS_BEGIN();
  EXERCISE_ALL_BASIC_MRML_METHODS(node1.GetPointer());
  // 4 warnings in vtkMRMLMarkupsDisplayNode::UpdateAssignedAttribute() are expected
  TESTING_OUTPUT_ASSERT_WARNINGS(4);
  TESTING_OUTPUT_ASSERT_WARNINGS_END();

  TEST_SET_GET_DOUBLE_RANGE(node1, TextScale, 0.0, 100.0);

  TEST_SET_GET_INT_RANGE(node1, GlyphType, -1, 10);

  for (int i = vtkMRMLMarkupsDisplayNode::GetMinimumGlyphType(); i <= vtkMRMLMarkupsDisplayNode::GetMaximumGlyphType(); i++)
  {
    node1->SetGlyphType(i);
    std::cout << i << " GetGlyphType = " << node1->GetGlyphType() << ", as string = " << node1->GetGlyphTypeAsString() << ", GetGlyphTypeAsString(" << i
              << ") = " << node1->GetGlyphTypeAsString(i) << std::endl;
  }

  // print out the enums
  std::cout << "Enum GlyphShapes:" << std::endl;
  std::cout << "    Vertex2D = " << vtkMRMLMarkupsDisplayNode::Vertex2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::Vertex2D) << std::endl;
  std::cout << "    Dash2D = " << vtkMRMLMarkupsDisplayNode::Dash2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::Dash2D) << std::endl;
  std::cout << "    Cross2D = " << vtkMRMLMarkupsDisplayNode::Cross2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::Cross2D) << std::endl;
  std::cout << "    CrossDot2D = " << vtkMRMLMarkupsDisplayNode::CrossDot2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::CrossDot2D) << std::endl;
  std::cout << "    ThickCross2D = " << vtkMRMLMarkupsDisplayNode::ThickCross2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::ThickCross2D)
            << std::endl;
  std::cout << "    Triangle2D = " << vtkMRMLMarkupsDisplayNode::Triangle2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::Triangle2D) << std::endl;
  std::cout << "    Square2D = " << vtkMRMLMarkupsDisplayNode::Square2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::Square2D) << std::endl;
  std::cout << "    Circle2D = " << vtkMRMLMarkupsDisplayNode::Circle2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::Circle2D) << std::endl;
  std::cout << "    Diamond2D = " << vtkMRMLMarkupsDisplayNode::Diamond2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::Diamond2D) << std::endl;
  std::cout << "    Arrow2D = " << vtkMRMLMarkupsDisplayNode::Arrow2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::Arrow2D) << std::endl;
  std::cout << "    ThickArrow2D = " << vtkMRMLMarkupsDisplayNode::ThickArrow2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::ThickArrow2D)
            << std::endl;
  std::cout << "    HookedArrow2D = " << vtkMRMLMarkupsDisplayNode::HookedArrow2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::HookedArrow2D)
            << std::endl;
  std::cout << "    StarBurst2D = " << vtkMRMLMarkupsDisplayNode::StarBurst2D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::StarBurst2D)
            << std::endl;
  std::cout << "    Sphere3D = " << vtkMRMLMarkupsDisplayNode::Sphere3D << ", as string = " << node1->GetGlyphTypeAsString(vtkMRMLMarkupsDisplayNode::Sphere3D) << std::endl;

  // spot test int to string mapping
  node1->SetGlyphType(vtkMRMLMarkupsDisplayNode::Sphere3D);
  if (strcmp(node1->GetGlyphTypeAsString(), "Sphere3D") != 0)
  {
    std::cerr << "ERROR: set the glyph type to " << vtkMRMLMarkupsDisplayNode::Sphere3D << ", but get glyph type as string returned " << node1->GetGlyphTypeAsString()
              << " instead of Sphere3D" << std::endl;
    return EXIT_FAILURE;
  }

  // test GlyphTypeIs3D
  node1->SetGlyphTypeFromString("Triangle2D");
  if (node1->GlyphTypeIs3D() == 1)
  {
    std::cerr << "ERROR: triangle 2d not recognised as a 2d glyph" << std::endl;
    return EXIT_FAILURE;
  }
  node1->SetGlyphTypeFromString("Sphere3D");
  if (node1->GlyphTypeIs3D() != 1)
  {
    std::cerr << "ERROR: sphere 3d not recognised as a 3d glyph" << std::endl;
    return EXIT_FAILURE;
  }

  TEST_SET_GET_DOUBLE_RANGE(node1, GlyphScale, -1.0, 25.6);

  TEST_SET_GET_BOOLEAN(node1, SliceProjection);
  TEST_SET_GET_VECTOR3_DOUBLE_RANGE(node1, SliceProjectionColor, 0.0, 1.0);

  node1->SetSliceProjectionOpacity(0.0);
  node1->SetSliceProjectionOpacity(0.5);
  if (node1->GetSliceProjectionOpacity() != 0.5)
  {
    std::cerr << "Failed to set projected opacity to 0.5" << std::endl;
    return EXIT_FAILURE;
  }
  node1->SetSliceProjectionOpacity(1.0);

  node1->SliceProjectionUseFiducialColorOn();
  if (node1->GetSliceProjectionUseFiducialColor() != true)
  {
    std::cerr << "Failed to turn use markup color on with slice projections"
              << ", slice projection = " << node1->GetSliceProjection() << std::endl;
    return EXIT_FAILURE;
  }
  node1->SliceProjectionUseFiducialColorOff();

  node1->SliceProjectionOutlinedBehindSlicePlaneOn();
  if (node1->GetSliceProjectionOutlinedBehindSlicePlane() != true)
  {
    std::cerr << "Failed to turn use outline behind slice plane on"
              << ", slice projection = " << node1->GetSliceProjection() << std::endl;
    return EXIT_FAILURE;
  }
  node1->SliceProjectionOutlinedBehindSlicePlaneOff();

  if (TestControlPointScalarDisplay() != EXIT_SUCCESS)
  {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
