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
#include "vtkMRMLColorTableNode.h"
#include "vtkMRMLMarkupsAngleNode.h"
#include "vtkMRMLMarkupsClosedCurveNode.h"
#include "vtkMRMLMarkupsCurveNode.h"
#include "vtkMRMLMarkupsFiducialDisplayNode.h"
#include "vtkMRMLMarkupsFiducialStorageNode.h"
#include "vtkMRMLMarkupsFiducialNode.h"
#include "vtkMRMLMarkupsJsonStorageNode.h"
#include "vtkMRMLMarkupsLineNode.h"
#include "vtkMRMLMarkupsPlaneDisplayNode.h"
#include "vtkMRMLMarkupsPlaneNode.h"
#include "vtkMRMLMarkupsPlaneJsonStorageNode.h"
#include "vtkMRMLMarkupsROIDisplayNode.h"
#include "vtkMRMLMarkupsROINode.h"
#include "vtkMRMLMarkupsROIJsonStorageNode.h"
#include "vtkMRMLMessageCollection.h"
#include "vtkMRMLStaticMeasurement.h"
#include "vtkURIHandler.h"
#include "vtkMRMLScene.h"
#include "vtkPolyData.h"

// MRMLLogic includes
#include <vtkMRMLApplicationLogic.h>

// VTK includes
#include <vtkAssignAttribute.h>
#include <vtkDoubleArray.h>
#include <vtkNew.h>
#include <vtkTestingOutputWindow.h>

// STD includes
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
// enable for more debugging output
const bool verbose = false;

//----------------------------------------------------------------------------
int CheckControlPointValues(vtkDoubleArray* array, int expectedNumberOfComponents, const std::vector<double>& expectedValues)
{
  CHECK_NOT_NULL(array);
  CHECK_INT(array->GetNumberOfComponents(), expectedNumberOfComponents);
  CHECK_INT(array->GetNumberOfValues(), static_cast<int>(expectedValues.size()));
  for (vtkIdType valueIndex = 0; valueIndex < array->GetNumberOfValues(); ++valueIndex)
  {
    const double actualValue = array->GetValue(valueIndex);
    const double expectedValue = expectedValues[static_cast<size_t>(valueIndex)];
    if (std::isnan(expectedValue))
    {
      CHECK_BOOL(std::isnan(actualValue), true);
    }
    else
    {
      CHECK_DOUBLE(actualValue, expectedValue);
    }
  }
  return EXIT_SUCCESS;
}
} // namespace

