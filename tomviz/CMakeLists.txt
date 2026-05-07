find_package(OpenGL REQUIRED)

include_directories(SYSTEM
  ${Qt6Widgets_INCLUDE_DIRS}
  ${Qt6Concurrent_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/thirdparty/pybind11/include)

add_subdirectory(core)
add_subdirectory(pvextensions)

set(SOURCES
  AbstractDataModel.h
  AbstractDataModel.cxx
  ActiveObjects.cxx
  ActiveObjects.h
  AddAlignReaction.cxx
  AddAlignReaction.h
  AddExpressionReaction.cxx
  AddExpressionReaction.h
  AddPythonSourceReaction.cxx
  AddPythonSourceReaction.h
  AddPythonTransformReaction.cxx
  AddRenderViewContextMenuBehavior.cxx
  AddRenderViewContextMenuBehavior.h
  AddResampleReaction.cxx
  AddResampleReaction.h
  AlignWidget.cxx
  AlignWidget.h
  AnimationHelperDialog.cxx
  AnimationHelperDialog.h
  ArrayWranglerReaction.cxx
  ArrayWranglerReaction.h
  AxesReaction.cxx
  AxesReaction.h
  Behaviors.cxx
  Behaviors.h
  BrightnessContrastWidget.cxx
  BrightnessContrastWidget.h
  CameraReaction.cxx
  CameraReaction.h
  CentralWidget.cxx
  CentralWidget.h
  CloneDataReaction.cxx
  CloneDataReaction.h
  ColorMap.cxx
  ColorMap.h
  ColorMapSettingsWidget.cxx
  ColorMapSettingsWidget.h
  ComboTextEditor.cxx
  ComboTextEditor.h
  ConformVolumeDialog.cxx
  ConformVolumeDialog.h
  ConformVolumeReaction.cxx
  ConformVolumeReaction.h
  ConvertToFloatReaction.cxx
  ConvertToFloatReaction.h
  CropReaction.cxx
  CropReaction.h
  SelectCylinderWidget.cxx
  SelectCylinderWidget.h
  SelectVolumeWidget.cxx
  SelectVolumeWidget.h
  DataBroker.cxx
  DataBroker.h
  DataBrokerLoadDialog.cxx
  DataBrokerLoadDialog.h
  DataBrokerLoadReaction.cxx
  DataBrokerLoadReaction.h
  DataBrokerSaveDialog.cxx
  DataBrokerSaveDialog.h
  DataBrokerSaveReaction.cxx
  DataBrokerSaveReaction.h
  DataExchangeFormat.cxx
  DataExchangeFormat.h
  DataPropertiesModel.cxx
  DataPropertiesModel.h
  legacy/DataSource.cxx
  legacy/DataSource.h
  DataTransformMenu.cxx
  DataTransformMenu.h
  DeleteDataReaction.cxx
  DeleteDataReaction.h
  legacy/DockerExecutor.cxx
  legacy/DockerExecutor.h
  legacy/DockerUtilities.cxx
  legacy/DockerUtilities.h
  DoubleSliderWidget.cxx
  DoubleSliderWidget.h
  DoubleSpinBox.cxx
  DoubleSpinBox.h
  DuplicateModuleReaction.h
  DuplicateModuleReaction.cxx
  EmdFormat.cxx
  EmdFormat.h
  ExportDataReaction.cxx
  ExportDataReaction.h
  legacy/ExternalPythonExecutor.cxx
  legacy/ExternalPythonExecutor.h
  FileFormatManager.cxx
  FileFormatManager.h
  FxiFormat.cxx
  FxiFormat.h
  ShiftRotationCenterWidget.cxx
  ShiftRotationCenterWidget.h
  GenericHDF5Format.cxx
  GenericHDF5Format.h
  GradientOpacityWidget.h
  GradientOpacityWidget.cxx
  Hdf5SubsampleWidget.h
  Hdf5SubsampleWidget.cxx
  HistogramManager.h
  HistogramManager.cxx
  HistogramWidget.h
  HistogramWidget.cxx
  Histogram2DWidget.h
  Histogram2DWidget.cxx
  ImageStackDialog.h
  ImageStackDialog.cxx
  ImageStackModel.h
  ImageStackModel.cxx
  InterfaceBuilder.h
  InterfaceBuilder.cxx
  InternalPythonHelper.h
  InternalPythonHelper.cxx
  IntSliderWidget.cxx
  IntSliderWidget.h
  ListEditorWidget.cxx
  ListEditorWidget.h
  LoadDataReaction.cxx
  LoadDataReaction.h
  LoadPaletteReaction.cxx
  LoadPaletteReaction.h
  LoadStackReaction.cxx
  LoadStackReaction.h
  LoadTimeSeriesReaction.cxx
  LoadTimeSeriesReaction.h
  Logger.cxx
  Logger.h
  ManualManipulationWidget.cxx
  ManualManipulationWidget.h
  MergeImagesDialog.cxx
  MergeImagesDialog.h
  MergeImagesReaction.cxx
  MergeImagesReaction.h
  MoleculeSource.cxx
  MoleculeSource.h
  OperatorSearchDialog.cxx
  OperatorSearchDialog.h
  MoleculeProperties.cxx
  MoleculeProperties.h
  MoleculePropertiesPanel.cxx
  MoleculePropertiesPanel.h
  InteractiveTransformWidget.cxx
  InteractiveTransformWidget.h
  legacy/Pipeline.cxx
  legacy/Pipeline.h
  legacy/PipelineExecutor.cxx
  legacy/PipelineExecutor.h
  legacy/PipelineManager.cxx
  legacy/PipelineManager.h
  legacy/PipelineModel.cxx
  legacy/PipelineModel.h
  legacy/PipelineProxy.cxx
  legacy/PipelineProxy.h
  legacy/PipelineWorker.cxx
  legacy/PipelineWorker.h
  PresetDialog.cxx
  PresetDialog.h
  PresetModel.cxx
  PresetModel.h
  ProgressDialog.cxx
  ProgressDialog.h
  ProgressDialogManager.cxx
  ProgressDialogManager.h
  PtychoDialog.cxx
  PtychoDialog.h
  PtychoRunner.cxx
  PtychoRunner.h
  legacy/PythonGeneratedDatasetReaction.cxx
  legacy/PythonGeneratedDatasetReaction.h
  PythonReader.cxx
  PythonReader.h
  PythonUtilities.cxx
  PythonUtilities.h
  PythonWriter.cxx
  PythonWriter.h
  PyXRFMakeHDF5Dialog.cxx
  PyXRFMakeHDF5Dialog.h
  PyXRFProcessDialog.cxx
  PyXRFProcessDialog.h
  PyXRFRunner.cxx
  PyXRFRunner.h
  pybind11/PythonTypeConversions.cxx
  pybind11/PythonTypeConversions.h
  QVTKGLWidget.cxx
  QVTKGLWidget.h
  RAWFileReaderDialog.h
  RAWFileReaderDialog.cxx
  Reaction.cxx
  Reaction.h
  RecentFilesMenu.cxx
  RecentFilesMenu.h
  ReconstructionReaction.cxx
  ReconstructionReaction.h
  ReconstructionWidget.h
  ReconstructionWidget.cxx
  ResetReaction.cxx
  ResetReaction.h
  RotateAlignWidget.cxx
  RotateAlignWidget.h
  SaveDataReaction.cxx
  SaveLoadStateReaction.cxx
  SaveLoadStateReaction.h
  SaveLoadTemplateReaction.cxx
  SaveLoadTemplateReaction.h
  SaveScreenshotDialog.cxx
  SaveScreenshotDialog.h
  SaveScreenshotReaction.cxx
  SaveScreenshotReaction.h
  SaveWebReaction.cxx
  ScaleActorBehavior.cxx
  ScaleActorBehavior.h
  SelectItemsDialog.cxx
  SelectItemsDialog.h
  SetDataTypeReaction.h
  SetDataTypeReaction.cxx
  SetTiltAnglesReaction.cxx
  SetTiltAnglesReaction.h
  SliceViewDialog.cxx
  SliceViewDialog.h
  SpinBox.cxx
  SpinBox.h
  legacy/ThreadedExecutor.cxx
  legacy/ThreadedExecutor.h
  TimeSeriesLabel.h
  TimeSeriesLabel.cxx
  TimeSeriesStep.h
  TomographyReconstruction.h
  TomographyReconstruction.cxx
  TomographyTiltSeries.h
  TomographyTiltSeries.cxx
  TransformUtils.h
  TransformUtils.cxx
  TransposeDataReaction.h
  TransposeDataReaction.cxx
  pipeline/transforms/CropTransform.h
  pipeline/transforms/CropTransform.cxx
  pipeline/transforms/ReconstructionTransform.h
  pipeline/transforms/ReconstructionTransform.cxx
  pipeline/transforms/TranslateAlignTransform.h
  pipeline/transforms/TranslateAlignTransform.cxx
  pipeline/PortData.cxx
  pipeline/PortData.h
  pipeline/PortType.h
  pipeline/NodeState.h
  pipeline/Port.cxx
  pipeline/Port.h
  pipeline/InputPort.cxx
  pipeline/InputPort.h
  pipeline/OutputPort.cxx
  pipeline/OutputPort.h
  pipeline/VolumeOutputPort.cxx
  pipeline/VolumeOutputPort.h
  pipeline/Link.cxx
  pipeline/Link.h
  pipeline/Node.cxx
  pipeline/Node.h
  pipeline/NodeFactory.cxx
  pipeline/NodeFactory.h
  pipeline/SourceNode.cxx
  pipeline/SourceNode.h
  pipeline/TransformNode.cxx
  pipeline/TransformNode.h
  pipeline/SinkNode.cxx
  pipeline/SinkNode.h
  pipeline/SinkGroupNode.cxx
  pipeline/SinkGroupNode.h
  pipeline/PassthroughOutputPort.cxx
  pipeline/PassthroughOutputPort.h
  pipeline/LegacyStateLoader.cxx
  pipeline/LegacyStateLoader.h
  pipeline/Pipeline.cxx
  pipeline/Pipeline.h
  pipeline/PipelineStateIO.cxx
  pipeline/PipelineStateIO.h
  pipeline/PipelineExecutor.cxx
  pipeline/PipelineExecutor.h
  pipeline/NodeExecutor.cxx
  pipeline/NodeExecutor.h
  pipeline/InternalNodeExecutor.cxx
  pipeline/InternalNodeExecutor.h
  pipeline/ExternalNodeExecutor.cxx
  pipeline/ExternalNodeExecutor.h
  pipeline/NodeExecutorFactory.cxx
  pipeline/NodeExecutorFactory.h
  pipeline/ProgressReader.cxx
  pipeline/ProgressReader.h
  pipeline/PythonNodeWrapper.cxx
  pipeline/PythonNodeWrapper.h
  pipeline/PythonNodeUtils.cxx
  pipeline/PythonNodeUtils.h
  pipeline/PythonNodeBackend.cxx
  pipeline/PythonNodeBackend.h
  pipeline/CustomNodeWidgetRegistry.cxx
  pipeline/CustomNodeWidgetRegistry.h
  pipeline/DefaultExecutor.cxx
  pipeline/DefaultExecutor.h
  pipeline/ExecutionFuture.cxx
  pipeline/ExecutionFuture.h
  pipeline/ThreadedExecutor.cxx
  pipeline/ThreadedExecutor.h
  pipeline/data/VolumeData.cxx
  pipeline/data/VolumeData.h
  pipeline/sources/SphereSource.cxx
  pipeline/sources/SphereSource.h
  pipeline/sources/ReaderSourceNode.cxx
  pipeline/sources/ReaderSourceNode.h
  pipeline/sources/PythonSource.cxx
  pipeline/sources/PythonSource.h
  pipeline/transforms/ThresholdTransform.cxx
  pipeline/transforms/ThresholdTransform.h
  pipeline/transforms/SetTiltAnglesTransform.cxx
  pipeline/transforms/SetTiltAnglesTransform.h
  pipeline/transforms/LegacyPythonTransform.cxx
  pipeline/transforms/LegacyPythonTransform.h
  pipeline/transforms/PythonTransform.cxx
  pipeline/transforms/PythonTransform.h
  pipeline/transforms/ConvertToFloatTransform.cxx
  pipeline/transforms/ConvertToFloatTransform.h
  pipeline/transforms/ConvertToVolumeTransform.cxx
  pipeline/transforms/ConvertToVolumeTransform.h
  pipeline/transforms/ArrayWranglerTransform.cxx
  pipeline/transforms/ArrayWranglerTransform.h
  pipeline/transforms/TransposeDataTransform.cxx
  pipeline/transforms/TransposeDataTransform.h
  pipeline/transforms/SnapshotTransform.cxx
  pipeline/transforms/SnapshotTransform.h
  pipeline/PortUtils.h
  pipeline/pybind11/PybindVTKTypeCaster.h
  pipeline/DeferredLinkInfo.h
  pipeline/sinks/VolumeStatsSink.cxx
  pipeline/sinks/VolumeStatsSink.h
  pipeline/sinks/LegacyModuleSink.cxx
  pipeline/sinks/LegacyModuleSink.h
  pipeline/sinks/VolumeSink.cxx
  pipeline/sinks/VolumeSink.h
  pipeline/sinks/SliceSink.cxx
  pipeline/sinks/SliceSink.h
  pipeline/sinks/ContourSink.cxx
  pipeline/sinks/ContourSink.h
  pipeline/sinks/ThresholdSink.cxx
  pipeline/sinks/ThresholdSink.h
  pipeline/sinks/SegmentSink.cxx
  pipeline/sinks/SegmentSink.h
  pipeline/sinks/OutlineSink.cxx
  pipeline/sinks/OutlineSink.h
  pipeline/sinks/ClipSink.cxx
  pipeline/sinks/ClipSink.h
  pipeline/sinks/RulerSink.cxx
  pipeline/sinks/RulerSink.h
  pipeline/sinks/ScaleCubeSink.cxx
  pipeline/sinks/ScaleCubeSink.h
  pipeline/sinks/PlotSink.cxx
  pipeline/sinks/PlotSink.h
  pipeline/sinks/MoleculeSink.cxx
  pipeline/sinks/MoleculeSink.h
  pipeline/sinks/VolumeSinkWidget.cxx
  pipeline/sinks/VolumeSinkWidget.h
  pipeline/sinks/ContourSinkWidget.cxx
  pipeline/sinks/ContourSinkWidget.h
  pipeline/sinks/ThresholdSinkWidget.cxx
  pipeline/sinks/ThresholdSinkWidget.h
  pipeline/sinks/SegmentSinkWidget.cxx
  pipeline/sinks/SegmentSinkWidget.h
  pipeline/ParameterBindingUtils.cxx
  pipeline/ParameterBindingUtils.h
  pipeline/ParameterInterfaceBuilder.cxx
  pipeline/ParameterInterfaceBuilder.h
  pipeline/CustomPythonNodeWidget.cxx
  pipeline/CustomPythonNodeWidget.h
  pipeline/EditNodeWidget.cxx
  pipeline/EditNodeWidget.h
  pipeline/NodePropertiesWidget.cxx
  pipeline/NodePropertiesWidget.h
  pipeline/PythonNodeEditorWidget.cxx
  pipeline/PythonNodeEditorWidget.h
  pipeline/NodePropertiesPanel.cxx
  pipeline/NodePropertiesPanel.h
  pipeline/NodeEditDialog.cxx
  pipeline/NodeEditDialog.h
  pipeline/PipelineUtils.cxx
  pipeline/PipelineUtils.h
  pipeline/PipelineControlsWidget.cxx
  pipeline/PipelineControlsWidget.h
  pipeline/PipelineStripWidget.cxx
  pipeline/PipelineStripWidget.h
  pipeline/VolumeScalarsModel.cxx
  pipeline/VolumeScalarsModel.h
  pipeline/VolumePropertiesWidget.cxx
  pipeline/VolumePropertiesWidget.h
  pipeline/vtkNonOrthoImagePlaneWidget.cxx
  pipeline/vtkNonOrthoImagePlaneWidget.h
  FileReader.cxx
  FileReader.h
  Tvh5Format.cxx
  Tvh5Format.h
  ViewsLayoutsSerializer.cxx
  ViewsLayoutsSerializer.h
  Utilities.cxx
  Utilities.h
  core/Variant.h
  ViewFrameActions.cxx
  ViewFrameActions.h
  ViewMenuManager.cxx
  ViewMenuManager.h
  vtkChartGradientOpacityEditor.cxx
  vtkChartGradientOpacityEditor.h
  vtkChartHistogram.cxx
  vtkChartHistogram.h
  vtkChartHistogramColorOpacityEditor.cxx
  vtkChartHistogramColorOpacityEditor.h
  vtkChartTransfer2DEditor.cxx
  vtkChartTransfer2DEditor.h
  vtkTransferFunctionBoxItem.h
  vtkTransferFunctionBoxItem.cxx
  vtkCustomBoxRepresentation.cxx
  vtkCustomBoxRepresentation.h
  vtkCustomPiecewiseControlPointsItem.cxx
  vtkCustomPiecewiseControlPointsItem.h
  vtkLengthScaleRepresentation.h
  vtkLengthScaleRepresentation.cxx
  vtkActiveScalarsProducer.h
  vtkActiveScalarsProducer.cxx
  vtkOMETiffReader.cxx
  vtkOMETiffReader.h
  vtkTriangleBar.cxx
  vtkTriangleBar.h
  WebExportWidget.cxx
  WebExportWidget.h
)

list(APPEND SOURCES
  loguru/loguru.cpp
  loguru/loguru.hpp
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/loguru)

list(APPEND SOURCES
  legacy/modules/Module.cxx
  legacy/modules/Module.h
  legacy/modules/ModuleClip.cxx
  legacy/modules/ModuleClip.h
  legacy/modules/ModuleContour.cxx
  legacy/modules/ModuleContour.h
  legacy/modules/ModuleContourWidget.cxx
  legacy/modules/ModuleContourWidget.h
  legacy/modules/ModuleFactory.cxx
  legacy/modules/ModuleFactory.h
  legacy/modules/ModuleManager.cxx
  legacy/modules/ModuleManager.h
  legacy/modules/ModuleScaleCube.cxx
  legacy/modules/ModuleScaleCube.h
  legacy/modules/ModuleScaleCubeWidget.cxx
  legacy/modules/ModuleScaleCubeWidget.h
  legacy/modules/ModuleMenu.cxx
  legacy/modules/ModuleMenu.h
  legacy/modules/ModuleMolecule.cxx
  legacy/modules/ModuleMolecule.h
  legacy/modules/ModuleOutline.cxx
  legacy/modules/ModuleOutline.h
  legacy/modules/ModulePlot.cxx
  legacy/modules/ModulePlot.h
  legacy/modules/ModuleRuler.cxx
  legacy/modules/ModuleRuler.h
  legacy/modules/ModuleSegment.cxx
  legacy/modules/ModuleSegment.h
  legacy/modules/ModuleSlice.cxx
  legacy/modules/ModuleSlice.h
  legacy/modules/ModuleThreshold.cxx
  legacy/modules/ModuleThreshold.h
  legacy/modules/ModuleVolume.cxx
  legacy/modules/ModuleVolume.h
  legacy/modules/ModuleVolumeWidget.cxx
  legacy/modules/ModuleVolumeWidget.h
  legacy/modules/ScalarsComboBox.cxx
  legacy/modules/ScalarsComboBox.h
  legacy/modules/VolumeManager.cxx
  legacy/modules/VolumeManager.h
)
# legacy/ subdirectories are NOT on the include path — use explicit
# prefixed includes (e.g. "legacy/DataSource.h", "legacy/modules/Module.h").

# acquisition/
list(APPEND SOURCES
  acquisition/AcquisitionWidget.cxx
  acquisition/AcquisitionWidget.h
  acquisition/AcquisitionClient.cxx
  acquisition/AcquisitionClient.h
  acquisition/AdvancedFormatWidget.cxx
  acquisition/AdvancedFormatWidget.h
  acquisition/BasicFormatWidget.cxx
  acquisition/BasicFormatWidget.h
  acquisition/Connection.cxx
  acquisition/Connection.h
  acquisition/ConnectionsWidget.cxx
  acquisition/ConnectionsWidget.h
  acquisition/ConnectionDialog.cxx
  acquisition/ConnectionDialog.h
  acquisition/CustomFormatWidget.cxx
  acquisition/CustomFormatWidget.h
  acquisition/JsonRpcClient.cxx
  acquisition/JsonRpcClient.h
  acquisition/PassiveAcquisitionWidget.cxx
  acquisition/PassiveAcquisitionWidget.h
  acquisition/RegexGroupDialog.cxx
  acquisition/RegexGroupDialog.h
  acquisition/RegexGroupsWidget.cxx
  acquisition/RegexGroupsWidget.h
  acquisition/RegexGroupSubstitution.cxx
  acquisition/RegexGroupSubstitution.h
  acquisition/RegexGroupSubstitutionDialog.cxx
  acquisition/RegexGroupSubstitutionDialog.h
  acquisition/RegexGroupsSubstitutionsWidget.cxx
  acquisition/RegexGroupsSubstitutionsWidget.h
  acquisition/StartServerDialog.cxx
  acquisition/StartServerDialog.h
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/acquisition)

list(APPEND SOURCES
  legacy/operators/CustomPythonOperatorWidget.cxx
  legacy/operators/CustomPythonOperatorWidget.h
  legacy/operators/EditOperatorWidget.cxx
  legacy/operators/EditOperatorWidget.h
  legacy/operators/Operator.cxx
  legacy/operators/Operator.h
  legacy/operators/OperatorDialog.cxx
  legacy/operators/OperatorDialog.h
  legacy/operators/OperatorFactory.cxx
  legacy/operators/OperatorFactory.h
  legacy/operators/OperatorProxy.cxx
  legacy/operators/OperatorProxy.h
  legacy/operators/OperatorPython.cxx
  legacy/operators/OperatorPython.h
  legacy/operators/OperatorResult.cxx
  legacy/operators/OperatorResult.h
  PipelineModuleMenu.cxx
  PipelineModuleMenu.h
)
# legacy/operators/ is NOT on the include path — use "legacy/operators/Operator.h" etc.

# animations/
list(APPEND SOURCES
  animations/ModuleAnimation.h
  animations/ContourAnimation.h
  animations/SliceAnimation.h
)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/animations)

