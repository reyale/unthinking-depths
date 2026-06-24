// Controls: Space=play/pause  ←/→=step  G=god  0/1=faction view  scroll=zoom  Q=quit

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

#include "entity.hpp"
#include "frame.hpp"
#include "grid.hpp"
#include "replay.hpp"
#include "replay_io.hpp"
#include "stats.hpp"
#include "wincheck.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

// ---- Map reconstruction ---------------------------------------------------

static game::Map map_from_log(const game::ReplayLog& log) {
  game::Map map = game::Map::make(log.map_w, log.map_h);
  for (int32_t y = 0; y < log.map_h; ++y)
    for (int32_t x = 0; x < log.map_w; ++x) {
      const auto& t = log.map_tiles[static_cast<size_t>(y * log.map_w + x)];
      if (t.terrain != game::Terrain::Open || t.resource_amount != 0)
        map.set_terrain({x, y}, t.terrain, t.resource_amount);
    }
  map.recount_passable();
  return map;
}

// ---- Color palette --------------------------------------------------------

static constexpr ImVec4 kFogBg    = {0.06f, 0.06f, 0.08f, 1.0f};
static constexpr ImVec4 kOpenBg   = {0.14f, 0.14f, 0.18f, 1.0f};
static constexpr ImVec4 kAsteroid = {0.35f, 0.30f, 0.22f, 1.0f};
static constexpr ImVec4 kNebula   = {0.22f, 0.15f, 0.35f, 1.0f};
static constexpr ImVec4 kResource = {0.15f, 0.32f, 0.15f, 1.0f};
static constexpr ImVec4 kFactionA = {0.40f, 0.70f, 1.00f, 1.0f};  // blue
static constexpr ImVec4 kFactionB = {1.00f, 0.42f, 0.32f, 1.0f};  // red

static ImU32 u32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }

// ---- Per-cell rendering data ----------------------------------------------

struct CellData {
  ImVec4  bg;
  char    glyph{'\0'};
  ImVec4  fg{};
  int32_t hp{-1}, max_hp{-1};
};

static CellData make_cell(const game::Map& map, const game::FrameState& fr,
                           int view, int32_t x, int32_t y) {
  // Visibility (mirrors snapshot.cpp fog logic)
  bool vis = (view < 0);
  if (!vis) {
    for (const auto& u : fr.units) {
      if (static_cast<int>(u.faction.value) != view) continue;
      if (std::abs(x - u.pos.x) + std::abs(y - u.pos.y) <= game::stats_for(u.type).sight)
        { vis = true; break; }
    }
    if (!vis) {
      for (const auto& s : fr.structures)
        if (static_cast<int>(s.faction.value) == view && s.pos.x == x && s.pos.y == y)
          { vis = true; break; }
    }
  }
  if (!vis) return {kFogBg};

  // Terrain background
  ImVec4 bg;
  switch (map.tile_at({x, y}).terrain) {
    case game::Terrain::Asteroid:     bg = kAsteroid; break;
    case game::Terrain::Nebula:       bg = kNebula;   break;
    case game::Terrain::ResourceNode: bg = kResource; break;
    default:                          bg = kOpenBg;   break;
  }

  // Structure (checked first; units overwrite if colocated)
  for (const auto& s : fr.structures) {
    if (s.pos.x != x || s.pos.y != y) continue;
    char g = s.type == game::StructureType::CommandCore ? 'C' :
             s.type == game::StructureType::Factory     ? 'T' : 'N';
    ImVec4 fg = s.faction.value == 0 ? kFactionA : kFactionB;
    return {bg, g, fg, s.hp, s.max_hp};
  }

  // Unit (last unit wins if multiple on same tile)
  const game::UnitFrame* top = nullptr;
  for (const auto& u : fr.units)
    if (u.pos.x == x && u.pos.y == y) top = &u;
  if (top) {
    char g = top->type == game::UnitType::Drone       ? 'd' :
             top->type == game::UnitType::Interceptor  ? 'i' :
             top->type == game::UnitType::Frigate       ? 'f' : 'a';
    ImVec4 fg = top->faction.value == 0 ? kFactionA : kFactionB;
    return {bg, g, fg, top->hp, top->max_hp};
  }

  return {bg};
}

// ---- Main -----------------------------------------------------------------

