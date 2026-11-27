cmake_minimum_required(VERSION 3.20)
project(TKGPU_Runtime VERSION 0.39.0 LANGUAGES C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(TKGPU_CORE
  src/core/TKGPUInterpreter.C src/core/history/TKGPUHistory.C src/core/history/TKGPUHistoryCommand.C src/core/TKGPUExpression.C src/core/numeric/TKGPURealExpr.C
  src/core/TKGPUParser.C src/core/TKGPUVariables.C src/core/commands/TKGPUVariableTrace.C src/core/TKGPUCommands.C
  src/core/TKGPUControl.C src/core/TKGPUCondition.C src/core/TKGPUObjectCall.C src/core/TKGPUCommandRegistry.C src/core/commands/TKGPUCommandIndex.C src/core/commands/TKGPUHostResultKinds.C src/core/commands/TKGPUObjectCommand.C src/core/commands/TKGPUBindings.C src/core/commands/TKGPUControlInvoke.C src/core/commands/TKGPUEntryValidation.C
  src/memory/TKGPUChecked.C src/memory/TKGPUBuffer.C
  src/text/TKGPUUtf8Decode.C src/text/TKGPUUtf8Encode.C
  src/object/TKGPUObject.C src/object/TKGPUObjectInteger.C src/object/TKGPUObjectDispatch.C src/object/TKGPUDock.C src/object/TKGPUBaseDialog.C src/object/TKGPUTitleBar.C
  src/core/TKGPUListParse.C src/core/TKGPUListFormat.C src/core/TKGPUListCommands.C
  src/render/TKGPUFrameBuffer.C src/render/TKGPUScene.C src/render/parallel/TKGPUParallelPaint.C src/render/parallel/TKGPUWorker.C
  src/render/async/TKGPUAsync.C src/render/async/TKGPUSnapshot.C
  src/render/raster/TKGPUClip.C src/render/raster/TKGPUStroke.C src/render/raster/TKGPUEllipse.C
  src/render/raster/TKGPUPixelBlend.C src/render/image/TKGPUImageBlit.C src/render/image/TKGPUPhoto.C
  src/render/text/TKGPUFallbackFont.C src/render/text/TKGPUTextRaster.C src/render/text/TKGPUTextLayout.C src/render/text/TKGPUNativeText.C src/render/text/TKGPULegacyUnicode.C
  src/widgets/image/TKGPUImageRegistry.C src/widgets/image/TKGPUImageCommands.C
  src/widgets/canvas/TKGPUCanvasItems.C src/widgets/canvas/TKGPUCanvasPaint.C src/widgets/canvas/TKGPUCanvasCommands.C
  src/widgets/ttk/TKGPUStyle.C src/widgets/ttk/TKGPUIndicators.C src/widgets/ttk/TKGPUExtra.C src/widgets/ttk/TKGPUChoice.C src/widgets/ttk/TKGPUNotebook.C src/widgets/ttk/TKGPUToggle.C src/widgets/ttk/TKGPUContainers.C src/widgets/ttk/tree/TKGPUTree.C src/widgets/controls/TKGPUControls.C src/widgets/controls/TKGPUEntry.C
  src/widgets/TKGPUWidgetTree.C src/widgets/TKGPUWidgetPathIndex.C src/widgets/TKGPUWidgetCommands.C src/widgets/listbox/TKGPUListbox.C src/widgets/text/TKGPUTextWidget.C
  src/layout/TKGPUGeometry.C src/events/TKGPUEventQueue.C
  src/events/timers/TKGPUTimerQueue.C src/events/timers/TKGPUTimerDispatch.C)
set(TKGPU_PROGRAMS src/shell/TKGPUShell.C src/platform/win32/TKGPUWinWindow.C
  tests/core/TKGPUCoreTests.C tests/core/TKGPUUtf8Tests.C tests/core/TKGPUBufferTests.C tests/core/TKGPUControlTests.C tests/core/TKGPUObjectTests.C tests/core/TKGPUCommandTests.C tests/core/TKGPUExpressionTests.C tests/core/TKGPUListTests.C tests/widgets/TKGPUWidgetTests.C tests/events/TKGPUTimerTests.C tests/render/TKGPURasterTests.C tests/render/TKGPUImageTests.C tests/render/TKGPUTextTests.C
  tests/widgets/canvas/TKGPUCanvasItemTests.C tests/widgets/image/TKGPUPhotoTests.C)
# Uppercase .C conventionally means C++; explicitly select ISO C.
set_source_files_properties(${TKGPU_CORE} ${TKGPU_PROGRAMS} PROPERTIES LANGUAGE C)
add_library(tkgpu_core STATIC ${TKGPU_CORE})
target_include_directories(tkgpu_core PUBLIC include)
if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(tkgpu_core PUBLIC Threads::Threads m)
endif()
if(MSVC)
  target_compile_options(tkgpu_core PRIVATE /W4)
else()
  target_compile_options(tkgpu_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
add_executable(tkgpu_shell src/shell/TKGPUShell.C)
target_link_libraries(tkgpu_shell PRIVATE tkgpu_core)
if(WIN32)
  add_executable(tkgpu_demo WIN32 src/platform/win32/TKGPUWinWindow.C)
  target_link_libraries(tkgpu_demo PRIVATE tkgpu_core user32 gdi32)
endif()
enable_testing()
add_executable(tkgpu_tests tests/core/TKGPUCoreTests.C)
target_link_libraries(tkgpu_tests PRIVATE tkgpu_core)
add_test(NAME core COMMAND tkgpu_tests)
add_executable(tkgpu_widget_tests tests/widgets/TKGPUWidgetTests.C)
target_link_libraries(tkgpu_widget_tests PRIVATE tkgpu_core)
add_test(NAME widgets COMMAND tkgpu_widget_tests)
set_source_files_properties(tests/widgets/TKGPUDockTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_dock_tests tests/widgets/TKGPUDockTests.C)
target_link_libraries(tkgpu_dock_tests PRIVATE tkgpu_core)
add_test(NAME dock_object COMMAND tkgpu_dock_tests)
set_source_files_properties(tests/widgets/TKGPUBaseDialogTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_base_dialog_tests tests/widgets/TKGPUBaseDialogTests.C)
target_link_libraries(tkgpu_base_dialog_tests PRIVATE tkgpu_core)
add_test(NAME base_dialog_object COMMAND tkgpu_base_dialog_tests)
set_source_files_properties(tests/widgets/TKGPUTitleBarTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_titlebar_tests tests/widgets/TKGPUTitleBarTests.C)
target_link_libraries(tkgpu_titlebar_tests PRIVATE tkgpu_core)
add_test(NAME titlebar_object COMMAND tkgpu_titlebar_tests)
add_executable(tkgpu_expression_tests tests/core/TKGPUExpressionTests.C)
target_link_libraries(tkgpu_expression_tests PRIVATE tkgpu_core)
add_test(NAME expressions COMMAND tkgpu_expression_tests)
add_executable(tkgpu_list_tests tests/core/TKGPUListTests.C)
target_link_libraries(tkgpu_list_tests PRIVATE tkgpu_core)
add_test(NAME lists COMMAND tkgpu_list_tests)
add_executable(tkgpu_control_tests tests/core/TKGPUControlTests.C)
target_link_libraries(tkgpu_control_tests PRIVATE tkgpu_core)
add_test(NAME control COMMAND tkgpu_control_tests)
add_executable(tkgpu_object_tests tests/core/TKGPUObjectTests.C)
target_link_libraries(tkgpu_object_tests PRIVATE tkgpu_core)
add_test(NAME objects COMMAND tkgpu_object_tests)
add_executable(tkgpu_command_tests tests/core/TKGPUCommandTests.C)
target_link_libraries(tkgpu_command_tests PRIVATE tkgpu_core)
add_test(NAME commands COMMAND tkgpu_command_tests)

# Separate preview library; never use the system Tcl/Tk library names.
set_target_properties(tkgpu_core PROPERTIES POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
set(TKGPU_COMPAT
  src/host/TKGPUHostRuntime.C src/host/commands/TKGPUHostCommands.C
  src/host/values/TKGPUHostValues.C src/host/images/TKGPUHostPhoto.C
  src/compat/tk/TKGPUAnimation.C src/compat/tk/TKGPUFont.C src/compat/tk/TKGPUHotkeys.C src/compat/tk/TKGPUClipboard.C src/compat/tk/TKGPUDialog.C src/compat/tk/TKGPUWindowCommand.C src/compat/tk/TKGPUWindowRegistry.C src/compat/tk/TKGPUWindowScene.C
  src/compat/TKGPUCompatInterp.C src/compat/interp/TKGPUCompatInit.C src/compat/interp/TKGPUCompatRecord.C src/platform/process/TKGPUExecutable.C src/compat/TKGPUCompatVars.C
  src/compat/commands/TKGPUCompatObjCommand.C src/compat/interp/TKGPUCompatEvalFile.C
  src/compat/interp/TKGPUCompatError.C src/compat/interp/TKGPUCompatLimits.C src/compat/interp/TKGPUCompatComplete.C src/compat/loader/TKGPUCompatLoad.C
  src/compat/bignum/TKGPUCompatBnMemory.C src/compat/bignum/TKGPUCompatBnRadix.C src/compat/bignum/TKGPUCompatBnParse.C
  src/compat/bignum/TKGPUCompatBnBinary.C src/compat/bignum/TKGPUCompatBnDecimal.C
  src/compat/bignum/TKGPUCompatBnObject.C
  src/compat/TKGPUCompatExpr.C src/compat/TKGPUCompatMemory.C src/compat/TKGPUCompatDString.C
  src/compat/TKGPUCompatList.C src/compat/TKGPUCompatBoolean.C
  src/compat/TKGPUCompatObject.C src/compat/TKGPUCompatUnicode.C src/compat/TKGPUCompatPhoto.C src/compat/TKGPUCompatInteger.C src/compat/TKGPUCompatDispatch.C
  src/compat/TKGPUCompatResult.C src/compat/TKGPUCompatObjVars.C
  src/compat/TKGPUCompatObjBoolean.C src/compat/TKGPUCompatByteArray.C
  src/compat/TKGPUCompatObjList.C src/compat/TKGPUCompatLifetime.C
  src/compat/types/TKGPUCompatTypes.C src/compat/types/TKGPUCompatDict.C src/compat/numeric/TKGPUCompatDouble.C src/compat/numeric/TKGPUCompatExprDouble.C
  src/compat/events/TKGPUNotifierState.C src/compat/events/TKGPUCompatTimer.C
  src/compat/events/script/TKGPUIdle.C src/compat/events/script/TKGPUAfter.C
  src/compat/events/TKGPUCompatEventLoop.C src/compat/events/TKGPUCompatNotifier.C src/compat/events/TKGPUCompatService.C src/platform/time/TKGPUClock.C
  src/platform/time/legacy/TKGPUTickExtend.C
  src/compat/events/queue/TKGPUThreadEventQueue.C src/compat/events/queue/TKGPUDeleteEvents.C
  src/compat/events/queue/TKGPUThreadEventDispatch.C src/compat/events/queue/TKGPUThreadRegistry.C
  src/compat/threads/TKGPUCompatMutex.C src/compat/threads/TKGPUCompatCondition.C
  src/compat/threads/TKGPUCompatThreadData.C src/compat/threads/TKGPUCompatExit.C
  src/compat/numeric/TKGPUCompatFormat.C
  src/compat/interp/TKGPUCompatCancel.C src/compat/interp/TKGPUCompatReturnOptions.C
  src/compat/interp/TKGPUCompatMisc.C src/compat/commands/TKGPUCompatEnsemble.C
  src/compat/interp/TKGPUCompatTraces.C src/compat/interp/TKGPUCompatSlaves.C
  src/compat/interp/TKGPUCompatNRE.C src/compat/threads/TKGPUCompatThreadsExt.C
  src/compat/types/TKGPUCompatEncoding.C src/compat/types/TKGPUCompatRegExp.C
  src/compat/types/TKGPUCompatZlib.C)
option(TKGPU_LEGACY_WINDOWS "Experimental x86 Win9x/XP native thread backend" OFF)
option(TKGPU_NATIVE_TEXT "Build native Unicode shaping backend" ON)
if(TKGPU_NATIVE_TEXT AND (NOT TKGPU_LEGACY_WINDOWS OR TKGPU_LEGACY_WINDOWS_TARGET STREQUAL "xp"))
  if(WIN32)
    target_compile_definitions(tkgpu_core PRIVATE TKGPU_TEXT_UNISCRIBE)
    target_link_libraries(tkgpu_core PUBLIC usp10 gdi32)
  else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(PANGO QUIET IMPORTED_TARGET pangocairo)
      if(PANGO_FOUND)
        target_compile_definitions(tkgpu_core PRIVATE TKGPU_TEXT_PANGO)
        target_link_libraries(tkgpu_core PUBLIC PkgConfig::PANGO)
      endif()
    endif()
  endif()
endif()
if(TKGPU_NATIVE_TEXT AND (NOT TKGPU_LEGACY_WINDOWS OR TKGPU_LEGACY_WINDOWS_TARGET STREQUAL "xp") AND (WIN32 OR PANGO_FOUND))
  set_source_files_properties(tests/render/TKGPUInternationalTextTests.C PROPERTIES LANGUAGE C)
  add_executable(tkgpu_international_text_tests tests/render/TKGPUInternationalTextTests.C)
  target_link_libraries(tkgpu_international_text_tests PRIVATE tkgpu_core)
  add_test(NAME international_text COMMAND tkgpu_international_text_tests)
  set_tests_properties(international_text PROPERTIES ENVIRONMENT "TKGPU_NATIVE_TEXT=1")
endif()
if(TKGPU_LEGACY_WINDOWS AND (NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4))
  message(FATAL_ERROR "Legacy Windows backend requires a 32-bit Windows toolchain")
endif()
if(TKGPU_LEGACY_WINDOWS)
  include(cmake/platform/WindowsLegacy.cmake)
  tkgpu_apply_legacy_profile(tkgpu_core)
  tkgpu_apply_legacy_profile(tkgpu_demo)
  tkgpu_apply_legacy_profile(tkgpu_shell)
endif()
if(WIN32)
  if(TKGPU_LEGACY_WINDOWS)
    list(APPEND TKGPU_COMPAT src/platform/threads/legacy/TKGPUThreadSyncLegacy.C
      src/platform/threads/legacy/TKGPUThreadLocalLegacy.C src/compat/threads/TKGPUCompatLegacyFinalize.C)
  else()
    list(APPEND TKGPU_COMPAT src/platform/threads/win32/TKGPUThreadSync.C
      src/platform/threads/win32/TKGPUThreadLocal.C)
  endif()
else()
  list(APPEND TKGPU_COMPAT src/compat/events/file/TKGPUFileHandlers.C
    src/platform/threads/posix/TKGPUThreadSync.C
    src/platform/threads/posix/TKGPUThreadLocal.C)
endif()
option(TKGPU_X11 "Enable native X11 window preview when available" ON)
option(TKGPU_COCOA "Experimental macOS Cocoa native window host" OFF)
if(WIN32)
  list(APPEND TKGPU_COMPAT src/platform/window/win32/TKGPUWinHost.C)
  if(NOT TKGPU_LEGACY_WINDOWS)
    include(CheckIncludeFile)
    check_include_file(d3d11.h TKGPU_HAVE_D3D11_H)
    if(TKGPU_HAVE_D3D11_H)
      list(APPEND TKGPU_COMPAT src/platform/window/win32/TKGPUD3D11Present.C)
    endif()
  endif()
elseif(APPLE AND TKGPU_COCOA)
  enable_language(OBJC)
elseif(UNIX AND NOT APPLE AND TKGPU_X11)
  find_package(X11 QUIET)
  if(X11_FOUND)
    list(APPEND TKGPU_COMPAT src/platform/window/x11/TKGPUX11Host.C)
  else()
    list(APPEND TKGPU_COMPAT src/platform/window/TKGPUUnavailableHost.C)
  endif()
else()
  list(APPEND TKGPU_COMPAT src/platform/window/TKGPUUnavailableHost.C)
endif()
set_source_files_properties(${TKGPU_COMPAT} tests/abi/TKGPUCompatTests.C
  tests/abi/TKGPUCompatVarTests.C tests/abi/TKGPUCompatExprTests.C
  tests/abi/TKGPUCompatListTests.C tests/abi/TKGPUCompatObjectTests.C
  tests/abi/TKGPUCompatResultTests.C tests/abi/TKGPUCompatByteTests.C
  tests/abi/TKGPUCompatObjListTests.C tests/abi/TKGPUAbiLayoutTests.C
  tests/abi/TKGPUAbiLayoutProbe.C tests/abi/types/TKGPUCompatTypeTests.C
  tests/abi/events/TKGPUCompatTimerTests.C tests/abi/threads/TKGPUCompatThreadTests.C tests/abi/events/TKGPUThreadEventTests.C
  tests/abi/commands/TKGPUCompatObjCommandTests.C tests/abi/interp/TKGPUCompatEvalFileTests.C tests/abi/bignum/TKGPUCompatBignumTests.C tests/abi/bignum/TKGPUCompatBignumSyntaxTests.C PROPERTIES LANGUAGE C)
add_library(tkgpu_compat SHARED ${TKGPU_COMPAT})
if(APPLE AND TKGPU_COCOA)
  target_sources(tkgpu_compat PRIVATE src/platform/window/cocoa/TKGPUCocoaHost.m)
  target_compile_definitions(tkgpu_compat PRIVATE TKGPU_COCOA)
  target_link_libraries(tkgpu_compat PRIVATE "-framework Cocoa" "-framework CoreGraphics")
  set_source_files_properties(src/platform/window/cocoa/TKGPUCocoaHost.m PROPERTIES COMPILE_OPTIONS "-fno-objc-arc")
endif()
target_compile_definitions(tkgpu_compat PRIVATE TKGPU_COMPAT_BUILD TKGPU_BUILD_VERSION="${PROJECT_VERSION}")
if(TKGPU_LEGACY_WINDOWS)
  target_compile_definitions(tkgpu_compat PUBLIC TKGPU_LEGACY_WINDOWS)
  tkgpu_apply_legacy_profile(tkgpu_compat)
endif()
# Static, bundled codec: identical implementation on Windows and Linux;
# no dependency on an incidental system DLL or a Tcl runtime.
add_library(tkgpu_zlib STATIC
  third_party/zlib/adler32.c third_party/zlib/crc32.c
  third_party/zlib/deflate.c third_party/zlib/trees.c
  third_party/zlib/inflate.c third_party/zlib/inffast.c
  third_party/zlib/inftrees.c third_party/zlib/zutil.c)
set_target_properties(tkgpu_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(tkgpu_zlib PRIVATE Z_SOLO)
target_include_directories(tkgpu_zlib PUBLIC third_party/zlib)
target_link_libraries(tkgpu_compat PRIVATE tkgpu_zlib)
target_link_libraries(tkgpu_compat PRIVATE tkgpu_core)
if(WIN32)
  target_link_libraries(tkgpu_compat PRIVATE user32 gdi32 comdlg32 shell32 ole32)
elseif(TKGPU_X11 AND X11_FOUND)
  target_link_libraries(tkgpu_compat PRIVATE X11::X11)
endif()
if(NOT WIN32)
  find_package(Threads REQUIRED)
  target_link_libraries(tkgpu_compat PRIVATE Threads::Threads)
endif()
if(WIN32 AND NOT TKGPU_LEGACY_WINDOWS AND TKGPU_HAVE_D3D11_H)
  target_compile_definitions(tkgpu_compat PRIVATE TKGPU_D3D11_PRESENT=1)
endif()
target_include_directories(tkgpu_compat PUBLIC include)
option(TKGPU_REFERENCE_STUBS "Opt-in audited reference Tcl 8.6 Stubs subset (Linux development)" OFF)
if(TKGPU_REFERENCE_STUBS)
  if(NOT UNIX OR APPLE)
    message(FATAL_ERROR "Reference Stubs preview is currently Linux-only")
  endif()
  find_path(TKGPU_TCL86_INCLUDE tcl.h PATH_SUFFIXES tcl8.6 REQUIRED)
  find_library(TKGPU_TCL86_STUB NAMES tclstub8.6 REQUIRED)
  set_source_files_properties(src/compat/stubs/TKGPUReferenceStubs.C
    tests/abi/types/TKGPUReferenceStubsTests.C PROPERTIES LANGUAGE C)
  target_sources(tkgpu_compat PRIVATE src/compat/stubs/TKGPUReferenceStubs.C)
  target_include_directories(tkgpu_compat PRIVATE "${TKGPU_TCL86_INCLUDE}")
  target_compile_definitions(tkgpu_compat PRIVATE TKGPU_REFERENCE_STUBS)
  set_source_files_properties(src/compat/stubs/TKGPUReferenceStubs.C
    PROPERTIES COMPILE_OPTIONS "-Werror=incompatible-pointer-types")
  add_executable(tkgpu_reference_stubs_tests tests/abi/types/TKGPUReferenceStubsTests.C)
  target_include_directories(tkgpu_reference_stubs_tests PRIVATE "${TKGPU_TCL86_INCLUDE}")
  target_link_libraries(tkgpu_reference_stubs_tests PRIVATE tkgpu_compat "${TKGPU_TCL86_STUB}")
  add_test(NAME reference_stubs_subset COMMAND tkgpu_reference_stubs_tests)
endif()
set_target_properties(tkgpu_compat PROPERTIES OUTPUT_NAME TKGPUCompat C_VISIBILITY_PRESET hidden)
# The independent host ABI is built alongside the legacy compatibility facade.
set_source_files_properties(tests/host/TKGPUHostTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_host_tests tests/host/TKGPUHostTests.C)
target_link_libraries(tkgpu_host_tests PRIVATE tkgpu_compat)
add_test(NAME host_abi COMMAND tkgpu_host_tests)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND AND NOT CMAKE_CROSSCOMPILING AND NOT TKGPU_LEGACY_WINDOWS)
  add_test(NAME host_bundle_inventory COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/host/TKGPUHostBundleTests.py")
  add_test(NAME host_qualification_evidence COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/host/TKGPUQualificationTests.py")
  add_test(NAME host_python COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/host/TKGPUHostPythonTests.py" "$<TARGET_FILE:tkgpu_compat>")
  add_test(NAME host_tkinter_api COMMAND "${Python3_EXECUTABLE}" -I
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/host/TKGPUHostTkinterTests.py" "$<TARGET_FILE:tkgpu_compat>")
  set_tests_properties(host_tkinter_api PROPERTIES SKIP_RETURN_CODE 77)
endif()
# Bundle the direct host without locating, copying or patching _tkinter.
set(TKGPU_HOST_BUNDLE "${CMAKE_BINARY_DIR}/tkgpu-host" CACHE PATH "Independent host bundle")
if(WIN32)
  set(TKGPU_HOST_BUNDLE_LIBRARY tkgp.dll)
elseif(APPLE)
  set(TKGPU_HOST_BUNDLE_LIBRARY libTKGPUCompat.dylib)
else()
  set(TKGPU_HOST_BUNDLE_LIBRARY libTKGPUCompat.so)
endif()
if(Python3_Interpreter_FOUND)
  add_custom_target(tkgpu_host_bundle
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/TKGPUHostBundle.py"
      --source "${CMAKE_CURRENT_SOURCE_DIR}" --library "$<TARGET_FILE:tkgpu_compat>"
      --name "${TKGPU_HOST_BUNDLE_LIBRARY}" --bundle "${TKGPU_HOST_BUNDLE}"
    DEPENDS tkgpu_compat VERBATIM)
endif()
add_executable(tkgpu_compat_tests tests/abi/TKGPUCompatTests.C)
target_link_libraries(tkgpu_compat_tests PRIVATE tkgpu_compat)
add_test(NAME compat_facade COMMAND tkgpu_compat_tests)
add_executable(tkgpu_compat_var_tests tests/abi/TKGPUCompatVarTests.C)
target_link_libraries(tkgpu_compat_var_tests PRIVATE tkgpu_compat)
add_test(NAME compat_variables COMMAND tkgpu_compat_var_tests)
add_executable(tkgpu_compat_expr_tests tests/abi/TKGPUCompatExprTests.C)
target_link_libraries(tkgpu_compat_expr_tests PRIVATE tkgpu_compat)
add_test(NAME compat_expressions COMMAND tkgpu_compat_expr_tests)
add_executable(tkgpu_compat_list_tests tests/abi/TKGPUCompatListTests.C)
target_link_libraries(tkgpu_compat_list_tests PRIVATE tkgpu_compat)
add_test(NAME compat_lists COMMAND tkgpu_compat_list_tests)
add_executable(tkgpu_utf8_tests tests/core/TKGPUUtf8Tests.C)
target_link_libraries(tkgpu_utf8_tests PRIVATE tkgpu_core)
add_test(NAME utf8 COMMAND tkgpu_utf8_tests)
add_executable(tkgpu_buffer_tests tests/core/TKGPUBufferTests.C)
target_link_libraries(tkgpu_buffer_tests PRIVATE tkgpu_core)
add_test(NAME buffer COMMAND tkgpu_buffer_tests)
add_executable(tkgpu_compat_object_tests tests/abi/TKGPUCompatObjectTests.C)
target_link_libraries(tkgpu_compat_object_tests PRIVATE tkgpu_compat)
add_test(NAME compat_objects COMMAND tkgpu_compat_object_tests)
add_executable(tkgpu_compat_result_tests tests/abi/TKGPUCompatResultTests.C)
target_link_libraries(tkgpu_compat_result_tests PRIVATE tkgpu_compat)
add_test(NAME compat_results COMMAND tkgpu_compat_result_tests)
add_executable(tkgpu_compat_byte_tests tests/abi/TKGPUCompatByteTests.C)
target_link_libraries(tkgpu_compat_byte_tests PRIVATE tkgpu_compat)
add_test(NAME compat_bytes COMMAND tkgpu_compat_byte_tests)
add_executable(tkgpu_compat_objlist_tests tests/abi/TKGPUCompatObjListTests.C)
target_link_libraries(tkgpu_compat_objlist_tests PRIVATE tkgpu_compat)
add_test(NAME compat_objlists COMMAND tkgpu_compat_objlist_tests)
add_executable(tkgpu_abi_layout_tests tests/abi/TKGPUAbiLayoutTests.C)
target_link_libraries(tkgpu_abi_layout_tests PRIVATE tkgpu_compat)
add_test(NAME abi_layout COMMAND tkgpu_abi_layout_tests)
add_executable(tkgpu_abi_layout_probe tests/abi/TKGPUAbiLayoutProbe.C)
target_include_directories(tkgpu_abi_layout_probe PRIVATE include)
add_executable(tkgpu_compat_type_tests tests/abi/types/TKGPUCompatTypeTests.C)
target_link_libraries(tkgpu_compat_type_tests PRIVATE tkgpu_compat)
add_test(NAME compat_types COMMAND tkgpu_compat_type_tests)
# An independently compiled shared module exercises the additive type API.
set_source_files_properties(tests/abi/types/TKGPUExternalTypeModule.C tests/abi/types/TKGPUExternalTypeTests.C PROPERTIES LANGUAGE C)
add_library(tkgpu_external_type_module SHARED tests/abi/types/TKGPUExternalTypeModule.C)
target_link_libraries(tkgpu_external_type_module PRIVATE tkgpu_compat)
add_executable(tkgpu_external_type_tests tests/abi/types/TKGPUExternalTypeTests.C)
target_link_libraries(tkgpu_external_type_tests PRIVATE tkgpu_external_type_module tkgpu_compat)
add_test(NAME compat_external_type_module COMMAND tkgpu_external_type_tests)
add_executable(tkgpu_timer_tests tests/events/TKGPUTimerTests.C)
target_link_libraries(tkgpu_timer_tests PRIVATE tkgpu_core)
add_test(NAME timers COMMAND tkgpu_timer_tests)
add_executable(tkgpu_compat_timer_tests tests/abi/events/TKGPUCompatTimerTests.C)
target_link_libraries(tkgpu_compat_timer_tests PRIVATE tkgpu_compat)
add_test(NAME compat_timers COMMAND tkgpu_compat_timer_tests)
add_executable(tkgpu_compat_thread_tests tests/abi/threads/TKGPUCompatThreadTests.C)
target_link_libraries(tkgpu_compat_thread_tests PRIVATE tkgpu_compat)
if(NOT WIN32)
  target_link_libraries(tkgpu_compat_thread_tests PRIVATE Threads::Threads)
endif()
add_test(NAME compat_threads COMMAND tkgpu_compat_thread_tests)
set_tests_properties(compat_threads PROPERTIES TIMEOUT 15)
add_executable(tkgpu_thread_event_tests tests/abi/events/TKGPUThreadEventTests.C)
target_link_libraries(tkgpu_thread_event_tests PRIVATE tkgpu_compat)
if(NOT WIN32)
  target_link_libraries(tkgpu_thread_event_tests PRIVATE Threads::Threads)
endif()
add_test(NAME thread_events COMMAND tkgpu_thread_event_tests)
set_tests_properties(thread_events PROPERTIES TIMEOUT 15)
add_executable(tkgpu_raster_tests tests/render/TKGPURasterTests.C)
target_link_libraries(tkgpu_raster_tests PRIVATE tkgpu_core)
add_test(NAME raster COMMAND tkgpu_raster_tests)
add_executable(tkgpu_canvas_item_tests tests/widgets/canvas/TKGPUCanvasItemTests.C)
target_link_libraries(tkgpu_canvas_item_tests PRIVATE tkgpu_core)
add_test(NAME canvas_items COMMAND tkgpu_canvas_item_tests)
add_executable(tkgpu_image_tests tests/render/TKGPUImageTests.C)
target_link_libraries(tkgpu_image_tests PRIVATE tkgpu_core)
add_test(NAME rgba_images COMMAND tkgpu_image_tests)
add_executable(tkgpu_photo_tests tests/widgets/image/TKGPUPhotoTests.C)
target_link_libraries(tkgpu_photo_tests PRIVATE tkgpu_core)
add_test(NAME photos COMMAND tkgpu_photo_tests)
add_executable(tkgpu_text_tests tests/render/TKGPUTextTests.C)
target_link_libraries(tkgpu_text_tests PRIVATE tkgpu_core)
add_test(NAME fallback_text COMMAND tkgpu_text_tests)
set_source_files_properties(tests/render/TKGPULegacyUnicodeTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_legacy_unicode_tests tests/render/TKGPULegacyUnicodeTests.C)
target_link_libraries(tkgpu_legacy_unicode_tests PRIVATE tkgpu_core)
add_test(NAME legacy_unicode COMMAND tkgpu_legacy_unicode_tests)
add_executable(tkgpu_compat_objcommand_tests tests/abi/commands/TKGPUCompatObjCommandTests.C)
target_link_libraries(tkgpu_compat_objcommand_tests PRIVATE tkgpu_compat)
add_test(NAME compat_objcommands COMMAND tkgpu_compat_objcommand_tests)
add_executable(tkgpu_compat_evalfile_tests tests/abi/interp/TKGPUCompatEvalFileTests.C)
target_link_libraries(tkgpu_compat_evalfile_tests PRIVATE tkgpu_compat)
add_test(NAME compat_evalfile COMMAND tkgpu_compat_evalfile_tests)
add_executable(tkgpu_compat_bignum_tests tests/abi/bignum/TKGPUCompatBignumTests.C)
target_link_libraries(tkgpu_compat_bignum_tests PRIVATE tkgpu_compat)
add_test(NAME compat_bignum COMMAND tkgpu_compat_bignum_tests)
add_executable(tkgpu_compat_bignum_syntax_tests tests/abi/bignum/TKGPUCompatBignumSyntaxTests.C)
target_link_libraries(tkgpu_compat_bignum_syntax_tests PRIVATE tkgpu_compat)
add_test(NAME compat_bignum_syntax COMMAND tkgpu_compat_bignum_syntax_tests)

set_source_files_properties(tests/platform/legacy/TKGPUTickExtendTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_legacy_tick_tests tests/platform/legacy/TKGPUTickExtendTests.C
  src/platform/time/legacy/TKGPUTickExtend.C)
add_test(NAME legacy_tick COMMAND tkgpu_legacy_tick_tests)

if(NOT WIN32)
  set_source_files_properties(tests/abi/events/TKGPUFileHandlerTests.C PROPERTIES LANGUAGE C)
  add_executable(tkgpu_file_handler_tests tests/abi/events/TKGPUFileHandlerTests.C)
  target_link_libraries(tkgpu_file_handler_tests PRIVATE tkgpu_compat Threads::Threads)
  add_test(NAME file_handlers COMMAND tkgpu_file_handler_tests)
  set_tests_properties(file_handlers PROPERTIES TIMEOUT 10)
endif()

set_source_files_properties(tests/core/TKGPURealExprTests.C tests/abi/numeric/TKGPUCompatExprDoubleTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_real_expr_tests tests/core/TKGPURealExprTests.C)
target_link_libraries(tkgpu_real_expr_tests PRIVATE tkgpu_core)
add_test(NAME real_expressions COMMAND tkgpu_real_expr_tests)
add_executable(tkgpu_compat_expr_double_tests tests/abi/numeric/TKGPUCompatExprDoubleTests.C)
target_link_libraries(tkgpu_compat_expr_double_tests PRIVATE tkgpu_compat)
add_test(NAME compat_expr_double COMMAND tkgpu_compat_expr_double_tests)

set_source_files_properties(tests/core/TKGPUHistoryTests.C tests/abi/interp/TKGPUCompatInitTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_history_tests tests/core/TKGPUHistoryTests.C)
target_link_libraries(tkgpu_history_tests PRIVATE tkgpu_core)
add_test(NAME history COMMAND tkgpu_history_tests)
add_executable(tkgpu_compat_init_tests tests/abi/interp/TKGPUCompatInitTests.C)
target_link_libraries(tkgpu_compat_init_tests PRIVATE tkgpu_compat)
add_test(NAME compat_init COMMAND tkgpu_compat_init_tests)

set_source_files_properties(tests/abi/tk/TKGPUCompatTkTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_compat_tk_tests tests/abi/tk/TKGPUCompatTkTests.C)
target_link_libraries(tkgpu_compat_tk_tests PRIVATE tkgpu_compat)
if(APPLE AND TKGPU_COCOA)
  target_compile_definitions(tkgpu_compat_tk_tests PRIVATE TKGPU_COCOA)
endif()
add_test(NAME compat_tk COMMAND tkgpu_compat_tk_tests)
set_tests_properties(compat_tk PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 15)

set_source_files_properties(tests/render/TKGPUParallelPaintTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_parallel_paint_tests tests/render/TKGPUParallelPaintTests.C)
target_link_libraries(tkgpu_parallel_paint_tests PRIVATE tkgpu_core)
add_test(NAME parallel_paint COMMAND tkgpu_parallel_paint_tests)
set_tests_properties(parallel_paint PROPERTIES TIMEOUT 20)

set_source_files_properties(tests/render/TKGPUAsyncTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_async_tests tests/render/TKGPUAsyncTests.C)
target_link_libraries(tkgpu_async_tests PRIVATE tkgpu_core)
add_test(NAME async_render COMMAND tkgpu_async_tests)
set_tests_properties(async_render PROPERTIES TIMEOUT 30)

if(UNIX AND NOT APPLE AND TKGPU_X11 AND X11_FOUND)
  set_source_files_properties(tests/platform/TKGPUX11AsyncTests.C PROPERTIES LANGUAGE C)
  add_executable(tkgpu_x11_async_tests tests/platform/TKGPUX11AsyncTests.C
    src/compat/tk/TKGPUWindowScene.C src/platform/window/x11/TKGPUX11Host.C)
  target_link_libraries(tkgpu_x11_async_tests PRIVATE tkgpu_core X11::X11)
  find_program(TKGPU_XVFB_RUN xvfb-run)
  if(TKGPU_XVFB_RUN)
    add_test(NAME x11_async COMMAND "${TKGPU_XVFB_RUN}" -a $<TARGET_FILE:tkgpu_x11_async_tests>)
    set_tests_properties(x11_async PROPERTIES TIMEOUT 30 SKIP_RETURN_CODE 77)
  endif()
endif()

set_source_files_properties(tests/widgets/TKGPUControlInputTests.C tests/core/TKGPUWidgetInvokeTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_control_input_tests tests/widgets/TKGPUControlInputTests.C)
target_link_libraries(tkgpu_control_input_tests PRIVATE tkgpu_core)
add_test(NAME control_input COMMAND tkgpu_control_input_tests)
add_executable(tkgpu_widget_invoke_tests tests/core/TKGPUWidgetInvokeTests.C)
target_link_libraries(tkgpu_widget_invoke_tests PRIVATE tkgpu_core)
add_test(NAME widget_invoke COMMAND tkgpu_widget_invoke_tests)

set_source_files_properties(tests/widgets/TKGPUControlCommandsTests.C tests/conformance/TKGPUControlDiff.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_control_commands_tests tests/widgets/TKGPUControlCommandsTests.C)
target_link_libraries(tkgpu_control_commands_tests PRIVATE tkgpu_core)
add_test(NAME control_commands COMMAND tkgpu_control_commands_tests)
add_executable(tkgpu_control_diff tests/conformance/TKGPUControlDiff.C)
target_link_libraries(tkgpu_control_diff PRIVATE tkgpu_core)
if(UNIX AND NOT APPLE)
  find_program(TKGPU_REF_TCLSH tclsh8.6)
  find_program(TKGPU_REF_XVFB xvfb-run)
  if(TKGPU_REF_TCLSH AND TKGPU_REF_XVFB)
    add_test(NAME reference_control_diff COMMAND ${CMAKE_COMMAND}
      -DPREVIEW=$<TARGET_FILE:tkgpu_control_diff>
      -DXVFB=${TKGPU_REF_XVFB} -DTCLSH=${TKGPU_REF_TCLSH}
      -DFIXTURE=${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/TKGPUControlDiff.tcl
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/conformance/TKGPUCompareControls.cmake)
    set_tests_properties(reference_control_diff PROPERTIES TIMEOUT 35)
  endif()
endif()

set_source_files_properties(tests/abi/events/TKGPUAfterTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_after_tests tests/abi/events/TKGPUAfterTests.C)
target_link_libraries(tkgpu_after_tests PRIVATE tkgpu_compat)
add_test(NAME after_update COMMAND tkgpu_after_tests)
set_tests_properties(after_update PROPERTIES TIMEOUT 20)

set_source_files_properties(tests/widgets/TKGPUStyleTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_style_tests tests/widgets/TKGPUStyleTests.C)
target_link_libraries(tkgpu_style_tests PRIVATE tkgpu_core)
add_test(NAME styles COMMAND tkgpu_style_tests)
if(UNIX)
  set_source_files_properties(tests/performance/TKGPUSceneBenchmark.C PROPERTIES LANGUAGE C)
  add_executable(tkgpu_scene_benchmark tests/performance/TKGPUSceneBenchmark.C)
  target_link_libraries(tkgpu_scene_benchmark PRIVATE tkgpu_core)
endif()

set_source_files_properties(tests/widgets/TKGPUIndicatorsTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_indicators_tests tests/widgets/TKGPUIndicatorsTests.C)
target_link_libraries(tkgpu_indicators_tests PRIVATE tkgpu_core)
add_test(NAME ttk_indicators COMMAND tkgpu_indicators_tests)

set_source_files_properties(tests/widgets/TKGPUExtraTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_extra_tests tests/widgets/TKGPUExtraTests.C)
target_link_libraries(tkgpu_extra_tests PRIVATE tkgpu_core)
add_test(NAME ttk_extra COMMAND tkgpu_extra_tests)

set_source_files_properties(tests/widgets/TKGPUChoiceTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_choice_tests tests/widgets/TKGPUChoiceTests.C)
target_link_libraries(tkgpu_choice_tests PRIVATE tkgpu_core)
add_test(NAME ttk_choices COMMAND tkgpu_choice_tests)

set_source_files_properties(tests/widgets/TKGPUNotebookTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_notebook_tests tests/widgets/TKGPUNotebookTests.C)
target_link_libraries(tkgpu_notebook_tests PRIVATE tkgpu_core)
add_test(NAME ttk_notebook COMMAND tkgpu_notebook_tests)

set_source_files_properties(tests/widgets/TKGPUTreeTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_tree_tests tests/widgets/TKGPUTreeTests.C)
target_link_libraries(tkgpu_tree_tests PRIVATE tkgpu_core)
add_test(NAME ttk_treeview COMMAND tkgpu_tree_tests)

set_source_files_properties(tests/widgets/TKGPUToggleTests.C tests/core/TKGPUToggleCoreTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_toggle_tests tests/widgets/TKGPUToggleTests.C)
target_link_libraries(tkgpu_toggle_tests PRIVATE tkgpu_core)
add_test(NAME ttk_toggles COMMAND tkgpu_toggle_tests)
add_executable(tkgpu_toggle_core_tests tests/core/TKGPUToggleCoreTests.C)
target_link_libraries(tkgpu_toggle_core_tests PRIVATE tkgpu_core)
add_test(NAME ttk_toggle_core COMMAND tkgpu_toggle_core_tests)

set_source_files_properties(tests/widgets/TKGPUContainersTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_containers_tests tests/widgets/TKGPUContainersTests.C)
target_link_libraries(tkgpu_containers_tests PRIVATE tkgpu_core)
add_test(NAME ttk_containers COMMAND tkgpu_containers_tests)

set_source_files_properties(tests/widgets/TKGPUListboxTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_listbox_tests tests/widgets/TKGPUListboxTests.C)
target_link_libraries(tkgpu_listbox_tests PRIVATE tkgpu_core)
add_test(NAME listbox COMMAND tkgpu_listbox_tests)

# Explicit development-only CPython bridge. Never installed or enabled by default.
if(TKGPU_LEGACY_WINDOWS)
  set(TKGPU_LEGACY_REFERENCE_EXTENSION "" CACHE FILEPATH "Audited target CPython _tkinter binary (Tcl/Tk 8.6 ABI)")
  if(TKGPU_LEGACY_REFERENCE_EXTENSION)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(TKGPU_CPYTHON_STAGE "${CMAKE_BINARY_DIR}/kerbot-tkgpu-candidate" CACHE PATH "Legacy staging base directory")
    add_custom_target(tkgpu_kerbot_candidate
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/TKGPUStageCPython.py"
        --target windows --windows-profile "${TKGPU_LEGACY_WINDOWS_TARGET}"
        --extension "${TKGPU_LEGACY_REFERENCE_EXTENSION}"
        --library "$<TARGET_FILE:tkgpu_compat>" --output "${TKGPU_CPYTHON_STAGE}"
      DEPENDS tkgpu_compat VERBATIM)
  endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    set(TKGPU_CPYTHON_STAGE "${CMAKE_BINARY_DIR}/kerbot-tkgpu-candidate" CACHE PATH "New directory for experimental CPython runtime candidate")
    add_custom_target(tkgpu_kerbot_candidate
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/TKGPUStageCPython.py"
        --library "$<TARGET_FILE:tkgpu_compat>" --output "${TKGPU_CPYTHON_STAGE}"
      DEPENDS tkgpu_compat VERBATIM)
  endif()
endif()

set_source_files_properties(tests/widgets/TKGPUTextWidgetTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_text_widget_tests tests/widgets/TKGPUTextWidgetTests.C)
target_link_libraries(tkgpu_text_widget_tests PRIVATE tkgpu_core)
add_test(NAME text_widget COMMAND tkgpu_text_widget_tests)

set_source_files_properties(tests/events/TKGPUBindingTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_binding_tests tests/events/TKGPUBindingTests.C)
target_link_libraries(tkgpu_binding_tests PRIVATE tkgpu_core)
add_test(NAME bindings COMMAND tkgpu_binding_tests)

set_source_files_properties(tests/widgets/TKGPUTextVariableTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_textvariable_tests tests/widgets/TKGPUTextVariableTests.C)
target_link_libraries(tkgpu_textvariable_tests PRIVATE tkgpu_core)
add_test(NAME textvariable COMMAND tkgpu_textvariable_tests)

# Qualify the direct host independently of any staged Tcl/Tk binary.
if(Python3_Interpreter_FOUND AND NOT CMAKE_CROSSCOMPILING AND NOT TKGPU_LEGACY_WINDOWS AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_custom_target(tkgpu_host_qualify
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/TKGPUQualifyHost.py"
      --build "${CMAKE_BINARY_DIR}" --bundle "${TKGPU_HOST_BUNDLE}" --config "$<CONFIG>" --require-pillow
    DEPENDS tkgpu_host_bundle tkgpu_host_tests VERBATIM USES_TERMINAL)
endif()

# Native target evidence must be executable on the target VM, not just built.
if(WIN32)
  find_package(Git QUIET)
  set(TKGPU_EVIDENCE_REVISION unknown)
  if(Git_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE TKGPU_EVIDENCE_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET RESULT_VARIABLE TKGPU_EVIDENCE_GIT_STATUS)
    if(NOT TKGPU_EVIDENCE_GIT_STATUS EQUAL 0)
      set(TKGPU_EVIDENCE_REVISION unknown)
    endif()
  endif()
  set_source_files_properties(tests/platform/TKGPUWindowsEvidence.C PROPERTIES LANGUAGE C)
  add_executable(tkgpu_windows_evidence tests/platform/TKGPUWindowsEvidence.C)
  target_link_libraries(tkgpu_windows_evidence PRIVATE tkgpu_compat)
  target_compile_definitions(tkgpu_windows_evidence PRIVATE TKGPU_SOURCE_REVISION="${TKGPU_EVIDENCE_REVISION}")
  if(TKGPU_LEGACY_WINDOWS)
    target_compile_definitions(tkgpu_windows_evidence PRIVATE TKGPU_WINDOWS_PROFILE="${TKGPU_LEGACY_WINDOWS_TARGET}")
    tkgpu_apply_legacy_profile(tkgpu_windows_evidence)
  endif()
endif()

set_source_files_properties(tests/abi/interp/TKGPUCompatLimitsTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_compat_limits_tests tests/abi/interp/TKGPUCompatLimitsTests.C)
target_link_libraries(tkgpu_compat_limits_tests PRIVATE tkgpu_compat)
add_test(NAME compat_resource_limits COMMAND tkgpu_compat_limits_tests)

set_source_files_properties(tests/abi/interp/TKGPUCompatResultOwnershipTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_compat_result_ownership_tests tests/abi/interp/TKGPUCompatResultOwnershipTests.C)
target_link_libraries(tkgpu_compat_result_ownership_tests PRIVATE tkgpu_compat)
add_test(NAME compat_result_ownership COMMAND tkgpu_compat_result_ownership_tests)

set_source_files_properties(tests/events/TKGPUNotifierApiTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_notifier_api_tests tests/events/TKGPUNotifierApiTests.C)
target_link_libraries(tkgpu_notifier_api_tests PRIVATE tkgpu_compat)
add_test(NAME notifier_api COMMAND tkgpu_notifier_api_tests)

set_source_files_properties(tests/abi/loader/TKGPUPluginFixture.C PROPERTIES LANGUAGE C)
add_library(tkgpu_plugin_fixture SHARED tests/abi/loader/TKGPUPluginFixture.C)

if(CMAKE_DL_LIBS)
  target_link_libraries(tkgpu_compat PRIVATE ${CMAKE_DL_LIBS})
endif()

set_source_files_properties(tests/abi/loader/TKGPUPluginLoadTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_plugin_load_tests tests/abi/loader/TKGPUPluginLoadTests.C)
target_link_libraries(tkgpu_plugin_load_tests PRIVATE tkgpu_compat)
add_dependencies(tkgpu_plugin_load_tests tkgpu_plugin_fixture)
add_test(NAME compat_plugin_load COMMAND tkgpu_plugin_load_tests $<TARGET_FILE:tkgpu_plugin_fixture>)

set_source_files_properties(tests/events/TKGPUDeleteEventsTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_delete_events_tests tests/events/TKGPUDeleteEventsTests.C)
target_link_libraries(tkgpu_delete_events_tests PRIVATE tkgpu_compat)
add_test(NAME delete_events COMMAND tkgpu_delete_events_tests)

set_source_files_properties(tests/abi/interp/TKGPUCompatStubsBatchTests.C PROPERTIES LANGUAGE C)
add_executable(tkgpu_compat_stubs_batch_tests tests/abi/interp/TKGPUCompatStubsBatchTests.C)
target_link_libraries(tkgpu_compat_stubs_batch_tests PRIVATE tkgpu_compat)
add_test(NAME compat_stubs_batch COMMAND tkgpu_compat_stubs_batch_tests)

if(TKGPU_REFERENCE_STUBS)
  foreach(case IN ITEMS limits result_ownership notifier delete_events plugin_load stubs_batch)
    if(case STREQUAL "limits")
      set(case_source tests/abi/interp/TKGPUCompatLimitsTests.C)
    elseif(case STREQUAL "result_ownership")
      set(case_source tests/abi/interp/TKGPUCompatResultOwnershipTests.C)
    elseif(case STREQUAL "notifier")
      set(case_source tests/events/TKGPUNotifierApiTests.C)
    elseif(case STREQUAL "delete_events")
      set(case_source tests/events/TKGPUDeleteEventsTests.C)
    elseif(case STREQUAL "stubs_batch")
      set(case_source tests/abi/interp/TKGPUCompatStubsBatchTests.C)
    else()
      set(case_source tests/abi/loader/TKGPUPluginLoadTests.C)
    endif()
    add_executable(tkgpu_reference_${case}_api_tests ${case_source})
    target_compile_definitions(tkgpu_reference_${case}_api_tests PRIVATE TKGPU_TEST_REFERENCE_STUBS)
    target_include_directories(tkgpu_reference_${case}_api_tests PRIVATE "${TKGPU_TCL86_INCLUDE}")
    target_link_libraries(tkgpu_reference_${case}_api_tests PRIVATE tkgpu_compat "${TKGPU_TCL86_STUB}")
    if(case STREQUAL "plugin_load")
      add_dependencies(tkgpu_reference_${case}_api_tests tkgpu_plugin_fixture)
      add_test(NAME reference_${case}_api COMMAND tkgpu_reference_${case}_api_tests $<TARGET_FILE:tkgpu_plugin_fixture>)
    else()
      add_test(NAME reference_${case}_api COMMAND tkgpu_reference_${case}_api_tests)
    endif()
  endforeach()
endif()
