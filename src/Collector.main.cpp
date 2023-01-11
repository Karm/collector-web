#include "ImGuiExt.h"
#include "MarkdownHelper.h"
#include "hello_imgui/hello_imgui.h"
#include "implot.h"
#include <chrono>
#include <cstdarg>
#include <emscripten/fetch.h>
#include <json.h>

#define MAX_RECORDS_ROW 1024

struct GlobalStats {
    float download_progress_bar = 0.f;
    bool download_in_progress = false;
    time_t download_timestamp;
    bool download_error = false;
} globals;

struct ParseOnce {
    ImU32 secondsTimeElapsed[MAX_RECORDS_ROW];
    ImU32 rssMB[MAX_RECORDS_ROW];
    ImU32 cpuInstructions[MAX_RECORDS_ROW];
    ImU32 secondsInGC[MAX_RECORDS_ROW];
} nativeParseOncePlus, nativeParseOnceMinus, hotspot;

struct PositionsLabels {
    ImU32 maxSeconds = 0;
    ImU32 maxMB = 0;
    ImU32 maxCPUInstructions = 0;
    ImU32 maxSecondsInGC = 0;
    char **labels;
    double positions[MAX_RECORDS_ROW];
    ImS32 elements = 0;
} pl;

inline ImU32 minmax(bool max, const ImU32 n, ...) {
    va_list p;
    va_start(p, n);
    ImU32 minmax = 0;
    for (ImU32 i = 0; i < n; i++) {
        const ImU32 tmp = va_arg(p, ImU32);
        if ((max && tmp > minmax) || !max && tmp < minmax) {
            minmax = tmp;
        }
    }
    va_end(p);
    return minmax;
}