//----------------------------------------------------------------------------
int TestControlPointScalarJsonPersistence(const std::string& fileName)
{
  const double undefined = std::numeric_limits<double>::quiet_NaN();

  vtkNew<vtkMRMLScene> sourceScene;
  vtkNew<vtkMRMLApplicationLogic> sourceApplicationLogic;
  sourceApplicationLogic->SetMRMLScene(sourceScene);
  vtkNew<vtkMRMLMarkupsFiducialNode> sourceNode;
  vtkNew<vtkMRMLMarkupsDisplayNode> sourceDisplayNode;
  vtkNew<vtkMRMLMarkupsJsonStorageNode> sourceStorageNode;
  sourceScene->AddNode(sourceNode);
  sourceScene->AddNode(sourceDisplayNode);
  sourceScene->AddNode(sourceStorageNode);
  sourceNode->SetAndObserveDisplayNodeID(sourceDisplayNode->GetID());
  sourceNode->SetAndObserveStorageNodeID(sourceStorageNode->GetID());
  sourceNode->AddNControlPoints(3);

  vtkNew<vtkDoubleArray> scalarValues;
  scalarValues->SetNumberOfTuples(3);
  scalarValues->SetValue(0, undefined);
  scalarValues->SetValue(1, -2.5);
  scalarValues->SetValue(2, 7.0);
  vtkNew<vtkMRMLStaticMeasurement> scalarMeasurement;
  scalarMeasurement->SetName("temperature");
  scalarMeasurement->SetControlPointValues(scalarValues);
  sourceNode->AddMeasurement(scalarMeasurement);

  vtkNew<vtkDoubleArray> allUndefinedScalarValues;
  allUndefinedScalarValues->SetNumberOfTuples(3);
  allUndefinedScalarValues->FillComponent(0, undefined);
  vtkNew<vtkMRMLStaticMeasurement> allUndefinedScalarMeasurement;
  allUndefinedScalarMeasurement->SetName("allUndefinedScalar");
  allUndefinedScalarMeasurement->SetControlPointValues(allUndefinedScalarValues);
  sourceNode->AddMeasurement(allUndefinedScalarMeasurement);

  vtkNew<vtkDoubleArray> rgbValues;
  rgbValues->SetNumberOfComponents(3);
  rgbValues->SetNumberOfTuples(3);
  const double red[3] = { 1.0, 0.0, 0.0 };
  const double partiallyUnsetRgb[3] = { std::numeric_limits<double>::infinity(), 0.25, -std::numeric_limits<double>::infinity() };
  const double blue[3] = { 0.0, 0.0, 1.0 };
  rgbValues->SetTypedTuple(0, red);
  rgbValues->SetTypedTuple(1, partiallyUnsetRgb);
  rgbValues->SetTypedTuple(2, blue);
  vtkNew<vtkMRMLStaticMeasurement> rgbMeasurement;
  rgbMeasurement->SetName("directColor");
  rgbMeasurement->SetControlPointValues(rgbValues);
  sourceNode->AddMeasurement(rgbMeasurement);

  vtkNew<vtkDoubleArray> rgbaValues;
  rgbaValues->SetNumberOfComponents(4);
  rgbaValues->SetNumberOfTuples(3);
  const double translucentOrange[4] = { 1.0, 0.5, 0.0, 0.25 };
  const double unsetRgba[4] = { undefined, undefined, undefined, undefined };
  const double translucentCyan[4] = { 0.0, 0.75, 1.0, 0.6 };
  rgbaValues->SetTypedTuple(0, translucentOrange);
  rgbaValues->SetTypedTuple(1, unsetRgba);
  rgbaValues->SetTypedTuple(2, translucentCyan);
  vtkNew<vtkMRMLStaticMeasurement> rgbaMeasurement;
  rgbaMeasurement->SetName("directColorWithAlpha");
  rgbaMeasurement->SetControlPointValues(rgbaValues);
  sourceNode->AddMeasurement(rgbaMeasurement);

  vtkNew<vtkDoubleArray> emptyRgbaValues;
  emptyRgbaValues->SetNumberOfComponents(4);
  emptyRgbaValues->SetNumberOfTuples(0);
  vtkNew<vtkMRMLStaticMeasurement> emptyRgbaMeasurement;
  emptyRgbaMeasurement->SetName("emptyRgba");
  emptyRgbaMeasurement->SetControlPointValues(emptyRgbaValues);
  sourceNode->AddMeasurement(emptyRgbaMeasurement);

  vtkNew<vtkMRMLColorTableNode> stableColorNode;
  stableColorNode->SetSingletonTag("ControlPointColorTest");
  stableColorNode->SaveWithSceneOff();
  sourceScene->AddNode(stableColorNode);
  const std::string stableColorNodeID = stableColorNode->GetID();

  sourceDisplayNode->SetScalarVisibility(true);
  sourceDisplayNode->SetControlPointScalarVisibility(true);
  sourceDisplayNode->SetActiveScalar("temperature", vtkAssignAttribute::POINT_DATA);
  sourceDisplayNode->SetScalarRangeFlag(vtkMRMLDisplayNode::UseManualScalarRange);
  sourceDisplayNode->SetScalarRange(-10.0, 10.0);
  sourceDisplayNode->SetAndObserveColorNodeID(stableColorNodeID.c_str());

  sourceStorageNode->SetFileName(fileName.c_str());
  CHECK_BOOL(sourceStorageNode->WriteData(sourceNode), true);
  CHECK_INT(sourceStorageNode->GetUserMessages()->GetNumberOfMessagesOfType(vtkCommand::WarningEvent), 0);
  CHECK_INT(sourceStorageNode->GetUserMessages()->GetNumberOfMessagesOfType(vtkCommand::ErrorEvent), 0);

  vtkNew<vtkMRMLScene> loadedScene;
  vtkNew<vtkMRMLApplicationLogic> loadedApplicationLogic;
  loadedApplicationLogic->SetMRMLScene(loadedScene);
  vtkNew<vtkMRMLMarkupsJsonStorageNode> loadedStorageNode;
  vtkNew<vtkMRMLColorTableNode> loadedStableColorNode;
  loadedStableColorNode->SetSingletonTag("ControlPointColorTest");
  loadedStableColorNode->SaveWithSceneOff();
  loadedScene->AddNode(loadedStableColorNode);
  loadedScene->AddNode(loadedStorageNode);
  loadedStorageNode->SetFileName(fileName.c_str());
  vtkMRMLMarkupsNode* loadedNode = loadedStorageNode->AddNewMarkupsNodeFromFile(fileName.c_str());
  CHECK_NOT_NULL(loadedNode);
  vtkMRMLMarkupsDisplayNode* loadedDisplayNode = vtkMRMLMarkupsDisplayNode::SafeDownCast(loadedNode->GetDisplayNode());
  CHECK_NOT_NULL(loadedDisplayNode);

  CHECK_INT(loadedNode->GetNumberOfControlPoints(), 3);
  vtkMRMLMeasurement* loadedScalarMeasurement = loadedNode->GetMeasurement("temperature");
  vtkMRMLMeasurement* loadedAllUndefinedScalarMeasurement = loadedNode->GetMeasurement("allUndefinedScalar");
  vtkMRMLMeasurement* loadedRgbMeasurement = loadedNode->GetMeasurement("directColor");
  vtkMRMLMeasurement* loadedRgbaMeasurement = loadedNode->GetMeasurement("directColorWithAlpha");
  vtkMRMLMeasurement* loadedEmptyRgbaMeasurement = loadedNode->GetMeasurement("emptyRgba");
  CHECK_NOT_NULL(loadedScalarMeasurement);
  CHECK_NOT_NULL(loadedAllUndefinedScalarMeasurement);
  CHECK_NOT_NULL(loadedRgbMeasurement);
  CHECK_NOT_NULL(loadedRgbaMeasurement);
  CHECK_NOT_NULL(loadedEmptyRgbaMeasurement);
  CHECK_EXIT_SUCCESS(CheckControlPointValues(loadedScalarMeasurement->GetControlPointValues(), 1, { undefined, -2.5, 7.0 }));
  CHECK_EXIT_SUCCESS(CheckControlPointValues(loadedAllUndefinedScalarMeasurement->GetControlPointValues(), 1, { undefined, undefined, undefined }));
  CHECK_EXIT_SUCCESS(CheckControlPointValues(
    loadedRgbMeasurement->GetControlPointValues(), 3, { 1.0, 0.0, 0.0, undefined, 0.25, undefined, 0.0, 0.0, 1.0 }));
  CHECK_EXIT_SUCCESS(CheckControlPointValues(
    loadedRgbaMeasurement->GetControlPointValues(), 4, { 1.0, 0.5, 0.0, 0.25, undefined, undefined, undefined, undefined, 0.0, 0.75, 1.0, 0.6 }));
  CHECK_EXIT_SUCCESS(CheckControlPointValues(loadedEmptyRgbaMeasurement->GetControlPointValues(), 4, {}));

  CHECK_BOOL(loadedDisplayNode->GetScalarVisibility(), true);
  CHECK_BOOL(loadedDisplayNode->GetControlPointScalarVisibility(), true);
  CHECK_STRING(loadedDisplayNode->GetActiveScalarName(), "temperature");
  CHECK_INT(loadedDisplayNode->GetActiveAttributeLocation(), vtkAssignAttribute::POINT_DATA);
  CHECK_INT(loadedDisplayNode->GetScalarRangeFlag(), vtkMRMLDisplayNode::UseManualScalarRange);
  CHECK_DOUBLE(loadedDisplayNode->GetScalarRange()[0], -10.0);
  CHECK_DOUBLE(loadedDisplayNode->GetScalarRange()[1], 10.0);
  CHECK_STRING(loadedDisplayNode->GetColorNodeID(), stableColorNodeID.c_str());

  // A scene-specific color node cannot be resolved from a standalone Markups file.
  vtkNew<vtkMRMLColorTableNode> sceneSpecificColorNode;
  sourceScene->AddNode(sceneSpecificColorNode);
  sourceDisplayNode->SetAndObserveColorNodeID(sceneSpecificColorNode->GetID());
  const std::string sceneSpecificFileName = fileName + ".scene-specific.mrk.json";
  sourceStorageNode->SetFileName(sceneSpecificFileName.c_str());
  sourceStorageNode->GetUserMessages()->ClearMessages();
  TESTING_OUTPUT_ASSERT_WARNINGS_BEGIN();
  CHECK_BOOL(sourceStorageNode->WriteData(sourceNode), true);
  TESTING_OUTPUT_ASSERT_WARNINGS_END();
  CHECK_BOOL(sourceStorageNode->GetUserMessages()->GetNumberOfMessagesOfType(vtkCommand::WarningEvent) >= 1, true);

  vtkNew<vtkMRMLScene> sceneSpecificLoadedScene;
  vtkNew<vtkMRMLApplicationLogic> sceneSpecificLoadedApplicationLogic;
  sceneSpecificLoadedApplicationLogic->SetMRMLScene(sceneSpecificLoadedScene);

  // The explicit empty colorNodeID in the file must override any unrelated
  // color node inherited from the target scene's display-node defaults.
  vtkNew<vtkMRMLColorTableNode> unrelatedDefaultColorNode;
  sceneSpecificLoadedScene->AddNode(unrelatedDefaultColorNode);
  vtkNew<vtkMRMLMarkupsDisplayNode> targetDefaultDisplayNode;
  targetDefaultDisplayNode->SetAndObserveColorNodeID(unrelatedDefaultColorNode->GetID());
  sceneSpecificLoadedScene->AddDefaultNode(targetDefaultDisplayNode);
  vtkMRMLMarkupsDisplayNode* storedTargetDefaultDisplayNode =
    vtkMRMLMarkupsDisplayNode::SafeDownCast(sceneSpecificLoadedScene->GetDefaultNodeByClass("vtkMRMLMarkupsDisplayNode"));
  CHECK_NOT_NULL(storedTargetDefaultDisplayNode);
  CHECK_STRING(storedTargetDefaultDisplayNode->GetColorNodeID(), unrelatedDefaultColorNode->GetID());

  vtkNew<vtkMRMLMarkupsJsonStorageNode> sceneSpecificLoadedStorageNode;
  sceneSpecificLoadedScene->AddNode(sceneSpecificLoadedStorageNode);
  sceneSpecificLoadedStorageNode->SetFileName(sceneSpecificFileName.c_str());
  vtkMRMLMarkupsNode* sceneSpecificLoadedNode = sceneSpecificLoadedStorageNode->AddNewMarkupsNodeFromFile(sceneSpecificFileName.c_str());
  CHECK_NOT_NULL(sceneSpecificLoadedNode);
  vtkMRMLMarkupsDisplayNode* sceneSpecificLoadedDisplayNode = vtkMRMLMarkupsDisplayNode::SafeDownCast(sceneSpecificLoadedNode->GetDisplayNode());
  CHECK_NOT_NULL(sceneSpecificLoadedDisplayNode);
  CHECK_BOOL(sceneSpecificLoadedDisplayNode->GetColorNodeID() == nullptr || sceneSpecificLoadedDisplayNode->GetColorNodeID()[0] == '\0', true);

  return EXIT_SUCCESS;
}

