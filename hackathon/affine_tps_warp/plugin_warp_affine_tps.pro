
TEMPLATE      = lib
CONFIG       += plugin 
CONFIG       += x86_64
CONFIG += qt warn_off
CONFIG -= app_bundle

V3DMAINPATH  = ../../v3d_main
INCLUDEPATH  += $$V3DMAINPATH/basic_c_fun
INCLUDEPATH  += $$V3DMAINPATH/common_lib/include
INCLUDEPATH  += $$V3DMAINPATH/jba/newmat11

LIBS         += -L../../v3d_main/jba/c++ -lv3dnewmat
LIBS         += -L../../v3d_main/common_lib/lib -lv3dtiff

HEADERS      += $$V3DMAINPATH/basic_c_fun/v3d_message.h
HEADERS      += $$V3DMAINPATH/basic_c_fun/basic_surf_objs.h
HEADERS      += $$V3DMAINPATH/basic_c_fun/stackutil.h
HEADERS      += $$V3DMAINPATH/basic_c_fun/mg_image_lib.h
HEADERS      += $$V3DMAINPATH/basic_c_fun/mg_utilities.h
HEADERS      += q_warp_affine_tps.h
HEADERS      += q_warp_affine_tps_dialogs.h
HEADERS      += plugin_warp_affine_tps.h

SOURCES      += $$V3DMAINPATH/basic_c_fun/v3d_message.cpp
SOURCES      += $$V3DMAINPATH/basic_c_fun/basic_surf_objs.cpp
SOURCES      += $$V3DMAINPATH/basic_c_fun/stackutil.cpp
SOURCES      += $$V3DMAINPATH/basic_c_fun/mg_image_lib.cpp
SOURCES      += $$V3DMAINPATH/basic_c_fun/mg_utilities.cpp
SOURCES      += q_warp_affine_tps.cpp
SOURCES      += q_warp_affine_tps_dialogs.cpp
SOURCES      += plugin_warp_affine_tps.cpp

TARGET        = $$qtLibraryTarget(plugin_warp_affin_tps)


#DESTDIR       = ../../v3d_main/v3d/plugins/alignment_affin_TPS_warp  #win32 qmake couldn't handle space in path

