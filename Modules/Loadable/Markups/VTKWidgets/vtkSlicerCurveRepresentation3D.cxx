/*=========================================================================

 Copyright (c) ProxSim ltd., Kwun Tong, Hong Kong. All Rights Reserved.

 See COPYRIGHT.txt
 or http://www.slicer.org/copyright/copyright.txt for details.

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 This file was originally developed by Davide Punzo, punzodavide@hotmail.it,
 and development was supported by ProxSim ltd.

=========================================================================*/

// VTK includes
#include "vtkActor2D.h"
#include "vtkCellLocator.h"
#include "vtkCleanPolyData.h"
#include "vtkGlyph3DMapper.h"
#include "vtkLookupTable.h"
#include "vtkMath.h"
#include "vtkPolyDataMapper.h"
#include "vtkPointData.h"
#include "vtkProperty.h"
#include "vtkRenderer.h"
#include "vtkSlicerCurveRepresentation3D.h"
#include <vtkTextActor.h>
#include <vtkTextProperty.h>
#include "vtkTubeFilter.h"
#include "vtkUnsignedCharArray.h"

// MRML includes
#include "vtkMRMLColorNode.h"
#include "vtkMRMLFolderDisplayNode.h"
#include "vtkMRMLInteractionEventData.h"
#include "vtkMRMLMarkupsCurveNode.h"
#include "vtkMRMLMarkupsDisplayNode.h"

vtkStandardNewMacro(vtkSlicerCurveRepresentation3D);

//----------------------------------------------------------------------
vtkSlicerCurveRepresentation3D::vtkSlicerCurveRepresentation3D()
{
  this->Line = vtkSmartPointer<vtkPolyData>::New();
  this->TubeFilter = vtkSmartPointer<vtkTubeFilter>::New();
  this->TubeFilter->SetInputData(this->Line);
  this->TubeFilter->SetNumberOfSides(20);
  this->TubeFilter->SetRadius(1);

  // Mappers
  this->LineMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->LineMapper->SetInputConnection(this->TubeFilter->GetOutputPort());

  this->LineOccludedMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
  this->LineOccludedMapper->SetInputConnection(this->TubeFilter->GetOutputPort());

  // Actors
  this->LineActor = vtkSmartPointer<vtkActor>::New();
  this->LineActor->SetMapper(this->LineMapper);
  this->LineActor->SetProperty(this->GetControlPointsPipeline(Unselected)->Property);

  this->LineOccludedActor = vtkSmartPointer<vtkActor>::New();
  this->LineOccludedActor->SetMapper(this->LineOccludedMapper);
  this->LineOccludedActor->SetProperty(this->GetControlPointsPipeline(Unselected)->OccludedProperty);

  this->CurvePointLocator = vtkSmartPointer<vtkCellLocator>::New();

  this->HideTextActorIfAllPointsOccluded = true;
}

//----------------------------------------------------------------------
vtkSlicerCurveRepresentation3D::~vtkSlicerCurveRepresentation3D() = default;

