cmake_minimum_required(VERSION 3.10)
project(collector LANGUAGES C CXX)

# Deal with dependencies
include(FetchContent)
FetchContent_Declare(
  hello_imgui
  URL        https://github.com/pthom/hello_imgui/archive/3431b2092d4d2098a334581749ebb7815ab4f5d9.zip
  URL_HASH   SHA1=89c50148b35d645fe652d0d64f3c4ab3cd478772
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/hello_imgui
  BINARY_DIR ${PROJECT_BINARY_DIR}/external/hello_imgui
)
FetchContent_Declare(
  imgui
  URL        https://github.com/ocornut/imgui/archive/9964740a47fda96ee937cfea272ccac85dc6a500.zip
  URL_HASH   SHA1=b6262092d64302041487927483613f45ba981a47
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/hello_imgui/external/imgui
  BINARY_DIR ${PROJECT_BINARY_DIR}/external/hello_imgui/external/imgui
)
FetchContent_Declare(
  implot
  URL        https://github.com/epezent/implot/archive/6978a3e1776821f628c08f4b59d439f0828920dc.zip
  URL_HASH   SHA1=793eb90386db8afbc4c655ddbea526c871aaae88
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/implot
  BINARY_DIR ${PROJECT_BINARY_DIR}/external/implot
)
FetchContent_Declare(
  ImGuiColorTextEdit
  URL        https://github.com/pthom/ImGuiColorTextEdit/archive/3c2f3c5b0b6ed2e5f30c15e48a011720a24d2c8f.zip
  URL_HASH   SHA1=76349984c4ddab8318cc0357249c35d65cb64dd3
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/ImGuiColorTextEdit
  BINARY_DIR ${PROJECT_BINARY_DIR}/external/ImGuiColorTextEdit
)
FetchContent_Declare(
  FunctionalPlus
  URL        https://github.com/Dobiasd/FunctionalPlus/archive/aab4a40948c8b5ca088a914dceb4d0020f925498.zip
  URL_HASH   SHA1=e765ad3d6fe75a4b299a0b99d373f12271c71dfc
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/FunctionalPlus
  BINARY_DIR ${PROJECT_BINARY_DIR}/external/FunctionalPlus
)
FetchContent_Declare(
  imgui_markdown
  URL        https://github.com/juliettef/imgui_markdown/archive/faa4561d74fd4a725427fcb619c96f3da97b064a.zip
  URL_HASH   SHA1=c2daa209f5f21776c056e287e670d8bee7240825
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/imgui_markdown
  BINARY_DIR ${PROJECT_BINARY_DIR}/external/imgui_markdown
)
FetchContent_Declare(
  json-c
  URL        https://github.com/json-c/json-c/archive/79c147203e2f90806fba0487fc26ab5e649c8755.zip
  URL_HASH   SHA1=b7d6e0ff2cd65c75b3dd720bac0e0b75f409ec64
  SOURCE_DIR ${PROJECT_SOURCE_DIR}/external/json-c
  BINARY_DIR ${PROJECT_BINARY_DIR}/external/json-c
)
# The goal is not to download everything again just becasue you need to
# re-import the CMake project into an IDE or something.
# When something goes south, manually deleting the ./external directory is a good move.
FetchContent_GetProperties(hello_imgui)
if (NOT hello_imgui_POPULATED AND NOT EXISTS "${PROJECT_SOURCE_DIR}/external/hello_imgui")
  FetchContent_Populate(hello_imgui)
endif()
FetchContent_GetProperties(imgui)
if (NOT imgui_POPULATED AND NOT EXISTS "${PROJECT_SOURCE_DIR}/external/imgui")
  FetchContent_Populate(imgui)
endif()
FetchContent_GetProperties(implot)
if (NOT implot_POPULATED AND NOT EXISTS "${PROJECT_SOURCE_DIR}/external/implot")
  FetchContent_Populate(implot)
endif()
FetchContent_GetProperties(ImGuiColorTextEdit)
if (NOT ImGuiColorTextEdit_POPULATED AND NOT EXISTS "${PROJECT_SOURCE_DIR}/external/ImGuiColorTextEdit")
  FetchContent_Populate(ImGuiColorTextEdit)
