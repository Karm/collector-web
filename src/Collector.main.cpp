#include "ImGuiExt.h"
#include "MarkdownHelper.h"
#include "hello_imgui/hello_imgui.h"
#include "implot.h"
#include "implot_internal.h"
#include <chrono>
#include <cstdarg>
#include <emscripten/fetch.h>
#include <json.h>

#define MAX_RECORDS_ROW 1024
#define MAX_LINES_CHART 5
#define MAX_LABEL_LENGTH 256
#define MAX_MARGIN(x) (x + x / 3)

struct Downloads {
    float download_progress_bar = 0.f;
    bool download_in_progress = false;
    time_t download_timestamp{};
    bool download_error = false;
} downloads;

struct BuildTime {
    ImU64 classes_jni[MAX_RECORDS_ROW];
    ImU64 fields_jni[MAX_RECORDS_ROW];
    ImU64 methods_jni[MAX_RECORDS_ROW];
    ImU64 classes_reflection[MAX_RECORDS_ROW];
    ImU64 fields_reflection[MAX_RECORDS_ROW];
    ImU64 resources_count[MAX_RECORDS_ROW];
    ImU64 resources_bytes[MAX_RECORDS_ROW];
    ImU64 classes_reachable[MAX_RECORDS_ROW];
    ImU64 classes_total[MAX_RECORDS_ROW];
    ImU64 fields_reachable[MAX_RECORDS_ROW];
    ImU64 fields_total[MAX_RECORDS_ROW];
    ImU64 methods_reachable[MAX_RECORDS_ROW];
    ImU64 methods_total[MAX_RECORDS_ROW];
    ImU64 methods_reflection[MAX_RECORDS_ROW];
    ImU64 total_build_time_ms[MAX_RECORDS_ROW];
    ImU64 gc_total_ms[MAX_RECORDS_ROW];
    ImU64 peak_rss_bytes[MAX_RECORDS_ROW];
    ImU64 image_heap_bytes[MAX_RECORDS_ROW];
    ImU64 image_total_bytes[MAX_RECORDS_ROW];
    ImU64 code_area_bytes[MAX_RECORDS_ROW];
} buildTime, buildTimeContemporary[MAX_LINES_CHART];

struct BuildTimeLabels {
    ImU64 max_classes_jni = 0;
    ImU64 max_fields_jni = 0;
    ImU64 max_methods_jni = 0;
    ImU64 max_classes_reflection = 0;
    ImU64 max_fields_reflection = 0;
    ImU64 max_resources_count = 0;
    ImU64 max_resources_bytes = 0;
    ImU64 max_classes_reachable = 0;
    ImU64 max_classes_total = 0;
    ImU64 max_fields_reachable = 0;
    ImU64 max_fields_total = 0;
    ImU64 max_methods_reachable = 0;
    ImU64 max_methods_total = 0;
    ImU64 max_methods_reflection = 0;
    ImU64 max_total_build_time_ms = 0;
    ImU64 max_gc_total_ms = 0;
    ImU64 max_peak_rss_bytes = 0;
    ImU64 max_image_heap_bytes = 0;
    ImU64 max_image_total_bytes = 0;
    ImU64 max_code_area_bytes = 0;
    char labels[MAX_RECORDS_ROW][MAX_LABEL_LENGTH]{};
    double positions[MAX_RECORDS_ROW]{};
    ImU32 elements = 0;
} buildTimeLabels, buildTimeLabelsContemporary[MAX_LINES_CHART];

struct BuildTimeContemporaryLines {
    ImU32 elements = 0;
    char labels[MAX_RECORDS_ROW][MAX_LABEL_LENGTH]{};
} buildTimeContemporaryLines;

#define CLASSES_JNI_IDX 6
#define CLASSES_REACHABLE_IDX 7
#define CLASSES_REFLECTION_IDX 8
#define CLASSES_TOTAL_IDX 9
#define CODE_AREA_BYTES_IDX 4
#define FIELDS_JNI_IDX 10
#define FIELDS_REACHABLE_IDX 11
#define FIELDS_REFLECTION_IDX 12
#define FIELDS_TOTAL_IDX 13
#define GC_TOTAL_MS_IDX 1
#define IMAGE_HEAP_BYTES_IDX 3
#define IMAGE_TOTAL_BYTES_IDX 5
#define METHODS_JNI_IDX 14
#define METHODS_REACHABLE_IDX 15
#define METHODS_REFLECTION_IDX 16
#define METHODS_TOTAL_IDX 17
#define PEAK_RSS_BYTES_IDX 2
#define RESOURCES_BYTES_IDX 18
#define RESOURCES_COUNT_IDX 19
#define TOTAL_BUILD_TIME_MS_IDX 0