list(APPEND SOURCES
  h5cpp/h5readwrite.cpp
)

list(APPEND SOURCES
  AboutDialog.h
  AboutDialog.cxx
  MainWindow.cxx
  MainWindow.h
  WelcomeDialog.cxx
  WelcomeDialog.h
)

set(exec_sources
  main.cxx
)

set(python_files
  AddPoissonNoise.py
  BinaryThreshold.py
  CircleMask.py
  ConnectedComponents.py
  ClipEdges.py
  OtsuMultipleThreshold.py
  BinaryDilate.py
  BinaryErode.py
  BinaryOpen.py
  BinaryClose.py
  DummyMolecule.py
  DeconvolutionDenoise.py
  SimilarityMetrics.py
  ElastixRegistration.py
  LabelObjectAttributes.py
  LabelObjectPrincipalAxes.py
  LabelObjectDistanceFromPrincipalAxis.py
  AutoTiltAxisRotationAlignment.py
  AutoTiltAxisShiftAlignment.py
  AutoCenterOfMassTiltImageAlignment.py
  AutoCrossCorrelationTiltImageAlignment.py
  PyStackRegImageAlignment.py
  ctf_correct.py
  Recon_DFT.py
  Recon_DFT_constraint.py
  Recon_WBP.py
  Recon_ART.py
  Recon_SIRT.py
  Recon_TV_minimization.py
  Recon_tomopy.py
  ShiftRotationCenter_tomopy.py
  FFT_AbsLog.py
  ManualManipulation.py
  Shift_Stack_Uniformly.py
  Shift3D.py
  Square_Root_Data.py
  SwapAxes.py
  Subtract_TiltSer_Background.py
  Subtract_TiltSer_Background_Auto.py
  RemoveBadPixelsTiltSeries.py
  NormalizeTiltSeries.py
  Rotate3D.py
  HannWindow3D.py
  GradientMagnitude_Sobel.py
  GradientMagnitude2D_Sobel.py
  LaplaceFilter.py
  GaussianFilter.py
  GaussianFilterTiltSeries.py
  PeronaMalikAnisotropicDiffusion.py
  MedianFilter.py
  Resample.py
  DeleteSlices.py
  ClearVolume.py
  ConstantDataset.py
  RandomParticles.py
  PtychoSource.py
  GenerateTiltSeries.py
  SetNegativeVoxelsToZero.py
  ShiftTiltSeriesRandomly.py
  STEM_probe.py
  InvertData.py
  Pad_Data.py
  BinVolumeByTwo.py
  BinTiltSeriesByTwo.py
  DefaultITKTransform.py
  BinaryMinMaxCurvatureFlow.py
  ReinterpretSignedToUnsigned.py
  SegmentParticles.py
  UnsharpMask.py
  AddConstant.py
  CylindricalCrop.py
  SegmentPores.py
  RotationAlign.py
  WienerFilter.py
  TV_Filter.py
  PoreSizeDistribution.py
  Tortuosity.py
  PowerSpectrumDensity.py
  FourierShellCorrelation.py
  RemoveArrays.py
  Recon_real_time_tomography.py
  )

