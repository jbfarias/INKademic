#include "AnnotationTagManagerActivity.h"

#include <Memory.h>

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>

#include <algorithm>
#include <array>
#include <string>

#include "AnnotationTagStore.h"
#include "MappedInputManager.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/TouchHeaderBackButton.h"
#include "components/UITheme.h"
#include "components/UIThemeTokens.h"
#include "components/UiAppHelpers.h"

namespace fui = freeink::ui;

namespace {
constexpr fui::ActionId ACTION_ROW = 1;
constexpr unsigned long TAG_DELETE_HOLD_MS = 1000;

std::string trimTagName(const std::string& value) {
  const size_t first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return {};
  const size_t last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}
}

AnnotationTagManagerActivity::AnnotationTagManagerActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
    : Activity("AnnotationTagManager", renderer, mappedInput),
      uiTarget(makeUiTarget(renderer)),
      app(uiTarget, uiTarget.deviceContext()) {}

int AnnotationTagManagerActivity::itemCount() const {
  return static_cast<int>(ANNOTATION_TAGS.count()) +
         (ANNOTATION_TAGS.count() < ANNOTATION_TAG_MAX ? 1 : 0);
}

void AnnotationTagManagerActivity::onEnter() {
  Activity::onEnter();
  ANNOTATION_TAGS.load();
  selectedIndex = 0;
  topIndex = 0;
  visibleRows = 1;
  longPressHandled = false;
  uiReady = false;
  app.setTheme(uiThemeTokens(uiTarget));
  app.on(ACTION_ROW, &AnnotationTagManagerActivity::onRowEvent, this);
  app.setScreen(&AnnotationTagManagerActivity::listScreen, this);
  requestUpdate();
}

void AnnotationTagManagerActivity::onExit() { Activity::onExit(); }

void AnnotationTagManagerActivity::onRowEvent(const fui::ActionEvent& event, void* user) {
  auto* self = static_cast<AnnotationTagManagerActivity*>(user);
  if (event.value < 0 || event.value >= self->itemCount()) return;
  self->selectedIndex = event.value;
  self->app.clearTapFlash();
  self->handleSelection();
}

void AnnotationTagManagerActivity::openEditor(const int tagIndex) {
  std::string initial;
  if (tagIndex >= 0) {
    const AnnotationTag* tag = ANNOTATION_TAGS.at(static_cast<uint8_t>(tagIndex));
    if (!tag) return;
    initial = tag->name;
  }

  auto editor = makeUniqueNoThrow<KeyboardEntryActivity>(
      renderer, mappedInput, tr(STR_TAG_NAME), initial, ANNOTATION_TAG_NAME_MAX - 1, InputType::Text, 1);
  if (!editor) {
    LOG_ERR("TAGS", "OOM allocating tag keyboard (%u bytes)", static_cast<unsigned>(sizeof(KeyboardEntryActivity)));
    return;
  }
  startActivityForResult(std::move(editor), [this, tagIndex](const ActivityResult& result) {
    if (!result.isCancelled) {
      const auto* keyboard = std::get_if<KeyboardResult>(&result.data);
      if (keyboard) {
        const std::string name = trimTagName(keyboard->text);
        const bool saved = tagIndex < 0 ? ANNOTATION_TAGS.add(name.c_str())
                                        : ANNOTATION_TAGS.rename(static_cast<uint8_t>(tagIndex), name.c_str());
        if (!saved) LOG_ERR("TAGS", "Could not save tag");
      }
    }
    requestUpdate();
  });
}

void AnnotationTagManagerActivity::showDeletePopup(const int tagIndex) {
  if (tagIndex < 0 || tagIndex >= static_cast<int>(ANNOTATION_TAGS.count())) return;
  const char* options[] = {tr(STR_CANCEL), tr(STR_DELETE)};
  deletePopup.show(tr(STR_DELETE_TAG), options, 2, 0, [this, tagIndex](const int optionIndex) {
    if (optionIndex == 1 && !ANNOTATION_TAGS.remove(static_cast<uint8_t>(tagIndex))) {
      LOG_ERR("TAGS", "Could not delete tag");
    }
    selectedIndex = std::min(selectedIndex, std::max(0, itemCount() - 1));
    requestUpdate();
  });
  requestUpdate();
}

void AnnotationTagManagerActivity::handleSelection() {
  const int tagCount = static_cast<int>(ANNOTATION_TAGS.count());
  if (selectedIndex == tagCount) {
    openEditor(-1);
  } else if (selectedIndex >= 0 && selectedIndex < tagCount) {
    openEditor(selectedIndex);
  }
}