/**
 * This order dictates the order of the columns and rows in the table.
 * Mind updating the indices above if you change the order.
 */
const char *ATTRIBUTE_NAMES[] = {
    "total_build_time_ms",
    "gc_total_ms",
    "peak_rss_bytes",
    "image_heap_bytes",
    "code_area_bytes",
    "image_total_bytes",
    "classes_jni",
    "classes_reachable",
    "classes_reflection",
    "classes_total",
    "fields_jni",
    "fields_reachable",
    "fields_reflection",
    "fields_total",
    "methods_jni",
    "methods_reachable",
    "methods_reflection",
    "methods_total",
    "resources_bytes",
    "resources_count"};

inline ImU64 max(const ImU64 n, ...) {
    va_list p;
    va_start(p, n);
    ImU64 max = 0;
    for (ImU64 i = 0; i < n; i++) {
        const ImU64 tmp = va_arg(p, ImU64);
        if (tmp > max) {
            max = tmp;
        }
    }
    va_end(p);
    return max;
}

inline ImU64 min(const ImU64 n, ...) {
    va_list p;
    va_start(p, n);
    ImU64 min = UINT64_MAX;
    for (ImU64 i = 0; i < n; i++) {
        const ImU64 tmp = va_arg(p, ImU64);
        if (tmp < min) {
            min = tmp;
        }
    }
    va_end(p);
    return min;
}

static ImVec4 quarkus_red_color = ImColor(IM_COL32(255, 0, 74, 255)).Value;
static ImVec4 quarkus_blue_color = ImColor(IM_COL32(70, 149, 235, 255)).Value;
static ImVec4 quarkus_magenta_color = ImColor(IM_COL32(205, 84, 225, 255)).Value;

inline bool cursorWithinXRegion(double mouse_x) {
    return mouse_x - (int) mouse_x < 0.1 || mouse_x - (int) mouse_x > 0.9;
}

int SecondsFormatter(double value, char *buff, int size, void *data) {
    const char *unit = (const char *) data;
    if (value == 0) {
        return snprintf(buff, size, "0 %s", unit);
    }
    return snprintf(buff, size, "%4.2f %s", value / 1000, unit);
}

int BytesFormatter(double value, char *buff, int size, void *data) {
    const char *unit = (const char *) data;
    static double v[] = {1024 * 1024 * 1024, 1024 * 1024, 1024, 1};
    static const char *p[] = {"G", "M", "k", ""};
    if (value == 0) {
        return snprintf(buff, size, "0 %s", unit);
    }
    for (int i = 0; i < 4; ++i) {
        if (fabs(value) >= v[i]) {
            return snprintf(buff, size, "%4.2f %s%s", value / v[i], p[i], unit);
        }
    }
    return snprintf(buff, size, "%4.2f %s%s", value / v[3], p[3], unit);
}