set(json_files
  AddPoissonNoise.json
  AutoCenterOfMassTiltImageAlignment.json
  AutoCrossCorrelationTiltImageAlignment.json
  AutoTiltAxisRotationAlignment.json
  AutoTiltAxisShiftAlignment.json
  PyStackRegImageAlignment.json
  BinaryThreshold.json
  CircleMask.json
  ConnectedComponents.json
  ClipEdges.json
  OtsuMultipleThreshold.json
  BinaryDilate.json
  BinaryErode.json
  BinaryOpen.json
  BinaryClose.json
  DummyMolecule.json
  DeconvolutionDenoise.json
  SimilarityMetrics.json
  ElastixRegistration.json
  LabelObjectAttributes.json
  LabelObjectPrincipalAxes.json
  LabelObjectDistanceFromPrincipalAxis.json
  ManualManipulation.json
  Shift_Stack_Uniformly.json
  Pad_Data.json
  Resample.json
  DeleteSlices.json
  GaussianFilter.json
  GaussianFilterTiltSeries.json
  PeronaMalikAnisotropicDiffusion.json
  MedianFilter.json
  Rotate3D.json
  GenerateTiltSeries.json
  ctf_correct.json
  Recon_ART.json
  Recon_DFT.json
  Recon_DFT_constraint.json
  Recon_TV_minimization.json
  Recon_tomopy.json
  ShiftRotationCenter_tomopy.json
  Recon_SIRT.json
  Recon_WBP.json
  Shift3D.json
  ShiftTiltSeriesRandomly.json
  SwapAxes.json
  BinaryMinMaxCurvatureFlow.json
  SegmentParticles.json
  UnsharpMask.json
  AddConstant.json
  CylindricalCrop.json
  SegmentPores.json
  RotationAlign.json
  WienerFilter.json
  TV_Filter.json
  PoreSizeDistribution.json
  Tortuosity.json
  PowerSpectrumDensity.json
  FourierShellCorrelation.json
  RemoveArrays.json
  Recon_real_time_tomography.json
  InvertData.json
  ConstantDataset.json
  RandomParticles.json
  PtychoSource.json
  STEM_probe.json
  )