//----------------------------------------------------------------------
void vtkSlicerCurveRepresentation3D::UpdateFromMRMLInternal(vtkMRMLNode* caller, unsigned long event, void* callData /*=nullptr*/)
{
  Superclass::UpdateFromMRMLInternal(caller, event, callData);

  this->NeedToRenderOn();

  vtkMRMLMarkupsCurveNode* markupsNode = vtkMRMLMarkupsCurveNode::SafeDownCast(this->GetMarkupsNode());
  if (!markupsNode || !this->IsDisplayable())
  {
    this->VisibilityOff();
    return;
  }

  vtkPolyData* curveWorld = markupsNode->GetCurveWorld();
  if (!curveWorld)
  {
    this->VisibilityOff();
    return;
  }

  this->VisibilityOn();

  // Direction markers update
  bool directionMarkersWasModified = false;
  if (this->MarkupsDisplayNode->GetLineDirectionVisibility() && this->MarkupsDisplayNode->GetLineDirectionVisibility3D() && curveWorld && curveWorld->GetNumberOfPoints() > 1)
  {
    // Physical line diameter: absolute (mm) or relative to line thickness.
    double lineDiameter =
      (this->MarkupsDisplayNode->GetCurveLineSizeMode() == vtkMRMLMarkupsDisplayNode::UseLineDiameter ? this->MarkupsDisplayNode->GetLineDiameter()
                                                                                                      : this->ControlPointSize * this->MarkupsDisplayNode->GetLineThickness());
    double markerSizeWorld = vtkMRMLMarkupsDisplayNode::LineDirectionMarkerBaseScaleFactor * this->MarkupsDisplayNode->GetLineDirectionMarkerScale() * lineDiameter;
    double markerSpacingWorld = this->MarkupsDisplayNode->GetLineDirectionMarkerSpacingScale() * markerSizeWorld;
    this->LineDirectionArrowPipeline->Mapper->SetScaleFactor(markerSizeWorld);

    // Only rebuild marker positions when spacing or curve geometry actually changed.
    // UpdateFromMRMLInternal is called on mouse hover, selection changes, etc.
    vtkMTimeType curveMTime = curveWorld->GetMTime();
    bool lineDirectionFirstToLastControlPoint = this->MarkupsDisplayNode->GetLineDirectionFirstToLastControlPoint();
    if (markerSpacingWorld != this->LineDirectionMarkerLastSpacing || curveMTime != this->LineDirectionMarkerLastGeometryMTime
        || lineDirectionFirstToLastControlPoint != this->LineDirectionFirstToLastControlPoint)
    {
      vtkMRMLMarkupsNode::BuildLineDirectionMarkers(curveWorld->GetPoints(),
                                                    this->CurveClosed,
                                                    markerSpacingWorld,
                                                    this->LineDirectionArrowPipeline->Points,
                                                    this->LineDirectionArrowPipeline->Normals,
                                                    lineDirectionFirstToLastControlPoint);
      directionMarkersWasModified = true;
      this->LineDirectionArrowPipeline->PointsPoly->Modified();
      this->LineDirectionMarkerLastSpacing = markerSpacingWorld;
      this->LineDirectionMarkerLastGeometryMTime = curveMTime;
      this->LineDirectionFirstToLastControlPoint = lineDirectionFirstToLastControlPoint;
    }
    this->LineDirectionArrowPipeline->Actor->SetVisibility(true);
  }
  else
  {
    this->LineDirectionArrowPipeline->Actor->SetVisibility(false);
  }

  // Properties label display
  // Display if there is at least one control point (even if preview)
  if (this->MarkupsDisplayNode->GetPropertiesLabelVisibility()   //
      && markupsNode->GetNumberOfDefinedControlPoints(true) > 0) // including preview
  {
    int controlPointIndex = 0;
    int numberOfDefinedControlPoints = markupsNode->GetNumberOfDefinedControlPoints(); // excluding previewed point
    if (numberOfDefinedControlPoints > 0)
    {
      // there is at least one placed point
      controlPointIndex = markupsNode->GetNthControlPointIndexByPositionStatus((numberOfDefinedControlPoints - 1) / 2, vtkMRMLMarkupsNode::PositionDefined);
    }
    else
    {
      // we only have a preview point
      controlPointIndex = markupsNode->GetNthControlPointIndexByPositionStatus(0, vtkMRMLMarkupsNode::PositionPreview);
    }
    // It would be better to show the properties label near a visible segment of the curve
    // but then we may need to iterate through many points whenever the camera rotates, so it may not worth the trouble.
    // Especially because labels could be still poorly positioned. Probably some more sophisticated auto-placement
    // with option for manual label position adjustment (and maybe anchor position adjustment) would be the best.
    markupsNode->GetNthControlPointPositionWorld(controlPointIndex, this->TextActorPositionWorld);
    this->TextActor->SetVisibility(true);
  }
  else
  {
    this->TextActor->SetVisibility(false);
  }

  // Line display

  for (int controlPointType = 0; controlPointType < NumberOfControlPointTypes; ++controlPointType)
  {
    ControlPointsPipeline3D* controlPoints = this->GetControlPointsPipeline(controlPointType);
    if (controlPointType == Project || controlPointType == ProjectBack)
    {
      // no projection display in 3D
      controlPoints->Actor->SetVisibility(false);
      controlPoints->OccludedActor->SetVisibility(false);
      controlPoints->LabelsActor->SetVisibility(false);
      controlPoints->LabelsOccludedActor->SetVisibility(false);
      continue;
    }

    // For backward compatibility, we hide labels if text scale is set to 0.
    controlPoints->LabelsActor->SetVisibility(this->MarkupsDisplayNode->GetPointLabelsVisibility() //
                                              && this->MarkupsDisplayNode->GetTextScale() > 0.0);
    controlPoints->GlyphMapper->SetScaleFactor(this->ControlPointSize);

    this->UpdateRelativeCoincidentTopologyOffsets(controlPoints->GlyphMapper, controlPoints->OccludedGlyphMapper);
  }

  this->UpdateRelativeCoincidentTopologyOffsets(this->LineMapper, this->LineOccludedMapper);

  double diameter =
    (this->MarkupsDisplayNode->GetCurveLineSizeMode() == vtkMRMLMarkupsDisplayNode::UseLineDiameter ? this->MarkupsDisplayNode->GetLineDiameter()
                                                                                                    : this->ControlPointSize * this->MarkupsDisplayNode->GetLineThickness());
  this->TubeFilter->SetRadius(diameter * 0.5);

  this->LineActor->SetVisibility(markupsNode->GetNumberOfControlPoints() >= 2);

  bool allControlPointsSelected = this->GetAllControlPointsSelected();
  int controlPointType = Active;
  if (this->MarkupsDisplayNode->GetActiveComponentType() != vtkMRMLMarkupsDisplayNode::ComponentLine)
  {
    controlPointType = allControlPointsSelected ? Selected : Unselected;
  }
  vtkProperty* pipelineProperty = this->GetControlPointsPipeline(controlPointType)->Property;
  this->LineActor->SetProperty(pipelineProperty);
  if (this->LineDirectionArrowPipeline->Actor->GetVisibility())
  {
    this->LineDirectionArrowPipeline->Actor->SetProperty(pipelineProperty);
    if (!directionMarkersWasModified)
    {
      this->LineDirectionArrowPipeline->PointsPoly->Modified();
    }
  }

  this->TextActor->SetTextProperty(this->GetControlPointsPipeline(controlPointType)->TextProperty);

  this->LineOccludedActor->SetProperty(this->GetControlPointsPipeline(controlPointType)->OccludedProperty);
  this->LineOccludedActor->SetVisibility(this->MarkupsDisplayNode            //
                                         && this->LineActor->GetVisibility() //
                                         && this->MarkupsDisplayNode->GetOccludedVisibility());

  bool allNodesHidden = true;
  for (int controlPointIndex = 0; controlPointIndex < markupsNode->GetNumberOfControlPoints(); controlPointIndex++)
  {
    if (markupsNode->GetNthControlPointPositionVisibility(controlPointIndex) //
        && (markupsNode->GetNthControlPointVisibility(controlPointIndex)))
    {
      allNodesHidden = false;
      break;
    }
  }

  if (this->CurveClosed && markupsNode->GetNumberOfControlPoints() > 2 && !allNodesHidden)
  {
    double centerPosWorld[3], orient[3] = { 0 };
    markupsNode->GetCenterOfRotationWorld(centerPosWorld);
    int centerControlPointType = allControlPointsSelected ? Selected : Unselected;
    if (this->MarkupsDisplayNode->GetActiveComponentType() == vtkMRMLMarkupsDisplayNode::ComponentCenterPoint)
    {
      centerControlPointType = Active;
      this->GetControlPointsPipeline(centerControlPointType)->ControlPoints->SetNumberOfPoints(0);
      this->GetControlPointsPipeline(centerControlPointType)->ControlPointsPolyData->GetPointData()->GetNormals()->SetNumberOfTuples(0);
    }
    this->GetControlPointsPipeline(centerControlPointType)->ControlPoints->InsertNextPoint(centerPosWorld);
    this->GetControlPointsPipeline(centerControlPointType)->ControlPointsPolyData->GetPointData()->GetNormals()->InsertNextTuple(orient);

    this->GetControlPointsPipeline(centerControlPointType)->ControlPoints->Modified();
    this->GetControlPointsPipeline(centerControlPointType)->ControlPointsPolyData->GetPointData()->GetNormals()->Modified();
    this->GetControlPointsPipeline(centerControlPointType)->ControlPointsPolyData->Modified();
    if (centerControlPointType == Active)
    {
      this->GetControlPointsPipeline(centerControlPointType)->Actor->VisibilityOn();
      this->GetControlPointsPipeline(centerControlPointType)->LabelsActor->VisibilityOff();
    }
  }

  // Per-control-point line gradient: when UseControlPointColors is on and no
  // curve-side scalar coloring is active, propagate the per-point colors onto
  // the curve world polydata as a "ControlPointColors" RGBA point scalar
  // array, interpolating between adjacent control points so the line gradients
  // along each segment.
  bool folderOverrideActiveCurve = false;
  if (this->MarkupsDisplayNode->GetFolderDisplayOverrideAllowed())
  {
    vtkMRMLDisplayableNode* displayableNode = this->MarkupsDisplayNode->GetDisplayableNode();
    folderOverrideActiveCurve = (vtkMRMLFolderDisplayNode::GetOverridingHierarchyDisplayNode(displayableNode) != nullptr);
  }
  const bool useCpColorsForLine = this->MarkupsDisplayNode->GetUseControlPointColors() //
                                  && !folderOverrideActiveCurve                          //
                                  && !this->MarkupsDisplayNode->GetScalarVisibility();

  if (useCpColorsForLine && curveWorld && curveWorld->GetNumberOfPoints() > 0)
  {
    int nCp = markupsNode->GetNumberOfControlPoints();
    bool anyOverride = false;
    for (int i = 0; i < nCp && !anyOverride; ++i)
    {
      if (markupsNode->IsNthControlPointColorOverridden(i))
      {
        anyOverride = true;
      }
    }
    if (anyOverride)
    {
      double fallbackColor[3] = { 1.0, 1.0, 1.0 };
      double* widgetColor = this->GetWidgetColor(Unselected);
      fallbackColor[0] = widgetColor[0];
      fallbackColor[1] = widgetColor[1];
      fallbackColor[2] = widgetColor[2];
      vtkIdType nCurve = curveWorld->GetNumberOfPoints();
      vtkSmartPointer<vtkUnsignedCharArray> arr =
        vtkUnsignedCharArray::SafeDownCast(curveWorld->GetPointData()->GetArray("ControlPointColors"));
      if (!arr || arr->GetNumberOfComponents() != 4)
      {
        arr = vtkSmartPointer<vtkUnsignedCharArray>::New();
        arr->SetName("ControlPointColors");
        arr->SetNumberOfComponents(4);
        curveWorld->GetPointData()->RemoveArray("ControlPointColors");
        curveWorld->GetPointData()->AddArray(arr);
      }
      arr->SetNumberOfTuples(nCurve);
      auto cpColor = [&](int cpIdx, double rgba[4])
      {
        if (cpIdx < 0 || cpIdx >= nCp || !markupsNode->IsNthControlPointColorOverridden(cpIdx))
        {
          rgba[0] = fallbackColor[0];
          rgba[1] = fallbackColor[1];
          rgba[2] = fallbackColor[2];
          rgba[3] = 1.0;
          return;
        }
        markupsNode->GetNthControlPointColor(cpIdx, rgba);
      };
      for (vtkIdType i = 0; i < nCurve; ++i)
      {
        int prevCp = markupsNode->GetControlPointIndexFromInterpolatedPointIndex(i);
        int nextCp = prevCp + 1;
        if (prevCp < 0)
        {
          prevCp = 0;
        }
        if (prevCp >= nCp)
        {
          prevCp = nCp - 1;
        }
        if (nextCp >= nCp)
        {
          nextCp = prevCp;
        }
        double prevRgba[4], nextRgba[4];
        cpColor(prevCp, prevRgba);
        cpColor(nextCp, nextRgba);
        double t = 0.0;
        if (prevCp != nextCp)
        {
          double prevPos[3], nextPos[3], curvePos[3];
          markupsNode->GetNthControlPointPositionWorld(prevCp, prevPos);
          markupsNode->GetNthControlPointPositionWorld(nextCp, nextPos);
          curveWorld->GetPoint(i, curvePos);
          double d1 = sqrt(vtkMath::Distance2BetweenPoints(prevPos, curvePos));
          double d2 = sqrt(vtkMath::Distance2BetweenPoints(curvePos, nextPos));
          double d = d1 + d2;
          t = (d > 1e-9) ? d1 / d : 0.0;
        }
        unsigned char rgba[4] = {
          static_cast<unsigned char>((prevRgba[0] * (1.0 - t) + nextRgba[0] * t) * 255.0 + 0.5),
          static_cast<unsigned char>((prevRgba[1] * (1.0 - t) + nextRgba[1] * t) * 255.0 + 0.5),
          static_cast<unsigned char>((prevRgba[2] * (1.0 - t) + nextRgba[2] * t) * 255.0 + 0.5),
          static_cast<unsigned char>((prevRgba[3] * (1.0 - t) + nextRgba[3] * t) * 255.0 + 0.5)
        };
        arr->SetTypedTuple(i, rgba);
      }
      arr->Modified();
      curveWorld->GetPointData()->SetActiveScalars("ControlPointColors");
      this->LineMapper->SetScalarVisibility(true);
      this->LineMapper->SetScalarModeToUsePointData();
      this->LineMapper->SetColorModeToDirectScalars();
      this->LineMapper->UseLookupTableScalarRangeOn();
      this->LineMapper->SetLookupTable(nullptr);
    }
    else
    {
      curveWorld->GetPointData()->RemoveArray("ControlPointColors");
      this->LineMapper->SetScalarVisibility(false);
    }
  }
  else
  {
    curveWorld->GetPointData()->RemoveArray("ControlPointColors");
  }

  // Scalars
  if (!useCpColorsForLine)
  {
    this->LineMapper->SetScalarVisibility(this->MarkupsDisplayNode->GetScalarVisibility());
  }
  // if the scalars are visible, set active scalars, the lookup table and the scalar range
  if (this->MarkupsDisplayNode->GetScalarVisibility())
  {
    // Set active display property so that it can be distinguished, given that color cannot be used for this when scalars are visible
    vtkProperty* activePipelineProperty = reinterpret_cast<vtkSlicerMarkupsWidgetRepresentation3D::ControlPointsPipeline3D*>(this->ControlPoints[Active])->Property;
    this->PreviousSpecularLightingCoeff = activePipelineProperty->GetSpecular();
    activePipelineProperty->SetSpecular(1.0);

    if (this->LineActor->GetVisibility())
    {
      this->LineMapper->SetScalarModeToUsePointData();

      if (this->MarkupsDisplayNode->GetScalarRangeFlag() == vtkMRMLDisplayNode::UseDirectMapping)
      {
        this->LineMapper->UseLookupTableScalarRangeOn(); // avoid warning about bad table range
        this->LineMapper->SetColorModeToDirectScalars();
        this->LineMapper->SetLookupTable(nullptr);
      }
      else
      {
        this->LineMapper->UseLookupTableScalarRangeOff();
        this->LineMapper->SetColorModeToMapScalars();

        // The renderer uses the lookup table scalar range to render colors. By default, UseLookupTableScalarRange
        // is set to false and SetScalarRange can be used on the mapper to map scalars into the lookup table. When set
        // to true, SetScalarRange has no effect and it is necessary to force the scalarRange on the lookup table manually.
        // Whichever way is used, the look up table range needs to be changed to render the correct scalar values, thus
        // one lookup table can not be shared by multiple mappers if any of those mappers needs to map using its scalar
        // values range. It is therefore necessary to make a copy of the colorNode vtkLookupTable in order not to impact
        // that lookup table original range.
        vtkSmartPointer<vtkLookupTable> dNodeLUT =
          vtkSmartPointer<vtkLookupTable>::Take(this->MarkupsDisplayNode->GetColorNode() ? this->MarkupsDisplayNode->GetColorNode()->CreateLookupTableCopy() : nullptr);
        this->LineMapper->SetLookupTable(dNodeLUT);
      }

      // Set scalar range
      this->LineMapper->SetScalarRange(this->MarkupsDisplayNode->GetScalarRange());
    }
  }
  else
  {
    vtkProperty* activePipelineProperty = reinterpret_cast<vtkSlicerMarkupsWidgetRepresentation3D::ControlPointsPipeline3D*>(this->ControlPoints[Active])->Property;
    activePipelineProperty->SetSpecular(this->PreviousSpecularLightingCoeff);
  }
}