void plotBuildTime(int row, int col, int attribute_index) {
    static char title[17] = {};
    sprintf(title, "## row %d col %d", row, col);
    ImU64 *values;
    double ymax;
    const char *y_label = ATTRIBUTE_NAMES[attribute_index];
    if (attribute_index == CLASSES_JNI_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_classes_jni);
        values = buildTime.classes_jni;
    } else if (attribute_index == CLASSES_REACHABLE_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_classes_reachable);
        values = buildTime.classes_reachable;
    } else if (attribute_index == CLASSES_REFLECTION_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_classes_reflection);
        values = buildTime.classes_reflection;
    } else if (attribute_index == CLASSES_TOTAL_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_classes_total);
        values = buildTime.classes_total;
    } else if (attribute_index == CODE_AREA_BYTES_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_code_area_bytes);
        values = buildTime.code_area_bytes;
    } else if (attribute_index == FIELDS_JNI_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_fields_jni);
        values = buildTime.fields_jni;
    } else if (attribute_index == FIELDS_REACHABLE_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_fields_reachable);
        values = buildTime.fields_reachable;
    } else if (attribute_index == FIELDS_REFLECTION_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_fields_reflection);
        values = buildTime.fields_reflection;
    } else if (attribute_index == FIELDS_TOTAL_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_fields_total);
        values = buildTime.fields_total;
    } else if (attribute_index == GC_TOTAL_MS_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_gc_total_ms);
        values = buildTime.gc_total_ms;
    } else if (attribute_index == IMAGE_HEAP_BYTES_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_image_heap_bytes);
        values = buildTime.image_heap_bytes;
    } else if (attribute_index == IMAGE_TOTAL_BYTES_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_image_total_bytes);
        values = buildTime.image_total_bytes;
    } else if (attribute_index == METHODS_JNI_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_methods_jni);
        values = buildTime.methods_jni;
    } else if (attribute_index == METHODS_REACHABLE_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_methods_reachable);
        values = buildTime.methods_reachable;
    } else if (attribute_index == METHODS_REFLECTION_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_methods_reflection);
        values = buildTime.methods_reflection;
    } else if (attribute_index == METHODS_TOTAL_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_methods_total);
        values = buildTime.methods_total;
    } else if (attribute_index == PEAK_RSS_BYTES_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_peak_rss_bytes);
        values = buildTime.peak_rss_bytes;
    } else if (attribute_index == RESOURCES_BYTES_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_resources_bytes);
        values = buildTime.resources_bytes;
    } else if (attribute_index == RESOURCES_COUNT_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_resources_count);
        values = buildTime.resources_count;
    } else if (attribute_index == TOTAL_BUILD_TIME_MS_IDX) {
        ymax = MAX_MARGIN(buildTimeLabels.max_total_build_time_ms);
        values = buildTime.total_build_time_ms;
    } else {
        //TODO: Error, inform user...
        printf("Error, index unknown.\n");
    }
    static ImPlotRect lims(0, 12, 0, 1);
    if (ImPlot::BeginAlignedPlots("AlignedGroup", true)) {
        if (ImPlot::BeginPlot(title)) {
            ImPlot::SetupAxes("Builder images", "##y-axis");
            if (attribute_index == CODE_AREA_BYTES_IDX || attribute_index == IMAGE_HEAP_BYTES_IDX ||
                attribute_index == IMAGE_TOTAL_BYTES_IDX || attribute_index == PEAK_RSS_BYTES_IDX ||
                attribute_index == RESOURCES_BYTES_IDX) {
                ImPlot::SetupAxisFormat(ImAxis_Y1, BytesFormatter, (void *) "B");
            } else if (attribute_index == TOTAL_BUILD_TIME_MS_IDX || attribute_index == GC_TOTAL_MS_IDX) {
                ImPlot::SetupAxisFormat(ImAxis_Y1, SecondsFormatter, (void *) "s");
            }
            ImPlot::SetupAxisLinks(ImAxis_X1, &lims.X.Min, &lims.X.Max);
            ImPlot::SetupAxesLimits(-0.5f, lims.X.Max, 0, ymax);
            ImPlot::SetupAxisTicks(ImAxis_X1, buildTimeLabels.positions, (int) buildTimeLabels.elements);
            ImPlot::SetNextLineStyle(quarkus_magenta_color, 1);
            ImPlot::PlotLine(y_label, values, (int) buildTimeLabels.elements);
            ImDrawList *draw_list = ImPlot::GetPlotDrawList();
            if (ImPlot::IsPlotHovered()) {
                ImPlotPoint mouse = ImPlot::GetPlotMousePos();
                const float label_rect_left = ImPlot::PlotToPixels(mouse.x - 0.12, mouse.y).x;
                const float label_rect_right = ImPlot::PlotToPixels(mouse.x + 0.12, mouse.y).x;
                const float label_rect_top = ImPlot::GetPlotPos().y;
                const float label_rect_bottom = label_rect_top + ImPlot::GetPlotSize().y;
                int round_mouse_x = (int) round(mouse.x);
                if (round_mouse_x < 0) {
                    round_mouse_x = 0;
                } else if (round_mouse_x >= buildTimeLabels.elements) {
                    round_mouse_x = (int) buildTimeLabels.elements - 1;
                }
                if (cursorWithinXRegion(mouse.x)) {
                    ImPlot::PushPlotClipRect();
                    draw_list->AddRectFilled(
                        ImVec2(label_rect_left, label_rect_top), ImVec2(label_rect_right, label_rect_bottom),
                        IM_COL32(70, 149, 235, 100));
                    ImPlot::PopPlotClipRect();
                }
                if (cursorWithinXRegion(mouse.x)) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s", buildTimeLabels.labels[round_mouse_x]);
                    ImGui::EndTooltip();
                }
            }
            ImPlot::EndPlot();
        }
        ImPlot::EndAlignedPlots();
    }
}