set(resource_files
  defaultcolormaps.json
  )

# Make symlinks where possible, file copies where necessary in the build tree.
if(UNIX)
  set(script_cmd "create_symlink")
else()
  set(script_cmd "copy_if_different")
endif()
file(MAKE_DIRECTORY "${tomviz_BINARY_DIR}/share/tomviz/scripts")
foreach(file ${python_files})
  list(APPEND SOURCES "python/${file}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/${file}"
    "${tomviz_BINARY_DIR}/share/tomviz/scripts/${file}")
  install(FILES "python/${file}"
    DESTINATION "${tomviz_data_install_dir}/scripts"
    COMPONENT "Scripts")
endforeach()
foreach(file ${json_files})
  list(APPEND SOURCES "python/${file}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/${file}"
    "${tomviz_BINARY_DIR}/share/tomviz/scripts/${file}")
  install(FILES "python/${file}"
    DESTINATION "${tomviz_data_install_dir}/scripts"
    COMPONENT "Scripts")
endforeach()
foreach(file ${resource_files})
  list(APPEND SOURCES "resources/${file}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/resources/${file}"
    "${tomviz_BINARY_DIR}/share/tomviz/${file}")
  install(FILES "resources/${file}"
    DESTINATION "${tomviz_data_install_dir}"
    COMPONENT runtime)
