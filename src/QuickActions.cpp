#include "QuickActions.h"

#include <I18n.h>

#include <vector>

#include "GlobalActions.h"
#include "activities/Activity.h"
#include "components/OptionPopup.h"

namespace QuickActions {
void showConfiguredPopup(OptionPopup& popup, const std::function<void()>& requestUpdate, ActionHandler actionHandler,
                         ActionFilter actionFilter) {
  std::vector<std::string> labels;
  std::vector<uint8_t> actions;
  labels.reserve(std::size(SETTINGS.quickActionSlots));
  actions.reserve(std::size(SETTINGS.quickActionSlots));
  for (const uint8_t action : SETTINGS.quickActionSlots) {
    const auto shortcutAction = static_cast<CrossPointSettings::SHORT_PWRBTN>(action);
    if (action == CrossPointSettings::IGNORE || !isQuickActionSlotActionAvailable(action) ||
        (actionFilter && !actionFilter(shortcutAction))) {
      continue;
    }
    labels.emplace_back(I18N.get(actionLabel(action)));
    actions.push_back(action);
  }
  if (actions.empty()) return;
  popup.show(StrId::STR_QUICK_ACTIONS, labels, 0,
             [actions = std::move(actions), actionHandler = std::move(actionHandler), &popup](const int selected) {
               if (selected >= 0 && static_cast<size_t>(selected) < actions.size()) {
                 const auto action = static_cast<CrossPointSettings::SHORT_PWRBTN>(actions[selected]);
                 // These actions read or write the current framebuffer immediately.
                 // Render the underlying screen after dismissing the popup so none of
                 // them captures or paints over the stale modal image. Other actions
                 // already schedule their own redraw and should not pay for an extra
                 // full-page render here.
                 if (action == CrossPointSettings::SHORT_PWRBTN::SLEEP ||
                     action == CrossPointSettings::SHORT_PWRBTN::QUICK_LOCK ||
                     (action == CrossPointSettings::SHORT_PWRBTN::SCREENSHOT &&
                      !activityManager.canSnapshotForSleepOverlay())) {
                   const auto updateResult = activityManager.requestUpdateAndWait();
                   if (action == CrossPointSettings::SHORT_PWRBTN::QUICK_LOCK &&
                       updateResult == RequestUpdateResult::Rendered) {
                     popup.skipPostSelectionUpdate();
                   }
                 }
                 if (actionHandler) {
                   actionHandler(action);
                 } else {
                   dispatchShortcutAction(action);
                 }
               }
             });
  requestUpdate();
}
}  // namespace QuickActions