//----------------------------------------------------------------------
void vtkSlicerCurveRepresentation3D::GetActors(vtkPropCollection* pc)
{
  this->Superclass::GetActors(pc);
  this->LineActor->GetActors(pc);
  this->LineOccludedActor->GetActors(pc);
}

//----------------------------------------------------------------------
void vtkSlicerCurveRepresentation3D::ReleaseGraphicsResources(vtkWindow* win)
{
  this->Superclass::ReleaseGraphicsResources(win);
  this->LineActor->ReleaseGraphicsResources(win);
  this->LineOccludedActor->ReleaseGraphicsResources(win);
}

//----------------------------------------------------------------------
int vtkSlicerCurveRepresentation3D::RenderOverlay(vtkViewport* viewport)
{
  int count = this->Superclass::RenderOverlay(viewport);
  if (this->LineActor->GetVisibility())
  {
    count += this->LineActor->RenderOverlay(viewport);
  }
  if (this->LineOccludedActor->GetVisibility())
  {
    count += this->LineOccludedActor->RenderOverlay(viewport);
  }
  return count;
}

//-----------------------------------------------------------------------------
int vtkSlicerCurveRepresentation3D::RenderOpaqueGeometry(vtkViewport* viewport)
{
  int count = this->Superclass::RenderOpaqueGeometry(viewport);
  if (this->LineActor->GetVisibility())
  {
    double diameter =
      (this->MarkupsDisplayNode->GetCurveLineSizeMode() == vtkMRMLMarkupsDisplayNode::UseLineDiameter ? this->MarkupsDisplayNode->GetLineDiameter()
                                                                                                      : this->ControlPointSize * this->MarkupsDisplayNode->GetLineThickness());
    this->TubeFilter->SetRadius(diameter * 0.5);
    count += this->LineActor->RenderOpaqueGeometry(viewport);
  }
  if (this->LineOccludedActor->GetVisibility())
  {
    count += this->LineOccludedActor->RenderOpaqueGeometry(viewport);
  }
  return count;
}