endforeach()

set(template_files
  Back_Projection_Reconstruction.tvsm
)

# Install templates
file(MAKE_DIRECTORY "${tomviz_BINARY_DIR}/share/tomviz/templates")
foreach(file ${template_files})
  list(APPEND SOURCES "templates/${file}")
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/templates/${file}"
    "${tomviz_BINARY_DIR}/share/tomviz/templates/${file}")
  install(FILES "templates/${file}"
    DESTINATION "${tomviz_data_install_dir}"
    COMPONENT runtime)
endforeach()

set(tomviz_python_modules
  __init__.py
  _internal.py
  dataset.py
  executor.py
  external_dataset.py
  fix_pdb.py
  nodes.py
  operators.py
  internal_dataset.py
  internal_utils.py
  itkutils.py
  utils.py
  web.py
  modules.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz")
foreach(file ${tomviz_python_modules})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/${file}"
    "${tomviz_python_binary_dir}/tomviz/${file}")
endforeach()

# Install the tomviz Python files.
install(DIRECTORY "${tomviz_SOURCE_DIR}/tomviz/python/tomviz"
        DESTINATION "${tomviz_python_install_dir}"
        USE_SOURCE_PERMISSIONS
        COMPONENT runtime)

set(tomviz_real_time_files
  logger.py
  pytvlib.py
  wbp.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/_realtime")
foreach(file ${tomviz_real_time_files})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/_realtime/${file}"
    "${tomviz_python_binary_dir}/tomviz/_realtime/${file}")