void AnnotationTagManagerActivity::loop() {
  if (deletePopup.handleInput(mappedInput, [this] { requestUpdate(); })) return;
  const int tagCount = static_cast<int>(ANNOTATION_TAGS.count());

  if (TouchHeaderBackButton::wasTapped(mappedInput, renderer) ||
      mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    finishAfterBackPress();
    return;
  }

  if (mappedInput.isPressed(MappedInputManager::Button::Confirm) &&
      mappedInput.getHeldTime() >= TAG_DELETE_HOLD_MS) {
    if (!longPressHandled && selectedIndex < static_cast<int>(ANNOTATION_TAGS.count())) {
      longPressHandled = true;
      showDeletePopup(selectedIndex);
    }
    return;
  }

  int tx = 0;
  int ty = 0;
  if (mappedInput.isScreenTouchLongPress(tx, ty, TAG_DELETE_HOLD_MS) && listRowStep > 0 && ty >= listTop &&
      ty < listBottom) {
    const int row = (ty - listTop) / listRowStep;
    const int touched = topIndex + row;
    if (row < visibleRows && (ty - listTop) % listRowStep < listRowHeight && touched < tagCount) {
      selectedIndex = touched;
      mappedInput.suppressNextTouchTap();
      showDeletePopup(selectedIndex);
    }
    return;
  }

  if (uiReady) {
    const fui::InputSnapshot snap = touchSnapshotFrom(mappedInput);
    if (snap.touchPressed || snap.touchReleased) {
      const auto event = app.route(snap);
      if (app.invalidated()) requestUpdate();
      if (event) return;
    }
  }

  if (mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    if (longPressHandled) {
      longPressHandled = false;
      return;
    }
    handleSelection();
    return;
  }

  const int count = itemCount();
  if (count <= 0) return;
  const auto swipe = mappedInput.wasSwipe();
  if (swipe == MappedInputManager::SwipeDir::Up || swipe == MappedInputManager::SwipeDir::Down) {
    const int delta = swipe == MappedInputManager::SwipeDir::Up ? visibleRows : -visibleRows;
    const int next = scrollListBy(topIndex, delta, visibleRows, count);
    if (next != topIndex) {
      topIndex = next;
      requestUpdate();
    }
    return;
  }

  const auto move = [this, count](const int next) {
    selectedIndex = next;
    topIndex = followListSelection(selectedIndex, topIndex, visibleRows, count);
    requestUpdate();
  };
  buttonNavigator.onNextRelease([this, count, &move] { move(ButtonNavigator::nextIndex(selectedIndex, count)); });
  buttonNavigator.onPreviousRelease(
      [this, count, &move] { move(ButtonNavigator::previousIndex(selectedIndex, count)); });
  buttonNavigator.onNextContinuous(
      [this, count, &move] { move(ButtonNavigator::nextPageIndex(selectedIndex, count, visibleRows)); });
  buttonNavigator.onPreviousContinuous(
      [this, count, &move] { move(ButtonNavigator::previousPageIndex(selectedIndex, count, visibleRows)); });
}

void AnnotationTagManagerActivity::listScreen(UiApp::ScreenType& screen, void* user) {
  static_cast<AnnotationTagManagerActivity*>(user)->buildListScreen(screen);
}

void AnnotationTagManagerActivity::buildListScreen(UiApp::ScreenType& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMargin(fui::Insets{
      static_cast<int16_t>(safe.y + metrics.topPadding + TouchHeaderBackButton::height(metrics, mappedInput)),
      static_cast<int16_t>(renderer.getScreenWidth() - safe.x - safe.width),
      static_cast<int16_t>(renderer.getScreenHeight() - safe.y - safe.height), static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  const int count = itemCount();
  if (count == 0) {
    screen.centeredText(tr(STR_NO_TAGS), screen.theme().bodyText);
    return;
  }

  std::array<fui::ListItem, ANNOTATION_TAG_MAX + 1> items{};
  for (uint8_t i = 0; i < ANNOTATION_TAGS.count(); ++i) {
    items[i].label = ANNOTATION_TAGS.at(i)->name;
    items[i].actionValue = static_cast<int16_t>(i);
  }
  if (static_cast<int>(ANNOTATION_TAGS.count()) < ANNOTATION_TAG_MAX) {
    items[ANNOTATION_TAGS.count()].label = tr(STR_ADD_TAG);
    items[ANNOTATION_TAGS.count()].actionValue = static_cast<int16_t>(ANNOTATION_TAGS.count());
  }

  fui::ListProps props;
  props.items = items.data();
  props.count = static_cast<uint16_t>(count);
  props.selectedIndex = static_cast<int16_t>(selectedIndex);
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  const fui::Rect bounds = screen.body();
  listTop = bounds.y;
  listBottom = bounds.bottom();
  const auto rows = configureUiList(props, screen.theme(), bounds);
  listRowHeight = props.rowHeight;
  listRowStep = props.rowHeight + props.rowGap;
  visibleRows = rows > 0 ? rows : 1;
  topIndex = scrollListBy(topIndex, 0, visibleRows, count);
  props.topIndex = static_cast<uint16_t>(topIndex);
  screen.list(props);
}

void AnnotationTagManagerActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const Rect header = TouchHeaderBackButton::headerRect(renderer, mappedInput);
  if (mappedInput.hasTouchHardware()) {
    TouchHeaderBackButton::draw(renderer, uiTarget, header, tr(STR_MANAGE_TAGS), false);
  } else {
    GUI.drawHeader(renderer, header, tr(STR_MANAGE_TAGS));
  }

  uiReady = false;
  app.render();
  uiReady = true;
  if (deletePopup.processRender(renderer, mappedInput)) return;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_SELECT), tr(STR_DIR_UP), tr(STR_DIR_DOWN));
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