endif()
FetchContent_GetProperties(FunctionalPlus)
if (NOT FunctionalPlus_POPULATED AND NOT EXISTS "${PROJECT_SOURCE_DIR}/external/FunctionalPlus")
  FetchContent_Populate(FunctionalPlus)
endif()
FetchContent_GetProperties(imgui_markdown)
if (NOT imgui_markdown_POPULATED AND NOT EXISTS "${PROJECT_SOURCE_DIR}/external/imgui_markdown")
  FetchContent_Populate(imgui_markdown)
endif()
FetchContent_GetProperties(json-c)
if (NOT json-c_POPULATED AND NOT EXISTS "${PROJECT_SOURCE_DIR}/external/json-c")
  FetchContent_Populate(json-c)
endif()

set(CMAKE_CXX_STANDARD 14)

# Debugging in Firefox/Chrome, including sources, still some hiccups
set(CMAKE_BUILD_TYPE DEBUG)
set(EMCC_DEBUG 1)
#set(CMAKE_C_FLAGS_DEBUG -g -O0 -gsource-map --source-map-base http://localhost:7777/)
#set(CMAKE_CXX_FLAGS_DEBUG -g -O0 -gsource-map --source-map-base http://localhost:7777/)
add_link_options("SHELL:-gsource-map")
add_link_options("SHELL:--source-map-base http://localhost:7777/")

# ImPlot library: Force ImDrawIdx=unsigned int (performance option for ImPlot)
add_compile_definitions("ImDrawIdx=unsigned int")

# Use https://emscripten.org/docs/api_reference/fetch.html
add_link_options("SHELL:-s FETCH=1")

# HelloImgui framework settings
# Select rendering backend:
set(HELLOIMGUI_USE_SDL_OPENGL3 ON CACHE BOOL "" FORCE)
# Assets, e.g. images, that are baked into the binary itself (i.e. NOT fetched as XHR resources)
set(local_assets_folder ${PROJECT_SOURCE_DIR}/assets)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/hello_imgui)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${HELLOIMGUI_BASEPATH}/hello_imgui_cmake)
include(${HELLOIMGUI_BASEPATH}/hello_imgui_cmake/emscripten/hello_imgui_emscripten_global_options.cmake)
include(hello_imgui_add_app)

# ImPlot library, charts widgets
set(implot_dir ${CMAKE_CURRENT_LIST_DIR}/external/implot)
# Text editor widgets and deps
set(textedit_dir ${CMAKE_CURRENT_LIST_DIR}/external/ImGuiColorTextEdit)
set(fplus_dir ${CMAKE_CURRENT_LIST_DIR}/external/FunctionalPlus/include)
set(imgui_markdown_dir ${CMAKE_CURRENT_LIST_DIR}/external/imgui_markdown)

# Stop JSON C from trying to build shared libs
unset(BUILD_SHARED_LIBS)
add_subdirectory(${PROJECT_SOURCE_DIR}/external/json-c)
# We need to to run json-c's cmake first to get headers processed.
set(json_c_processed_dir ${CMAKE_CURRENT_BINARY_DIR}/external/json-c)
set(json_c_dir ${CMAKE_CURRENT_LIST_DIR}/external/json-c)

hello_imgui_add_app(collector-web
    src/Collector.main.cpp
    src/ImGuiExt.cpp
    src/ImGuiExt.h
    src/MarkdownHelper.cpp
    src/MarkdownHelper.h
    src/HyperlinkHelper.cpp
    src/HyperlinkHelper.h
    ${implot_dir}/implot.h
    ${implot_dir}/implot_internal.h
    ${implot_dir}/implot.cpp
    ${implot_dir}/implot_items.cpp
    ${json_c_processed_dir}/json.h
    )
target_include_directories(collector-web PRIVATE
    ${implot_dir}
    ${textedit_dir}
    ${fplus_dir}
    ${imgui_markdown_dir}
    ${json_c_dir}
    ${json_c_processed_dir}
)

target_link_libraries(collector-web PRIVATE json-c)

# Test data, TODO: Remove...
file(COPY ${CMAKE_CURRENT_LIST_DIR}/data/perf_data.json
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_LIST_DIR}/data/favicon.ico
     DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
