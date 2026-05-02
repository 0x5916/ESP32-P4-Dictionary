#include "ui/ui.h"
#include "ui_overlays.h"

void ui_events_init(void)
{
    ui_overlays_bind_textarea(objects.ta_search);
}
