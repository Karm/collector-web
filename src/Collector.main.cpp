#include "ImGuiExt.h"
#include "MarkdownHelper.h"
#include "emscripten_browser_clipboard.h"
#include "hello_imgui/hello_imgui.h"
#include "implot.h"
#include "implot_internal.h"
#include <chrono>
#include <cstdarg>
#include <emscripten.h>
#include <emscripten/fetch.h>
#include <iomanip>
#include <json.h>

#define MAX_RECORDS_ROW 1024
#define MAX_LABEL_LENGTH 256
#define MAX_MARGIN(x) (x + x / 3)
// Visual, chart labeling tooltip
#define TOOLTIP_REGION 0.12
#define MAX_ATTRIBUTE_LENGTH 24

struct BTimePerf {
    double data[MAX_RECORDS_ROW]{};
    char name[MAX_ATTRIBUTE_LENGTH]{};
    bool visible = true;
    double max = 0;
};

/**
 * Keep these indexes aligned with the order in bTimePerfData[].
 */
#define TOTAL_BUILD_TIME_MS_IDX 0
#define GC_TOTAL_MS_IDX 1
#define PEAK_RSS_BYTES_IDX 2
#define IMAGE_HEAP_BYTES_IDX 3
#define CODE_AREA_BYTES_IDX 4
#define IMAGE_TOTAL_BYTES_IDX 5
#define CLASSES_JNI_IDX 6
#define CLASSES_REACHABLE_IDX 7
#define CLASSES_REFLECTION_IDX 8
#define CLASSES_TOTAL_IDX 9
#define FIELDS_JNI_IDX 10
#define FIELDS_REACHABLE_IDX 11
#define FIELDS_REFLECTION_IDX 12
#define FIELDS_TOTAL_IDX 13
#define METHODS_JNI_IDX 14
#define METHODS_REACHABLE_IDX 15
#define METHODS_REFLECTION_IDX 16
#define METHODS_TOTAL_IDX 17
#define RESOURCES_BYTES_IDX 18
#define RESOURCES_COUNT_IDX 19
BTimePerf bTimePerfData[]{
    {.name = "total_build_time_ms"},
    {.name = "gc_total_ms"},
    {.name = "peak_rss_bytes"},
    {.name = "image_heap_bytes"},
    {.name = "code_area_bytes"},
    {.name = "image_total_bytes"},
    {.name = "classes_jni"},
    {.name = "classes_reachable"},
    {.name = "classes_reflection"},
    {.name = "classes_total"},
    {.name = "fields_jni"},
    {.name = "fields_reachable"},
    {.name = "fields_reflection"},
    {.name = "fields_total"},
    {.name = "methods_jni"},
    {.name = "methods_reachable"},
    {.name = "methods_reflection"},
    {.name = "methods_total"},
    {.name = "resources_bytes"},
    {.name = "resources_count"}};

struct BTimePerfMeta {
    const size_t number_of_attributes = *(&bTimePerfData + 1) - bTimePerfData;
    ImU32 sorted_by = TOTAL_BUILD_TIME_MS_IDX;
    char labels[MAX_RECORDS_ROW][MAX_LABEL_LENGTH]{};
    double positions[MAX_RECORDS_ROW]{};
    ImU32 elements = 0;
} bTimePerfMeta;

struct Downloads {
    float download_progress_bar = 0.f;
    bool download_in_progress = false;
    time_t download_timestamp{};
    bool download_error = false;
    char api_token[129]{};
    bool has_api_token = false;
    bool api_key_popup = true;
} downloads;

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

void swap_double(double *a, double *b) {
    const double temp = *a;
    *a = *b;
    *b = temp;
}

void swap_str(char *a, char *b) {
    char temp[MAX_LABEL_LENGTH];
    strncpy(temp, a, MAX_LABEL_LENGTH);
    strncpy(a, b, MAX_LABEL_LENGTH);
    strncpy(b, temp, MAX_LABEL_LENGTH);
}