int TestStoragNode(vtkMRMLMarkupsNode* markupsNode, vtkMRMLMarkupsStorageNode* storageNode, const std::string& fileName)
{
  std::cout << "--------------------------------" << std::endl;
  std::cout << "TestStoragNode for " << markupsNode->GetClassName() << " with " << storageNode->GetClassName() << std::endl;

  // set up a scene
  vtkNew<vtkMRMLScene> scene;

  // Application logic - Handle creation of vtkMRMLSelectionNode and vtkMRMLInteractionNode
  vtkNew<vtkMRMLApplicationLogic> applicationLogic;
  applicationLogic->SetMRMLScene(scene);

  scene->AddNode(markupsNode);

  vtkNew<vtkMRMLMarkupsDisplayNode> dispNode;
  scene->AddNode(dispNode);
  markupsNode->SetAndObserveDisplayNodeID(dispNode->GetID());

  scene->AddNode(storageNode);
  markupsNode->SetAndObserveStorageNodeID(storageNode->GetID());

  // add a markup with one point with non default values
  int modifiedPointIndex = markupsNode->AddNControlPoints(1);
  double orientation[4] = { 0.2, 1.0, 0.0, 0.0 };
  markupsNode->SetNthControlPointOrientation(modifiedPointIndex, orientation);
  markupsNode->ResetNthControlPointID(modifiedPointIndex);
  std::string associatedNodeID = std::string("testingAssociatedID");
  markupsNode->SetNthControlPointAssociatedNodeID(modifiedPointIndex, associatedNodeID);
  markupsNode->SetNthControlPointSelected(modifiedPointIndex, false);
  markupsNode->SetNthControlPointVisibility(modifiedPointIndex, false);
  markupsNode->SetNthControlPointLocked(modifiedPointIndex, true);

  std::string label = std::string("Testing label");
  markupsNode->SetNthControlPointLabel(modifiedPointIndex, label);
  std::string desc = std::string("description with spaces");
  markupsNode->SetNthControlPointDescription(modifiedPointIndex, desc);
  // NAN should not be present, but we test this case anyway
  // to make sure that having a NAN does not break reading or writing.
  double inputPoint[3] = { -9.9, 1.1, NAN };
  markupsNode->SetNthControlPointPosition(modifiedPointIndex, inputPoint);

  // and add a markup with 1 point, default values
  if (markupsNode->GetMaximumNumberOfControlPoints() != 1)
  {
    int defaultPointIndex = markupsNode->AddNControlPoints(1);
    CHECK_INT(defaultPointIndex, 1);
  }

  int emptyLabelIndex = -1;
  int commaIndex = -1;
  int quotesIndex = -1;
  bool testManyPoints = markupsNode->GetMaximumNumberOfControlPoints() < 0 || markupsNode->GetMaximumNumberOfControlPoints() > 5;
  if (testManyPoints)
  {
    // and another one unsetting the label
    emptyLabelIndex = markupsNode->AddNControlPoints(1);
    markupsNode->SetNthControlPointLabel(emptyLabelIndex, "");

    // add another one with a label and description that have commas in them
    commaIndex = markupsNode->AddNControlPoints(1);
    markupsNode->SetNthControlPointLabel(commaIndex, "Label, commas, two");
    markupsNode->SetNthControlPointDescription(commaIndex, "Description one comma \"and two quotes\", for more testing");

    // add another one with a label and description with complex combos of commas and quotes
    quotesIndex = markupsNode->AddNControlPoints(1);
    markupsNode->SetNthControlPointLabel(quotesIndex, "Label with end quotes \"around the last phrase\"");
    markupsNode->SetNthControlPointDescription(quotesIndex, "\"Description fully quoted\"");
  }

  //
  // test write
  //

  if (verbose)
  {
    std::cout << "\nWriting this markup to file:" << std::endl;
    vtkIndent indent;
    markupsNode->PrintSelf(std::cout, indent);
    std::cout << std::endl;
  }

  storageNode->SetFileName(fileName.c_str());
  std::cout << "Writing " << storageNode->GetFileName() << std::endl;
  // Writing fcsv files is expected to log a deprecation warning.
  bool isDeprecatedFcsvFormat = storageNode->IsA("vtkMRMLMarkupsFiducialStorageNode") != 0;
  if (isDeprecatedFcsvFormat)
  {
    TESTING_OUTPUT_ASSERT_WARNINGS_BEGIN();
  }
  CHECK_BOOL(storageNode->WriteData(markupsNode), true);
  if (isDeprecatedFcsvFormat)
  {
    TESTING_OUTPUT_ASSERT_WARNINGS(1);
    TESTING_OUTPUT_ASSERT_WARNINGS_END();
  }

  //
  // test read
  //

  vtkSmartPointer<vtkMRMLMarkupsNode> markupsNode2 = vtkSmartPointer<vtkMRMLMarkupsNode>::Take(vtkMRMLMarkupsNode::SafeDownCast(markupsNode->CreateNodeInstance()));

  vtkNew<vtkMRMLScene> scene2;
  // Application logic - Handle creation of vtkMRMLSelectionNode and vtkMRMLInteractionNode
  vtkNew<vtkMRMLApplicationLogic> applicationLogic2;
  applicationLogic2->SetMRMLScene(scene2);

  vtkSmartPointer<vtkMRMLMarkupsStorageNode> snode2 = vtkSmartPointer<vtkMRMLMarkupsStorageNode>::Take(vtkMRMLMarkupsStorageNode::SafeDownCast(storageNode->CreateNodeInstance()));

  scene2->AddNode(snode2);
  scene2->AddNode(markupsNode2);
  markupsNode2->SetAndObserveStorageNodeID(snode2->GetID());
  snode2->SetFileName(storageNode->GetFileName());

  std::cout << "Reading from " << snode2->GetFileName() << std::endl;
  CHECK_BOOL(snode2->ReadData(markupsNode2), true);

  if (verbose)
  {
    std::cout << "\nMarkup read from file = " << std::endl;
    vtkIndent indent;
    markupsNode2->PrintSelf(std::cout, indent);
    std::cout << std::endl;
  }

  // test values on the first markup
  double newOrientation[4] = { -5, -5, -5, -5 };
  markupsNode2->GetNthControlPointOrientation(modifiedPointIndex, newOrientation);
  std::cout << "Orientation from read file: [" << newOrientation[0] << ", " << newOrientation[1] << ", " << newOrientation[2] << ", " << newOrientation[3] << "]" << std::endl;
  for (int r = 0; r < 4; r++)
  {
    CHECK_DOUBLE_TOLERANCE(newOrientation[r], orientation[r], 1e-3);
  }
  CHECK_STD_STRING(markupsNode2->GetNthControlPointAssociatedNodeID(modifiedPointIndex), associatedNodeID);
  CHECK_BOOL(markupsNode2->GetNthControlPointSelected(modifiedPointIndex), false);
  CHECK_BOOL(markupsNode2->GetNthControlPointVisibility(modifiedPointIndex), false);
  CHECK_BOOL(markupsNode2->GetNthControlPointLocked(modifiedPointIndex), true);
  CHECK_STD_STRING(markupsNode2->GetNthControlPointLabel(modifiedPointIndex), label);
  CHECK_STD_STRING(markupsNode2->GetNthControlPointDescription(modifiedPointIndex), desc);

  double outputPoint[3] = { -3, -3, -3 };
  markupsNode2->GetNthControlPointPosition(modifiedPointIndex, outputPoint);
  CHECK_DOUBLE_TOLERANCE(outputPoint[0], inputPoint[0], 0.1);
  CHECK_DOUBLE_TOLERANCE(outputPoint[1], inputPoint[1], 0.1);
  CHECK_DOUBLE_TOLERANCE(outputPoint[2], inputPoint[2], 0.1);

  if (testManyPoints)
  {
    // test the unset label on the third markup
    CHECK_STD_STRING(markupsNode2->GetNthControlPointLabel(emptyLabelIndex), "");
  }

  // now read it again with a display node defined
  vtkNew<vtkMRMLMarkupsDisplayNode> dispNode2;
  scene->AddNode(dispNode2);
  markupsNode2->SetAndObserveDisplayNodeID(dispNode2->GetID());
  std::cout << "Added display node, re-reading from " << snode2->GetFileName() << std::endl;
  CHECK_BOOL(snode2->ReadData(markupsNode2), true);

  //
  // test with RAS coordinate system
  storageNode->UseRASOn();
  std::cout << "Writing file in RAS coordinate system: " << storageNode->GetFileName() << std::endl;
  if (isDeprecatedFcsvFormat)
  {
    TESTING_OUTPUT_ASSERT_WARNINGS_BEGIN();
  }
  CHECK_BOOL(storageNode->WriteData(markupsNode), true);
  if (isDeprecatedFcsvFormat)
  {
    TESTING_OUTPUT_ASSERT_WARNINGS(1);
    TESTING_OUTPUT_ASSERT_WARNINGS_END();
  }

  // read it in after clearing out the test data
  // Set to use LPS to verify that not this hint but the coordinate system that
  // is specified in the file is taken into account.
  snode2->UseLPSOn();
  markupsNode2->RemoveAllControlPoints();
  std::cout << "Reading file that uses RAS coordinate system from " << snode2->GetFileName() << std::endl;

  CHECK_BOOL(snode2->ReadData(markupsNode2), true);
  CHECK_BOOL(snode2->GetUseRAS(), true);

  if (verbose)
  {
    std::cout << "\nMarkups specified in RAS read from file: " << storageNode->GetFileName() << std::endl;
    vtkIndent indent;
    markupsNode2->PrintSelf(std::cout, indent);
  }

  // check the point coordinates are correct when stored in files in RAS coordinate system
  double outputPointLoadedFromRASFile[3];
  markupsNode2->GetNthControlPointPosition(modifiedPointIndex, outputPointLoadedFromRASFile);
  CHECK_DOUBLE_TOLERANCE(outputPointLoadedFromRASFile[0], inputPoint[0], 0.1);
  CHECK_DOUBLE_TOLERANCE(outputPointLoadedFromRASFile[1], inputPoint[1], 0.1);
  CHECK_DOUBLE_TOLERANCE(outputPointLoadedFromRASFile[2], inputPoint[2], 0.1);

  if (testManyPoints)
  {
    // check for commas in the markup label and description
    std::string labelWithCommas = markupsNode2->GetNthControlPointLabel(commaIndex);
    int numCommas = std::count(labelWithCommas.begin(), labelWithCommas.end(), ',');
    CHECK_INT(numCommas, 2);
    std::string descriptionWithCommasAndQuotes = markupsNode2->GetNthControlPointDescription(commaIndex);
    numCommas = std::count(descriptionWithCommasAndQuotes.begin(), descriptionWithCommasAndQuotes.end(), ',');
    CHECK_INT(numCommas, 1);
    int numQuotes = std::count(descriptionWithCommasAndQuotes.begin(), descriptionWithCommasAndQuotes.end(), '"');
    CHECK_INT(numQuotes, 2);

    // check ending quoted label
    std::string labelWithQuotes = markupsNode2->GetNthControlPointLabel(quotesIndex);
    numQuotes = std::count(labelWithQuotes.begin(), labelWithQuotes.end(), '"');
    CHECK_INT(numQuotes, 2);

    // check fully quoted description
    std::string descWithQuotes = markupsNode2->GetNthControlPointDescription(quotesIndex);
    numQuotes = std::count(descWithQuotes.begin(), descWithQuotes.end(), '"');
    CHECK_INT(numQuotes, 2);
  }

  return EXIT_SUCCESS;
}