void BuildTimeContemporary_Plots() {
    if (buildTimeLabels.elements > 0) {
        static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                       ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable;
        if (ImGui::BeginTable("##table 3", 3, flags, ImVec2(-1, 0))) {
            ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthStretch, 33.0f);
            ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthStretch, 33.0f);
            ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch, 33.0f);
            ImGui::TableHeadersRow();
            ImPlot::PushColormap(ImPlotColormap_Deep);
            const size_t num_attributes = *(&ATTRIBUTE_NAMES + 1) - ATTRIBUTE_NAMES;
            const ImU32 rows3 = num_attributes / 3;
            const ImU32 rowsReminder = num_attributes % 3;
            int row = 0;
            for (; row < rows3; row++) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(row);
                plotBuildTime(row, 0, row * 3);
                ImGui::PopID();
                ImGui::TableSetColumnIndex(1);
                ImGui::PushID(row);
                plotBuildTime(row, 1, row * 3 + 1);
                ImGui::PopID();
                ImGui::TableSetColumnIndex(2);
                ImGui::PushID(row);
                plotBuildTime(row, 2, row * 3 + 2);
                ImGui::PopID();
            }
            if (rowsReminder > 0) {
                ImGui::TableNextRow();
                row++;
            }
            for (int col = 0; col < rowsReminder; col++) {
                ImGui::TableSetColumnIndex(col);
                ImGui::PushID(row);
                plotBuildTime(row, col, (row - 1) * 3 + col);
                ImGui::PopID();
            }
        }
        ImPlot::PopColormap();
        ImGui::EndTable();
    }
}

void BuildTime_Plots() {
    if (buildTimeLabels.elements > 0) {
        static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                       ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable;
        if (ImGui::BeginTable("##table 3", 3, flags, ImVec2(-1, 0))) {
            ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthStretch, 33.0f);
            ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthStretch, 33.0f);
            ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch, 33.0f);
            ImGui::TableHeadersRow();
            ImPlot::PushColormap(ImPlotColormap_Deep);
            const size_t num_attributes = *(&ATTRIBUTE_NAMES + 1) - ATTRIBUTE_NAMES;
            const ImU32 rows3 = num_attributes / 3;
            const ImU32 rowsReminder = num_attributes % 3;
            int row = 0;
            for (; row < rows3; row++) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::PushID(row);
                plotBuildTime(row, 0, row * 3);
                ImGui::PopID();
                ImGui::TableSetColumnIndex(1);
                ImGui::PushID(row);
                plotBuildTime(row, 1, row * 3 + 1);
                ImGui::PopID();
                ImGui::TableSetColumnIndex(2);
                ImGui::PushID(row);
                plotBuildTime(row, 2, row * 3 + 2);
                ImGui::PopID();
            }
            if (rowsReminder > 0) {
                ImGui::TableNextRow();
                row++;
            }
            for (int col = 0; col < rowsReminder; col++) {
                ImGui::TableSetColumnIndex(col);
                ImGui::PushID(row);
                plotBuildTime(row, col, (row - 1) * 3 + col);
                ImGui::PopID();
            }
        }
        ImPlot::PopColormap();
        ImGui::EndTable();
    }
}

