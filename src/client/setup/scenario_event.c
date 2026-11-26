/* Scenario events pack the round state, team states and three record words.
 * Two words combine low and high parts by addition with word wrap.
 * Event creation runs only for a host and advances the fixed event ring.
 */
#include "setup/scenario_event.h"
#include "logic.h"
#include "sr_runtime.h"

/* Pack one scenario record without changing its source fields. */
void createevent_updatescenario(int scenario)
{
    const ScenarioEventData *data;
    Event *event;
    uint32_t state;
    if (!sr_server)
        return;
    data = &sr_data_game.scenario_events[scenario];
    event = &sr_events[sr_numofevents];
    event->type = 11;
    event->tick = sr_game_ticks;
    state = (uint32_t)scenario * 16u + (uint32_t)sr_game.round_mode;
    state += (uint32_t)*(int *)TEAM_FIELD(team0_scenario_state, 0) * 0x100u;
    state += (uint32_t)*(int *)TEAM_FIELD(team0_scenario_state, 1) * 0x400u;
    state += (uint32_t)*(int *)TEAM_FIELD(team0_scenario_state, 2) * 0x1000u;
    event->param = (int)state;
    event->attach_type = (int)((uint32_t)data->first_low + (uint32_t)data->first_high * 0x10000u);
    event->attach_id = (int)((uint32_t)data->second_low + (uint32_t)data->second_high * 0x10000u);
    event->range_mode = data->third;
    sr_numofevents = (sr_numofevents + 1) & 0xffff;
}