void ShowDemo_BarPlots() {
    static bool horz = false;
    ImS32 ticks, count;
    count = pl.elements;
    static const std::string desNA = "# Demo data\n"
                                     "No dataset downloaded.\n";
    static const std::string desdata = "# Demo data\n"
                                       "This is a demo dataset, made on a laptop with [Mandrel integration tests](https://github.com/Karm/mandrel-integration-tests/tree/master/apps)\n";
    MarkdownHelper::Markdown(count > 0 ? desdata : desNA);
    ticks = count / 3;
    if (count > 0) {
        ImGui::Checkbox("Horizontal", &horz);
    }
    if (count > 0 && ImPlot::BeginPlot("Time to complete, shorter the better")) {
        ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);
        if (horz) {
            ImPlot::SetupAxesLimits(0, pl.maxSeconds + (pl.maxSeconds / 5), -0.5, ticks - 0.5, ImGuiCond_Always);
            ImPlot::SetupAxes("Run time [s]", "Mandrel version, Quarkus version", 0, ImPlotAxisFlags_Invert);
            ImPlot::SetupAxisTicks(ImAxis_Y1, pl.positions, ticks, pl.labels);
            ImPlot::PlotBars("Native ParseOnce+", nativeParseOncePlus.secondsTimeElapsed, count, 0.2, -0.2, ImPlotBarsFlags_Horizontal);
            ImPlot::PlotBars("Native ParseOnce-", nativeParseOnceMinus.secondsTimeElapsed, count, 0.2, 0, ImPlotBarsFlags_Horizontal);
            ImPlot::PlotBars("HotSpot", hotspot.secondsTimeElapsed, count, 0.2, 0.2, ImPlotBarsFlags_Horizontal);
        } else {
            ImPlot::SetupAxesLimits(-0.5, ticks - 0.5, 0, pl.maxSeconds + (pl.maxSeconds / 5), ImGuiCond_Always);
            ImPlot::SetupAxes("Mandrel version, Quarkus version", "Run time [s]");
            ImPlot::SetupAxisTicks(ImAxis_X1, pl.positions, ticks, pl.labels);
            ImPlot::PlotBars("Native ParseOnce+", nativeParseOncePlus.secondsTimeElapsed, count, 0.2, -0.2);
            ImPlot::PlotBars("Native ParseOnce-", nativeParseOnceMinus.secondsTimeElapsed, count, 0.2, 0);
            ImPlot::PlotBars("HotSpot", hotspot.secondsTimeElapsed, count, 0.2, 0.2);
        }
        ImPlot::EndPlot();
    }
    if (count > 0 && ImPlot::BeginPlot("Peak RSS, shorter the better")) {
        ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);
        if (horz) {
            ImPlot::SetupAxesLimits(0, pl.maxMB + (pl.maxMB / 5), -0.5, ticks - 0.5, ImGuiCond_Always);
            ImPlot::SetupAxes("RSS [MB]", "Mandrel version, Quarkus version", 0, ImPlotAxisFlags_Invert);
            ImPlot::SetupAxisTicks(ImAxis_Y1, pl.positions, ticks, pl.labels);
            ImPlot::PlotBars("Native ParseOnce+", nativeParseOncePlus.rssMB, count, 0.2, -0.2, ImPlotBarsFlags_Horizontal);
            ImPlot::PlotBars("Native ParseOnce-", nativeParseOnceMinus.rssMB, count, 0.2, 0, ImPlotBarsFlags_Horizontal);
            ImPlot::PlotBars("HotSpot", hotspot.rssMB, count, 0.2, 0.2, ImPlotBarsFlags_Horizontal);
        } else {
            ImPlot::SetupAxesLimits(-0.5, ticks - 0.5, 0, pl.maxMB + (pl.maxMB / 5), ImGuiCond_Always);
            ImPlot::SetupAxes("Mandrel version, Quarkus version", "RSS [MB]");
            ImPlot::SetupAxisTicks(ImAxis_X1, pl.positions, ticks, pl.labels);
            ImPlot::PlotBars("Native ParseOnce+", nativeParseOncePlus.rssMB, count, 0.2, -0.2);
            ImPlot::PlotBars("Native ParseOnce-", nativeParseOnceMinus.rssMB, count, 0.2, 0);
            ImPlot::PlotBars("HotSpot", hotspot.rssMB, count, 0.2, 0.2);
        }
        ImPlot::EndPlot();
    }
    if (count > 0 && ImPlot::BeginPlot("Time spent in GC, shorter the better")) {
        ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);
        if (horz) {
            ImPlot::SetupAxesLimits(0, pl.maxSecondsInGC + (pl.maxSecondsInGC / 5), -0.5, ticks - 0.5, ImGuiCond_Always);
            ImPlot::SetupAxes("Seconds spent in SerialGC", "Mandrel version, Quarkus version", 0, ImPlotAxisFlags_Invert);
            ImPlot::SetupAxisTicks(ImAxis_Y1, pl.positions, ticks, pl.labels);
            ImPlot::PlotBars("Native ParseOnce+", nativeParseOncePlus.secondsInGC, count, 0.2, -0.2, ImPlotBarsFlags_Horizontal);
            ImPlot::PlotBars("Native ParseOnce-", nativeParseOnceMinus.secondsInGC, count, 0.2, 0, ImPlotBarsFlags_Horizontal);
            ImPlot::PlotBars("HotSpot", hotspot.secondsInGC, count, 0.2, 0.2, ImPlotBarsFlags_Horizontal);
        } else {
            ImPlot::SetupAxesLimits(-0.5, ticks - 0.5, 0, pl.maxSecondsInGC + (pl.maxSecondsInGC / 5), ImGuiCond_Always);
            ImPlot::SetupAxes("Mandrel version, Quarkus version", "Seconds spent in SerialGC");
            ImPlot::SetupAxisTicks(ImAxis_X1, pl.positions, ticks, pl.labels);
            ImPlot::PlotBars("Native ParseOnce+", nativeParseOncePlus.secondsInGC, count, 0.2, -0.2);
            ImPlot::PlotBars("Native ParseOnce-", nativeParseOnceMinus.secondsInGC, count, 0.2, 0);
            ImPlot::PlotBars("HotSpot", hotspot.secondsInGC, count, 0.2, 0.2);
        }
        ImPlot::EndPlot();
    }
    if (count > 0 && ImPlot::BeginPlot("CPU instructions executed, fewer the better")) {
        ImPlot::SetupLegend(ImPlotLocation_East, ImPlotLegendFlags_Outside);
        if (horz) {
            ImPlot::SetupAxesLimits(0, pl.maxCPUInstructions + (pl.maxCPUInstructions / 5), -0.5, ticks - 0.5, ImGuiCond_Always);
            ImPlot::SetupAxes("Billions of instructions", "Mandrel version, Quarkus version", 0, ImPlotAxisFlags_Invert);
            ImPlot::SetupAxisTicks(ImAxis_Y1, pl.positions, ticks, pl.labels);
            ImPlot::PlotBars("Native ParseOnce+", nativeParseOncePlus.cpuInstructions, count, 0.2, -0.2, ImPlotBarsFlags_Horizontal);
            ImPlot::PlotBars("Native ParseOnce-", nativeParseOnceMinus.cpuInstructions, count, 0.2, 0, ImPlotBarsFlags_Horizontal);
            ImPlot::PlotBars("HotSpot", hotspot.cpuInstructions, count, 0.2, 0.2, ImPlotBarsFlags_Horizontal);
        } else {
            ImPlot::SetupAxesLimits(-0.5, ticks - 0.5, 0, pl.maxCPUInstructions + (pl.maxCPUInstructions / 5), ImGuiCond_Always);
            ImPlot::SetupAxes("Mandrel version, Quarkus version", "Billions of instructions");
            ImPlot::SetupAxisTicks(ImAxis_X1, pl.positions, ticks, pl.labels);
            ImPlot::PlotBars("Native ParseOnce+", nativeParseOncePlus.cpuInstructions, count, 0.2, -0.2);
            ImPlot::PlotBars("Native ParseOnce-", nativeParseOnceMinus.cpuInstructions, count, 0.2, 0);
            ImPlot::PlotBars("HotSpot", hotspot.cpuInstructions, count, 0.2, 0.2);
        }
        ImPlot::EndPlot();
    }
}