void processBuildtimeJSON(const char *data, int length) {
    buildTimeLabels.elements = 0;
    json_object *root;
    enum json_tokener_error jerr;
    struct json_tokener *tok = json_tokener_new();
    root = json_tokener_parse_ex(tok, data, length);
    if ((jerr = json_tokener_get_error(tok)) != json_tokener_success) {
        printf("Error: %s\n", json_tokener_error_desc(jerr));
        // TODO: There are extra chars. We should abort, inform user...
    }
    if (tok->char_offset < length) {
        printf("Error: Something else happened...\n");
        // TODO: There are extra chars. We should abort, inform user...
    }
    if (root == nullptr) {
        // TODO abort, pop up a message...
        printf("Error: The root element is null...\n");
    }

    //TODO:
    // Use hashmap from https://github.com/nothings/stb/blob/master/stb_ds.h   ?

    const json_object *tag_array = json_object_object_get(root, "tag");
    if (tag_array == nullptr) {
        printf("Error: No tag array found in the JSON file.\n");
    }
    const json_object *created_array = json_object_object_get(root, "created_at");
    if (created_array == nullptr) {
        printf("Error: No created_at array found in the JSON file.\n");
    }

    const size_t array_length = min(2, json_object_array_length(tag_array), MAX_RECORDS_ROW);
    // If I comment this printf, it crashes :-)
    printf("array_length: %zu\n", array_length);
    for (int j = 0; j < array_length; j++) {
        char *tag_ = const_cast<char *>(json_object_get_string(json_object_array_get_idx(tag_array, j)));
        const char *created_at_ = json_object_get_string(json_object_array_get_idx(created_array, j));
        char *tmp_ptr;
        char *builder_image = strtok_r(tag_, ",", &tmp_ptr);
        char *quarkus_version = strtok_r(nullptr, ",", &tmp_ptr);
        memset(buildTimeLabels.labels[j], 0, MAX_LABEL_LENGTH);
        sprintf(buildTimeLabels.labels[j], "Image: %s\nQuarkus: %s\nTimestamp: %s", builder_image, quarkus_version, created_at_);
        buildTimeLabels.positions[j] = j;// 1 element has 1 position
    }

    const size_t num_attributes = *(&ATTRIBUTE_NAMES + 1) - ATTRIBUTE_NAMES;

    // TODO: Full picture - would new builds replace the older ones?


    for (int i = 0; i < num_attributes; i++) {
        printf("Processing %s\n", ATTRIBUTE_NAMES[i]);
        const char *attribute_name = ATTRIBUTE_NAMES[i];
        const json_object *array = json_object_object_get(root, attribute_name);
        //const size_t array_length = min(2, json_object_array_length(array), MAX_RECORDS_ROW);
        if (strncmp(ATTRIBUTE_NAMES[CLASSES_JNI_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.classes_jni[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.classes_jni[j] > buildTimeLabels.max_classes_jni) {
                    buildTimeLabels.max_classes_jni = buildTime.classes_jni[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[CLASSES_REACHABLE_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.classes_reachable[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.classes_reachable[j] > buildTimeLabels.max_classes_reachable) {
                    buildTimeLabels.max_classes_reachable = buildTime.classes_reachable[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[CLASSES_REFLECTION_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.classes_reflection[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.classes_reflection[j] > buildTimeLabels.max_classes_reflection) {
                    buildTimeLabels.max_classes_reflection = buildTime.classes_reflection[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[CLASSES_TOTAL_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.classes_total[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.classes_total[j] > buildTimeLabels.max_classes_total) {
                    buildTimeLabels.max_classes_total = buildTime.classes_total[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[CODE_AREA_BYTES_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.code_area_bytes[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.code_area_bytes[j] > buildTimeLabels.max_code_area_bytes) {
                    buildTimeLabels.max_code_area_bytes = buildTime.code_area_bytes[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[FIELDS_JNI_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.fields_jni[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.fields_jni[j] > buildTimeLabels.max_fields_jni) {
                    buildTimeLabels.max_fields_jni = buildTime.fields_jni[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[FIELDS_REACHABLE_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.fields_reachable[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.fields_reachable[j] > buildTimeLabels.max_fields_reachable) {
                    buildTimeLabels.max_fields_reachable = buildTime.fields_reachable[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[FIELDS_REFLECTION_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.fields_reflection[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.fields_reflection[j] > buildTimeLabels.max_fields_reflection) {
                    buildTimeLabels.max_fields_reflection = buildTime.fields_reflection[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[FIELDS_TOTAL_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.fields_total[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.fields_total[j] > buildTimeLabels.max_fields_total) {
                    buildTimeLabels.max_fields_total = buildTime.fields_total[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[GC_TOTAL_MS_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.gc_total_ms[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.gc_total_ms[j] > buildTimeLabels.max_gc_total_ms) {
                    buildTimeLabels.max_gc_total_ms = buildTime.gc_total_ms[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[IMAGE_HEAP_BYTES_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.image_heap_bytes[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.image_heap_bytes[j] > buildTimeLabels.max_image_heap_bytes) {
                    buildTimeLabels.max_image_heap_bytes = buildTime.image_heap_bytes[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[IMAGE_TOTAL_BYTES_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.image_total_bytes[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.image_total_bytes[j] > buildTimeLabels.max_image_total_bytes) {
                    buildTimeLabels.max_image_total_bytes = buildTime.image_total_bytes[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[METHODS_JNI_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.methods_jni[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.methods_jni[j] > buildTimeLabels.max_methods_jni) {
                    buildTimeLabels.max_methods_jni = buildTime.methods_jni[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[METHODS_REACHABLE_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.methods_reachable[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.methods_reachable[j] > buildTimeLabels.max_methods_reachable) {
                    buildTimeLabels.max_methods_reachable = buildTime.methods_reachable[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[METHODS_REFLECTION_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.methods_reflection[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.methods_reflection[j] > buildTimeLabels.max_methods_reflection) {
                    buildTimeLabels.max_methods_reflection = buildTime.methods_reflection[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[METHODS_TOTAL_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.methods_total[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.methods_total[j] > buildTimeLabels.max_methods_total) {
                    buildTimeLabels.max_methods_total = buildTime.methods_total[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[PEAK_RSS_BYTES_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.peak_rss_bytes[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.peak_rss_bytes[j] > buildTimeLabels.max_peak_rss_bytes) {
                    buildTimeLabels.max_peak_rss_bytes = buildTime.peak_rss_bytes[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[RESOURCES_BYTES_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.resources_bytes[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.resources_bytes[j] > buildTimeLabels.max_resources_bytes) {
                    buildTimeLabels.max_resources_bytes = buildTime.resources_bytes[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[RESOURCES_COUNT_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.resources_count[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.resources_count[j] > buildTimeLabels.max_resources_count) {
                    buildTimeLabels.max_resources_count = buildTime.resources_count[j];
                }
            }
        } else if (strncmp(ATTRIBUTE_NAMES[TOTAL_BUILD_TIME_MS_IDX], attribute_name, strlen(attribute_name)) == 0) {
            for (int j = 0; j < array_length; j++) {
                buildTime.total_build_time_ms[j] = json_object_get_uint64(json_object_array_get_idx(array, j));
                if (buildTime.total_build_time_ms[j] > buildTimeLabels.max_total_build_time_ms) {
                    buildTimeLabels.max_total_build_time_ms = buildTime.total_build_time_ms[j];
                }
            }
        }
    }



    json_tokener_free(tok);


    buildTimeLabels.elements = array_length;
}

void downloadBuildtimeSucceeded(emscripten_fetch_t *fetch) {
    processBuildtimeJSON(fetch->data, fetch->numBytes);
    emscripten_fetch_close(fetch);// Free data associated with the fetch.
    downloads.download_in_progress = false;
    downloads.download_progress_bar = 0;
    downloads.download_error = false;
    downloads.download_timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

void downloadProgress(emscripten_fetch_t *fetch) {
    downloads.download_in_progress = true;
    downloads.download_error = false;
    if (fetch->totalBytes) {
        //printf("Downloading %s.. %.2f%% complete.\n", fetch->url, fetch->dataOffset * 100.0 / fetch->totalBytes);
        downloads.download_progress_bar = (float) fetch->dataOffset / (float) fetch->totalBytes;
    } else {
        // We don't know, so Windows 95 copy files progress bar...
        if (downloads.download_progress_bar >= 1) {
            downloads.download_progress_bar = 0;
        }
        downloads.download_progress_bar += .01f;
        //printf("Downloading %s.. %lld bytes complete.\n", fetch->url, fetch->dataOffset + fetch->numBytes);
    }
}

void downloadFailed(emscripten_fetch_t *fetch) {
    printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);
    emscripten_fetch_close(fetch);// Also free data on failure.
    downloads.download_error = true;
    downloads.download_progress_bar = 0;
    downloads.download_in_progress = false;
    downloads.download_timestamp = NULL;
}

//void CommandGui(AppState &state) {
void CommandGui() {
    HelloImGui::ImageFromAsset("quarkus.png");
    ImGui::SameLine();
    ImGui::TextWrapped("Quarkus Mandrel\nStats, charts, operations");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Quarkus Mandrel perf charts demo...");
    }
    ImGui::Separator();
    if (ImGui::Button("Download buildtime dataset")) {
        if (!downloads.download_in_progress) {
            emscripten_fetch_attr_t attr;
            emscripten_fetch_attr_init(&attr);
            strcpy(attr.requestMethod, "GET");
            attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
            attr.onsuccess = downloadBuildtimeSucceeded;
            attr.onerror = downloadFailed;
            attr.onprogress = downloadProgress;
            emscripten_fetch(&attr, "build_perf_data.json");
        }
    }
    if (ImGui::IsItemHovered() && downloads.download_in_progress) {
        ImGui::SetTooltip("Download is already in progress...");
    }
    if (buildTimeLabels.elements > 0 && !downloads.download_in_progress) {
        static bool show_legend = false;
        ImGui::Checkbox("List the whole legend", &show_legend);
        if (show_legend) {
            for (int i = 0; i < buildTimeLabels.elements; i++) {
                ImGui::TextWrapped("%d%s - %s", i, (i < 10 ? " " : ""), buildTimeLabels.labels[i]);
            }
        }
    }
}

void StatusBarGui() {
    ImGui::Text("Dataset status:");
    ImGui::SameLine();
    if (downloads.download_in_progress) {
        ImGui::ProgressBar(downloads.download_progress_bar, ImVec2(100.f, 15.f));
    } else {
        if (downloads.download_timestamp == NULL) {
            if (downloads.download_error) {
                ImGui::Text("Failed to download data.");
            } else {
                ImGui::Text("No downloaded data.");
            }
        } else {
            ImGui::Text("Downloaded on %s", ctime(&(downloads.download_timestamp)));
        }
    }
    static std::string credit = "Proudly powered by [imgui](https://github.com/ocornut/imgui), "
                                "[hello_imgui](https://github.com/pthom/hello_imgui) and "
                                "[implot](https://github.com/epezent/implot).";
    ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 420.f);
    MarkdownHelper::Markdown(credit);
    ImGui::SameLine();
}

int main(int, char **) {
    HelloImGui::RunnerParams runnerParams;
    runnerParams.appWindowParams.windowTitle = "Collector";
    runnerParams.appWindowParams.windowSize = {800, 600};
    runnerParams.imGuiWindowParams.showStatusBar = true;
    runnerParams.callbacks.ShowStatus = StatusBarGui;
    runnerParams.imGuiWindowParams.showMenuBar = true;
    runnerParams.callbacks.LoadAdditionalFonts = MarkdownHelper::LoadFonts;
    runnerParams.imGuiWindowParams.defaultImGuiWindowType = HelloImGui::DefaultImGuiWindowType::ProvideFullScreenDockSpace;

    HelloImGui::DockingSplit splitMainLeft;
    splitMainLeft.initialDock = "MainDockSpace";
    splitMainLeft.newDock = "LeftSpace";
    splitMainLeft.direction = ImGuiDir_Left;
    splitMainLeft.ratio = 0.15f;
    runnerParams.dockingParams.dockingSplits = {splitMainLeft};

    HelloImGui::DockableWindow commandsWindow;
    commandsWindow.label = "Operations";
    commandsWindow.dockSpaceName = "LeftSpace";
    commandsWindow.GuiFonction = CommandGui;

    HelloImGui::DockableWindow buildTimeWindow;
    buildTimeWindow.label = "Build time: Full picture";
    buildTimeWindow.canBeClosed = false;
    buildTimeWindow.dockSpaceName = "MainDockSpace";
    buildTimeWindow.GuiFonction = BuildTime_Plots;

    HelloImGui::DockableWindow buildTimeContemporaryWindow;
    buildTimeContemporaryWindow.label = "Build time: Contemporary images";
    buildTimeContemporaryWindow.canBeClosed = false;
    buildTimeContemporaryWindow.dockSpaceName = "MainDockSpace";
    buildTimeContemporaryWindow.GuiFonction = BuildTimeContemporary_Plots;

    runnerParams.dockingParams.dockableWindows = {commandsWindow, buildTimeWindow, buildTimeContemporaryWindow};//, chartsWindow, charts2Window, charts3Window};

    auto implotContext = ImPlot::CreateContext();
    HelloImGui::Run(runnerParams);
    ImPlot::DestroyContext(implotContext);
    return 0;
}
