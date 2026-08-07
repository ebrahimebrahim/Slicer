/*==============================================================================

  Program: 3D Slicer

  Copyright (c) 3D Slicer contributors. All Rights Reserved.

  See COPYRIGHT.txt
  or http://www.slicer.org/copyright/copyright.txt for details.

==============================================================================*/

// Markups includes
#include "vtkSlicerCurveRepresentation2D.h"
#include "vtkSlicerCurveRepresentation3D.h"
#include "vtkSlicerPointsRepresentation2D.h"
#include "vtkSlicerPointsRepresentation3D.h"

// MRML includes
#include <vtkMRMLColorTableNode.h>
#include <vtkMRMLMarkupsClosedCurveNode.h>
#include <vtkMRMLMarkupsDisplayNode.h>
#include <vtkMRMLMarkupsFiducialNode.h>
#include <vtkMRMLScene.h>
#include <vtkMRMLSliceNode.h>
#include <vtkMRMLStaticMeasurement.h>
#include <vtkMRMLViewNode.h>

// VTK includes
#include <vtkCommand.h>
#include <vtkDoubleArray.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkGlyph2D.h>
#include <vtkGlyph3DMapper.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>
#include <vtkRenderer.h>
#include <vtkUnsignedCharArray.h>
#include <vtkVector.h>

// STD includes
#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

namespace
{
constexpr const char* CONTROL_POINT_COLORS_ARRAY_NAME = "controlPointColors";

class vtkSlicerPointsRepresentation3DTestHelper : public vtkSlicerPointsRepresentation3D
{
public:
  static vtkSlicerPointsRepresentation3DTestHelper* New();
  vtkTypeMacro(vtkSlicerPointsRepresentation3DTestHelper, vtkSlicerPointsRepresentation3D);

  vtkGlyph3DMapper* GetGlyphMapper(int controlPointType) { return this->GetControlPointsPipeline(controlPointType)->GlyphMapper; }
  vtkGlyph3DMapper* GetOccludedGlyphMapper(int controlPointType) { return this->GetControlPointsPipeline(controlPointType)->OccludedGlyphMapper; }

protected:
  vtkSlicerPointsRepresentation3DTestHelper() = default;
  ~vtkSlicerPointsRepresentation3DTestHelper() override = default;
};

vtkStandardNewMacro(vtkSlicerPointsRepresentation3DTestHelper);

class vtkSlicerPointsRepresentation2DTestHelper : public vtkSlicerPointsRepresentation2D
{
public:
  static vtkSlicerPointsRepresentation2DTestHelper* New();
  vtkTypeMacro(vtkSlicerPointsRepresentation2DTestHelper, vtkSlicerPointsRepresentation2D);

  vtkPolyData* GetGlyphOutput(int controlPointType) { return this->GetControlPointsPipeline(controlPointType)->Glypher->GetOutput(); }
  vtkPolyDataMapper2D* GetMapper(int controlPointType) { return this->GetControlPointsPipeline(controlPointType)->Mapper; }

protected:
  vtkSlicerPointsRepresentation2DTestHelper() = default;
  ~vtkSlicerPointsRepresentation2DTestHelper() override = default;
};

vtkStandardNewMacro(vtkSlicerPointsRepresentation2DTestHelper);

class vtkSlicerCurveRepresentation2DTestHelper : public vtkSlicerCurveRepresentation2D
{
public:
  static vtkSlicerCurveRepresentation2DTestHelper* New();
  vtkTypeMacro(vtkSlicerCurveRepresentation2DTestHelper, vtkSlicerCurveRepresentation2D);