int partition(double arr[], int low, int high, ImU32 selected_attribute) {
    const double pivot = arr[high];
    int i = low - 1;
    for (int j = low; j <= high - 1; j++) {
        if (arr[j] < pivot) {
            i++;
            swap_double(&arr[i], &arr[j]);
            // Swap corresponding elements in other arrays
            for (int k = 0; k < bTimePerfMeta.number_of_attributes; k++) {
                if (k != selected_attribute) {
                    swap_double(&bTimePerfData[k].data[i], &bTimePerfData[k].data[j]);
                }
            }
            swap_str(bTimePerfMeta.labels[i], bTimePerfMeta.labels[j]);
        }
    }
    swap_double(&arr[i + 1], &arr[high]);
    // Swap corresponding elements in other arrays
    for (int k = 0; k < bTimePerfMeta.number_of_attributes; k++) {
        if (k != selected_attribute) {
            swap_double(&bTimePerfData[k].data[i + 1], &bTimePerfData[k].data[high]);
        }
    }
    swap_str(bTimePerfMeta.labels[i + 1], bTimePerfMeta.labels[high]);
    return (i + 1);
}

/**
 * This is your usual quicksort, naive as in a textbook, no optimizations.
 * In addition to that, it shuffles data in all our data arrays to stay aligned,
 * i.e. if data x used to be on index 3 and now it's on index 4, this move takes
 * place in all arrays, not just the one we are sorting.
 * @param arr
 * @param low
 * @param high
 * @param selected_attribute
 */
void qsort_shuffle(double arr[], int low, int high, ImU32 selected_attribute) {
    if (low < high) {
        const int pi = partition(arr, low, high, selected_attribute);
        qsort_shuffle(arr, low, pi - 1, selected_attribute);
        qsort_shuffle(arr, pi + 1, high, selected_attribute);
    }
}

void sort_and_shuffle(ImU32 selected_attribute) {
    if (selected_attribute >= bTimePerfMeta.number_of_attributes) {
        printf("Invalid selected_attribute index.\n");
        return;
    }
    if (bTimePerfMeta.elements < 1) {
        return;
    }
    bTimePerfMeta.sorted_by = selected_attribute;
    qsort_shuffle(bTimePerfData[selected_attribute].data, 0, (int) bTimePerfMeta.elements - 1, selected_attribute);
}

/**
 * We use this to draw a rectangle around x axes indices when cursor is near.
 * @param mouse_x
 * @return
 */
inline bool cursorWithinXRegion(double mouse_x) {
    return abs(mouse_x - (int) round(mouse_x)) < TOOLTIP_REGION;
}

void plotBuildTime(int row, int col, ImU32 attribute_index) {
    const double *values = bTimePerfData[attribute_index].data;
    const double ymax = MAX_MARGIN(bTimePerfData[attribute_index].max);
    static char title[32]{};
    sprintf(title, "## row %d col %d i %d", row, col, attribute_index);
    static ImPlotRect lims(0, 12, 0, 1);
    if (ImPlot::BeginAlignedPlots("AlignedGroup", true)) {
        if (ImPlot::BeginPlot(title, ImVec2(-1, 0), ImPlotFlags_NoCentralMenu)) {
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
            ImPlot::SetupAxisTicks(ImAxis_X1, bTimePerfMeta.positions, (int) bTimePerfMeta.elements);
            if (attribute_index != bTimePerfMeta.sorted_by) {
                ImPlot::SetNextLineStyle(quarkus_magenta_color, 1);
            } else {
                ImPlot::SetNextLineStyle(quarkus_blue_color, 1);
            }

            const char *y_label = bTimePerfData[attribute_index].name;
            // Drawing the actual line chart
            ImPlot::PlotLine(y_label, values, (int) bTimePerfMeta.elements);

            // Rectangle marking a point on x axes for all charts
            static int round_mouse_x_all = 0;
            ImPlot::PushPlotClipRect();
            const float label_rect_left = ImPlot::PlotToPixels(round_mouse_x_all - TOOLTIP_REGION, 0).x;
            const float label_rect_right = ImPlot::PlotToPixels(round_mouse_x_all + TOOLTIP_REGION, 0).x;
            const float label_rect_top = ImPlot::GetPlotPos().y;
            const float label_rect_bottom = label_rect_top + ImPlot::GetPlotSize().y;
            ImDrawList *draw_list = ImPlot::GetPlotDrawList();
            draw_list->AddRectFilled(
                ImVec2(label_rect_left, label_rect_top), ImVec2(label_rect_right, label_rect_bottom),
                IM_COL32(70, 149, 235, 100));
            ImPlot::PopPlotClipRect();
            // Tooltip
            const ImPlotPoint mouse = ImPlot::GetPlotMousePos();
            if (ImPlot::IsPlotHovered() && cursorWithinXRegion(mouse.x)) {
                int round_mouse_x = (int) round(mouse.x);
                if (round_mouse_x < 0) {
                    round_mouse_x = 0;
                } else if (round_mouse_x >= bTimePerfMeta.elements) {
                    round_mouse_x = (int) bTimePerfMeta.elements - 1;
                }
                round_mouse_x_all = round_mouse_x;
                ImGui::BeginTooltip();
                ImGui::Text("%s", bTimePerfMeta.labels[round_mouse_x]);
                ImGui::EndTooltip();
            }

            if (ImPlot::BeginCustomContext()) {
                if (ImGui::MenuItem("Sort by ", y_label)) {
                    sort_and_shuffle(attribute_index);
                }
                if (ImGui::MenuItem("Hide ", y_label)) {
                    bTimePerfData[attribute_index].visible = false;
                }
                ImPlot::EndCustomContext(true);// true = append standard menu
            }
            ImPlot::EndPlot();
        }
        ImPlot::EndAlignedPlots();
    }
}