//-----------------------------------------------------------------------------
int vtkSlicerCurveRepresentation3D::RenderTranslucentPolygonalGeometry(vtkViewport* viewport)
{
  int count = this->Superclass::RenderTranslucentPolygonalGeometry(viewport);
  if (this->LineActor->GetVisibility())
  {
    // The internal actor needs to share property keys.
    // This ensures the mapper state is consistent and allows depth peeling to work as expected.
    this->LineActor->SetPropertyKeys(this->GetPropertyKeys());
    count += this->LineActor->RenderTranslucentPolygonalGeometry(viewport);
  }
  if (this->LineOccludedActor->GetVisibility())
  {
    // The internal actor needs to share property keys.
    // This ensures the mapper state is consistent and allows depth peeling to work as expected.
    this->LineOccludedActor->SetPropertyKeys(this->GetPropertyKeys());
    count += this->LineOccludedActor->RenderTranslucentPolygonalGeometry(viewport);
  }
  return count;
}

//-----------------------------------------------------------------------------
vtkTypeBool vtkSlicerCurveRepresentation3D::HasTranslucentPolygonalGeometry()
{
  if (this->Superclass::HasTranslucentPolygonalGeometry())
  {
    return true;
  }
  if (this->LineActor->GetVisibility() && this->LineActor->HasTranslucentPolygonalGeometry())
  {
    return true;
  }
  if (this->LineOccludedActor->GetVisibility() && this->LineOccludedActor->HasTranslucentPolygonalGeometry())
  {
    return true;
  }
  return false;
}