void processJSON(const char *data, int length) {
    json_object *jobj = nullptr;
    enum json_tokener_error jerr;
    struct json_tokener *tok = json_tokener_new();
    do {
        jobj = json_tokener_parse_ex(tok, data, length);
    } while ((jerr = json_tokener_get_error(tok)) == json_tokener_continue);
    if (jerr != json_tokener_success) {
        // TODO: We should handle the error, talk to the user...
        printf("Error: %s\n", json_tokener_error_desc(jerr));
    }
    if (tok->char_offset < length) {
        printf("Something else happened...\n");
        // TODO: There are extra chars. We should abort, inform user...
    }
    if (jobj == nullptr) {
        // TODO abort, pop up a message...
        printf("JOBJ NUll....\n");
    }

    size_t elements_count = json_object_array_length(jobj);
    if (elements_count >= MAX_RECORDS_ROW) {
        printf("Do some Error pop up...  Our buffer for a row is %d, while the json has %zu records", MAX_RECORDS_ROW, elements_count);
        elements_count = MAX_RECORDS_ROW - 1;
    }

    // TODO: Use one big preallocated chunk, one big malloc at the start, one free
    // TODO: at the end. All these little mallocs and frees are killing perf.
    if (pl.labels != nullptr) {
        for (int i = 0; i < elements_count / 3; i++) {
            free(pl.labels[i]);
        }
        free(pl.labels);
    }
    pl.labels = (char **) (malloc(sizeof(char) * elements_count / 3));
    pl.elements = elements_count;
    int hotspot_c = 0;
    int nativeParseOncePlus_c = 0;
    int nativeParseOnceMinus_c = 0;

    for (int i = 0; i < elements_count; i++) {
        json_object *element = json_object_array_get_idx(jobj, i);

        json_object *secondsTimeElapsed_ = json_object_object_get(element, "secondsTimeElapsed");
        double secondsTimeElapsed = json_object_get_double(secondsTimeElapsed_);

        json_object *mandrelVersion_ = json_object_object_get(element, "mandrelVersion");
        const char *mandrelVersion = json_object_get_string(mandrelVersion_);

        json_object *quarkusVersion_ = json_object_object_get(element, "quarkusVersion");
        const char *quarkusVersion = json_object_get_string(quarkusVersion_);

        json_object *file_ = json_object_object_get(element, "file");
        const char *file = json_object_get_string(file_);

        json_object *rssKb_ = json_object_object_get(element, "rssKb");
        int rssMB = json_object_get_int(rssKb_) / 1024;

        json_object *instructionsG_ = json_object_object_get(element, "instructions");
        ImU32 instructionsG = json_object_get_uint64(instructionsG_) / 1000000000;

        json_object *secondsInGC_ = json_object_object_get(element, "timeSpentInGCs");
        int secondsInGC = json_object_get_double(secondsInGC_);

        char *version = (char *) malloc(strlen(mandrelVersion) + strlen(quarkusVersion) + 2);
        strcpy(version, mandrelVersion);
        strcat(version, ",");
        strcat(version, quarkusVersion);

        if (strncmp(file, "java", 4) == 0) {
            hotspot.secondsTimeElapsed[hotspot_c] = (ImU32) round(secondsTimeElapsed);
            hotspot.rssMB[hotspot_c] = rssMB;
            hotspot.cpuInstructions[hotspot_c] = instructionsG;
            hotspot.secondsInGC[hotspot_c] = secondsInGC;
            pl.labels[hotspot_c] = version;
            pl.positions[hotspot_c] = hotspot_c;
            hotspot_c++;
        } else if (strstr(file, "+ParseOnce")) {
            nativeParseOncePlus.secondsTimeElapsed[nativeParseOncePlus_c] = (ImU32) round(secondsTimeElapsed);
            nativeParseOncePlus.rssMB[nativeParseOncePlus_c] = rssMB;
            nativeParseOncePlus.cpuInstructions[nativeParseOncePlus_c] = instructionsG;
            nativeParseOncePlus.secondsInGC[nativeParseOncePlus_c] = secondsInGC;
            nativeParseOncePlus_c++;
        } else if (strstr(file, "-ParseOnce")) {
            nativeParseOnceMinus.secondsTimeElapsed[nativeParseOnceMinus_c] = (ImU32) round(secondsTimeElapsed);
            nativeParseOnceMinus.rssMB[nativeParseOnceMinus_c] = rssMB;
            nativeParseOnceMinus.cpuInstructions[nativeParseOnceMinus_c] = instructionsG;
            nativeParseOnceMinus.secondsInGC[nativeParseOnceMinus_c] = secondsInGC;
            nativeParseOnceMinus_c++;
        } else {
            printf("Error, do something about it. Pop up?");
        }
    }

    pl.maxSeconds = minmax(true, 3,
                           *std::max_element(hotspot.secondsTimeElapsed, hotspot.secondsTimeElapsed + hotspot_c),
                           *std::max_element(nativeParseOnceMinus.secondsTimeElapsed, nativeParseOnceMinus.secondsTimeElapsed + nativeParseOnceMinus_c),
                           *std::max_element(nativeParseOncePlus.secondsTimeElapsed, nativeParseOncePlus.secondsTimeElapsed + nativeParseOncePlus_c));
    pl.maxMB = minmax(true, 3,
                      *std::max_element(hotspot.rssMB, hotspot.rssMB + hotspot_c),
                      *std::max_element(nativeParseOnceMinus.rssMB, nativeParseOnceMinus.rssMB + nativeParseOnceMinus_c),
                      *std::max_element(nativeParseOncePlus.rssMB, nativeParseOncePlus.rssMB + nativeParseOncePlus_c));
    pl.maxCPUInstructions = minmax(true, 3,
                                   *std::max_element(hotspot.cpuInstructions, hotspot.cpuInstructions + hotspot_c),
                                   *std::max_element(nativeParseOnceMinus.cpuInstructions, nativeParseOnceMinus.cpuInstructions + nativeParseOnceMinus_c),
                                   *std::max_element(nativeParseOncePlus.cpuInstructions, nativeParseOncePlus.cpuInstructions + nativeParseOncePlus_c));
    pl.maxSecondsInGC = minmax(true, 3,
                               *std::max_element(hotspot.secondsInGC, hotspot.secondsInGC + hotspot_c),
                               *std::max_element(nativeParseOnceMinus.secondsInGC, nativeParseOnceMinus.secondsInGC + nativeParseOnceMinus_c),
                               *std::max_element(nativeParseOncePlus.secondsInGC, nativeParseOncePlus.secondsInGC + nativeParseOncePlus_c));
    json_tokener_free(tok);
}