int main(int argc, char** argv) {
  if (argc < 2) {
    fprintf(stderr,
        "usage: ud_viz_imgui <replay.ud>\n"
        "  Space=play/pause  Arrows=step  G/0/1=view  Scroll=zoom  Q=quit\n");
    return 1;
  }

  game::ReplayLog log;
  try {
    log = game::read_replay_file(argv[1]);
  } catch (const game::ReplayVersionError& e) {
    fprintf(stderr, "error: %s\n       This replay requires a different engine build.\n",
            e.what());
    return 1;
  } catch (const std::exception& e) {
    fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }

  fprintf(stderr, "Re-simulating %zu ticks on %dx%d map...\n",
          log.ticks.size(), log.map_w, log.map_h);
  if (game::replay(log) != log.expected_hash)
    fprintf(stderr, "warning: hash mismatch — replay may not reproduce original match\n");

  game::Map map = map_from_log(log);
  auto frames = game::replay_frames(log);
  if (frames.empty()) { fprintf(stderr, "error: empty replay\n"); return 1; }
  fprintf(stderr, "Ready (%zu frames)\n", frames.size());

  // ---- GLFW + OpenGL setup -------------------------------------------------

  if (!glfwInit()) { fprintf(stderr, "glfwInit failed\n"); return 1; }
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

  GLFWwindow* win = glfwCreateWindow(1280, 800, "Unthinking Depths", nullptr, nullptr);
  if (!win) { glfwTerminate(); fprintf(stderr, "glfwCreateWindow failed\n"); return 1; }
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui::GetIO().IniFilename = nullptr; // don't write imgui.ini

  ImGui_ImplGlfw_InitForOpenGL(win, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  // ---- Playback state ------------------------------------------------------

  int    cur     = 0;
  int    view    = -1;   // -1=god, 0/1=faction
  bool   playing = false;
  float  fps     = 10.0f;
  float  cs      = 24.0f;  // cell size in pixels
  double last_t  = glfwGetTime();
  const int total = static_cast<int>(frames.size());

  // ---- Main loop -----------------------------------------------------------

  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
    ImGuiIO& io = ImGui::GetIO();

    // Auto-advance
    if (playing) {
      double now = glfwGetTime();
      if (now - last_t >= 1.0 / static_cast<double>(fps)) {
        if (cur + 1 < total) ++cur; else playing = false;
        last_t = now;
      }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Mouse-wheel zoom (only when not hovering an ImGui widget)
    if (!io.WantCaptureMouse)
      cs = std::clamp(cs + io.MouseWheel * 2.0f, 8.0f, 64.0f);

    // Keyboard shortcuts
    if (!io.WantCaptureKeyboard) {
      if (ImGui::IsKeyPressed(ImGuiKey_Space)) {
        playing = !playing;
        last_t = glfwGetTime();
      }
      if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && cur + 1 < total) { ++cur; playing = false; }
      if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)  && cur > 0)         { --cur; playing = false; }
      if (ImGui::IsKeyPressed(ImGuiKey_G)) view = -1;
      if (ImGui::IsKeyPressed(ImGuiKey_0)) view = 0;
      if (ImGui::IsKeyPressed(ImGuiKey_1)) view = 1;
      if (ImGui::IsKeyPressed(ImGuiKey_Q)) glfwSetWindowShouldClose(win, GLFW_TRUE);
    }

    // ---- Full-screen host window ------------------------------------------
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("##ud", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);

    // ---- Control bar -------------------------------------------------------
    const auto& fr = frames[static_cast<size_t>(cur)];

    ImGui::SetNextItemWidth(300.0f);
    char tick_fmt[32];
    snprintf(tick_fmt, sizeof(tick_fmt), "%%d / %d", total - 1);
    if (ImGui::SliderInt("##t", &cur, 0, total - 1, tick_fmt)) playing = false;
    ImGui::SameLine();
    if (ImGui::Button(playing ? "Pause" : " Play ")) { playing = !playing; last_t = glfwGetTime(); }
    ImGui::SameLine();
    if (ImGui::ArrowButton("##l", ImGuiDir_Left)  && cur > 0)      { --cur; playing = false; }
    ImGui::SameLine(0, 2);
    if (ImGui::ArrowButton("##r", ImGuiDir_Right) && cur+1 < total){ ++cur; playing = false; }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70.0f);
    ImGui::SliderFloat("fps##s", &fps, 1.0f, 30.0f, "%.0f");
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    if (ImGui::Button("God")) view = -1;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4{kFactionA.x * 0.6f, kFactionA.y * 0.6f, kFactionA.z * 0.6f, 1.0f});
    if (ImGui::Button(" A ")) view = 0;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,
        ImVec4{kFactionB.x * 0.6f, kFactionB.y * 0.6f, kFactionB.z * 0.6f, 1.0f});
    if (ImGui::Button(" B ")) view = 1;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(65.0f);
    ImGui::SliderFloat("px##z", &cs, 8.0f, 64.0f, "%.0f");
    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();
    ImGui::TextColored(kFactionA, "A  e:%-4d a:%d",
        fr.resources[0].energy, fr.resources[0].alloy);
    ImGui::SameLine();
    ImGui::TextColored(kFactionB, "B  e:%-4d a:%d",
        fr.resources[1].energy, fr.resources[1].alloy);

    if (fr.result.has_value()) {
      const auto& res = *fr.result;
      ImGui::SameLine();
      if (res.reason == game::WinReason::Draw) {
        ImGui::TextColored({0.9f, 0.85f, 0.2f, 1.0f}, " | DRAW");
      } else {
        ImVec4 wc = res.winner.value == 0 ? kFactionA : kFactionB;
        const char* why =
            res.reason == game::WinReason::BaseDestroyed      ? "base" :
            res.reason == game::WinReason::TerritoryThreshold ? "territory" : "tick cap";
        ImGui::TextColored(wc, " | %s wins (%s)",
            res.winner.value == 0 ? "A" : "B", why);
      }
    }

    ImGui::Separator();

    // ---- Scrollable grid child window ------------------------------------
    ImGui::BeginChild("##grid", {0, 0}, false, ImGuiWindowFlags_HorizontalScrollbar);
    ImDrawList* dl     = ImGui::GetWindowDrawList();
    ImVec2      origin = ImGui::GetCursorScreenPos();

    for (int32_t y = 0; y < map.height; ++y) {
      for (int32_t x = 0; x < map.width; ++x) {
        CellData cd = make_cell(map, fr, view, x, y);
        ImVec2 p0 = {origin.x + x * cs, origin.y + y * cs};
        ImVec2 p1 = {p0.x + cs - 1.0f,  p0.y + cs - 1.0f};

        dl->AddRectFilled(p0, p1, u32(cd.bg));

        if (cd.glyph) {
          char buf[2] = {cd.glyph, '\0'};
          ImVec2 tsz = ImGui::CalcTextSize(buf);
          ImVec2 tp  = {p0.x + (cs - tsz.x) * 0.5f, p0.y + (cs - tsz.y) * 0.5f};
          dl->AddText(tp, u32(cd.fg), buf);

          // HP bar along the bottom edge of the cell
          if (cd.hp >= 0 && cd.max_hp > 0 && cs >= 14.0f) {
            float ratio = static_cast<float>(cd.hp) / static_cast<float>(cd.max_hp);
            float bar_h = std::max(2.0f, cs * 0.08f);
            ImVec2 b0   = {p0.x, p1.y - bar_h};
            ImVec2 b1   = {p1.x, p1.y};
            dl->AddRectFilled(b0, b1, u32({0.12f, 0.12f, 0.12f, 1.0f}));
            // green → yellow → red
            ImVec4 hc = ratio > 0.5f
                ? ImVec4{0.2f + (1.0f - ratio) * 1.6f, 0.85f, 0.2f, 1.0f}
                : ImVec4{1.0f, ratio * 1.7f, 0.1f, 1.0f};
            dl->AddRectFilled(b0, {p0.x + (cs - 1.0f) * ratio, p1.y}, u32(hc));
          }
        }
      }
    }

    // Reserve layout space so the child window scroll extents are correct
    ImGui::Dummy({map.width * cs, map.height * cs});
    ImGui::EndChild();

    ImGui::End();

    // ---- Render ----------------------------------------------------------
    ImGui::Render();
    int fb_w, fb_h;
    glfwGetFramebufferSize(win, &fb_w, &fb_h);
    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(win);
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}