void BuildTime_Plots() {
    if (bTimePerfMeta.elements > 0) {
        char first[IMPLOT_LABEL_MAX_SIZE];
        char last[IMPLOT_LABEL_MAX_SIZE];
        char gap[IMPLOT_LABEL_MAX_SIZE];
        const double gap_value = bTimePerfData[bTimePerfMeta.sorted_by].data[bTimePerfMeta.elements - 1] - bTimePerfData[bTimePerfMeta.sorted_by].data[0];
        if (bTimePerfMeta.sorted_by == CODE_AREA_BYTES_IDX || bTimePerfMeta.sorted_by == IMAGE_HEAP_BYTES_IDX ||
            bTimePerfMeta.sorted_by == IMAGE_TOTAL_BYTES_IDX || bTimePerfMeta.sorted_by == PEAK_RSS_BYTES_IDX ||
            bTimePerfMeta.sorted_by == RESOURCES_BYTES_IDX) {
            BytesFormatter(bTimePerfData[bTimePerfMeta.sorted_by].data[0], first, sizeof(first), (void *) "B");
            BytesFormatter(bTimePerfData[bTimePerfMeta.sorted_by].data[bTimePerfMeta.elements - 1], last, sizeof(last), (void *) "B");
            BytesFormatter(gap_value, gap, sizeof(gap), (void *) "B");
        } else if (bTimePerfMeta.sorted_by == TOTAL_BUILD_TIME_MS_IDX || bTimePerfMeta.sorted_by == GC_TOTAL_MS_IDX) {
            SecondsFormatter(bTimePerfData[bTimePerfMeta.sorted_by].data[0], first, sizeof(first), (void *) "s");
            SecondsFormatter(bTimePerfData[bTimePerfMeta.sorted_by].data[bTimePerfMeta.elements - 1], last, sizeof(last), (void *) "s");
            SecondsFormatter(gap_value, gap, sizeof(gap), (void *) "s");
        } else {
            snprintf(first, sizeof(first), "%4.2f", bTimePerfData[bTimePerfMeta.sorted_by].data[0]);
            snprintf(last, sizeof(last), "%4.2f", bTimePerfData[bTimePerfMeta.sorted_by].data[bTimePerfMeta.elements - 1]);
            snprintf(gap, sizeof(gap), "%4.2f", gap_value);
        }

        ImGui::BeginGroup();
        {
            //Could be useful, but would need to call the sorting function too...
            //ImGui::Combo("Dataset sorted by: ", &buildTimeLabels.sorted_by, ATTRIBUTE_NAMES, IM_ARRAYSIZE(ATTRIBUTE_NAMES));
            ImGui::Text("Dataset sorted by: %s", bTimePerfData[bTimePerfMeta.sorted_by].name);
            ImGui::BulletText("First in dataset: %s\nValue: %s", bTimePerfMeta.labels[0], first);
            ImGui::BulletText("Last in dataset: %s\nValue: %s", bTimePerfMeta.labels[bTimePerfMeta.elements - 1], last);
            ImGui::BulletText("Gap between the first and the last: %s", gap);
        }
        ImGui::EndGroup();
        ImGui::SameLine();
        ImGui::BeginGroup();
        {
            for (int i = 0; i < bTimePerfMeta.number_of_attributes; i++) {
                static char title[MAX_ATTRIBUTE_LENGTH]{};
                sprintf(title, "%-*s", MAX_ATTRIBUTE_LENGTH, bTimePerfData[i].name);
                ImGui::Checkbox(title, &bTimePerfData[i].visible);
                if ((i + 1) % 4 != 0) { ImGui::SameLine(); }
            }
            if (ImGui::Button("Hide all")) {
                for (int i = 0; i < bTimePerfMeta.number_of_attributes; i++) {
                    bTimePerfData[i].visible = false;
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Show all")) {
                for (int i = 0; i < bTimePerfMeta.number_of_attributes; i++) {
                    bTimePerfData[i].visible = true;
                }
            }
        }
        ImGui::EndGroup();

        static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
                                       ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable;
        if (ImGui::BeginTable("##table 3", 3, flags, ImVec2(-1, 0))) {
            ImGui::TableSetupColumn("##col1", ImGuiTableColumnFlags_WidthStretch, 33.0f);
            ImGui::TableSetupColumn("##col2", ImGuiTableColumnFlags_WidthStretch, 33.0f);
            ImGui::TableSetupColumn("##col3", ImGuiTableColumnFlags_WidthStretch, 33.0f);
            ImGui::TableHeadersRow();
            ImPlot::PushColormap(ImPlotColormap_Deep);

            int column_index = 3;
            int row_index = -1;
            for (int i = 0; i < bTimePerfMeta.number_of_attributes; i++) {
                if (bTimePerfData[i].visible) {
                    if (column_index > 2) {
                        ImGui::TableNextRow();
                        column_index = 0;
                        row_index++;
                    }
                    ImGui::TableSetColumnIndex(column_index);
                    plotBuildTime(row_index, column_index, i);
                    column_index++;
                }
            }
        }

        ImPlot::PopColormap();
        ImGui::EndTable();
    }
}

void processBuildtimeJSON(const char *data, uint64_t length) {
    bTimePerfMeta.elements = 0;
    json_object *root;
    enum json_tokener_error jerr;
    struct json_tokener *tok = json_tokener_new();
    root = json_tokener_parse_ex(tok, data, (int) length);
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

    const ImU64 array_length = min(2, (ImU64) json_object_array_length(tag_array), (ImU64) MAX_RECORDS_ROW);
    for (int j = 0; j < array_length; j++) {
        char *tag_ = const_cast<char *>(json_object_get_string(json_object_array_get_idx(tag_array, j)));
        const char *created_at_ = json_object_get_string(json_object_array_get_idx(created_array, j));
        char *tmp_ptr;
        char *builder_image = strtok_r(tag_, ",", &tmp_ptr);
        char *quarkus_version = strtok_r(nullptr, ",", &tmp_ptr);
        memset(bTimePerfMeta.labels[j], 0, MAX_LABEL_LENGTH);
        sprintf(bTimePerfMeta.labels[j], "Image: %s\nQuarkus: %s\nTimestamp: %s", builder_image, quarkus_version, created_at_);
        bTimePerfMeta.positions[j] = j;// 1 element has 1 position
    }

    // TODO: Full picture - would new builds replace the older ones?
    for (int i = 0; i < bTimePerfMeta.number_of_attributes; i++) {
        const json_object *array = json_object_object_get(root, bTimePerfData[i].name);
        for (int j = 0; j < array_length; j++) {
            bTimePerfData[i].data[j] = json_object_get_double(json_object_array_get_idx(array, j));
            if (bTimePerfData[i].data[j] > bTimePerfData[i].max) {
                bTimePerfData[i].max = bTimePerfData[i].data[j];
            }
        }
    }

    sort_and_shuffle(TOTAL_BUILD_TIME_MS_IDX);
    json_tokener_free(tok);
    bTimePerfMeta.elements = array_length;
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
        downloads.download_progress_bar = (float) fetch->dataOffset / (float) fetch->totalBytes;
    } else {
        // We don't know, so Windows 95 copy files progress bar...
        if (downloads.download_progress_bar >= 1) {
            downloads.download_progress_bar = 0;
        }
        downloads.download_progress_bar += .01f;
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

EM_JS(void, setCookie, (const char *name, const char *value), {
    var d = new Date();
    d.setDate(d.getDate() + 365);
    document.cookie = UTF8ToString(name) + "=" + UTF8ToString(value) + ";SameSite=Strict;expires=" + d.toUTCString() + ";path=/";
});

EM_JS(char *, getCookie, (const char *name), {
    var cookies = document.cookie.split(';');
    var name_ = UTF8ToString(name);
    for (var i = 0; i < cookies.length; i++) {
        var cookie = cookies[i].trim();
        if (cookie.indexOf(name_ + "=") == 0) {
            var cookieValue = cookie.substring(name_.length + 1, cookie.length);
            var lengthBytes = lengthBytesUTF8(cookieValue) + 1;
            var stringOnWasmHeap = _malloc(lengthBytes);
            stringToUTF8(cookieValue, stringOnWasmHeap, lengthBytes);
            return stringOnWasmHeap;
        }
    }
    return null;
});

static std::string clipboard_content;
char const *get_content_for_imgui(void *user_data [[maybe_unused]]) {
    //printf("get_content_for_imgui: %s\n", clipboard_content.c_str());
    return clipboard_content.c_str();
}

void set_content_from_imgui(void *user_data [[maybe_unused]], char const *text) {
    clipboard_content = text;
    //printf("set_content_from_imgui: %s\n", clipboard_content.c_str());
    emscripten_browser_clipboard::copy(clipboard_content);
}

void APIKeyModal() {
    if (downloads.api_key_popup) {
        ImGui::OpenPopup("API key");
    }
    if (ImGui::BeginPopupModal("API key", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char *cookieValue;
        static bool firstTime = true;
        if (cookieValue == nullptr && firstTime) {
            cookieValue = getCookie("apikey");
            if (cookieValue != nullptr) {
                strncpy(downloads.api_token, cookieValue, min(2, (ImU64) IM_ARRAYSIZE(downloads.api_token), (ImU64) strlen(cookieValue)));
            }
            firstTime = false;
        }
        // Multiline text input might look better, but the pasted text doesn't wrap, because token is
        // a single giant word. TODO: Make it wrap nicely.
        // ImGui::InputTextMultiline("##APIKey", api_token, IM_ARRAYSIZE(api_token), ImVec2(ImGui::GetTextLineHeight() * 16, ImGui::GetTextLineHeight() * 16));
        ImGui::TextWrapped("Your API key is stored as a cookie. Don't have one? Drop an email to karm@redhat.com");
        ImGui::Separator();
        ImGui::Text("Current API key:");
        ImGui::Separator();
        ImGui::TextWrapped("%s", downloads.api_token);
        ImGui::Separator();
        ImGui::Text("Insert your API key:");
        ImGui::Separator();
        ImGui::PushItemWidth(249);
        ImGui::InputText("##APIKey", downloads.api_token, IM_ARRAYSIZE(downloads.api_token));
        ImGui::PopItemWidth();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            setCookie("apikey", downloads.api_token);
            if (cookieValue != nullptr) {
                free(cookieValue);
                cookieValue = nullptr;
            }
            ImGui::CloseCurrentPopup();
            downloads.api_key_popup = false;
            downloads.has_api_token = strlen(downloads.api_token) > 0;
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
            downloads.api_key_popup = false;
        }
        ImGui::EndPopup();
    }
}

inline void LoadAPIKey() {
    static char *cookieValue;
    static bool firstTime = true;
    if (cookieValue == nullptr && firstTime) {
        cookieValue = getCookie("apikey");
        if (cookieValue != nullptr && strlen(cookieValue) > 0) {
            strncpy(downloads.api_token, cookieValue, min(2, (ImU64) IM_ARRAYSIZE(downloads.api_token), (ImU64) strlen(cookieValue)));
            downloads.api_key_popup = false;
            downloads.has_api_token = true;
        }
        firstTime = false;
    }
    APIKeyModal();
}

//void CommandGui(AppState &state) {
void CommandGui() {
    LoadAPIKey();
    HelloImGui::ImageFromAsset("quarkus.png");
    ImGui::SameLine();
    ImGui::TextWrapped("Quarkus Mandrel\nStats, charts, operations");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Quarkus Mandrel perf charts demo...");
    }
    ImGui::Separator();
    if (ImGui::Button("Download buildtime dataset now")) {
        if (!downloads.download_in_progress) {
            emscripten_fetch_attr_t attr;
            emscripten_fetch_attr_init(&attr);
            strcpy(attr.requestMethod, "GET");
            const char *headers[] = {"Content-Type", "application/json", "token", downloads.api_token, 0};
            attr.requestHeaders = headers;
            attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
            attr.onsuccess = downloadBuildtimeSucceeded;
            attr.onerror = downloadFailed;
            attr.onprogress = downloadProgress;
            //emscripten_fetch(&attr, "http://127.0.0.1:8080/api/v1/image-stats/experiment/build-perf-karm-1.0.0-runner");
            emscripten_fetch(&attr, "https://stage-collector.foci.life/api/v1/image-stats/experiment/build-perf-karm-1.0.0-runner");
            //emscripten_fetch(&attr, "build_perf_data.json");
        }
    }
    if (ImGui::IsItemHovered() && downloads.download_in_progress) {
        ImGui::SetTooltip("Download is already in progress...");
    }
    if (bTimePerfMeta.elements > 0 && !downloads.download_in_progress) {
        static bool show_legend = false;
        ImGui::Checkbox("List the whole legend", &show_legend);
        if (show_legend) {
            // TODO: Impacts FPS when there are many records, we should pre-construct the string and cache it.
            for (int i = 0; i < bTimePerfMeta.elements; i++) {
                ImGui::TextWrapped("%d%s - %s", i, (i < 10 ? " " : ""), bTimePerfMeta.labels[i]);
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
    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();
    ImGui::Text("API token: %s", downloads.has_api_token ? "Set" : "Missing");
    static std::string credit = "Proudly powered by [imgui](https://github.com/ocornut/imgui), "
                                "[hello_imgui](https://github.com/pthom/hello_imgui) and "
                                "[implot](https://github.com/epezent/implot).";
    ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 420.f);
    MarkdownHelper::Markdown(credit);
    ImGui::SameLine();
}

int main(int, char **) {
    // Propagates clipboard data from the browser as there is no native API for it.
    emscripten_browser_clipboard::paste([](std::string const &paste_data, void *callback_data [[maybe_unused]]) {
        // printf("Paste callback: %s\n", paste_data.c_str());
        // IDE says that this move has no effect. It has effect in my WASM build.
        clipboard_content = std::move(paste_data);
    });
    HelloImGui::RunnerParams runnerParams;
    runnerParams.appWindowParams.windowTitle = "Collector";
    runnerParams.appWindowParams.windowSize = {800, 600};
    runnerParams.imGuiWindowParams.showStatusBar = true;
    runnerParams.callbacks.ShowStatus = StatusBarGui;
    runnerParams.imGuiWindowParams.showMenuBar = true;
    runnerParams.callbacks.ShowMenus = []() {
        if (ImGui::BeginMenu("Settings")) {
            if (ImGui::MenuItem("API Key")) {
                downloads.api_key_popup = true;
                APIKeyModal();
            }
            ImGui::EndMenu();
        }
    };
    runnerParams.callbacks.LoadAdditionalFonts = MarkdownHelper::LoadFonts;
    runnerParams.imGuiWindowParams.defaultImGuiWindowType = HelloImGui::DefaultImGuiWindowType::ProvideFullScreenDockSpace;
    runnerParams.callbacks.PostInit = []() {
        ImGuiIO &imgui_io = ImGui::GetIO();
        imgui_io.GetClipboardTextFn = get_content_for_imgui;
        imgui_io.SetClipboardTextFn = set_content_from_imgui;
    };
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

    runnerParams.dockingParams.dockableWindows = {commandsWindow, buildTimeWindow};//, buildTimeContemporaryWindow};//, chartsWindow, charts2Window, charts3Window};

    //runnerParams.fpsIdling TODO: Newer HelloImgui version needed

    auto implotContext = ImPlot::CreateContext();
    HelloImGui::Run(runnerParams);
    ImPlot::DestroyContext(implotContext);
    return 0;
}
