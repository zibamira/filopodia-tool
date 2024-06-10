
find_package(AvizoAppsITK REQUIRED)
find_package(AvizoAppsOIV REQUIRED)
find_package(AvizoAppsQt5 REQUIRED)
find_package(AvizoAppsEigen REQUIRED)

file(GLOB C_CXX_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} *.c *.cc *.cpp *.cxx *.h *.hh *.hpp *.hxx)

xpand_add_library(hxfilo
   SHARED
       ${C_CXX_SOURCES}
)

target_link_libraries(hxfilo
   PRIVATE
   AvizoApps::Qt5Core
   AvizoApps::Qt5Gui
   AvizoApps::Qt5Widgets
   AvizoApps::Qt5Script
   AvizoApps::Inventor
   AvizoApps::ITKCommon
   AvizoApps::ITKWatersheds
#    AvizoApps::ITKVNLInstantiation
   AvizoApps::ITKSys
   AvizoApps::ITKVcl
   AvizoApps::ITKVnlAlgo
   AvizoApps::ITKV3p_netlib
   AvizoApps::ITKVnl
   AvizoApps::ITKExpat
   AvizoApps::ITKGDCMCommon
   AvizoApps::ITKGDCMDICT
   AvizoApps::ITKGDCMDSED
   AvizoApps::ITKGDCMIOD
   AvizoApps::ITKGDCMMEXD
   AvizoApps::ITKGDCMMSFF
   AvizoApps::ITKGDCMCharls
   AvizoApps::ITKGDCMJpeg12
   AvizoApps::ITKGDCMJpeg16
   AvizoApps::ITKGDCMJpeg8
   AvizoApps::ITKGDCMOpenJpeg
   AvizoApps::ITKGDCMSocketxx
#    AvizoApps::ITKTransform
   amiramesh
   hxcolor
   # hxcontour2surface
   # hxcontourtree
   hxcore
   hxdistmap
   hxfield
   hxitk
   hxlatticex
   hxlatticexbase
   hxlines
   hxneuroneditor
   hxneurontree
   hxskeleton
   hxspatialgraph
   hxspreadsheet
   hxsurface
   hxunits
   hxvisageviewer
   hxvsdata
   mclib
   # pointmatching
   vsvolren
)

add_custom_command(
   TARGET hxfilo
   POST_BUILD
   COMMAND
       ${CMAKE_COMMAND}
   ARGS
       -E
       copy_if_different
       "${CMAKE_CURRENT_SOURCE_DIR}/share/resources/hxfilo.rc"
       "${CMAKE_SOURCE_DIR}/../share/resources/hxfilo.rc"
) 