int vtkMRMLMarkupsStorageNodeTest2(int argc, char* argv[])
{
  vtkNew<vtkMRMLMarkupsFiducialStorageNode> storageNodeFcsv;
  EXERCISE_ALL_BASIC_MRML_METHODS(storageNodeFcsv);

  vtkNew<vtkMRMLMarkupsJsonStorageNode> storageNodeJson;
  EXERCISE_ALL_BASIC_MRML_METHODS(storageNodeJson);

  // Test if information can be saved to file and retrieved
  std::string tempFolder = ".";
  if (argc > 1)
  {
    tempFolder = std::string(argv[1]);
  }
  CHECK_EXIT_SUCCESS(TestStoragNode(vtkSmartPointer<vtkMRMLMarkupsFiducialNode>::New(),
                                    vtkSmartPointer<vtkMRMLMarkupsFiducialStorageNode>::New(),
                                    tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-fiducial-temp.fcsv"));
  CHECK_EXIT_SUCCESS(TestStoragNode(vtkSmartPointer<vtkMRMLMarkupsFiducialNode>::New(),
                                    vtkSmartPointer<vtkMRMLMarkupsJsonStorageNode>::New(),
                                    tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-fiducial-temp.mrk.json"));
  CHECK_EXIT_SUCCESS(TestStoragNode(
    vtkSmartPointer<vtkMRMLMarkupsLineNode>::New(), vtkSmartPointer<vtkMRMLMarkupsJsonStorageNode>::New(), tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-line-temp.mrk.json"));
  CHECK_EXIT_SUCCESS(TestStoragNode(
    vtkSmartPointer<vtkMRMLMarkupsAngleNode>::New(), vtkSmartPointer<vtkMRMLMarkupsJsonStorageNode>::New(), tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-angle-temp.mrk.json"));
  CHECK_EXIT_SUCCESS(TestStoragNode(vtkSmartPointer<vtkMRMLMarkupsCurveNode>::New(),
                                    vtkSmartPointer<vtkMRMLMarkupsJsonStorageNode>::New(),
                                    tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-opencurve-temp.mrk.json"));
  CHECK_EXIT_SUCCESS(TestStoragNode(vtkSmartPointer<vtkMRMLMarkupsClosedCurveNode>::New(),
                                    vtkSmartPointer<vtkMRMLMarkupsJsonStorageNode>::New(),
                                    tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-closedcurve-temp.mrk.json"));
  CHECK_EXIT_SUCCESS(TestStoragNode(vtkSmartPointer<vtkMRMLMarkupsPlaneNode>::New(),
                                    vtkSmartPointer<vtkMRMLMarkupsPlaneJsonStorageNode>::New(),
                                    tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-plane-temp.mrk.json"));
  CHECK_EXIT_SUCCESS(TestStoragNode(
    vtkSmartPointer<vtkMRMLMarkupsROINode>::New(), vtkSmartPointer<vtkMRMLMarkupsROIJsonStorageNode>::New(), tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-roi-temp.mrk.json"));
  CHECK_EXIT_SUCCESS(TestControlPointScalarJsonPersistence(tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-control-point-scalars-temp.mrk.json"));

  // Test if markups node can be instantiated correctly
  vtkNew<vtkMRMLScene> scene;
  // Application logic - Handle creation of vtkMRMLSelectionNode and vtkMRMLInteractionNode
  vtkNew<vtkMRMLApplicationLogic> applicationLogic;
  applicationLogic->SetMRMLScene(scene);
  scene->RegisterNodeClass(vtkSmartPointer<vtkMRMLMarkupsPlaneJsonStorageNode>::New());
  scene->RegisterNodeClass(vtkSmartPointer<vtkMRMLMarkupsROIJsonStorageNode>::New());

  scene->AddNode(storageNodeJson);

  vtkMRMLMarkupsNode* fiducialNode2 = storageNodeJson->AddNewMarkupsNodeFromFile(std::string(tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-fiducial-temp.mrk.json").c_str());
  CHECK_NOT_NULL(fiducialNode2);
  CHECK_STRING(fiducialNode2->GetClassName(), "vtkMRMLMarkupsFiducialNode");

  vtkMRMLMarkupsNode* lineNode = storageNodeJson->AddNewMarkupsNodeFromFile(std::string(tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-line-temp.mrk.json").c_str());
  CHECK_NOT_NULL(lineNode);
  CHECK_STRING(lineNode->GetClassName(), "vtkMRMLMarkupsLineNode");

  vtkMRMLMarkupsNode* angleNode = storageNodeJson->AddNewMarkupsNodeFromFile(std::string(tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-angle-temp.mrk.json").c_str());
  CHECK_NOT_NULL(angleNode);
  CHECK_STRING(angleNode->GetClassName(), "vtkMRMLMarkupsAngleNode");

  vtkMRMLMarkupsNode* openCurveNode = storageNodeJson->AddNewMarkupsNodeFromFile(std::string(tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-opencurve-temp.mrk.json").c_str());
  CHECK_NOT_NULL(openCurveNode);
  CHECK_STRING(openCurveNode->GetClassName(), "vtkMRMLMarkupsCurveNode");

  vtkMRMLMarkupsNode* closedurveNode = storageNodeJson->AddNewMarkupsNodeFromFile(std::string(tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-closedcurve-temp.mrk.json").c_str());
  CHECK_NOT_NULL(closedurveNode);
  CHECK_STRING(closedurveNode->GetClassName(), "vtkMRMLMarkupsClosedCurveNode");

  vtkMRMLMarkupsNode* planeNode = storageNodeJson->AddNewMarkupsNodeFromFile(std::string(tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-plane-temp.mrk.json").c_str());
  CHECK_NOT_NULL(planeNode);
  CHECK_STRING(planeNode->GetClassName(), "vtkMRMLMarkupsPlaneNode");

  vtkMRMLMarkupsNode* roiNode = storageNodeJson->AddNewMarkupsNodeFromFile(std::string(tempFolder + "/vtkMRMLMarkupsStorageNodeTest2-roi-temp.mrk.json").c_str());
  CHECK_NOT_NULL(roiNode);
  CHECK_STRING(roiNode->GetClassName(), "vtkMRMLMarkupsROINode");

  return EXIT_SUCCESS;
}
