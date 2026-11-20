/* Vehicle history for human angle updates.
 * A new angle frame takes the current vehicle as its previous vehicle.
 * The caller supplies a human slot in the pool.
 */
#include "human_queries.h"
#include "sr_runtime.h"

/* Copy the current vehicle index to the history field. */
void setuphumanangles(int human)
{
    sr_humans[human].last_vehicle = sr_humans[human].vehicle_id;
}