void downloadSucceeded(emscripten_fetch_t *fetch) {
    processJSON(fetch->data, fetch->numBytes);
    emscripten_fetch_close(fetch);// Free data associated with the fetch.
    globals.download_in_progress = false;
    globals.download_progress_bar = 0;
    globals.download_error = false;
    globals.download_timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

void downloadProgress(emscripten_fetch_t *fetch) {
    globals.download_in_progress = true;
    globals.download_error = false;
    if (fetch->totalBytes) {
        //printf("Downloading %s.. %.2f%% complete.\n", fetch->url, fetch->dataOffset * 100.0 / fetch->totalBytes);
        globals.download_progress_bar = (float) fetch->dataOffset / (float) fetch->totalBytes;
    } else {
        // We don't know, so Windows 95 copy files progress bar...
        if (globals.download_progress_bar >= 1) {
            globals.download_progress_bar = 0;
        }
        globals.download_progress_bar += .01f;
        //printf("Downloading %s.. %lld bytes complete.\n", fetch->url, fetch->dataOffset + fetch->numBytes);
    }
}

void downloadFailed(emscripten_fetch_t *fetch) {
    printf("Downloading %s failed, HTTP failure status code: %d.\n", fetch->url, fetch->status);
    emscripten_fetch_close(fetch);// Also free data on failure.
    globals.download_error = true;
    globals.download_progress_bar = 0;
    globals.download_in_progress = false;
    globals.download_timestamp = NULL;
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
    //ImGui::SameLine();
    if (ImGui::Button("Download dataset now")) {
        //state.rocketState = AppState::RocketState::Preparing;
        //state.rocket_progress = state.rocket_progress + 0.4f;
        if (!globals.download_in_progress) {
            emscripten_fetch_attr_t attr;
            emscripten_fetch_attr_init(&attr);
            strcpy(attr.requestMethod, "GET");
            attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
            attr.onsuccess = downloadSucceeded;
            attr.onerror = downloadFailed;
            attr.onprogress = downloadProgress;
            emscripten_fetch(&attr, "perf_data.json");
        }
    }
    if (ImGui::IsItemHovered() && globals.download_in_progress) {
        ImGui::SetTooltip("Download is already in progress...");
    }
}

//void MainWindow(AppState &state) {
void MainWindow() {
    //ImGui::Begin("Charts", nullptr, ImGuiWindowFlags_MenuBar);
    //ImGui::Text("Downloaded text: %s", test_data);
    ShowDemo_BarPlots();
}

//void StatusBarGui(const AppState &appState) {
void StatusBarGui() {
    //  if (appState.rocketState == AppState::RocketState::Preparing) {
    //    ImGui::Text("Rocket completion: ");
    //  ImGui::SameLine();
    //ImGui::ProgressBar(appState.rocket_progress, ImVec2(100.f, 15.f));
    //}
    ImGui::Text("Dataset status:");
    ImGui::SameLine();
    if (globals.download_in_progress) {
        ImGui::ProgressBar(globals.download_progress_bar, ImVec2(100.f, 15.f));
    } else {
        if (globals.download_timestamp == NULL) {
            if (globals.download_error) {
                ImGui::Text("Failed to download data.");
            } else {
                ImGui::Text("No downloaded data.");
            }
        } else {
            ImGui::Text("Downloaded on %s", ctime(&(globals.download_timestamp)));
        }
    }
    static std::string credit = "Proudly powered by [imgui](https://github.com/ocornut/imgui), "
                                "[hello_imgui](https://github.com/pthom/hello_imgui) and "
                                "[implot](https://github.com/epezent/implot).";
    ImGui::SameLine(ImGui::GetIO().DisplaySize.x - 420.f);
    //ImGui::SameLine();
    MarkdownHelper::Markdown(credit);
    ImGui::SameLine();
}

int main(int, char **) {
    //AppState appState;
    HelloImGui::RunnerParams runnerParams;
    runnerParams.appWindowParams.windowTitle = "Collector";
    runnerParams.appWindowParams.windowSize = {800, 600};
    runnerParams.imGuiWindowParams.showStatusBar = true;
    //runnerParams.callbacks.ShowStatus = [&appState] { StatusBarGui(appState); };
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
    //commandsWindow.GuiFonction = [&appState]() { CommandGui(appState); };
    commandsWindow.GuiFonction = CommandGui;
    HelloImGui::DockableWindow chartsWindow;
    chartsWindow.label = "ParseOnce charts";
    chartsWindow.canBeClosed = false;
    chartsWindow.dockSpaceName = "MainDockSpace";
    //chartsWindow.GuiFonction = [&appState] { MainWindow(appState); };
    chartsWindow.GuiFonction = MainWindow;
    runnerParams.dockingParams.dockableWindows = {commandsWindow, chartsWindow};
    auto implotContext = ImPlot::CreateContext();
    HelloImGui::Run(runnerParams);
    ImPlot::DestroyContext(implotContext);
    return 0;
}