endforeach()

set(tomviz_python_io_modules
  __init__.py
  _internal.py
  _databroker.py
  ser.py
  dm.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/io")
foreach(file ${tomviz_python_io_modules})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/io/${file}"
    "${tomviz_python_binary_dir}/tomviz/io/${file}")
endforeach()

set(tomviz_python_pyxrf_modules
  __init__.py
  ic_names.py
  load_output.py
  sids.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/pyxrf")
foreach(file ${tomviz_python_pyxrf_modules})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/pyxrf/${file}"
    "${tomviz_python_binary_dir}/tomviz/pyxrf/${file}")
endforeach()

set(tomviz_python_ptycho_modules
  __init__.py
  ptycho.py
)

file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/ptycho")
foreach(file ${tomviz_python_ptycho_modules})
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/ptycho/${file}"
    "${tomviz_python_binary_dir}/tomviz/ptycho/${file}")
endforeach()

file(GLOB tomviz_python_state_modules "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/state/*.py")
file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/state")
foreach(file_ ${tomviz_python_state_modules})
  get_filename_component(file ${file_} NAME)
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/state/${file}"
    "${tomviz_python_binary_dir}/tomviz/state/${file}")
endforeach()

# Autodiscover python reader/writers added to tomviz/python/tomviz/io/formats
file(GLOB tomviz_python_io_formats "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/io/formats/*.py")
file(MAKE_DIRECTORY "${tomviz_python_binary_dir}/tomviz/io/formats")
foreach(file_ ${tomviz_python_io_formats})
  get_filename_component(file ${file_} NAME)
  execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
    "${tomviz_SOURCE_DIR}/tomviz/python/tomviz/io/formats/${file}"
    "${tomviz_python_binary_dir}/tomviz/io/formats/${file}")