//----------------------------------------------------------------------
double* vtkSlicerCurveRepresentation3D::GetBounds()
{
  vtkBoundingBox boundingBox;
  const std::vector<vtkProp*> actors({ this->LineActor });
  this->AddActorsBounds(boundingBox, actors, Superclass::GetBounds());
  boundingBox.GetBounds(this->Bounds);
  return this->Bounds;
}

//----------------------------------------------------------------------
void vtkSlicerCurveRepresentation3D::CanInteract(vtkMRMLInteractionEventData* interactionEventData, int& foundComponentType, int& foundComponentIndex, double& closestDistance2)
{
  foundComponentType = vtkMRMLMarkupsDisplayNode::ComponentNone;
  vtkMRMLMarkupsNode* markupsNode = this->GetMarkupsNode();
  if (!markupsNode || markupsNode->GetLocked() || markupsNode->GetNumberOfControlPoints() < 1 //
      || !interactionEventData)
  {
    return;
  }
  Superclass::CanInteract(interactionEventData, foundComponentType, foundComponentIndex, closestDistance2);
  if (foundComponentType != vtkMRMLMarkupsDisplayNode::ComponentNone)
  {
    return;
  }

  this->CanInteractWithCurve(interactionEventData, foundComponentType, foundComponentIndex, closestDistance2);
}

