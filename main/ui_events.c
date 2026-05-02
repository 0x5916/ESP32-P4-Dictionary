#include "ui/ui.h"
#include "ui_overlays.h"

void ui_events_init(void)
{
    ui_overlays_bind_textarea(objects.ta_search);
    ui_overlays_bind_search_launcher(objects.ta_search_fake);
    ui_overlays_bind_search_back_button(objects.search_btn_back);
}
