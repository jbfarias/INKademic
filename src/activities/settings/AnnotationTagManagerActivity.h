#pragma once

#include <FreeInkApp.h>
#include <FreeInkUIGfxRenderer.h>

#include <atomic>

#include "activities/Activity.h"
#include "components/OptionPopup.h"
#include "util/ButtonNavigator.h"

class AnnotationTagManagerActivity final : public Activity {
 public:
  AnnotationTagManagerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  using UiApp = freeink::ui::FreeInkApp<20, 4>;

  ButtonNavigator buttonNavigator;
  OptionPopup deletePopup;
  int selectedIndex = 0;
  int visibleRows = 1;
  int topIndex = 0;
  int listTop = 0;
  int listBottom = 0;
  int listRowHeight = 0;
  int listRowStep = 0;
  bool longPressHandled = false;

  freeink::ui::GfxRendererTarget uiTarget;
  UiApp app;
  std::atomic<bool> uiReady{false};

  int itemCount() const;
  void handleSelection();
  void openEditor(int tagIndex);
  void showDeletePopup(int tagIndex);
  static void listScreen(UiApp::ScreenType& screen, void* user);
  static void onRowEvent(const freeink::ui::ActionEvent& event, void* user);
  void buildListScreen(UiApp::ScreenType& screen);
};