//-----------------------------------------------------------------------------
void vtkSlicerCurveRepresentation3D::PrintSelf(ostream& os, vtkIndent indent)
{
  // Superclass typedef defined in vtkTypeMacro() found in vtkSetGet.h
  this->Superclass::PrintSelf(os, indent);

  if (this->LineActor)
  {
    os << indent << "Line Visibility: " << this->LineActor->GetVisibility() << "\n";
  }
  else
  {
    os << indent << "Line Visibility: (none)\n";
  }
}

//-----------------------------------------------------------------------------
void vtkSlicerCurveRepresentation3D::SetMarkupsNode(vtkMRMLMarkupsNode* markupsNode)
{
  if (this->MarkupsNode != markupsNode)
  {
    if (markupsNode)
    {
      vtkNew<vtkCleanPolyData> cleaner;
      cleaner->PointMergingOn();
      cleaner->SetInputConnection(markupsNode->GetCurveWorldConnection());
      this->TubeFilter->SetInputConnection(cleaner->GetOutputPort());
    }
    else
    {
      this->TubeFilter->SetInputData(this->Line);
    }
  }
  this->Superclass::SetMarkupsNode(markupsNode);
}

//----------------------------------------------------------------------
void vtkSlicerCurveRepresentation3D::CanInteractWithCurve(vtkMRMLInteractionEventData* interactionEventData, int& foundComponentType, int& componentIndex, double& closestDistance2)
{
  if (!this->MarkupsNode || this->MarkupsNode->GetLocked() //
      || this->MarkupsNode->GetNumberOfControlPoints() < 2 //
      || !this->GetVisibility() || !interactionEventData)
  {
    return;
  }

  vtkPolyData* curveWorld = this->MarkupsNode->GetCurveWorld();
  if (!curveWorld || curveWorld->GetNumberOfCells() < 1)
  {
    return;
  }

  this->CurvePointLocator->SetDataSet(curveWorld);
  this->CurvePointLocator->Update();

  double closestPointDisplay[3] = { 0.0 };
  vtkIdType cellId = -1;
  int subId = -1;
  // dist2 is initialized to -1.0 because this is how FindClosestPoint indicates that no closest point is found
  double dist2 = -1.0;
  if (interactionEventData->IsWorldPositionValid())
  {
    const double* worldPosition = interactionEventData->GetWorldPosition();
    this->CurvePointLocator->FindClosestPoint(worldPosition, closestPointDisplay, cellId, subId, dist2);
  }

  if (dist2 >= 0 && dist2 < this->ControlPointSize + this->PickingTolerance * this->GetScreenScaleFactor() * this->ViewScaleFactorMmPerPixel)
  {
    closestDistance2 = dist2 / this->ViewScaleFactorMmPerPixel / this->ViewScaleFactorMmPerPixel;
    foundComponentType = vtkMRMLMarkupsDisplayNode::ComponentLine;
    componentIndex = this->MarkupsNode->GetControlPointIndexFromInterpolatedPointIndex(subId);
  }
}