  vtkPolyData* GetGlyphOutput(int controlPointType) { return this->GetControlPointsPipeline(controlPointType)->Glypher->GetOutput(); }

protected:
  vtkSlicerCurveRepresentation2DTestHelper() = default;
  ~vtkSlicerCurveRepresentation2DTestHelper() override = default;
};

vtkStandardNewMacro(vtkSlicerCurveRepresentation2DTestHelper);

class vtkSlicerCurveRepresentation3DTestHelper : public vtkSlicerCurveRepresentation3D
{
public:
  static vtkSlicerCurveRepresentation3DTestHelper* New();
  vtkTypeMacro(vtkSlicerCurveRepresentation3DTestHelper, vtkSlicerCurveRepresentation3D);

protected:
  vtkSlicerCurveRepresentation3DTestHelper() = default;
  ~vtkSlicerCurveRepresentation3DTestHelper() override = default;
};

vtkStandardNewMacro(vtkSlicerCurveRepresentation3DTestHelper);

bool CheckColor(vtkPolyData* polyData, vtkIdType tupleIndex, const std::array<unsigned char, 4>& expected)
{
  vtkUnsignedCharArray* colors = vtkUnsignedCharArray::SafeDownCast(polyData->GetPointData()->GetArray(CONTROL_POINT_COLORS_ARRAY_NAME));
  if (!colors || colors->GetNumberOfComponents() != 4 || tupleIndex < 0 || tupleIndex >= colors->GetNumberOfTuples())
  {
    std::cerr << "Missing or invalid control point color array" << std::endl;
    return false;
  }

  unsigned char actual[4] = { 0, 0, 0, 0 };
  colors->GetTypedTuple(tupleIndex, actual);
  if (!std::equal(actual, actual + 4, expected.begin()))
  {
    std::cerr << "Unexpected RGBA tuple: " << static_cast<int>(actual[0]) << ", " << static_cast<int>(actual[1]) << ", " << static_cast<int>(actual[2]) << ", "
              << static_cast<int>(actual[3]) << std::endl;
    return false;
  }
  return true;
}

bool CheckRepeatedColor(vtkPolyData* polyData, const std::array<unsigned char, 4>& expected)
{
  vtkUnsignedCharArray* colors = vtkUnsignedCharArray::SafeDownCast(polyData->GetPointData()->GetArray(CONTROL_POINT_COLORS_ARRAY_NAME));
  if (!colors || polyData->GetNumberOfPoints() <= 1 || colors->GetNumberOfTuples() != polyData->GetNumberOfPoints())
  {
    std::cerr << "Control point color was not expanded over the 2D glyph" << std::endl;
    return false;
  }

  for (vtkIdType tupleIndex = 0; tupleIndex < colors->GetNumberOfTuples(); ++tupleIndex)
  {
    unsigned char actual[4] = { 0, 0, 0, 0 };
    colors->GetTypedTuple(tupleIndex, actual);
    if (!std::equal(actual, actual + 4, expected.begin()))
    {
      std::cerr << "2D glyph contains an unexpected RGBA tuple" << std::endl;
      return false;
    }
  }
  return true;
}

vtkIdType CountColor(vtkPolyData* polyData, const std::array<unsigned char, 4>& expected)
{
  vtkUnsignedCharArray* colors = vtkUnsignedCharArray::SafeDownCast(polyData->GetPointData()->GetArray(CONTROL_POINT_COLORS_ARRAY_NAME));
  if (!colors || colors->GetNumberOfComponents() != 4 || colors->GetNumberOfTuples() != polyData->GetNumberOfPoints())
  {
    std::cerr << "Missing or invalid control point color array" << std::endl;
    return -1;
  }

  vtkIdType count = 0;
  for (vtkIdType tupleIndex = 0; tupleIndex < colors->GetNumberOfTuples(); ++tupleIndex)
  {
    unsigned char actual[4] = { 0, 0, 0, 0 };
    colors->GetTypedTuple(tupleIndex, actual);
    if (std::equal(actual, actual + 4, expected.begin()))
    {
      ++count;
    }
  }
  return count;
}

void ConfigureRepresentation(vtkSlicerMarkupsWidgetRepresentation* representation,
                             vtkRenderer* renderer,
                             vtkMRMLAbstractViewNode* viewNode,
                             vtkMRMLMarkupsDisplayNode* displayNode)
{
  representation->SetRenderer(renderer);
  representation->SetViewNode(viewNode);
  representation->SetMarkupsDisplayNode(displayNode);
  representation->UpdateFromMRML(nullptr, 0);
}
}