endforeach()

# Install documentation (user guide)
file(MAKE_DIRECTORY "${tomviz_BINARY_DIR}/share/tomviz/docs")
execute_process(COMMAND ${CMAKE_COMMAND} -E ${script_cmd}
  "${tomviz_SOURCE_DIR}/docs/TomvizBasicUserGuide.pdf"
  "${tomviz_BINARY_DIR}/share/tomviz/docs/TomvizBasicUserGuide.pdf")
install(FILES "${tomviz_SOURCE_DIR}/docs/TomvizBasicUserGuide.pdf"
  DESTINATION "${tomviz_data_install_dir}/docs"
  COMPONENT "Documentation")

if(APPLE AND TOMVIZ_MACOSX_BUNDLE)
  list(APPEND exec_sources icons/tomviz.icns)
  set(MACOSX_BUNDLE_ICON_FILE tomviz.icns)
  set(MACOSX_BUNDLE_BUNDLE_VERSION "${tomviz_version}")
  set_source_files_properties(icons/tomviz.icns PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
elseif(WIN32)
  list(APPEND exec_sources icons/tomviz.rc)
endif()

include_directories(
  ${CMAKE_CURRENT_BINARY_DIR}/core
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/core
  ${CMAKE_CURRENT_SOURCE_DIR}/pipeline)

configure_file(tomvizConfig.h.in tomvizConfig.h @ONLY)
configure_file(tomvizPythonConfig.h.in tomvizPythonConfig.h @ONLY)

add_library(tomvizlib STATIC ${SOURCES} ${accel_srcs})
if(NOT WIN32)
  set_target_properties(tomvizlib PROPERTIES OUTPUT_NAME tomviz)
  set_target_properties(tomvizlib PROPERTIES AUTOUIC_SEARCH_PATHS
      "${CMAKE_CURRENT_SOURCE_DIR}/legacy;${CMAKE_CURRENT_SOURCE_DIR}/legacy/modules;${CMAKE_CURRENT_SOURCE_DIR}/legacy/operators;${CMAKE_CURRENT_SOURCE_DIR}/pipeline")
endif()

# Need to set this property so that in stack traces, tomviz symbols
# can be resolved.
set_target_properties(tomvizlib PROPERTIES ENABLE_EXPORTS TRUE)

set_property(TARGET tomvizlib PROPERTY POSITION_INDEPENDENT_CODE TRUE)

find_package(Viskores QUIET)

if(APPLE AND TOMVIZ_MACOSX_BUNDLE)
  add_executable(tomviz MACOSX_BUNDLE ${exec_sources} resources.qrc pipeline/pipeline_icons.qrc)
else()
  add_executable(tomviz WIN32 ${exec_sources} resources.qrc pipeline/pipeline_icons.qrc)
endif()
target_link_libraries(tomviz PRIVATE tomvizlib ${OPENGL_LIBRARIES} ${extra_link_libraries})
if(Viskores_FOUND)
  target_link_libraries(tomviz PRIVATE viskores::cont)
endif()

# Need to set this property so that in stack traces, tomviz symbols
# can be resolved.
set_target_properties(tomviz PROPERTIES ENABLE_EXPORTS TRUE)

target_link_libraries(tomvizlib
  PUBLIC
    tomvizcore
    ParaView::pqApplicationComponents
    ParaView::pqPython
    ParaView::RemotingExport
    ParaView::RemotingViews
    VTK::jsoncpp
    VTK::pugixml
    VTK::tiff
    VTK::CommonColor
    VTK::CommonCore
    VTK::CommonDataModel
    VTK::DomainsChemistry
    VTK::IOChemistry
    VTK::hdf5
    VTK::InteractionStyle
    VTK::RenderingVolume
    VTK::RenderingVolumeOpenGL2
    Qt6::Network
    Qt6::Widgets
  PRIVATE
    Qt6::Core5Compat
    Python3::Python
    $<$<PLATFORM_ID:Windows>:dbghelp shell32>
    pybind11::embed
    VTK::WrappingPythonCore
    VTK::IOImage
    VTK::IOXML
    VTK::IOLegacy
    VTK::RenderingCore
    VTK::FiltersCore
    VTK::FiltersSources
    VTK::FiltersGeneral
    VTK::FiltersModeling
    VTK::InteractionWidgets
    VTK::ImagingCore
    VTK::RenderingGridAxes
    VTK::ChartsCore
    VTK::ViewsContext2D)

if(APPLE AND TOMVIZ_MACOSX_BUNDLE)
  set_target_properties(tomviz
    PROPERTIES
      MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/tomviz.plist.in)
  install(TARGETS tomviz DESTINATION Applications COMPONENT runtime)
else()
  install(TARGETS tomviz DESTINATION "${INSTALL_RUNTIME_DIR}" COMPONENT runtime)
endif()
install(TARGETS tomvizlib
  RUNTIME DESTINATION "${INSTALL_RUNTIME_DIR}"
  LIBRARY DESTINATION "${INSTALL_LIBRARY_DIR}"
  ARCHIVE DESTINATION "${INSTALL_ARCHIVE_DIR}")

if(tomviz_data_DIR)
  add_definitions(-DTOMVIZ_DATA)
  install(DIRECTORY "${tomviz_data_DIR}"
    DESTINATION "${tomviz_data_install_dir}"
    USE_SOURCE_PERMISSIONS
    COMPONENT runtime)
endif()

if(tomviz_templates_DIR)
  add_definitions(-DTOMVIZ_TEMPLATES)
  install(DIRECTORY "${tomviz_templates_DIR}"
    DESTINATION "${tomviz_data_install_dir}"
    USE_SOURCE_PERMISSIONS
    COMPONENT runtime)
endif()

add_subdirectory(pybind11)