//----------------------------------------------------------------------------
int vtkSlicerMarkupsWidgetRepresentationTest1(int, char*[])
{
  vtkNew<vtkMRMLScene> scene;
  vtkNew<vtkMRMLViewNode> viewNode;
  scene->AddNode(viewNode);

  vtkNew<vtkRenderer> renderer;
  vtkNew<vtkGenericOpenGLRenderWindow> renderWindow;
  renderWindow->AddRenderer(renderer);

  vtkNew<vtkMRMLColorTableNode> colorNode;
  colorNode->SetTypeToUser();
  colorNode->SetNumberOfColors(3);
  colorNode->GetLookupTable()->SetTableRange(0.0, 255.0);
  colorNode->SetColor(0, "zero", 0.0, 0.0, 1.0, 1.0);
  colorNode->SetColor(1, "one", 1.0, 0.0, 0.0, 0.5);
  colorNode->SetColor(2, "two", 0.0, 1.0, 0.0, 1.0);
  if (!colorNode->SetTerminologyFromString(1, "~SCT^123^Category~SCT^456^One~^^~~^^~^^"))
  {
    std::cerr << "Failed to configure terminology-aware test color table" << std::endl;
    return EXIT_FAILURE;
  }
  scene->AddNode(colorNode);

  vtkNew<vtkMRMLMarkupsFiducialNode> markupsNode;
  scene->AddNode(markupsNode);
  markupsNode->AddControlPoint(vtkVector3d(0.0, 0.0, 0.0));
  markupsNode->AddControlPoint(vtkVector3d(10.0, 0.0, 0.0));
  markupsNode->AddControlPoint(vtkVector3d(20.0, 0.0, 0.0));
  markupsNode->SetNthControlPointSelected(0, false);

  vtkNew<vtkMRMLMarkupsDisplayNode> displayNode;
  scene->AddNode(displayNode);
  markupsNode->AddAndObserveDisplayNodeID(displayNode->GetID());
  displayNode->SetAndObserveColorNodeID(colorNode->GetID());
  displayNode->SetActiveScalarName("category");
  displayNode->SetScalarRangeFlag(vtkMRMLDisplayNode::UseColorNodeScalarRange);
  displayNode->SetControlPointScalarVisibility(true);
  displayNode->SetActiveControlPoint(2);
  displayNode->SetColor(0.2, 0.4, 0.6);
  displayNode->SetSelectedColor(1.0, 0.5, 0.5);
  displayNode->SetActiveColor(0.4, 1.0, 0.0);

  vtkNew<vtkDoubleArray> categoryValues;
  categoryValues->InsertNextValue(1.0);
  categoryValues->InsertNextValue(2.0);
  categoryValues->InsertNextValue(0.0);
  vtkNew<vtkMRMLStaticMeasurement> categoryMeasurement;
  categoryMeasurement->SetName("category");
  categoryMeasurement->SetControlPointValues(categoryValues);
  markupsNode->AddMeasurement(categoryMeasurement);

  vtkNew<vtkSlicerPointsRepresentation3DTestHelper> representation;
  ConfigureRepresentation(representation, renderer, viewNode, displayNode);

  vtkPolyData* unselectedPoints = representation->GetControlPointsPolyData(vtkSlicerMarkupsWidgetRepresentation::Unselected);
  vtkPolyData* selectedPoints = representation->GetControlPointsPolyData(vtkSlicerMarkupsWidgetRepresentation::Selected);
  vtkPolyData* activePoints = representation->GetControlPointsPolyData(vtkSlicerMarkupsWidgetRepresentation::Active);
  if (unselectedPoints->GetNumberOfPoints() != 1 || selectedPoints->GetNumberOfPoints() != 1 || activePoints->GetNumberOfPoints() != 1
      || !CheckColor(unselectedPoints, 0, { 255, 0, 0, 128 }) || !CheckColor(selectedPoints, 0, { 0, 255, 0, 255 }))
  {
    return EXIT_FAILURE;
  }

  for (vtkGlyph3DMapper* mapper : { representation->GetGlyphMapper(vtkSlicerMarkupsWidgetRepresentation::Unselected),
                                    representation->GetOccludedGlyphMapper(vtkSlicerMarkupsWidgetRepresentation::Unselected) })
  {
    if (!mapper->GetScalarVisibility() || mapper->GetScalarMode() != VTK_SCALAR_MODE_USE_POINT_FIELD_DATA || mapper->GetColorMode() != VTK_COLOR_MODE_DIRECT_SCALARS)
    {
      std::cerr << "3D glyph mapper is not configured for direct point-field RGBA" << std::endl;
      return EXIT_FAILURE;
    }
  }
  if (representation->GetGlyphMapper(vtkSlicerMarkupsWidgetRepresentation::Active)->GetScalarVisibility())
  {
    std::cerr << "Active control points must retain their interaction color" << std::endl;
    return EXIT_FAILURE;
  }

  // A non-finite scalar falls back to the normal unselected color.
  categoryMeasurement->GetControlPointValues()->SetValue(0, std::numeric_limits<double>::quiet_NaN());
  representation->UpdateFromMRML(markupsNode, vtkCommand::ModifiedEvent);
  if (!CheckColor(unselectedPoints, 0, { 51, 102, 153, 255 }))
  {
    return EXIT_FAILURE;
  }

  // Terminology-backed scalars are row indices, not continuous values mapped
  // through the lookup-table range.
  categoryMeasurement->GetControlPointValues()->SetValue(0, 1.5);
  representation->UpdateFromMRML(markupsNode, vtkCommand::ModifiedEvent);
  if (!CheckColor(unselectedPoints, 0, { 51, 102, 153, 255 }))
  {
    std::cerr << "A non-integral terminology row did not use the fallback color" << std::endl;
    return EXIT_FAILURE;
  }
  categoryMeasurement->GetControlPointValues()->SetValue(0, 99.0);
  representation->UpdateFromMRML(markupsNode, vtkCommand::ModifiedEvent);
  if (!CheckColor(unselectedPoints, 0, { 51, 102, 153, 255 }))
  {
    std::cerr << "An out-of-range terminology row did not use the fallback color" << std::endl;
    return EXIT_FAILURE;
  }

  displayNode->SetScalarRangeFlag(vtkMRMLDisplayNode::UseDirectMapping);
  categoryMeasurement->GetControlPointValues()->SetValue(0, 1.0);
  representation->UpdateFromMRML(displayNode, vtkCommand::ModifiedEvent);
  if (!CheckColor(unselectedPoints, 0, { 51, 102, 153, 255 }))
  {
    std::cerr << "A one-component value was incorrectly treated as a direct color" << std::endl;
    return EXIT_FAILURE;
  }

  // Ordinary one-component measurements use continuous lookup-table mapping.
  // The range deliberately differs from the table's row indices.
  vtkNew<vtkMRMLColorTableNode> continuousColorNode;
  continuousColorNode->SetTypeToUser();
  continuousColorNode->SetNumberOfColors(2);
  continuousColorNode->GetLookupTable()->SetTableRange(100.0, 200.0);
  continuousColorNode->SetColor(0, "low", 1.0, 1.0, 0.0, 1.0);
  continuousColorNode->SetColor(1, "high", 0.0, 1.0, 1.0, 1.0);
  scene->AddNode(continuousColorNode);

  vtkNew<vtkDoubleArray> continuousValues;
  continuousValues->InsertNextValue(10.0);
  continuousValues->InsertNextValue(0.0);
  continuousValues->InsertNextValue(5.0);
  categoryMeasurement->SetControlPointValues(continuousValues);
  displayNode->SetAndObserveColorNodeID(continuousColorNode->GetID());
  displayNode->SetScalarRangeFlag(vtkMRMLDisplayNode::UseDataScalarRange);
  displayNode->UpdateScalarRange();
  representation->UpdateFromMRML(displayNode, vtkCommand::ModifiedEvent);
  if (!CheckColor(unselectedPoints, 0, { 0, 255, 255, 255 }) || !CheckColor(selectedPoints, 0, { 255, 255, 0, 255 }))
  {
    std::cerr << "Continuous scalar values were not mapped through the color table" << std::endl;
    return EXIT_FAILURE;
  }
  double sharedLookupTableRange[2] = { 0.0, 0.0 };
  continuousColorNode->GetLookupTable()->GetTableRange(sharedLookupTableRange);
  if (sharedLookupTableRange[0] != 100.0 || sharedLookupTableRange[1] != 200.0)
  {
    std::cerr << "Control point rendering modified the shared color node lookup-table range" << std::endl;
    return EXIT_FAILURE;
  }

  displayNode->SetControlPointScalarVisibility(false);
  representation->UpdateFromMRML(displayNode, vtkCommand::ModifiedEvent);
  if (!CheckColor(unselectedPoints, 0, { 51, 102, 153, 255 }) || !CheckColor(selectedPoints, 0, { 255, 128, 128, 255 }))
  {
    std::cerr << "Disabling control point scalar visibility did not restore standard colors" << std::endl;
    return EXIT_FAILURE;
  }
  displayNode->SetControlPointScalarVisibility(true);

  // Three- and four-component values are accepted only in direct mapping mode.
  vtkNew<vtkDoubleArray> directValues;
  directValues->SetNumberOfComponents(4);
  directValues->InsertNextTuple4(0.1, 0.2, 0.3, 0.4);
  directValues->InsertNextTuple4(0.5, 0.6, 0.7, 0.8);
  directValues->InsertNextTuple4(0.9, 1.0, 0.0, 1.0);
  categoryMeasurement->SetControlPointValues(directValues);
  representation->UpdateFromMRML(markupsNode, vtkCommand::ModifiedEvent);
  if (!CheckColor(unselectedPoints, 0, { 51, 102, 153, 255 }) || !CheckColor(selectedPoints, 0, { 255, 128, 128, 255 }))
  {
    std::cerr << "Multi-component tuples must not override standard colors without direct mapping" << std::endl;
    return EXIT_FAILURE;
  }

  displayNode->SetScalarRangeFlag(vtkMRMLDisplayNode::UseDirectMapping);
  representation->UpdateFromMRML(markupsNode, vtkCommand::ModifiedEvent);
  if (!CheckColor(unselectedPoints, 0, { 26, 51, 77, 102 }) || !CheckColor(selectedPoints, 0, { 128, 153, 179, 204 }))
  {
    return EXIT_FAILURE;
  }

  vtkDoubleArray* storedDirectValues = categoryMeasurement->GetControlPointValues();
  storedDirectValues->SetComponent(0, 2, std::numeric_limits<double>::quiet_NaN());
  storedDirectValues->Modified();
  representation->UpdateFromMRML(markupsNode, vtkCommand::ModifiedEvent);
  if (!CheckColor(unselectedPoints, 0, { 51, 102, 153, 255 }) || !CheckColor(selectedPoints, 0, { 128, 153, 179, 204 }))
  {
    std::cerr << "A non-finite direct-color tuple did not use the fallback color" << std::endl;
    return EXIT_FAILURE;
  }
  categoryMeasurement->SetControlPointValues(directValues);

  vtkNew<vtkDoubleArray> directRgbValues;
  directRgbValues->SetNumberOfComponents(3);
  directRgbValues->InsertNextTuple3(1.2, -0.2, 0.5);
  directRgbValues->InsertNextTuple3(0.0, 1.0, 0.25);
  directRgbValues->InsertNextTuple3(0.9, 1.0, 0.0);
  categoryMeasurement->SetControlPointValues(directRgbValues);
  representation->UpdateFromMRML(markupsNode, vtkCommand::ModifiedEvent);
  if (!CheckColor(unselectedPoints, 0, { 255, 0, 128, 255 }) || !CheckColor(selectedPoints, 0, { 0, 255, 64, 255 }))
  {
    std::cerr << "Direct RGB colors were not clamped or assigned an opaque alpha" << std::endl;
    return EXIT_FAILURE;
  }

  categoryMeasurement->SetControlPointValues(directValues);
  representation->UpdateFromMRML(markupsNode, vtkCommand::ModifiedEvent);

  // vtkGlyph2D produces multiple output points for each input point. Verify that
  // the transient RGBA tuple is expanded over the complete glyph.
  vtkNew<vtkMRMLSliceNode> sliceNode;
  scene->AddNode(sliceNode);
  sliceNode->SetDimensions(200, 200, 1);
  sliceNode->SetFieldOfView(200.0, 200.0, 1.0);
  sliceNode->UpdateMatrices();
  renderWindow->SetSize(200, 200);

  vtkNew<vtkSlicerPointsRepresentation2DTestHelper> representation2D;
  ConfigureRepresentation(representation2D, renderer, sliceNode, displayNode);
  vtkPolyData* unselectedGlyph = representation2D->GetGlyphOutput(vtkSlicerMarkupsWidgetRepresentation::Unselected);
  vtkPolyData* selectedGlyph = representation2D->GetGlyphOutput(vtkSlicerMarkupsWidgetRepresentation::Selected);
  if (!CheckRepeatedColor(unselectedGlyph, { 26, 51, 77, 102 }) || !CheckRepeatedColor(selectedGlyph, { 128, 153, 179, 204 }))
  {
    return EXIT_FAILURE;
  }
  for (int controlPointType : { vtkSlicerMarkupsWidgetRepresentation::Unselected, vtkSlicerMarkupsWidgetRepresentation::Selected })
  {
    vtkPolyDataMapper2D* mapper = representation2D->GetMapper(controlPointType);
    if (!mapper->GetScalarVisibility() || mapper->GetScalarMode() != VTK_SCALAR_MODE_USE_POINT_DATA || mapper->GetColorMode() != VTK_COLOR_MODE_DIRECT_SCALARS)
    {
      std::cerr << "2D glyph mapper is not configured for direct point-data RGBA" << std::endl;
      return EXIT_FAILURE;
    }
  }
  if (representation2D->GetMapper(vtkSlicerMarkupsWidgetRepresentation::Active)->GetScalarVisibility()
      || representation2D->GetMapper(vtkSlicerMarkupsWidgetRepresentation::Project)->GetScalarVisibility()
      || representation2D->GetMapper(vtkSlicerMarkupsWidgetRepresentation::ProjectBack)->GetScalarVisibility())
  {
    std::cerr << "Active and projected 2D control points must retain their existing colors" << std::endl;
    return EXIT_FAILURE;
  }

  // The closed-curve center is synthetic and always receives a fallback tuple.
  vtkNew<vtkMRMLMarkupsClosedCurveNode> closedCurveNode;
  scene->AddNode(closedCurveNode);
  closedCurveNode->AddControlPoint(vtkVector3d(0.0, 0.0, 0.0));
  closedCurveNode->AddControlPoint(vtkVector3d(10.0, 0.0, 0.0));
  closedCurveNode->AddControlPoint(vtkVector3d(0.0, 10.0, 0.0));

  vtkNew<vtkMRMLMarkupsDisplayNode> closedCurveDisplayNode;
  scene->AddNode(closedCurveDisplayNode);
  closedCurveNode->AddAndObserveDisplayNodeID(closedCurveDisplayNode->GetID());
  closedCurveDisplayNode->SetAndObserveColorNodeID(colorNode->GetID());
  closedCurveDisplayNode->SetActiveScalarName("category");
  closedCurveDisplayNode->SetScalarRangeFlag(vtkMRMLDisplayNode::UseColorNodeScalarRange);
  closedCurveDisplayNode->SetControlPointScalarVisibility(true);
  closedCurveDisplayNode->SetColor(0.2, 0.4, 0.6);
  closedCurveDisplayNode->SetSelectedColor(1.0, 0.5, 0.5);
  closedCurveDisplayNode->SetActiveColor(0.4, 1.0, 0.0);

  vtkNew<vtkDoubleArray> closedCurveValues;
  closedCurveValues->InsertNextValue(0.0);
  closedCurveValues->InsertNextValue(1.0);
  closedCurveValues->InsertNextValue(2.0);
  vtkNew<vtkMRMLStaticMeasurement> closedCurveMeasurement;
  closedCurveMeasurement->SetName("category");
  closedCurveMeasurement->SetControlPointValues(closedCurveValues);
  closedCurveNode->AddMeasurement(closedCurveMeasurement);

  vtkNew<vtkSlicerCurveRepresentation3DTestHelper> curveRepresentation;
  ConfigureRepresentation(curveRepresentation, renderer, viewNode, closedCurveDisplayNode);
  vtkPolyData* selectedCurvePoints = curveRepresentation->GetControlPointsPolyData(vtkSlicerMarkupsWidgetRepresentation::Selected);
  if (selectedCurvePoints->GetNumberOfPoints() != 4 || CountColor(selectedCurvePoints, { 255, 128, 128, 255 }) != 1)
  {
    std::cerr << "The synthetic 3D center did not retain its fallback color" << std::endl;
    return EXIT_FAILURE;
  }

  vtkNew<vtkSlicerCurveRepresentation2DTestHelper> curveRepresentation2D;
  ConfigureRepresentation(curveRepresentation2D, renderer, sliceNode, closedCurveDisplayNode);
  vtkPolyData* selectedCurvePoints2D = curveRepresentation2D->GetControlPointsPolyData(vtkSlicerMarkupsWidgetRepresentation::Selected);
  vtkPolyData* selectedCurveGlyph2D = curveRepresentation2D->GetGlyphOutput(vtkSlicerMarkupsWidgetRepresentation::Selected);
  vtkIdType numberOfFallbackGlyphPoints = CountColor(selectedCurveGlyph2D, { 255, 128, 128, 255 });
  const vtkIdType numberOfCurveGlyphPoints = selectedCurveGlyph2D->GetNumberOfPoints();
  if (selectedCurvePoints2D->GetNumberOfPoints() != 4 || numberOfCurveGlyphPoints % selectedCurvePoints2D->GetNumberOfPoints() != 0
      || numberOfFallbackGlyphPoints != numberOfCurveGlyphPoints / selectedCurvePoints2D->GetNumberOfPoints())
  {
    std::cerr << "The synthetic 2D center did not retain its fallback color" << std::endl;
    return EXIT_FAILURE;
  }

  closedCurveDisplayNode->SetActiveComponent(vtkMRMLMarkupsDisplayNode::ComponentCenterPoint, 0);
  curveRepresentation->UpdateFromMRML(closedCurveDisplayNode, vtkCommand::ModifiedEvent);
  curveRepresentation2D->UpdateFromMRML(closedCurveDisplayNode, vtkCommand::ModifiedEvent);
  vtkPolyData* activeCurvePoints = curveRepresentation->GetControlPointsPolyData(vtkSlicerMarkupsWidgetRepresentation::Active);
  vtkPolyData* activeCurvePoints2D = curveRepresentation2D->GetControlPointsPolyData(vtkSlicerMarkupsWidgetRepresentation::Active);
  if (activeCurvePoints->GetNumberOfPoints() != 1 || activeCurvePoints2D->GetNumberOfPoints() != 1)
  {
    std::cerr << "The active synthetic center was not isolated in the active pipeline" << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
